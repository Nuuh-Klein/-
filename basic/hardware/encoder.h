#ifndef _ENCODER_H
#define _ENCODER_H
#include "config.h"

void Encoder_Init(void);
extern volatile uint8_t Encoder_Key_Flag;
uint8_t Encoder_GetKeyPressed(void);
int16_t Encoder_GetDelta(void);
void Encoder_ClearCount(void);
void Encoder_Key_EXTI_Init(void);
/* Call regularly before reading the debounced press event. */
void Encoder_Service(uint32_t now);
/* Ordered detent events retain reversals while the OLED is busy. */
uint8_t Encoder_PopStep(int8_t *direction);
uint8_t Encoder_TakeOverflow(void);

#endif
