#include "stm32f10x.h"                  // Device header
#include "encoder.h"
#include "config.h"

volatile int16_t Encoder_Count = 0;
volatile uint8_t Encoder_Key_Flag = 0;  //按键标志，1=按下

static volatile uint8_t encoder_state;
static volatile int8_t encoder_quadrature_delta;

//格雷码状态转换表，只有完成一个完整正交周期才更新计数
static const int8_t encoder_transition[16] = {
     0,  1, -1,  0,
    -1,  0,  0,  1,
     1,  0,  0, -1,
     0, -1,  1,  0
};

static uint8_t Encoder_ReadState(void)
{
    uint8_t state = 0;
    if (GPIO_ReadInputDataBit(ENCODER_PORT, ENCODER_Pin_CH1) != Bit_RESET) state |= 0x02;
    if (GPIO_ReadInputDataBit(ENCODER_PORT, ENCODER_Pin_CH2) != Bit_RESET) state |= 0x01;
    return state;
}

static void Encoder_ProcessTransition(void)
{
    uint8_t new_state = Encoder_ReadState();
    uint8_t transition_index = (uint8_t)((encoder_state << 2) | new_state);
    int8_t step = encoder_transition[transition_index];

    if (step != 0)
    {
        encoder_quadrature_delta += step;
        if (encoder_quadrature_delta >= 4)
        {
            if (Encoder_Count < 32767) Encoder_Count++;
            encoder_quadrature_delta = 0;
        }
        else if (encoder_quadrature_delta <= -4)
        {
            if (Encoder_Count > -32768) Encoder_Count--;
            encoder_quadrature_delta = 0;
        }
    }
    else if ((new_state ^ encoder_state) == 0x03)
    {
        //若两个位同时发生变化，丢弃
        encoder_quadrature_delta = 0;
    }
    encoder_state = new_state;
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
    Encoder_Count = 0;
    Encoder_Key_Flag = 0;
    encoder_state = Encoder_ReadState();
    encoder_quadrature_delta = 0;

    //两相都检测双边沿，用完整正交状态序列累计一格，避免在中断中延时消抖
    GPIO_EXTILineConfig(ENCODER_PORTSOURCE, ENCODER_PINSOURCE_CH1);  // PB0 -> EXTI0
    GPIO_EXTILineConfig(ENCODER_PORTSOURCE, ENCODER_PINSOURCE_CH2);  // PB1 -> EXTI1

    EXTI_InitTypeDef EXTI_InitStructure;
    EXTI_InitStructure.EXTI_Line = ENCODER_EXTI_LINE_CH1 | ENCODER_EXTI_LINE_CH2;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Rising_Falling;
    EXTI_Init(&EXTI_InitStructure);
    EXTI_ClearITPendingBit(ENCODER_EXTI_LINE_CH1 | ENCODER_EXTI_LINE_CH2);

    NVIC_InitTypeDef NVIC_InitStructure;
    NVIC_InitStructure.NVIC_IRQChannel = EXTI0_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_Init(&NVIC_InitStructure);

    NVIC_InitStructure.NVIC_IRQChannel = EXTI1_IRQn;
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
    Encoder_Key_Flag = 0;

    GPIO_EXTILineConfig(ENC_KEY_PORTSOURCE, ENC_KEY_PINSOURCE);  // PB10 -> EXTI10

    EXTI_InitStructure.EXTI_Line = ENC_KEY_EXTI_LINE;      // EXTI_Line10
    EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
    EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
    EXTI_InitStructure.EXTI_LineCmd = ENABLE;
    EXTI_Init(&EXTI_InitStructure);
    EXTI_ClearITPendingBit(ENC_KEY_EXTI_LINE);

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
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    temp = Encoder_Count;
    Encoder_Count = 0;
    if (primask == 0U) __enable_irq();
    return temp;
}

uint8_t Encoder_GetKeyPressed(void)
{
    uint8_t pressed;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    pressed = Encoder_Key_Flag;
    Encoder_Key_Flag = 0;
    if (primask == 0U) __enable_irq();
    return pressed;
}

// 手动清空计数器
void Encoder_ClearCount(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    Encoder_Count = 0;
    encoder_quadrature_delta = 0;
    encoder_state = Encoder_ReadState();
    if (primask == 0U) __enable_irq();
}

static void Encoder_HandleEXTI(void)
{
    uint8_t pending = 0;
    if (EXTI_GetITStatus(ENCODER_EXTI_LINE_CH1) == SET)
    {
        pending |= 0x01;
        EXTI_ClearITPendingBit(ENCODER_EXTI_LINE_CH1);
    }
    if (EXTI_GetITStatus(ENCODER_EXTI_LINE_CH2) == SET)
    {
        pending |= 0x02;
        EXTI_ClearITPendingBit(ENCODER_EXTI_LINE_CH2);
    }
    if (pending != 0U) Encoder_ProcessTransition();
}

void EXTI0_IRQHandler(void)
{
    Encoder_HandleEXTI();
}

void EXTI1_IRQHandler(void)
{
    Encoder_HandleEXTI();
}

// PB10 的按键中断与 EXTI10~15 共用 IRQ；中断中只置位，不执行阻塞延时。
void EXTI15_10_IRQHandler(void)
{
    if(EXTI_GetITStatus(ENC_KEY_EXTI_LINE) == SET)
    {
        if(GPIO_ReadInputDataBit(ENC_KEY_PORT, ENC_KEY_PIN) == 0)  // 确认仍按下
            Encoder_Key_Flag = 1;
        EXTI_ClearITPendingBit(ENC_KEY_EXTI_LINE);
    }
}
