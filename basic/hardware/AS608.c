#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "AS608.h"
#include "Delay.h"
#include "config.h"

uint8_t AS608_RxBuffer[AS608_RX_BUFFER_SIZE];
uint8_t AS608_LastCommand;
uint8_t AS608_LEDStatus = AS608_ERR_TIMEOUT;
static AS608_StatusCallback AS608_Callback;
static AS608_CancelCallback AS608_Cancel;
static uint16_t AS608_Capacity;

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
    return 1;
}

static uint8_t AS608_ReadByte(uint8_t *data, uint32_t timeout)
{
	if (timeout == 0) return 0;
	SysTick->CTRL = 0;
	SysTick->LOAD = (SystemCoreClock / 1000U) - 1U;
	SysTick->VAL = 0;
	SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk;

    while (USART_GetFlagStatus(AS608_USART, USART_FLAG_RXNE) == RESET) {
		if (SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) {
			timeout--;
			if (timeout == 0) {
				SysTick->CTRL = 0;
				return 0;
			}
		}
    }
	SysTick->CTRL = 0;
    *data = (uint8_t)USART_ReceiveData(AS608_USART);
    return 1;
}

static void AS608_FlushRx(void)
{
    uint8_t count;
    for (count = 0; count < AS608_RX_BUFFER_SIZE; count++) {
        if (USART_GetFlagStatus(AS608_USART, USART_FLAG_RXNE) == RESET) break;
        (void)USART_ReceiveData(AS608_USART);
    }
}

//USART2: PA2 = STM32 TX -> AS608 RX, PA3 = STM32 RX <- AS608 TX
void AS608_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    AS608_Capacity = 0;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = AS608_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(AS608_USART, &USART_InitStructure);
    USART_Cmd(AS608_USART, ENABLE);
    Delay_ms(200); /* Allow the sensor firmware to finish its power-on sequence. */
    AS608_FlushRx();

    /* Try to enable the blue Aura LED on compatible AS608 modules. */
    {
        uint8_t led_param[4] = {0x03, 0x00, 0x02, 0x00};
        AS608_LastCommand = 0x35;
        AS608_SendCmd(0x35, led_param, 4);
        AS608_LEDStatus = AS608_ReceiveAck(200);
    }
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
    if (!AS608_SendByte(0xEF) || !AS608_SendByte(0x01) ||
        !AS608_SendByte(0xFF) || !AS608_SendByte(0xFF) ||
        !AS608_SendByte(0xFF) || !AS608_SendByte(0xFF) ||
        !AS608_SendByte(AS608_PACKET_TYPE_CMD) ||
        !AS608_SendByte((uint8_t)(length >> 8)) || !AS608_SendByte((uint8_t)length) ||
        !AS608_SendByte(cmd)) return AS608_ERR_TIMEOUT;
    for (i = 0; i < param_len; i++) {
        if (!AS608_SendByte(param[i])) return AS608_ERR_TIMEOUT;
        checksum = (uint16_t)(checksum + param[i]);
    }
    if (!AS608_SendByte((uint8_t)(checksum >> 8)) ||
        !AS608_SendByte((uint8_t)checksum)) return AS608_ERR_TIMEOUT;
    return AS608_OK;
}

/* Read a complete variable-length ACK packet, including its checksum. */
uint8_t AS608_ReceiveAck(uint32_t timeout)
{
    uint16_t length;
    uint16_t checksum;
    uint16_t received_checksum;
    uint8_t i;

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
    if (checksum != received_checksum) return AS608_ERR_RECV;
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
