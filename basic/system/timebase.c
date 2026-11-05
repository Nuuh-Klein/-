#include "stm32f10x.h"
#include "timebase.h"

static volatile uint32_t milliseconds;

void Timebase_Init(void)
{
    RCC_ClocksTypeDef clocks;
    TIM_TimeBaseInitTypeDef timer;
    NVIC_InitTypeDef irq;
    uint32_t timer_clock;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_GetClocksFreq(&clocks);
    timer_clock = clocks.PCLK1_Frequency;
    if ((RCC->CFGR & RCC_CFGR_PPRE1) != 0U) timer_clock *= 2U;
    TIM_Cmd(TIM3, DISABLE);
    TIM_InternalClockConfig(TIM3);
    TIM_TimeBaseStructInit(&timer);
    timer.TIM_Prescaler = (uint16_t)(timer_clock / 1000000U - 1U);
    timer.TIM_Period = 999U;
    TIM_TimeBaseInit(TIM3, &timer);
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
    milliseconds = 0;
    irq.NVIC_IRQChannel = TIM3_IRQn;
    irq.NVIC_IRQChannelPreemptionPriority = 2;
    irq.NVIC_IRQChannelSubPriority = 0;
    irq.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&irq);
    TIM_ITConfig(TIM3, TIM_IT_Update, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

uint32_t Timebase_Millis(void)
{
    return milliseconds;
}

void TIM3_IRQHandler(void)
{
    if (TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET) {
        TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
        milliseconds++;
    }
}
