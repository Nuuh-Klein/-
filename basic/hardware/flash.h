#ifndef __FLASH_H
#define __FLASH_H
#include "stm32f10x_flash.h"

uint32_t FLASH_ReadWord(uint32_t Address);

uint16_t FLASH_ReadHalfWord(uint32_t Address);

uint8_t FLASH_ReadBit(uint32_t Address);

FLASH_Status FLASH_EraseALLPages(void);

FLASH_Status FLASH_EraseAPage(uint32_t PageAddress);

FLASH_Status FLASH_WriteWord(uint32_t Address,uint32_t Data);
	
FLASH_Status FLASH_WriteHalfWord(uint32_t Address,uint16_t Data);

#endif
