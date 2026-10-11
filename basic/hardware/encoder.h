#ifndef _ENCODER_H
#define _ENCODER_H

#include"config.h"
void Encoder_Init(void);
extern volatile uint8_t Encoder_Key_Flag;
int16_t Encoder_GetDelta(void);
void Encoder_ClearCount(void);
void Encoder_Key_EXTI_Init(void);
#endif