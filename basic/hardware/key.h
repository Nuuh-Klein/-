#ifndef _KEY_H
#define _KEY_H

#include "stm32f10x.h"
#include "config.h"

#define KEY_NONE 0xFFU

extern uint8_t InputBuf[PWD_MAX_LEN];
extern uint8_t SavedPwd[PWD_MAX_LEN];
extern uint8_t InputCnt;
extern uint8_t Key_SaveFlag;

void KEY_Init(void);
/* Returns one debounced key event, or KEY_NONE when there is no new event. */
uint8_t KEY_Scan(void);
/* Physical matrix position (S1-S16) of the most recently reported key. */
uint8_t KEY_GetLastPosition(void);
void Pwd_Input_Process(uint8_t key);
void Pwd_ClearInput(void);
void Pwd_Save(void);
void Pwd_PrintInputToOLED(void);

#endif
