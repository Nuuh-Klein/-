#include "stm32f10x.h"                  // Device header
#include "Delay.h"
#include "LED.h"
#include "Key.h"

uint8_t KeyNum;
int main(void)
{
    uint8_t KeyNum;
	LED_Init();
	Key_Init();
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA|  RCC_APB2Periph_GPIOB,ENABLE);
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode=GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Pin=GPIO_Pin_All;
	GPIO_InitStructure.GPIO_Speed=GPIO_Speed_50MHz;
    GPIO_Init(GPIOA,&GPIO_InitStructure);
    GPIO_Init(GPIOB,&GPIO_InitStructure);
 	while(1)
	{
		int i;
		while(i < 8)
        {
            GPIO_Write(GPIOB, 0xFFFF);    
            GPIO_Write(GPIOA, ~(1 << i));
            Delay_ms(500);
            i++;
        }
        GPIO_Write(GPIOA, 0xFFFF);
        GPIO_Write(GPIOB, ~(1 << 0));
        Delay_ms(100);
        i++;
        GPIO_Write(GPIOA, 0xFFFF);
        GPIO_Write(GPIOB, ~(1 << 1));
        Delay_ms(100);
        i++;
        i = 0;
    }
}
