#ifndef __AS608_H
#define __AS608_H

#include "stm32f10x.h"
#include <stdint.h>

/* 这些是指令码，他们都是固定的，不需要宏定义*/
#define AS608_OK            0x00   // 成功
#define AS608_ERR_RECV      0x01   // 收包错误
#define AS608_ERR_NO_FINGER 0x02   // 无手指
#define AS608_ERR_IMAGE     0x03   // 图像质量差
#define AS608_ERR_MERGE     0x0A   // 合并特征失败
#define AS608_ERR_LIB_FULL  0x06   // 指纹库满
#define AS608_ERR_NO_MATCH  0x09   // 未搜索到匹配
#define AS608_ERR_TIMEOUT   0xFF   // 超时

/* 初始化 */
void AS608_Init(void);

/*通信*/
void AS608_SendCmd(uint8_t cmd, uint8_t *param, uint8_t param_len);
uint8_t AS608_ReceiveAck(uint32_t timeout);

/*指纹操作 */
uint8_t Finger_Search(uint16_t *match_id, uint16_t *match_score);
uint8_t AS608_Enroll(uint16_t id);                    // 录入指纹
uint8_t Finger_Delete(uint16_t id);              // 删除指纹

#endif