#include "stm32f10x.h"      
#include "Delay.h"  // Device header
void buzzer1(void)
{
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOB,&GPIO_InitStructure);
		GPIO_ResetBits(GPIOB,GPIO_Pin_13);
		Delay_ms(50);
		GPIO_SetBits(GPIOB,GPIO_Pin_13);
}
