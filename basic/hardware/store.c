#include "stm32f10x.h"                  // Device header
#include "flash.h"
#include "config.h"

uint16_t Store_Data[Store_Data_Length];

void Store_Init(void){
	if(FLASH_ReadHalfWord(Store_Start_Address)!=0xA5A5){//A5A5是初始标志位
		FLASH_EraseAPage(Store_Start_Address);
		FLASH_WriteHalfWord(Store_Start_Address,0xA5A5);
		for(int i=1;i<Store_Data_Length;i++){
			FLASH_WriteHalfWord(Store_Start_Address+i*2,0x0000);
		}
	}
	for(int i=0;i<Store_Data_Length;i++){
		Store_Data[i]=FLASH_ReadHalfWord(Store_Start_Address+i*2);
	}
}

void Store_Save(void){
	FLASH_EraseAPage(Store_Start_Address);
	for(int i=0;i<Store_Data_Length;i++){
		FLASH_WriteHalfWord(Store_Start_Address+i*2,Store_Data[i]);
	}
}

void Store_Clear(void){
	for(int i=1;i<Store_Data_Length;i++){
		Store_Data[i]=0x0000;
	}
	Store_Save();
}