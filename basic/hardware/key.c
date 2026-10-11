#include "stm32f10x.h"                  // Device header
#include "key.h"
#include "delay.h"
#include "Delay.h"
#include "OLED.h"
//全局变量
uint8_t InputBuf[PWD_MAX_LEN] = {0};
uint8_t SavedPwd[PWD_MAX_LEN] = {0};
uint8_t InputCnt = 0;
uint8_t Key_SaveFlag = 0;


void KEY_Init(void)                      //键盘初始化
{
	GPIO_InitTypeDef GPIO_InitStruct;  //初始化 行 R1~R4 PA0~PA3 推挽输出
	RCC_APB2PeriphClockCmd(KEY_ROW_RCC, ENABLE);
	GPIO_InitStruct.GPIO_Pin = R1_PIN | R2_PIN | R3_PIN | R4_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(KEY_ROW_PORT, &GPIO_InitStruct);  //初始化 列 C1~C4 PA4~PA7 上拉输入
	GPIO_SetBits(KEY_ROW_PORT, R1_PIN | R2_PIN | R3_PIN | R4_PIN); //所有行默认拉高
	RCC_APB2PeriphClockCmd(KEY_COL_RCC, ENABLE);
	GPIO_InitStruct.GPIO_Pin = C1_PIN | C2_PIN | C3_PIN | C4_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(KEY_COL_PORT, &GPIO_InitStruct);
}

/**
 * @brief  键盘扫描函数
 * @retval 0：无按键；0~9数字；10=D确认；11=*清空
 */
uint8_t KEY_Scan(void)
{
	uint8_t key = 0;

	//扫描R1(PA0)  1 2 3 A
   GPIO_ResetBits(KEY_ROW_PORT, R1_PIN);   // ① 把R1这一行拉低（输出0）
   // 读C1列，如果是0 → 按键1按下
    if(GPIO_ReadInputDataBit(KEY_COL_PORT, C1_PIN)==0)
   {
      Delay_ms(20);                     //② 消抖：机械按键按下瞬间电平抖动，等20ms稳定
      while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C1_PIN)); //③ 阻塞等待，直到按键松开！
      Delay_ms(20);                    //松手消抖
      key=1;                           //返回键值=1
    }   
   //重复操作
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C2_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C2_PIN));Delay_ms(20); key=2;}
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C3_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C3_PIN));Delay_ms(20); key=3;}
	GPIO_SetBits(KEY_ROW_PORT, R1_PIN);

	//扫描R2(PA1)  4 5 6 B
	GPIO_ResetBits(KEY_ROW_PORT, R2_PIN);
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C1_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C1_PIN));Delay_ms(20); key=4;}
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C2_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C2_PIN));Delay_ms(20); key=5;}
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C3_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C3_PIN));Delay_ms(20); key=6;}
	GPIO_SetBits(KEY_ROW_PORT, R2_PIN);

	//扫描R3(PA2)  7 8 9 C
	GPIO_ResetBits(KEY_ROW_PORT, R3_PIN);
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C1_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C1_PIN));Delay_ms(20); key=7;}
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C2_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C2_PIN));Delay_ms(20); key=8;}
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C3_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C3_PIN));Delay_ms(20); key=9;}
	GPIO_SetBits(KEY_ROW_PORT, R3_PIN);

	//扫描R4(PA3)  * 0 清空 确认
	GPIO_ResetBits(KEY_ROW_PORT, R4_PIN);
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C1_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C1_PIN));Delay_ms(20); key=11;} // *清空
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C2_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C2_PIN));Delay_ms(20); key=0;}  //数字0
	if(GPIO_ReadInputDataBit(KEY_COL_PORT, C4_PIN)==0){Delay_ms(20);while(!GPIO_ReadInputDataBit(KEY_COL_PORT,C4_PIN));Delay_ms(20); key=10;} //D确认
	GPIO_SetBits(KEY_ROW_PORT, R4_PIN);

	return key;
}

/**
 * @brief  清空当前输入缓冲区
 */
void Pwd_ClearInput(void)
{
	uint8_t i;
	for(i = 0; i < PWD_MAX_LEN; i++)
	{
		InputBuf[i] = 0;
	}
	InputCnt = 0;
}

void Pwd_Save(void)  //密码保存函数：临时输入 拷贝 到保存数组
{
	uint8_t i;
	for(i = 0; i < PWD_MAX_LEN; i++)
	{
		SavedPwd[i] = InputBuf[i];
	}
	Key_SaveFlag = 1; //标记保存成功
}

/**
 * @brief  密码输入处理函数
 * @param  key：扫描得到的键值
 * @note   输满6位，按下D才会执行保存；（4，3）清空全部输入
 */
void Pwd_Input_Process(uint8_t key)
{
	if(key >= 0 && key <= 9) //数字0~9录入
	{
		if(InputCnt < PWD_MAX_LEN) //未满6位才录入
		{
			InputBuf[InputCnt] = key;
			InputCnt++;
		}
	}
	else if(key == 11) // * 清空
	{
		Pwd_ClearInput();
	}
	else if(key == 10) //（4，4）确认保存
	{
		if(InputCnt == PWD_MAX_LEN) //必须刚好6位才保存
		{
			Pwd_Save();
		}
	}
}

void Pwd_PrintInputToOLED(void)    //每输入 1 位，屏幕显示一个`*`，看不到真实数字
{
	uint8_t i;
	OLED_Clear();
	OLED_ShowString(0,0,"Input Pwd:");
	for(i = 0; i < 6; i++)
	{
		if(i < InputCnt)
		{
			OLED_ShowChar(8 + i*12, 24, '*');
		}
		else
		{
			OLED_ShowChar(8 + i*12, 24, ' ');
		}
	}
}
