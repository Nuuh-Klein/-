#include "stm32f10x.h"                  // Device header
#include "config.h"

// 读取一个字（32 位）
uint32_t FLASH_ReadWord(uint32_t Address){
	return *((volatile uint32_t *)(Address));
}

// 读取一个半字（16 位）
uint16_t FLASH_ReadHalfWord(uint32_t Address){
	return *((volatile uint16_t *)(Address));
}

// 读取一个字节（8 位）
uint8_t FLASH_ReadBit(uint32_t Address){
	return *((volatile uint8_t *)(Address));
}

// 擦除全部页
void FLASH_EraseALLPages(void){
	FLASH_Unlock();
	FLASH_EraseAllPages();
	FLASH_Lock();
}

// 擦除某一页（PageAddress 必须是页起始地址，F103 每页 1KB）
void FLASH_EraseAPage(uint32_t PageAddress){
	FLASH_Unlock();
	FLASH_ErasePage(PageAddress);
	FLASH_Lock();
}

// 写入一个字（32 位）
void FLASH_WriteWord(uint32_t Address,uint32_t Data){
	FLASH_Unlock();
	FLASH_ProgramWord(Address,Data);
	FLASH_Lock();
}

// 写入一个半字（16 位）
void FLASH_WriteHalfWord(uint32_t Address,uint16_t Data){
	FLASH_Unlock();
	FLASH_ProgramHalfWord(Address,Data);
	FLASH_Lock();
}
