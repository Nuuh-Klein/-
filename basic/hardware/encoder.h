#ifndef _ENCODER_H
#define _ENCODER_H
#include"config.h"

void Encoder_Init(void);
extern volatile uint8_t Encoder_Key_Flag;
uint8_t Encoder_GetKeyPressed(void);//读取并清除锁存的按键事件
int16_t Encoder_GetDelta(void);
void Encoder_ClearCount(void);
void Encoder_Key_EXTI_Init(void);
#endif
