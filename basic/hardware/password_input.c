#include "password_input.h"
#include "OLED.h"

uint8_t InputBuf[PWD_MAX_LEN] = {0};
uint8_t SavedPwd[PWD_MAX_LEN] = {0};
uint8_t InputCnt = 0;
uint8_t Key_SaveFlag = 0;

void Pwd_ClearInput(void)
{
	uint8_t i;
	for (i = 0; i < PWD_MAX_LEN; i++)
	{
		InputBuf[i] = 0;
		SavedPwd[i] = 0;
	}
	InputCnt = 0;
	Key_SaveFlag = 0;
}

void Pwd_Save(void)
{
	uint8_t i;
	if (InputCnt != PWD_MAX_LEN)
	{
		Key_SaveFlag = 0;
		return;
	}
	for (i = 0; i < PWD_MAX_LEN; i++)
	{
		SavedPwd[i] = InputBuf[i];
	}
	Key_SaveFlag = 1;
}

void Pwd_Input_Process(uint8_t key)
{
	if (key <= 9U)
	{
		if (InputCnt < PWD_MAX_LEN)
		{
			InputBuf[InputCnt++] = key;
			Key_SaveFlag = 0;
		}
	}
	else if (key == 11U)
	{
		if (InputCnt > 0U)
		{
			InputBuf[--InputCnt] = 0;
		}
		Key_SaveFlag = 0;
	}
	else if ((key == 15U) && (InputCnt == PWD_MAX_LEN))
	{
		Pwd_Save();
	}
}

void Pwd_PrintInputToOLED(void)
{
	uint8_t i;
	uint8_t count = (InputCnt > PWD_MAX_LEN) ? PWD_MAX_LEN : InputCnt;
	OLED_Clear();
	OLED_ShowString(1, 1, "Input:");
	for (i = 0; i < PWD_MAX_LEN; i++)
	{
		OLED_ShowChar(2, (uint8_t)(2U + i * 2U), (i < count) ? '*' : ' ');
	}
}
