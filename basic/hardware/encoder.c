#include "stm32f10x.h"                  // Device header
#include "encoder.h"
#include "config.h"

volatile int16_t Encoder_Count = 0;
volatile uint8_t Encoder_Key_Flag = 0;  //按键标志，1=按下


//消抖
static void Enc_Debounce_Delay(void)
{
    volatile uint32_t i;
    for (i = 0; i < 20000; i++);   // 72MHz 下约 1ms 左右
}

void Encoder_Init(void)
{
    RCC_APB2PeriphClockCmd(ENCODER_RCC_PORT, ENABLE);      // GPIOB 时钟
    RCC_APB2PeriphClockCmd(ENCODER_AFIO_RCC, ENABLE);      // AFIO 时钟（EXTI 必须）

    GPIO_InitTypeDef GPIO_InitStructure;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Pin = ENCODER_Pin_CH1 | ENCODER_Pin_CH2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ENCODER_PORT, &GPIO_InitStructure);          // PB0/PB1 上拉输入

    /* 只用 A 相(PB0)的下降沿做计数中断，B 相(PB1)只读电平判方向、不开中断。
       EC11 转一格 = 一个完整正交周期，A 相只下降一次 => 一格只加/减 1。*/
    GPIO_EXTILineConfig(ENCODER_PORTSOURCE, ENCODER_PINSOURCE_CH1);  // PB0 -> EXTI0

    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = ENCODER_EXTI_LINE_CH1;   // EXTI_Line0
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);
}

void Encoder_Key_EXTI_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    EXTI_InitTypeDef EXTI_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB2PeriphClockCmd(ENC_KEY_RCC_PORT | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Pin = ENC_KEY_PIN;             // PB10
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ENC_KEY_PORT, &GPIO_InitStructure);

    GPIO_EXTILineConfig(ENC_KEY_PORTSOURCE, ENC_KEY_PINSOURCE);  // PB10 -> EXTI10

    EXTI_InitStructure.EXTI_Line = ENC_KEY_EXTI_LINE;      // EXTI_Line10
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = ENC_KEY_IRQn;     // EXTI15_10_IRQn
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 3;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

// 读取本次旋转差值，读完自动清零（读-清零必须原子，否则中断里的计数会丢）
int16_t Encoder_GetDelta(void)
{
    int16_t temp;
    __disable_irq();
    temp = Encoder_Count;
    Encoder_Count = 0;
    __enable_irq();
    return temp;
}

// 手动清空计数器
void Encoder_ClearCount(void)
{
    __disable_irq();
    Encoder_Count = 0;
    __enable_irq();
}

// A 相下降沿中断：消抖后读 B 相判方向，一格加/减 1
void EXTI0_IRQHandler(void)
{
    if(EXTI_GetITStatus(ENCODER_EXTI_LINE_CH1) == SET)
    {
        Enc_Debounce_Delay();                               // 等抖动结束
        if(GPIO_ReadInputDataBit(ENCODER_PORT, ENCODER_Pin_CH1) == 0)  // 确认 A 仍为低
        {
            if(GPIO_ReadInputDataBit(ENCODER_PORT, ENCODER_Pin_CH2) == 1)
                Encoder_Count++;
            else
                Encoder_Count--;
        }
        EXTI_ClearITPendingBit(ENCODER_EXTI_LINE_CH1);      // 最后清标志
    }
}

// 旋钮按键中断（PB10 在 EXTI10，与 10~15 共用一个 IRQ，函数名必须是 EXTI15_10）
void EXTI15_10_IRQHandler(void)
{
    if(EXTI_GetITStatus(ENC_KEY_EXTI_LINE) == SET)
    {
        Enc_Debounce_Delay();
        if(GPIO_ReadInputDataBit(ENC_KEY_PORT, ENC_KEY_PIN) == 0)  // 确认仍按下
            Encoder_Key_Flag = 1;
        EXTI_ClearITPendingBit(ENC_KEY_EXTI_LINE);
    }
}
