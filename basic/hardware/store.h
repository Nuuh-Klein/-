#ifndef __STORE_H
#define __STORE_H
#include "stm32f10x_flash.h"

extern uint16_t Store_Data[];

FLASH_Status Store_Init(void);

FLASH_Status Store_Save(void);

FLASH_Status Store_Clear(void);
#endif
