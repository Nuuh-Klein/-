#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"

void KEY_Test_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	//行 PA0~PA3 推挽输出
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
	GPIO_SetBits(GPIOA, GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3);
	//列 PA4~PA7 上拉输入
	GPIO_InitStruct.GPIO_Pin = GPIO_Pin_4|GPIO_Pin_5|GPIO_Pin_6|GPIO_Pin_7;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(GPIOA, &GPIO_InitStruct);
}

uint8_t ScanKey(void)
{
	uint8_t key=0;
	GPIO_ResetBits(GPIOA,GPIO_Pin_0);
	if(GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_4)==0)
	{
		Delay_ms(20);
		while(!GPIO_ReadInputDataBit(GPIOA,GPIO_Pin_4));
		key=1;
	}
	GPIO_SetBits(GPIOA,GPIO_Pin_0);
	return key;
}

int main(void)
{
	uint8_t k;
	OLED_Init();
	KEY_Test_Init();
	OLED_Clear();
	while(1)
	{
		k = ScanKey();
		if(k != 0)
		{
			OLED_Clear();
			OLED_ShowString(0,0,"KEY PRESSED");
		}
	}
}
