#include "stm32f10x.h"                  // Device header
#include "encoder.h"
#include "config.h"

volatile int16_t Encoder_Count = 0;
volatile uint8_t Encoder_Key_Flag = 0;  //按键标志，1=按下

static volatile uint8_t encoder_state;
static volatile int8_t encoder_quadrature_delta;

#define ENCODER_EVENT_SLOTS 64U
#if ENCODER_DIRECTION != 1 && ENCODER_DIRECTION != -1
#error ENCODER_DIRECTION must be 1 or -1
#endif
#if ENCODER_EDGES_PER_STEP != 2 && ENCODER_EDGES_PER_STEP != 4
#error ENCODER_EDGES_PER_STEP must be 2 or 4
#endif
static volatile int8_t encoder_events[ENCODER_EVENT_SLOTS];
static volatile uint8_t event_read, event_write, event_overflow;
static uint8_t button_candidate, button_stable;
static uint32_t button_since;

static void Encoder_QueueStep(int8_t direction)
{
    uint8_t next = (uint8_t)((event_write + 1U) % ENCODER_EVENT_SLOTS);
    if (direction > 0) {
        if (Encoder_Count < 32767) Encoder_Count++;
    } else {
        if (Encoder_Count > -32768) Encoder_Count--;
    }
    if (next == event_read) { event_overflow = 1; return; }
    encoder_events[event_write] = direction;
    event_write = next;
}

// 格雷码状态转换表；按配置的每格边沿数输出完整机械步进事件。
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
        if (encoder_quadrature_delta >= ENCODER_EDGES_PER_STEP)
        {
            Encoder_QueueStep(ENCODER_DIRECTION);
            encoder_quadrature_delta = 0;
        }
        else if (encoder_quadrature_delta <= -ENCODER_EDGES_PER_STEP)
        {
            Encoder_QueueStep(-ENCODER_DIRECTION);
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
    event_read = event_write = event_overflow = 0;
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
    /* Require a stable release if the button is held during boot. */
    button_candidate = button_stable = 1;
    button_since = 0;

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

void Encoder_Service(uint32_t now)
{
    uint8_t pressed = GPIO_ReadInputDataBit(ENC_KEY_PORT, ENC_KEY_PIN) == Bit_RESET;
    if (pressed != button_candidate) {
        button_candidate = pressed;
        button_since = now;
    } else if (pressed != button_stable &&
               (uint32_t)(now - button_since) >= ENCODER_KEY_DEBOUNCE_MS) {
        button_stable = pressed;
        if (pressed) Encoder_Key_Flag = 1;
    }
}

uint8_t Encoder_PopStep(int8_t *direction)
{
    uint32_t primask;
    uint8_t available;
    if (direction == 0) return 0;
    primask = __get_PRIMASK();
    __disable_irq();
    available = event_read != event_write;
    if (available) {
        *direction = encoder_events[event_read];
        event_read = (uint8_t)((event_read + 1U) % ENCODER_EVENT_SLOTS);
    }
    if (primask == 0U) __enable_irq();
    return available;
}

uint8_t Encoder_TakeOverflow(void)
{
    uint32_t primask = __get_PRIMASK();
    uint8_t overflow;
    __disable_irq();
    overflow = event_overflow;
    event_overflow = 0;
    if (primask == 0U) __enable_irq();
    return overflow;
}

// 手动清空计数器
void Encoder_ClearCount(void)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    Encoder_Count = 0;
    event_read = event_write = event_overflow = 0;
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

// PB10 中断只清除标志，主循环 Encoder_Service 按稳定电平消抖。
void EXTI15_10_IRQHandler(void)
{
    if(EXTI_GetITStatus(ENC_KEY_EXTI_LINE) == SET)
    {
        EXTI_ClearITPendingBit(ENC_KEY_EXTI_LINE);
    }
}
