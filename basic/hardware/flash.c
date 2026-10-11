#include "stm32f10x.h"                  // Device header
#include "config.h"

static uint8_t FLASH_IsValidRange(uint32_t address, uint32_t size)
{
	return (address >= USER_FLASH_BASE_ADDRESS) &&
	       (address < USER_FLASH_END_ADDRESS) &&
	       (size <= USER_FLASH_END_ADDRESS - address);
}

// 读取一个字（32 位）
uint32_t FLASH_ReadWord(uint32_t Address){
	if (((Address & 3U) != 0U) || !FLASH_IsValidRange(Address, 4U)) return 0xFFFFFFFFU;
	return *((volatile uint32_t *)(Address));
}

// 读取一个半字（16 位）
uint16_t FLASH_ReadHalfWord(uint32_t Address){
	if (((Address & 1U) != 0U) || !FLASH_IsValidRange(Address, 2U)) return 0xFFFFU;
	return *((volatile uint16_t *)(Address));
}

// 读取一个字节（8 位）
uint8_t FLASH_ReadBit(uint32_t Address){
	if (!FLASH_IsValidRange(Address, 1U)) return 0xFFU;
	return *((volatile uint8_t *)(Address));
}

// 擦除全部页
FLASH_Status FLASH_EraseALLPages(void){
	FLASH_Status status;
	FLASH_Unlock();
	status = FLASH_EraseAllPages();
	FLASH_Lock();
	return status;
}

// 擦除某一页（PageAddress 必须是页起始地址，F103 每页 1KB）
FLASH_Status FLASH_EraseAPage(uint32_t PageAddress){
	FLASH_Status status;
	if (!FLASH_IsValidRange(PageAddress, USER_FLASH_PAGE_SIZE) ||
	    ((PageAddress - USER_FLASH_BASE_ADDRESS) % USER_FLASH_PAGE_SIZE) != 0U)
		return FLASH_ERROR_PG;
	FLASH_Unlock();
	status = FLASH_ErasePage(PageAddress);
	FLASH_Lock();
	return status;
}

// 写入一个字（32 位）
FLASH_Status FLASH_WriteWord(uint32_t Address,uint32_t Data){
	FLASH_Status status;
	if (((Address & 3U) != 0U) || !FLASH_IsValidRange(Address, 4U)) return FLASH_ERROR_PG;
	FLASH_Unlock();
	status = FLASH_ProgramWord(Address,Data);
	FLASH_Lock();
	return status;
}

// 写入一个半字（16 位）
FLASH_Status FLASH_WriteHalfWord(uint32_t Address,uint16_t Data){
	FLASH_Status status;
	if (((Address & 1U) != 0U) || !FLASH_IsValidRange(Address, 2U)) return FLASH_ERROR_PG;
	FLASH_Unlock();
	status = FLASH_ProgramHalfWord(Address,Data);
	FLASH_Lock();
	return status;
}
