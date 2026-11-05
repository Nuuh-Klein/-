#ifndef _LED_H
#define _LED_H
#include"config.h"
/* Active-high control: On = high, Off = low. Green PC15, red PC14. */


void LED_Init(void);
void LED_Green_On(void);
void LED_Green_Off(void);
void LED_Red_On(void);
void LED_Red_Off(void);
void LED_Alloff(void);


#endif
