#ifndef __CONFIG.H
#define __CONFIG.H
#include "stm32f10x.h"                  // Device header

//密码参数
#define PASSWORD_LEN 6        //密码长度为6位
#define MAX_ERROR_COUNT 3     //最长错误次数3次
#define LOCKING_TIME 30       //锁定时长为30秒

//LED参数分配（端口/引脚）


//蜂鸣器参数分配


//定时器参数分配

//共享变量声明
extern volatile uint16_t current_state;          //状态机目前的状态
extern volatile uint16_t error_count;            //输入错误的次数
extern volatile int locking_remain;         //锁定剩余的时长
extern volatile uint16_t origin_password[6];     //初始密码
extern volatile uint16_t set_password[6];        //设置密码

#endif
