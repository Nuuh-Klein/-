#include "stm32f10x.h"  
#include "Delay.h"  // Device header
#include "config.h"

void Buzzer_Init(void) // 初始化蜂鸣器 GPIO；当前驱动按低电平鸣叫配置。
{
RCC_APB2PeriphClockCmd(BUZZER_GPIO_CLK,ENABLE);
		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Pin = BUZZER_GPIO_PIN    ;
	  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(BUZZER_GPIO_PORT,&GPIO_InitStructure);
	GPIO_SetBits(BUZZER_GPIO_PORT,BUZZER_GPIO_PIN);
}
void buzzer1(void) // 发出一次短提示音。
{
		GPIO_ResetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
		Delay_ms(BUZZER_BEEP_MS);
		GPIO_SetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
}
void buzzer2(void) // 按设定次数发出间歇报警音，完成后自动停止。
{
	uint8_t i=0;
	while(i<BUZZER_ALARM_TIMES){
	GPIO_ResetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
	Delay_ms(BUZZER_BEEP_MS);
	GPIO_SetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
	Delay_ms(BUZZER_PAUSE_MS);
	i++;
	}
}
