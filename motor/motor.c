#include "stm32f10x.h"                  // Device header

#define PWM_TIM                 TIM2
#define PWM_TIM_CLK             RCC_APB1Periph_TIM2

#define PWM_TIM_CHANNEL         TIM_Channel_1  //通道一

#define PWM_GPIO_PORT           GPIOA
#define PWM_GPIO_PIN            GPIO_Pin_15
#define PWM_GPIO_CLK            RCC_APB2Periph_GPIOA//引脚
 
#define PWM_AFIO_CLK            RCC_APB2Periph_AFIO
#define PWM_REMAP_PIN           GPIO_PartialRemap1_TIM2//将TIM2_CH1重映射到p15

#define PWM_TIM_PRESCALER       (72 - 1)        // PSC
#define PWM_TIM_PERIOD          (20000 - 1)     // ARR

#define PWM_PULSE_0_DEG         500//close
#define PWM_PULSE_90_DEG        1500//open

void PWM_Init(void) //初始化  
{
    RCC_APB1PeriphClockCmd(PWM_TIM_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(PWM_GPIO_CLK, ENABLE);
    RCC_APB2PeriphClockCmd(PWM_AFIO_CLK, ENABLE);
    
    GPIO_PinRemapConfig(PWM_REMAP_PIN, ENABLE);
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);  

    TIM_InternalClockConfig(PWM_TIM);

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Pin  = PWM_GPIO_PIN;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWM_GPIO_PORT, &GPIO_InitStructure);

    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = PWM_TIM_PERIOD;
    TIM_TimeBaseInitStructure.TIM_Prescaler = PWM_TIM_PRESCALER;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(PWM_TIM, &TIM_TimeBaseInitStructure);

    TIM_OCInitTypeDef TIM_OCInitStructure;
    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable; 
    TIM_OCInitStructure.TIM_Pulse = PWM_PULSE_0_DEG;

    TIM_OC1Init(PWM_TIM, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);

    TIM_Cmd(PWM_TIM, ENABLE);
}

void PWM_close(void)   // 0°
{
    TIM_SetCompare1(PWM_TIM, PWM_PULSE_0_DEG);
}

void PWM_open(void)    // 90°
{
    TIM_SetCompare1(PWM_TIM, PWM_PULSE_90_DEG);
}
