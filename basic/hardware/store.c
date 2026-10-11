#include "stm32f10x.h"                  // Device header
#include "flash.h"
#include "config.h"

uint16_t Store_Data[Store_Data_Length];

FLASH_Status Store_Init(void){
	FLASH_Status status;
	if(FLASH_ReadHalfWord(Store_Start_Address)!=0xA5A5){// 0xA5A5 是存储区初始化标记。
		status = FLASH_EraseAPage(Store_Start_Address);
		if (status != FLASH_COMPLETE) return status;
		for(int i=1;i<Store_Data_Length;i++){
			status = FLASH_WriteHalfWord(Store_Start_Address+i*2,0x0000);
			if (status != FLASH_COMPLETE) return status;
		}
		//仅在所有默认值写入完成后提交该页
		status = FLASH_WriteHalfWord(Store_Start_Address,0xA5A5);
		if (status != FLASH_COMPLETE) return status;
	}
	for(int i=0;i<Store_Data_Length;i++){
		Store_Data[i]=FLASH_ReadHalfWord(Store_Start_Address+i*2);
	}
	return FLASH_COMPLETE;
}

FLASH_Status Store_Save(void){
	FLASH_Status status;
	// 擦除密码存储页后，将 RAM 中的全部数据（含初始化标记）写回。
	Store_Data[0] = 0xA5A5;
	status = FLASH_EraseAPage(Store_Start_Address);
	if (status != FLASH_COMPLETE) return status;
	for(int i=1;i<Store_Data_Length;i++){
		status = FLASH_WriteHalfWord(Store_Start_Address+i*2,Store_Data[i]);
		if (status != FLASH_COMPLETE) return status;
	}
	//标记字段用于提交，最后写入，以便检测被中断的写入操作
	status = FLASH_WriteHalfWord(Store_Start_Address,Store_Data[0]);
	if (status != FLASH_COMPLETE) return status;
	return FLASH_COMPLETE;
}

FLASH_Status Store_Clear(void){
	//保留下标 0 的初始化标记，将其余存储项清零。
	for(int i=1;i<Store_Data_Length;i++){
		Store_Data[i]=0x0000;
	}
	return Store_Save();
}
