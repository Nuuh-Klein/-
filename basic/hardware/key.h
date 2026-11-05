#ifndef _KEY_H
#define _KEY_H

#include "stm32f10x.h"
#include "config.h"
#include "password_input.h" /* Compatibility for existing input callers. */

#define KEY_NONE 0xFFU

void KEY_Init(void);
/* Returns one debounced key event, or KEY_NONE when there is no new event. */
uint8_t KEY_Scan(void);
/* Physical matrix position (S1-S16) of the most recently reported key. */
uint8_t KEY_GetLastPosition(void);

#endif
