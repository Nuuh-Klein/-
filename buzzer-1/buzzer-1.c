#include "stm32f10x.h"      
#include "Delay.h"  // Device header
void Init(void){//初始化
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOB,&GPIO_InitStructure);
	GPIO_SetBits(GPIOB,GPIO_Pin_13);
}
void buzzer1(void)//按键音效
{
		GPIO_ResetBits(GPIOB,GPIO_Pin_13);
		Delay_ms(50);
		GPIO_SetBits(GPIOB,GPIO_Pin_13);
}
void buzzer2(void){//报警声，响过一段时间后自动停止
	uint8_t i=0;
	while(i<20){
	GPIO_ResetBits(GPIOB,GPIO_Pin_13);
	Delay_ms(50);
	GPIO_SetBits(GPIOB,GPIO_Pin_13);
	Delay_ms(50);
	i++;
	}
}
