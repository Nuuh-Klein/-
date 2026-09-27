#include "stm32f10x.h"                  // Device header
#include "config.h"

uint32_t FLASH_ReadWord(uint32_t Address){
	return *((volatile uint32_t *)(Address));
}

uint16_t FLASH_ReadHalfWord(uint32_t Address){
	return *((volatile uint16_t *)(Address));
}

uint8_t FLASH_ReadBit(uint32_t Address){
	return *((volatile uint8_t *)(Address));
}

void FLASH_EraseALLPages(void){
	FLASH_Unlock();
	FLASH_EraseAllPages();
	FLASH_Lock();
}

void FLASH_EraseAPage(uint32_t PageAddress){
	FLASH_Unlock();
	FLASH_ErasePage(PageAddress);
	FLASH_Lock();
}
