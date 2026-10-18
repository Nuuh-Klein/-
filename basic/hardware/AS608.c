#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "AS608.h"
#include "Delay.h"
#include "config.h"

uint8_t AS608_RxBuffer[AS608_RX_BUFFER_SIZE];
uint8_t AS608_LastCommand;
AS608_UARTDiagnostics AS608_LastDiag;
static AS608_StatusCallback AS608_Callback;
static AS608_CancelCallback AS608_Cancel;
static uint16_t AS608_Capacity;
static uint32_t AS608_HostAddress = AS608_DEVICE_ADDRESS;
#define AS608_UART_ERROR_MASK (USART_SR_PE | USART_SR_FE | USART_SR_NE | USART_SR_ORE)
#define AS608_UART_QUEUE_SIZE 128U
static volatile uint8_t AS608_Queue[AS608_UART_QUEUE_SIZE];
static volatile uint8_t AS608_QueueErrors[AS608_UART_QUEUE_SIZE];
static volatile uint8_t AS608_QueueHead, AS608_QueueTail;
static volatile uint16_t AS608_WireCount, AS608_WireErrors, AS608_QueueDropped;
static volatile uint8_t AS608_WireFirst[8];

/* 中断内只读SR/DR并缓存，不做OLED、延时或协议解析。 */
void USART2_IRQHandler(void)
{
    uint16_t status = (uint16_t)AS608_USART->SR;
    uint16_t errors = status & AS608_UART_ERROR_MASK;
    if ((status & USART_SR_RXNE) != 0U || errors != 0U) {
        uint8_t data = (uint8_t)USART_ReceiveData(AS608_USART);
        AS608_WireErrors |= errors;
        if ((status & USART_SR_RXNE) != 0U) {
            uint8_t next = (uint8_t)((AS608_QueueHead + 1U) & (AS608_UART_QUEUE_SIZE - 1U));
            if (AS608_WireCount < 8U) AS608_WireFirst[AS608_WireCount] = data;
            if (AS608_WireCount != 0xFFFFU) AS608_WireCount++;
            if (next != AS608_QueueTail) {
                AS608_Queue[AS608_QueueHead] = data;
                AS608_QueueErrors[AS608_QueueHead] = (uint8_t)errors;
                AS608_QueueHead = next;
            } else if (AS608_QueueDropped != 0xFFFFU) AS608_QueueDropped++;
        }
    }
}

static void AS608_SyncRxDiagnostics(void)
{
    uint8_t i;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    AS608_LastDiag.rx_bytes = AS608_WireCount;
    AS608_LastDiag.uart_errors = AS608_WireErrors;
    AS608_LastDiag.rx_dropped = AS608_QueueDropped;
    for (i = 0; i < 8U; i++) AS608_LastDiag.first_rx[i] = AS608_WireFirst[i];
    if (mask == 0U) __enable_irq();
}

void AS608_SetHostAddress(uint32_t address)
{
    AS608_HostAddress = address;
}

static void AS608_ResetDiagnostics(void)
{
    uint8_t i;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    AS608_QueueHead = AS608_QueueTail = 0;
    AS608_WireCount = AS608_WireErrors = AS608_QueueDropped = 0;
    AS608_LastDiag.tx_bytes = 0;
    AS608_LastDiag.rx_bytes = 0;
    AS608_LastDiag.uart_errors = 0;
    AS608_LastDiag.tx_complete = 0;
    AS608_LastDiag.ack_valid = 0;
    AS608_LastDiag.rx_dropped = 0;
    for (i = 0; i < 8U; i++) AS608_LastDiag.first_rx[i] = AS608_WireFirst[i] = 0;
    if (mask == 0U) __enable_irq();
}

void AS608_SetCancelCallback(AS608_CancelCallback callback)
{
    AS608_Cancel = callback;
}

static uint8_t AS608_IsCancelled(void)
{
    return (AS608_Cancel != 0) && AS608_Cancel();
}

void AS608_SetStatusCallback(AS608_StatusCallback callback)
{
    AS608_Callback = callback;
}

static void AS608_ReportStatus(uint8_t status)
{
    if (AS608_Callback != 0) AS608_Callback(status);
}

static uint8_t AS608_SendByte(uint8_t data)
{
    uint32_t timeout = 100000U;
    while (USART_GetFlagStatus(AS608_USART, USART_FLAG_TXE) == RESET) {
        if (timeout-- == 0U) return 0;
    }
    USART_SendData(AS608_USART, data);
    AS608_LastDiag.tx_bytes++;
    return 1;
}

static uint8_t AS608_ReadByte(uint8_t *data, uint32_t timeout)
{
	if (timeout == 0) return 0;
	SysTick->CTRL = 0;
	SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

	while (1) {
        AS608_SyncRxDiagnostics();
        if (AS608_QueueTail != AS608_QueueHead) {
            uint8_t tail = AS608_QueueTail;
            uint8_t received = AS608_Queue[tail];
            uint8_t errors = AS608_QueueErrors[tail];
            AS608_QueueTail = (uint8_t)((tail + 1U) & (AS608_UART_QUEUE_SIZE - 1U));
            if (errors == 0U) {
                SysTick->CTRL = 0;
                *data = received;
                return 1;
            }
        }
		if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
			timeout--;
			if (timeout == 0) {
				SysTick->CTRL = 0;
				return 0;
			}
		}
    }
}

static void AS608_FlushRx(void)
{
    uint8_t count;
    uint32_t mask = __get_PRIMASK();
    __disable_irq();
    AS608_QueueTail = AS608_QueueHead;
    for (count = 0; count < AS608_RX_BUFFER_SIZE; count++) {
        FlagStatus pending = USART_GetFlagStatus(AS608_USART, USART_FLAG_RXNE);
        uint16_t status = (uint16_t)AS608_USART->SR;
        if (pending == RESET && (status & AS608_UART_ERROR_MASK) == 0U) break;
        (void)USART_ReceiveData(AS608_USART);
    }
    if (mask == 0U) __enable_irq();
}

static uint8_t AS608_WaitTxComplete(void)
{
    uint32_t timeout = 1000000U;
    while (USART_GetFlagStatus(AS608_USART, USART_FLAG_TC) == RESET) {
        if (timeout-- == 0U) return 0;
    }
    AS608_LastDiag.tx_complete = 1;
    return 1;
}

//USART2: PA2 = STM32 TX -> AS608 RX, PA3 = STM32 RX <- AS608 TX
void AS608_InitSerial(uint32_t baudrate, uint16_t stop_bits)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    NVIC_DisableIRQ(USART2_IRQn);
    NVIC_ClearPendingIRQ(USART2_IRQn);
    AS608_Capacity = 0;
    AS608_HostAddress = AS608_DEVICE_ADDRESS;
    AS608_ResetDiagnostics();

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);
    USART_DeInit(AS608_USART);
    GPIO_PinRemapConfig(GPIO_Remap_USART2, DISABLE); // 明确恢复PA2/PA3。
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE); // 保持工程原SWD调试配置。

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU; // UART空闲高电平，避免断线时浮空。
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = baudrate;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = stop_bits;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(AS608_USART, &USART_InitStructure);
    USART_Cmd(AS608_USART, ENABLE);
    AS608_FlushRx();
    USART_ITConfig(AS608_USART, USART_IT_RXNE, ENABLE);
    USART_ITConfig(AS608_USART, USART_IT_ERR, ENABLE);
    NVIC_InitStructure.NVIC_IRQChannel = USART2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
}

void AS608_Init(void)
{
    AS608_InitSerial(AS608_BAUDRATE, AS608_STOP_BITS);
    Delay_ms(AS608_POWER_ON_DELAY);
    AS608_FlushRx();
}

uint8_t AS608_SendCmd(uint8_t cmd, uint8_t *param, uint8_t param_len)
{
    uint16_t length;
    uint16_t checksum;
    uint8_t i;

    if ((param_len != 0) && (param == 0)) return AS608_ERR_RECV;
    length = (uint16_t)(param_len + 3);
    checksum = (uint16_t)(AS608_PACKET_TYPE_CMD + (length >> 8) +
                          (length & 0xFF) + cmd);
    AS608_FlushRx();
    AS608_ResetDiagnostics();
    AS608_LastCommand = cmd;
    if (!AS608_SendByte(0xEF) || !AS608_SendByte(0x01) ||
        !AS608_SendByte((uint8_t)(AS608_HostAddress >> 24)) ||
        !AS608_SendByte((uint8_t)(AS608_HostAddress >> 16)) ||
        !AS608_SendByte((uint8_t)(AS608_HostAddress >> 8)) ||
        !AS608_SendByte((uint8_t)AS608_HostAddress) ||
        !AS608_SendByte(AS608_PACKET_TYPE_CMD) ||
        !AS608_SendByte((uint8_t)(length >> 8)) || !AS608_SendByte((uint8_t)length) ||
        !AS608_SendByte(cmd)) return AS608_ERR_TIMEOUT;
    for (i = 0; i < param_len; i++) {
        if (!AS608_SendByte(param[i])) return AS608_ERR_TIMEOUT;
        checksum = (uint16_t)(checksum + param[i]);
    }
    if (!AS608_SendByte((uint8_t)(checksum >> 8)) ||
        !AS608_SendByte((uint8_t)checksum)) return AS608_ERR_TIMEOUT;
    return AS608_WaitTxComplete() ? AS608_OK : AS608_ERR_TIMEOUT;
}

/* Read a complete variable-length ACK packet, including its checksum. */
uint8_t AS608_ReceiveAck(uint32_t timeout)
{
    uint16_t length;
    uint16_t checksum;
    uint16_t received_checksum;
    uint8_t i;
    AS608_LastDiag.ack_valid = 0;

    /* Bound header search so noise cannot keep the caller blocked indefinitely. */
    {
        uint8_t previous = 0;
        uint8_t searched;
        uint32_t interbyte_timeout = (timeout < AS608_INTERBYTE_TIMEOUT) ?
                                     timeout : AS608_INTERBYTE_TIMEOUT;
        for (searched = 0; searched < AS608_RX_BUFFER_SIZE; searched++) {
            uint8_t current;
            uint32_t byte_timeout = (searched == 0U) ? timeout : interbyte_timeout;
            if (!AS608_ReadByte(&current, byte_timeout)) return AS608_ERR_TIMEOUT;
            if ((previous == 0xEF) && (current == 0x01)) break;
            previous = current;
        }
        if (searched >= AS608_RX_BUFFER_SIZE) return AS608_ERR_RECV;
    }
    AS608_RxBuffer[0] = 0xEF;
    AS608_RxBuffer[1] = 0x01;
    for (i = 2; i < 9; i++) {
        if (!AS608_ReadByte(&AS608_RxBuffer[i], AS608_INTERBYTE_TIMEOUT)) return AS608_ERR_TIMEOUT;
    }
    if (AS608_RxBuffer[6] != AS608_PACKET_TYPE_ACK) return AS608_ERR_RECV;
    length = (uint16_t)(((uint16_t)AS608_RxBuffer[7] << 8) | AS608_RxBuffer[8]);
    if (length < 3 || length > AS608_RX_BUFFER_SIZE - 9) return AS608_ERR_RECV;
    for (i = 0; i < length; i++) {
        if (!AS608_ReadByte(&AS608_RxBuffer[9 + i], AS608_INTERBYTE_TIMEOUT)) return AS608_ERR_TIMEOUT;
    }
    checksum = (uint16_t)(AS608_RxBuffer[6] + AS608_RxBuffer[7] + AS608_RxBuffer[8]);
    for (i = 0; i < length - 2; i++) checksum = (uint16_t)(checksum + AS608_RxBuffer[9 + i]);
    received_checksum = (uint16_t)(((uint16_t)AS608_RxBuffer[9 + length - 2] << 8) |
                                   AS608_RxBuffer[9 + length - 1]);
    AS608_SyncRxDiagnostics();
    if (checksum != received_checksum || AS608_LastDiag.rx_dropped != 0U) return AS608_ERR_RECV;
    AS608_LastDiag.ack_valid = 1;
    return AS608_RxBuffer[9];
}

static uint8_t AS608_Command(uint8_t command, uint8_t *param, uint8_t param_len)
{
    AS608_LastCommand = command;
    if (AS608_SendCmd(command, param, param_len) != AS608_OK) return AS608_ERR_TIMEOUT;
    return AS608_ReceiveAck(AS608_DEFAULT_TIMEOUT);
}

static uint16_t AS608_AckLength(void)
{
    return (uint16_t)(((uint16_t)AS608_RxBuffer[7] << 8) | AS608_RxBuffer[8]);
}

uint8_t AS608_VerifyPassword(void)
{
    uint32_t password = AS608_PASSWORD;
    uint8_t param[4] = {(uint8_t)(password >> 24), (uint8_t)(password >> 16),
                        (uint8_t)(password >> 8), (uint8_t)password};
    return AS608_Command(0x13, param, 4);
}

uint8_t AS608_ReadLibraryInfo(uint16_t *capacity, uint16_t *count)
{
    uint8_t ack;
    uint16_t size;
    if (capacity == 0 || count == 0) return AS608_ERR_RECV;
    *capacity = 0;
    *count = 0;
    AS608_Capacity = 0;
    ack = AS608_Command(0x0F, 0, 0); // ReadSysPara
    if (ack != AS608_OK) return ack;
    if (AS608_AckLength() < 19U) return AS608_ERR_RECV;
    size = (uint16_t)(((uint16_t)AS608_RxBuffer[14] << 8) | AS608_RxBuffer[15]);
    if (size == 0U) return AS608_ERR_RECV;
    ack = AS608_Command(0x1D, 0, 0); // TemplateNum
    if (ack != AS608_OK) return ack;
    if (AS608_AckLength() < 5U) return AS608_ERR_RECV;
    *count = (uint16_t)(((uint16_t)AS608_RxBuffer[10] << 8) | AS608_RxBuffer[11]);
    if (*count > size) return AS608_ERR_RECV;
    *capacity = size;
    AS608_Capacity = size;
    return AS608_OK;
}

uint8_t AS608_FindFreeID(uint16_t *id)
{
    uint16_t page;
    uint16_t offset;
    uint8_t ack;
    if (id == 0 || AS608_Capacity == 0U) return AS608_ERR_RECV;
    *id = 0;
    for (page = 0; page < ((uint32_t)AS608_Capacity + 255U) / 256U; page++) {
        uint8_t param = (uint8_t)page;
        ack = AS608_Command(0x1F, &param, 1); // ReadIndexTable，32字节位图
        if (ack != AS608_OK) return ack;
        if (AS608_AckLength() < 35U) return AS608_ERR_RECV;
        for (offset = 0; offset < 256U; offset++) {
            uint32_t candidate = (uint32_t)page * 256U + offset;
            if (candidate >= AS608_Capacity) break;
            if ((AS608_RxBuffer[10U + offset / 8U] & (1U << (offset % 8U))) == 0U) {
                *id = (uint16_t)candidate;
                return AS608_OK;
            }
        }
    }
    return AS608_ERR_LIB_FULL;
}

uint8_t AS608_ReadTemplateCount(uint16_t *count)
{
    uint8_t ack;
    if (count == 0) return AS608_ERR_RECV;
    *count = 0;
    ack = AS608_Command(0x1D, 0, 0);
    if (ack != AS608_OK) return ack;
    if (AS608_AckLength() < 5U) return AS608_ERR_RECV;
    *count = (uint16_t)(((uint16_t)AS608_RxBuffer[10] << 8) | AS608_RxBuffer[11]);
    return AS608_OK;
}

/* 短等待之间扫描取消键；串口收包内部不能调用共享SysTick的键盘扫描。 */
static uint8_t AS608_AutoNextAck(uint32_t *remaining)
{
    AS608_LastDiag.ack_valid = 0;
    while (*remaining >= 2U) {
        uint8_t ack;
        uint16_t before = AS608_LastDiag.rx_bytes;
        if (AS608_IsCancelled()) return AS608_ERR_CANCELLED;
        ack = AS608_ReceiveAck(2U);
        if (AS608_LastDiag.ack_valid) return ack;
        if (ack != AS608_ERR_TIMEOUT || AS608_LastDiag.rx_bytes != before) return ack;
        *remaining -= 2U;
    }
    return AS608_ERR_WAIT_TIMEOUT;
}

static uint8_t AS608_StopAuto(uint8_t result)
{
    /* 模块仍在等待手指时，结束本地等待必须同时终止模块操作，防止随后意外写入。
     * 取消确认可能与旧进度包交错；下一次操作前main重新校验密码。
     */
    uint8_t command = AS608_LastCommand;
    AS608_UARTDiagnostics failed = AS608_LastDiag;
    (void)AS608_Command(0x30, 0, 0);
    AS608_LastCommand = command; // 错误页仍指向失败的录入/比对指令。
    AS608_LastDiag = failed;
    return result;
}

uint8_t AS608_JM101Enroll(uint16_t id)
{
    /* 两次采集，必须移开；上报步骤；禁止覆盖ID、禁止重复录入相同指纹。 */
    uint8_t param[5] = {(uint8_t)(id >> 8), (uint8_t)id, 2U, 0U, 0x11U};
    uint8_t ack, packets;
    uint8_t accepted = 0;
    uint32_t remaining = AS608_FINGER_WAIT_TIMEOUT;
    if (AS608_IsCancelled()) return AS608_ERR_CANCELLED;
    ack = AS608_SendCmd(0x31, param, 5);
    if (ack != AS608_OK) return AS608_StopAuto(ack);
    for (packets = 0; packets < 64U; packets++) {
        uint8_t stage, capture;
        ack = AS608_AutoNextAck(&remaining);
        if (!AS608_LastDiag.ack_valid) return AS608_StopAuto(ack);
        if (AS608_AckLength() < 5U) return AS608_StopAuto(AS608_ERR_RECV);
        stage = AS608_RxBuffer[10]; capture = AS608_RxBuffer[11];
        /* 手册规定特征提取失败时模块会继续尝试采集，不能提前宣布操作结束。 */
        if (ack == AS608_ERR_FEATURE && stage == 2U && accepted) continue;
        if (ack != AS608_OK) return ack;
        if (stage == 0U) { accepted = 1; AS608_ReportStatus(AS608_ENROLL_WAIT_FIRST); }
        else if (!accepted) return AS608_StopAuto(AS608_ERR_RECV);
        else if (stage == 2U && capture == 1U) AS608_ReportStatus(AS608_ENROLL_WAIT_REMOVE);
        else if (stage == 3U && capture == 1U) AS608_ReportStatus(AS608_ENROLL_WAIT_SECOND);
        else if (stage == 2U && capture == 2U) AS608_ReportStatus(AS608_ENROLL_MERGE);
        else if (stage == 4U) AS608_ReportStatus(AS608_ENROLL_STORE);
        else if (stage == 6U && capture == 0xF2U) return AS608_OK;
    }
    return AS608_StopAuto(AS608_ERR_RECV);
}

uint8_t AS608_JM101Search(uint16_t *match_id, uint16_t *match_score)
{
    uint8_t param[5] = {3U, 0xFFU, 0xFFU, 0U, 1U}; // 安全等级3，搜索全库，上报步骤。
    uint8_t ack, packets;
    uint8_t accepted = 0;
    uint32_t remaining = AS608_FINGER_WAIT_TIMEOUT;
    if (match_id == 0 || match_score == 0) return AS608_ERR_RECV;
    *match_id = *match_score = 0;
    if (AS608_IsCancelled()) return AS608_ERR_CANCELLED;
    ack = AS608_SendCmd(0x32, param, 5);
    if (ack != AS608_OK) return AS608_StopAuto(ack);
    for (packets = 0; packets < 16U; packets++) {
        ack = AS608_AutoNextAck(&remaining);
        if (!AS608_LastDiag.ack_valid) return AS608_StopAuto(ack);
        if (ack != AS608_OK) return ack;
        if (AS608_AckLength() < 8U) return AS608_StopAuto(AS608_ERR_RECV);
        if (AS608_RxBuffer[10] == 0U) accepted = 1;
        else if (!accepted) return AS608_StopAuto(AS608_ERR_RECV);
        else if (AS608_RxBuffer[10] == 5U) {
            *match_id = (uint16_t)(((uint16_t)AS608_RxBuffer[11] << 8) | AS608_RxBuffer[12]);
            *match_score = (uint16_t)(((uint16_t)AS608_RxBuffer[13] << 8) | AS608_RxBuffer[14]);
            return AS608_OK;
        }
    }
    return AS608_StopAuto(AS608_ERR_RECV);
}

/* Capture one image, waiting for a finger if the sensor reports 0x02. */
static uint8_t AS608_CaptureToBuffer(uint8_t buffer_number)
{
    uint8_t ack;
    uint32_t waited_ms = 0;

    do {
        if (AS608_IsCancelled()) return AS608_ERR_CANCELLED;
        ack = AS608_Command(0x01, 0, 0); /* GenImg */
        if (ack == AS608_ERR_NO_FINGER && waited_ms < AS608_FINGER_WAIT_TIMEOUT) {
            Delay_ms(100);
            waited_ms += 100U;
        }
    } while ((ack == AS608_ERR_NO_FINGER) && (waited_ms < AS608_FINGER_WAIT_TIMEOUT));
    if (ack == AS608_ERR_NO_FINGER) return AS608_ERR_WAIT_TIMEOUT;
    if (ack != AS608_OK) return ack;
    return AS608_Command(0x02, &buffer_number, 1); /* Img2Tz */
}

uint8_t AS608_WaitFingerRemoved(void)
{
    uint8_t ack;
    uint32_t waited_ms = 0;

    do {
        if (AS608_IsCancelled()) return AS608_ERR_CANCELLED;
        ack = AS608_Command(0x01, 0, 0); /* GenImg */
        if (ack == AS608_OK && waited_ms < AS608_FINGER_WAIT_TIMEOUT) {
            Delay_ms(100);
            waited_ms += 100U;
        }
    } while ((ack == AS608_OK) && (waited_ms < AS608_FINGER_WAIT_TIMEOUT));
    if (ack == AS608_OK) return AS608_ERR_WAIT_TIMEOUT;
    return ack == AS608_ERR_NO_FINGER ? AS608_OK : ack;
}

uint8_t AS608_Enroll(uint16_t id)
{
    uint8_t ack;
    uint8_t buffer_number_1 = 0x01;
    uint8_t buffer_number_2 = 0x02;
    uint8_t store_param[3] = {0x01, (uint8_t)(id >> 8), (uint8_t)id};

    AS608_ReportStatus(AS608_ENROLL_WAIT_FIRST);
    ack = AS608_CaptureToBuffer(buffer_number_1); if (ack != AS608_OK) return ack;
    AS608_ReportStatus(AS608_ENROLL_WAIT_REMOVE);
    ack = AS608_WaitFingerRemoved(); if (ack != AS608_OK) return ack;
    AS608_ReportStatus(AS608_ENROLL_WAIT_SECOND);
    ack = AS608_CaptureToBuffer(buffer_number_2); if (ack != AS608_OK) return ack;
    AS608_ReportStatus(AS608_ENROLL_MERGE);
    if (AS608_IsCancelled()) return AS608_ERR_CANCELLED;
    ack = AS608_Command(0x05, 0, 0); if (ack != AS608_OK) return ack;
    AS608_ReportStatus(AS608_ENROLL_STORE);
    if (AS608_IsCancelled()) return AS608_ERR_CANCELLED;
    return AS608_Command(0x06, store_param, 3);
}

uint8_t Finger_Search(uint16_t *match_id, uint16_t *match_score)
{
    uint8_t ack;
    uint8_t buffer_number = 0x01;
    uint8_t search[5] = {0x01, 0x00, 0x00, 0x00, 0x00};

    if ((match_id == 0) || (match_score == 0)) return AS608_ERR_RECV;
    *match_id = 0;
    *match_score = 0;
    if (AS608_Capacity == 0U) {
        uint16_t capacity;
        uint16_t count;
        ack = AS608_ReadLibraryInfo(&capacity, &count);
        if (ack != AS608_OK) return ack;
    }
    search[3] = (uint8_t)(AS608_Capacity >> 8);
    search[4] = (uint8_t)AS608_Capacity;
    ack = AS608_CaptureToBuffer(buffer_number); if (ack != AS608_OK) return ack;
    ack = AS608_Command(0x04, search, 5);
    if (ack == AS608_OK) {
        /* Search ACK must contain confirmation + ID + confidence + checksum. */
        if ((((uint16_t)AS608_RxBuffer[7] << 8) | AS608_RxBuffer[8]) < 7) {
            return AS608_ERR_RECV;
        }
        *match_id = (uint16_t)(((uint16_t)AS608_RxBuffer[10] << 8) | AS608_RxBuffer[11]);
        *match_score = (uint16_t)(((uint16_t)AS608_RxBuffer[12] << 8) | AS608_RxBuffer[13]);
    }
    return ack;
}

uint8_t Finger_Delete(uint16_t id)
{
    uint8_t param[2] = {(uint8_t)(id >> 8), (uint8_t)id};
    return AS608_Command(0x0C, param, 2);
}

uint8_t AS608_DeleteChar(uint16_t id)
{
    return Finger_Delete(id);
}
