#ifndef _KEY_H
#define _KEY_H
#include "stm32f10x.h"                  // Device header


#include"config.h"


extern uint8_t InputBuf[PWD_MAX_LEN];	//临时输入缓冲区（正在输入的数字）
extern uint8_t SavedPwd[PWD_MAX_LEN];	//确认后保存的6位密码
extern uint8_t InputCnt;				//当前输入了几位
extern uint8_t Key_SaveFlag;			//保存成功标志位

void KEY_Init(void);						//键盘IO初始化
uint8_t KEY_Scan(void);						//键盘扫描，返回键值
void Pwd_Input_Process(uint8_t key);		//密码输入逻辑处理
void Pwd_ClearInput(void);					//清空当前输入缓冲区
void Pwd_Save(void);						//密码存储函数：InputBuf拷贝至SavedPwd

#endif