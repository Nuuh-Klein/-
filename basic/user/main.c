#include "stm32f10x.h"
#include "OLED.h"
#include "key.h"

static char KEY_GetLabel(uint8_t key)
{
	switch (key)
	{
		case 0: return '0';
		case 1: return '1';
		case 2: return '2';
		case 3: return '3';
		case 4: return '4';
		case 5: return '5';
		case 6: return '6';
		case 7: return '7';
		case 8: return '8';
		case 9: return '9';
		case 10: return 'D';
		case 11: return '*';
		case 12: return 'A';
		case 13: return 'B';
		case 14: return 'C';
		case 15: return '#';
		default: return '?';
	}
}

int main(void)
{
	uint8_t key;
	uint16_t event_count = 0;

	OLED_Init();
	KEY_Init();
	OLED_Clear();
	OLED_ShowString(1, 1, "4x4 Key Test");
	OLED_ShowString(2, 1, "Press S1-S16");
	OLED_ShowString(3, 1, "Last S: 00 Key:-");
	OLED_ShowString(4, 1, "Events: 0000");

	while (1)
	{
		key = KEY_Scan();
		if (key != KEY_NONE)
		{
			event_count++;
			OLED_ShowNum(3, 9, KEY_GetLastPosition(), 2);
			OLED_ShowChar(3, 16, KEY_GetLabel(key));
			OLED_ShowNum(4, 9, event_count, 4);
		}
	}
}
