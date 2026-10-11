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

void AS608_SetStatusCallback(AS608_StatusCallback callback)
{
    AS608_Callback = callback;
}

static void AS608_ReportStatus(uint8_t status)
{
    if (AS608_Callback != 0) AS608_Callback(status);
}

static void AS608_SendByte(uint8_t data)
{
    while (USART_GetFlagStatus(AS608_USART, USART_FLAG_TXE) == RESET) { }
    USART_SendData(AS608_USART, data);
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
    while (USART_GetFlagStatus(AS608_USART, USART_FLAG_RXNE) != RESET) {
        (void)USART_ReceiveData(AS608_USART);
    }
}

/* USART2: PA2 = STM32 TX -> AS608 RX, PA3 = STM32 RX <- AS608 TX. */
void AS608_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

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
    AS608_FlushRx();

    /* Try to enable the blue Aura LED on compatible AS608 modules. */
    {
        uint8_t led_param[4] = {0x03, 0x00, 0x02, 0x00};
        AS608_LastCommand = 0x35;
        AS608_SendCmd(0x35, led_param, 4);
        AS608_LEDStatus = AS608_ReceiveAck(200);
    }
}

void AS608_SendCmd(uint8_t cmd, uint8_t *param, uint8_t param_len)
{
    uint16_t length;
    uint16_t checksum;
    uint8_t i;

    if ((param_len != 0) && (param == 0)) return;
    length = (uint16_t)(param_len + 3);
    checksum = (uint16_t)(AS608_PACKET_TYPE_CMD + (length >> 8) +
                          (length & 0xFF) + cmd);
    AS608_FlushRx();
    AS608_SendByte(0xEF); AS608_SendByte(0x01);
    AS608_SendByte(0xFF); AS608_SendByte(0xFF);
    AS608_SendByte(0xFF); AS608_SendByte(0xFF);
    AS608_SendByte(AS608_PACKET_TYPE_CMD);
    AS608_SendByte((uint8_t)(length >> 8)); AS608_SendByte((uint8_t)length);
    AS608_SendByte(cmd);
    for (i = 0; i < param_len; i++) {
        AS608_SendByte(param[i]);
        checksum = (uint16_t)(checksum + param[i]);
    }
    AS608_SendByte((uint8_t)(checksum >> 8));
    AS608_SendByte((uint8_t)checksum);
}

/* Read a complete variable-length ACK packet, including its checksum. */
uint8_t AS608_ReceiveAck(uint32_t timeout)
{
    uint16_t length;
    uint16_t checksum;
    uint16_t received_checksum;
    uint8_t i;

    /* Ignore stray bytes and synchronize on the adjacent 0xEF 0x01 header. */
    {
        uint8_t previous = 0;
        while (1) {
            uint8_t current;
            if (!AS608_ReadByte(&current, timeout)) return AS608_ERR_TIMEOUT;
            if ((previous == 0xEF) && (current == 0x01)) break;
            previous = current;
        }
    }
    AS608_RxBuffer[0] = 0xEF;
    AS608_RxBuffer[1] = 0x01;
    for (i = 2; i < 9; i++) {
        if (!AS608_ReadByte(&AS608_RxBuffer[i], timeout)) return AS608_ERR_TIMEOUT;
    }
    if (AS608_RxBuffer[6] != AS608_PACKET_TYPE_ACK) return AS608_ERR_RECV;
    length = (uint16_t)(((uint16_t)AS608_RxBuffer[7] << 8) | AS608_RxBuffer[8]);
    if (length < 3 || length > AS608_RX_BUFFER_SIZE - 9) return AS608_ERR_RECV;
    for (i = 0; i < length; i++) {
        if (!AS608_ReadByte(&AS608_RxBuffer[9 + i], timeout)) return AS608_ERR_TIMEOUT;
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
    AS608_SendCmd(command, param, param_len);
    return AS608_ReceiveAck(AS608_DEFAULT_TIMEOUT);
}

/* Capture one image, waiting for a finger if the sensor reports 0x02. */
static uint8_t AS608_CaptureToBuffer(uint8_t buffer_number)
{
    uint8_t ack;

    do {
        ack = AS608_Command(0x01, 0, 0); /* GenImg */
        if (ack == AS608_ERR_NO_FINGER) Delay_ms(100);
    } while (ack == AS608_ERR_NO_FINGER);
    if (ack != AS608_OK) return ack;
    return AS608_Command(0x02, &buffer_number, 1); /* Img2Tz */
}

uint8_t AS608_WaitFingerRemoved(void)
{
    uint8_t ack;

    do {
        ack = AS608_Command(0x01, 0, 0); /* GenImg */
        if (ack == AS608_OK) Delay_ms(100);
    } while (ack == AS608_OK);
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
    ack = AS608_Command(0x05, 0, 0); if (ack != AS608_OK) return ack;
    AS608_ReportStatus(AS608_ENROLL_STORE);
    return AS608_Command(0x06, store_param, 3);
}

uint8_t Finger_Search(uint16_t *match_id, uint16_t *match_score)
{
    uint8_t ack;
    uint8_t buffer_number = 0x01;
    uint8_t search[5] = {0x01, 0x00, 0x00, 0x03, 0xE8};

    if ((match_id == 0) || (match_score == 0)) return AS608_ERR_RECV;
    ack = AS608_Command(0x01, 0, 0); if (ack != AS608_OK) return ack;
    ack = AS608_Command(0x02, &buffer_number, 1); if (ack != AS608_OK) return ack;
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
