#include "stm32f10x.h"                  // Device header   
#include "led.h"
#include "config.h"
void LED_Alloff(void)
{
LED_Green_Off();
LED_Red_Off();
}
void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(LED_RCC_PORT, ENABLE);
	GPIO_InitTypeDef 
    GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin=LED_Green_Pin|LED_Red_Pin;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
	GPIO_Init(LED_PORT,&GPIO_InitStructure);
	LED_Alloff();
}
void LED_Green_On(void)
{
	GPIO_ResetBits(LED_PORT,LED_Green_Pin);
	}
void LED_Green_Off(void)
{
	GPIO_SetBits(LED_PORT,LED_Green_Pin);
	}
void LED_Red_On(void)
{
	GPIO_ResetBits(LED_PORT,LED_Red_Pin);
	}
void LED_Red_Off(void)
{
	GPIO_SetBits(LED_PORT,LED_Red_Pin);
	}
