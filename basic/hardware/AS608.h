#ifndef __AS608_H
#define __AS608_H
#include "stm32f10x.h"
#include <stdint.h>
#include "config.h"

//指令码
#define AS608_OK               0x00   // 成功
#define AS608_ERR_RECV         0x01   // 收包错误
#define AS608_ERR_NO_FINGER    0x02   // 无手指
#define AS608_ERR_IMAGE        0x03   // 图像质量差
#define AS608_ERR_FEATURE      0x07   // 特征提取失败
#define AS608_ERR_MERGE        0x0A   // 合并特征失败
#define AS608_ERR_IMAGE_MESS   0x06   // 图像过乱，无法生成特征
#define AS608_ERR_LIB_FULL     0xFC   // 本地状态：索引表中没有空闲位置
#define AS608_ERR_CANCELLED    0xFD   // 本地状态：用户取消等待
#define AS608_ERR_NO_MATCH     0x09   // 未搜索到匹配
#define AS608_ERR_TIMEOUT      0xFF   // 超时
#define AS608_ERR_WAIT_TIMEOUT 0xFE   // 等待手指放置/移开超时
#define AS608_ERR_ID_OCCUPIED   0x22  // JM101：目标ID已有模板，禁止覆盖
#define AS608_ERR_DUPLICATE     0x27  // JM101：同一指纹已存在

#define AS608_ENROLL_WAIT_FIRST   1
#define AS608_ENROLL_WAIT_REMOVE  2
#define AS608_ENROLL_WAIT_SECOND  3
#define AS608_ENROLL_MERGE        4
#define AS608_ENROLL_STORE        5

typedef void (*AS608_StatusCallback)(uint8_t status);
typedef uint8_t (*AS608_CancelCallback)(void);

typedef struct {
    uint16_t tx_bytes;     // 成功写入发送寄存器的字节数
    uint16_t rx_bytes;     // 本次事务中断读取的原始字节数（含坏字节/包头前噪声）
    uint16_t uart_errors;  // SR低4位：PE/FE/NE/ORE
    uint16_t rx_dropped;   // 本地中断接收队列满而丢弃的字节数，与硬件ORE区分
    uint8_t tx_complete;   // 最后一个停止位已发完（TC）
    uint8_t ack_valid;     // 收到校验和正确的完整ACK，即使确认码非0也为1
    uint8_t first_rx[8];   // 最先收到的8个原始字节，便于调试器查看
} AS608_UARTDiagnostics;
extern AS608_UARTDiagnostics AS608_LastDiag;

void AS608_Init(void);
/* 仅配置本地USART及接收中断；调用通信函数时须允许中断，不修改模块参数。 */
void AS608_InitSerial(uint32_t baudrate, uint16_t stop_bits);
void AS608_SetHostAddress(uint32_t address);

//通信
uint8_t AS608_SendCmd(uint8_t cmd, uint8_t *param, uint8_t param_len);
/* timeout is the first-byte response wait; subsequent bytes use the configured inter-byte limit. */
uint8_t AS608_ReceiveAck(uint32_t timeout);
extern uint8_t AS608_LastCommand;       // 最近一次发送的指令，供通信诊断使用
void AS608_SetStatusCallback(AS608_StatusCallback callback);
/* 取消回调仅在命令之间调用，不能在串口收包使用 SysTick 时调用 KEY_Scan/Delay。 */
void AS608_SetCancelCallback(AS608_CancelCallback callback);
uint8_t AS608_WaitFingerRemoved(void);
uint8_t AS608_VerifyPassword(void);
uint8_t AS608_ReadLibraryInfo(uint16_t *capacity, uint16_t *count);
uint8_t AS608_FindFreeID(uint16_t *id);
uint8_t AS608_ReadTemplateCount(uint16_t *count);
/* JM101 V1.8一键指令：逐包读取进度，最终保存/搜索结果才返回成功。 */
uint8_t AS608_JM101Enroll(uint16_t id);
uint8_t AS608_JM101Search(uint16_t *match_id, uint16_t *match_score);

//指纹操作
uint8_t AS608_Enroll(uint16_t id);                    // 录入指纹
uint8_t Finger_Search(uint16_t *match_id, uint16_t *match_score);
uint8_t AS608_DeleteChar(uint16_t id);                // 删除指纹
uint8_t Finger_Delete(uint16_t id);                   // 删除指纹（兼容旧接口）
#endif
