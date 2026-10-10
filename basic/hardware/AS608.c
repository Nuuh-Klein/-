#include "stm32f10x.h"         
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include <stdint.h>         //我们的指纹都在as608内部，它负责存储对比删除等一系列具体操作，通过和stm的传输和接收信号实现一系列功能
#include "config.h"

#define AS608_USART      USART2
#define AS608_BAUDRATE   57600 // 接收频率

uint8_t AS608_RxBuffer[32];  //RX 接收缓冲区

/* 下面进行GPIO和USART2的初始化 PA2-TX PA3-RX*/
void AS608_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART2, ENABLE);

    /* PA2 - TX 复用推挽*/
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /* PA3 - RX 浮空输入 */
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    /*串口常量*/
    USART_InitStructure.USART_BaudRate = AS608_BAUDRATE;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(AS608_USART, &USART_InitStructure);

    USART_Cmd(AS608_USART, ENABLE);
}

/* 向AS608发送一个字节 */
void AS608_SendByte(uint8_t data)
{
    while (USART_GetFlagStatus(AS608_USART, USART_FLAG_TXE) == RESET);
    USART_SendData(AS608_USART, data);
}

/*这个函数用于发送stm对as608的命令包
 * cmd       :指令码，我们在录入指纹的过程中会用到很多指令码，不过不需要我们主动输入了
 * param     :参数数组之指针，无参数传 0
 * param_len :参数长度无参数传 0
 */
void AS608_SendCmd(uint8_t cmd, uint8_t *param, uint8_t param_len)
{
    uint16_t len = 1 + param_len + 2;   // 
    uint16_t sum = 0x01 + (len >> 8) + (len & 0xFF) + cmd;
    uint8_t i;

    AS608_SendByte(0xEF);
    AS608_SendByte(0x01);
    AS608_SendByte(0xFF);
    AS608_SendByte(0xFF);
    AS608_SendByte(0xFF);
    AS608_SendByte(0xFF);
    AS608_SendByte(0x01);               // 这几个参数是包头，芯片地址和包标识，不用改变
    AS608_SendByte(len >> 8);
    AS608_SendByte(len & 0xFF);
    AS608_SendByte(cmd);

    for (i = 0; i < param_len; i++) {
        AS608_SendByte(param[i]);
        sum += param[i];
    }//这个把我们的命令输出

    AS608_SendByte(sum >> 8);
    AS608_SendByte(sum & 0xFF);//输出校验和，防止干扰
}

/* as608接受stm的指令返回接受码，超时返回 0xFF
 * 普通应答共十二字节，确认码在AS608_RxBuffer[9]
 */
uint8_t AS608_ReceiveAck(uint32_t timeout)   //输入延时信号
{
    uint8_t i;
    uint32_t t;

    /* 等待接受第一个信号 0xEF */
    t = timeout;
    while (USART_GetFlagStatus(AS608_USART, USART_FLAG_RXNE) == RESET) {
        if (t-- == 0) return 0xFF;  //如果超时了就自动报错
    }
    AS608_RxBuffer[0] = USART_ReceiveData(AS608_USART);
    if (AS608_RxBuffer[0] != 0xEF) return 0xFF;  

    /*接收剩余的11个信号*/
    for (i = 1; i < 12; i++) {
        t = timeout;
        while (USART_GetFlagStatus(AS608_USART, USART_FLAG_RXNE) == RESET) {
            if (t-- == 0) return 0xFF;
        }
        AS608_RxBuffer[i] = USART_ReceiveData(AS608_USART);
    }

    return AS608_RxBuffer[9];   //返回确认码，返回0x00就代表成功，其他都是失败
}

/* 录入指纹到指定id（uint16_t id）
   返回0x00代表成功 */
uint8_t AS608_Enroll(uint16_t id)//这个id就是地址，为后续管理做准备
{
    uint8_t ack;
    uint8_t buf1[1] = {0x01};//这两个buf是临时缓冲区，输入并合并两次输入的指纹后就没用了
    uint8_t buf2[1] = {0x02};
    uint8_t store_param[3] = {0x01, (uint8_t)(id >> 8), (uint8_t)(id & 0xFF)};

    /* 1. 第一次采集指纹信息 */
    AS608_SendCmd(0x01, 0, 0);          // 这个是拍照指令
    ack = AS608_ReceiveAck(0xFFFFF);     //ack返回0x00代表输入成功 
    if (ack != 0x00) return ack;

    /* 2. 提取第一次采集指纹的特征 */
    AS608_SendCmd(0x02, buf1, 1);       // 传入一号缓冲区
    ack = AS608_ReceiveAck(0xFFFFF);
    if (ack != 0x00) return ack;

    /* 3.简单延时，准备第二次录入指纹 */
    for (volatile uint32_t i = 0; i < 0xFFFFFF; i++);  

    /* 4. 第二次采集指纹信息 */
    AS608_SendCmd(0x01, 0, 0);          
    ack = AS608_ReceiveAck(0xFFFFF);
    if (ack != 0x00) return ack;

    /* 4. 提取第二次采集指纹的特征 */
    AS608_SendCmd(0x02, buf2, 1);       //到缓冲二区 
    ack = AS608_ReceiveAck(0xFFFFF);
    if (ack != 0x00) return ack;

    /* 5. 合并两个缓冲区，生成这次需要的指纹模板*/
    AS608_SendCmd(0x05, 0, 0);          // PS_RegModel
    ack = AS608_ReceiveAck(0xFFFFF);
    if (ack != 0x00) return ack;

    /* 6. 将生成的模板输送到指定ID */
    AS608_SendCmd(0x06, store_param, 3); // PS_StoreChar
    ack = AS608_ReceiveAck(0xFFFFF);

    return ack;   // 0x00 代表操作成功
}

uint8_t Finger_Search(uint16_t *match_id, uint16_t *match_score)
{
    uint8_t ack;
    uint8_t p1 = 0x01;
    uint8_t search[5] = {0x01, 0x00, 0x00, 0x03, 0xE8};  // ???1, ?0??, ?1000?

    /*采集图像*/
    AS608_SendCmd(0x01, 0, 0);
    ack = AS608_ReceiveAck(0xFFFFF);
    if (ack != 0x00) return ack;

    /* 将输入的指纹特征送入缓冲区 */
    AS608_SendCmd(0x02, &p1, 1);
    ack = AS608_ReceiveAck(0xFFFFF);
    if (ack != 0x00) return ack;

    /* 搜索，从一区开始搜索符合上面特征的一千个指纹 */
    AS608_SendCmd(0x04, search, 5);
    ack = AS608_ReceiveAck(0xFFFFF);//将结果赋值给ack
    if (ack == 0x00) {
        *match_id    = ((uint16_t)AS608_RxBuffer[10] << 8) | AS608_RxBuffer[11];
        *match_score = ((uint16_t)AS608_RxBuffer[12] << 8) | AS608_RxBuffer[13];
    }//比对成功的话返回id和可信度
    return ack;//最终返回的是是否比对成功的结果
}


uint8_t Finger_Delete(uint16_t id)
{
    uint8_t param[2] = {id >> 8, id & 0xFF};   // 1.把id拆成两字节
    AS608_SendCmd(0x0C, param, 2);              // 2. 发送删除指令
    return AS608_ReceiveAck(0xFFFFF);           // 3. 接收是否成功的结果，成功是0x00
}


















