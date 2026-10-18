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

#define AS608_ENROLL_WAIT_FIRST   1
#define AS608_ENROLL_WAIT_REMOVE  2
#define AS608_ENROLL_WAIT_SECOND  3
#define AS608_ENROLL_MERGE        4
#define AS608_ENROLL_STORE        5

typedef void (*AS608_StatusCallback)(uint8_t status);
typedef uint8_t (*AS608_CancelCallback)(void);

void AS608_Init(void);

//通信
uint8_t AS608_SendCmd(uint8_t cmd, uint8_t *param, uint8_t param_len);
/* timeout is the first-byte response wait; subsequent bytes use the configured inter-byte limit. */
uint8_t AS608_ReceiveAck(uint32_t timeout);
extern uint8_t AS608_LastCommand;       // 最近一次发送的指令，供通信诊断使用
extern uint8_t AS608_LEDStatus;         // 初始化时灯控命令的应答码
void AS608_SetStatusCallback(AS608_StatusCallback callback);
/* 取消回调仅在命令之间调用，不能在串口收包使用 SysTick 时调用 KEY_Scan/Delay。 */
void AS608_SetCancelCallback(AS608_CancelCallback callback);
uint8_t AS608_WaitFingerRemoved(void);
uint8_t AS608_VerifyPassword(void);
uint8_t AS608_ReadLibraryInfo(uint16_t *capacity, uint16_t *count);
uint8_t AS608_FindFreeID(uint16_t *id);

//指纹操作
uint8_t AS608_Enroll(uint16_t id);                    // 录入指纹
uint8_t Finger_Search(uint16_t *match_id, uint16_t *match_score);
uint8_t AS608_DeleteChar(uint16_t id);                // 删除指纹
uint8_t Finger_Delete(uint16_t id);                   // 删除指纹（兼容旧接口）
#endif
