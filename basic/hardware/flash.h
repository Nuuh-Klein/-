#ifndef __FLASH.H
#define __FLASH.H
uint32_t FLASH_ReadWord(uint32_t Address);

uint16_t FLASH_ReadHalfWord(uint32_t Address);

uint8_t FLASH_ReadBit(uint32_t Address);

void FLASH_EraseALLPages(void);

void FLASH_EraseAPage(uint32_t PageAddress);

void FLASH_WriteWord(uint32_t Address,uint32_t Data);
	
void FLASH_WriteHalfWord(uint32_t Address,uint16_t Data);

#endif
