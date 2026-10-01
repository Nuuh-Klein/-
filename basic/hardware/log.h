#ifndef __LOG_H
#define __LOG_H

#include "stm32f10x.h"                  // Device header（提供 uint16_t / uint32_t 等类型）

/* ======================== 开锁审计日志模块（创意四：Audit Log） ========================
 * 功能：把每一次开锁事件 {方式, 成败, 序号} 以定长记录写入内部 FLASH 后部的一块日志区，
 *       写满后自动覆盖最旧记录，支持 OLED + 编码器翻阅“最近 N 条记录”。
 *
 * 依赖：flash.c 提供的 FLASH_ReadHalfWord / FLASH_WriteHalfWord / FLASH_EraseAPage。
 * 注意：日志区地址/长度等参数定义在 config.h（LOG_START_ADDRESS / LOG_RECORD_SIZE 等）。
 * ======================================================================================== */

/* ---- 开锁方式（method）：记录“这次是用什么方式开的锁” ---- */
typedef enum {
	LOG_METHOD_PASSWORD = 1,   // 数字密码
	LOG_METHOD_PATTERN  = 2,   // 图形手势密码（创意一）
	LOG_METHOD_FINGER   = 3,   // 指纹识别（AS608）
	LOG_METHOD_DURESS   = 4,   // 胁迫密码（创意二，静默告警）
} Log_Method_t;

/* ---- 开锁结果（result）：记录这次尝试成功还是失败 ---- */
typedef enum {
	LOG_RESULT_FAIL = 0,   // 失败
	LOG_RESULT_PASS = 1,   // 成功
} Log_Result_t;

/* ---- 单条日志记录：定长 6 字节（3 个半字），对应 {方式, 成败, 序号} ---- */
typedef struct {
	uint16_t method;   // 开锁方式（取值见 Log_Method_t）
	uint16_t result;   // 成败（取值见 Log_Result_t）
	uint16_t serial;   // 全局递增的事件序号（用于区分先后 / 显示）
} Log_Record_t;

/* ---- 对外接口 ---- */
void     Log_Init(void);                                        // 上电扫描日志区，恢复有效条数与下一序号
void     Log_Write(uint16_t method, uint16_t result);           // 追加一条开锁事件（自动带上递增序号）
uint16_t Log_GetCount(void);                                    // 返回当前有效日志条数
uint8_t  Log_Read(uint16_t recent_index, Log_Record_t *record); // 读“第 recent_index 新”的一条（0=最新）

#endif
