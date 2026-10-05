#include "stm32f10x.h"                  // Device header
#include "encoder.h"
#include "config.h"

volatile int16_t Encoder_Count = 0;
volatile uint8_t Encoder_Key_Flag = 0;  //按键标志，1=按下

void Encoder_Init(void)
{
	/*开启时钟*/
	RCC_APB2PeriphClockCmd(ENCODER_RCC_PORT, ENABLE);		//开启GPIOB的时钟
	RCC_APB2PeriphClockCmd(ENCODER_AFIO_RCC, ENABLE);		//开启AFIO的时钟，外部中断必须开启AFIO的时钟
	/*GPIO初始化*/
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Pin = ENCODER_Pin_CH1 | ENCODER_Pin_CH2;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(ENCODER_PORT, &GPIO_InitStructure);						//将PB0和PB1引脚初始化为上拉输入
	/*AFIO选择中断引脚*/
	GPIO_EXTILineConfig(ENCODER_PORTSOURCE,ENCODER_PINSOURCE_CH1);//将外部中断的0号线映射到GPIOB，即选择PB0为外部中断引脚
	GPIO_EXTILineConfig(ENCODER_PORTSOURCE,ENCODER_PINSOURCE_CH2);//将外部中断的1号线映射到GPIOB，即选择PB1为外部中断引脚
	/*EXTI初始化*/
	/*EXTI初始化 单独配置EXTI0*/
    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = EXTI_Line0;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_Init(&EXTI_InitStructure);

    /*EXTI初始化 单独配置EXTI1*/
    EXTI_InitStructure.EXTI_Line = EXTI_Line1;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_Init(&EXTI_InitStructure);		
	
	/*NVIC配置*/
	NVIC_InitTypeDef NVIC_InitStructure;						//定义结构体变量
	NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;			//选择配置NVIC的EXTI0线
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;	//指定NVIC线路的抢占优先级为1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;			//指定NVIC线路的响应优先级为1
	NVIC_Init(&NVIC_InitStructure);								//将结构体变量交给NVIC_Init，配置NVIC外设

	NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;			//选择配置NVIC的EXTI1线
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;				//指定NVIC线路使能
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;	//指定NVIC线路的抢占优先级为1
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 2;			//指定NVIC线路的响应优先级为2
	NVIC_Init(&NVIC_InitStructure);								//将结构体变量交给NVIC_Init，配置NVIC外设
}


void Encoder_Key_EXTI_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	EXTI_InitTypeDef EXTI_InitStructure;
	NVIC_InitTypeDef NVIC_InitStructure;

	//开启时钟（GPIOB+AFIO，AFIO时钟Encoder_Init已经开了，这里可重复开，无害）
	RCC_APB2PeriphClockCmd(ENC_KEY_RCC_PORT | RCC_APB2Periph_AFIO, ENABLE);

	//PB10上拉输入
	GPIO_InitStructure.GPIO_Pin = ENC_KEY_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(ENC_KEY_PORT, &GPIO_InitStructure);

	//AFIO映射PB2到EXTI2
	GPIO_EXTILineConfig(ENC_KEY_PORTSOURCE, ENC_KEY_PINSOURCE);

	//EXTI配置 下降沿触发
	EXTI_InitStructure.EXTI_Line = ENC_KEY_EXTI_LINE;
	EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	//NVIC
	NVIC_InitStructure.NVIC_IRQChannel =ENC_KEY_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);
}



//读取本次旋转差值，读完自动清零
int16_t Encoder_GetDelta(void)
{
	int16_t temp = Encoder_Count;
	Encoder_Count = 0;
	return temp;
}
//手动清空计数器
void Encoder_ClearCount(void)
{
	Encoder_Count = 0;
}
//中断函数
void EXTI0_IRQHandler(void)
{
	if(EXTI_GetITStatus(ENCODER_EXTI_LINE_CH1) == SET)
	{
		uint8_t B = GPIO_ReadInputDataBit(ENCODER_PORT, ENCODER_Pin_CH2);
		if(B == 1)
		{
			Encoder_Count++;
		}
		else
		{
			Encoder_Count--;
		}
		EXTI_ClearITPendingBit(ENCODER_EXTI_LINE_CH1);
	}
}
void EXTI1_IRQHandler(void)
{
	if(EXTI_GetITStatus(ENCODER_EXTI_LINE_CH2) == SET)
	{
		uint8_t A = GPIO_ReadInputDataBit(ENCODER_PORT, ENCODER_Pin_CH1);
		if(A == 0)
		{
			Encoder_Count++;
		}
		else
		{
			Encoder_Count--;
		}
		EXTI_ClearITPendingBit(ENCODER_EXTI_LINE_CH2);
	}
}
//旋钮按下中断
void EXTI2_IRQHandler(void)
{
	if(EXTI_GetITStatus(ENC_KEY_EXTI_LINE) == SET)
	{
		Encoder_Key_Flag = 1;	//置标志，交给main处理
		EXTI_ClearITPendingBit(ENC_KEY_EXTI_LINE);
	}
}