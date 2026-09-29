#include "stm32f10x.h"  
#include "Delay.h"  // Device header

#define BUZZER_GPIO_PORT   GPIOB
#define BUZZER_GPIO_PIN    GPIO_Pin_13
#define BUZZER_GPIO_CLK    RCC_APB2Periph_GPIOB

#define BUZZER_BEEP_MS     50    // 按键声
#define BUZZER_PAUSE_MS    50    // 按键停止
#define BUZZER_ALARM_TIMES 20    // 报警循环次数

void Init(void){//初始化
RCC_APB2PeriphClockCmd(BUZZER_GPIO_CLK,ENABLE);
		GPIO_InitTypeDef GPIO_InitStructure;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Pin = BUZZER_GPIO_PIN    ;
	  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(BUZZER_GPIO_PORT,&GPIO_InitStructure);
	GPIO_SetBits(BUZZER_GPIO_PORT,BUZZER_GPIO_PIN);
}
void buzzer1(void)//按键音效
{
		GPIO_ResetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
		Delay_ms(BUZZER_BEEP_MS);
		GPIO_SetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
}
void buzzer2(void){//报警声，响过一段时间后自动停止
	uint8_t i=0;
	while(i<BUZZER_ALARM_TIMES){
	GPIO_ResetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
	Delay_ms(BUZZER_BEEP_MS);
	GPIO_SetBits(BUZZER_GPIO_PORT, BUZZER_GPIO_PIN);
	Delay_ms(BUZZER_PAUSE_MS);
	i++;
	}
}
