#include "stm32f10x.h"
#include "key.h"
#include "Delay.h"
#include "OLED.h"

#define KEY_DEBOUNCE_SAMPLES 20U
#define KEY_RAW_NONE         0xFFU
#define KEY_RAW_AMBIGUOUS    0xFEU

uint8_t InputBuf[PWD_MAX_LEN] = {0};
uint8_t SavedPwd[PWD_MAX_LEN] = {0};
uint8_t InputCnt = 0;
uint8_t Key_SaveFlag = 0;

static uint8_t key_stable_raw = KEY_RAW_NONE;
static uint8_t key_candidate_raw = KEY_RAW_NONE;
static uint8_t key_debounce_count = 0;
static uint8_t key_multi_lock = 0;
static uint8_t key_last_position = 0;

static const uint16_t key_row_pins[4] = {R1_PIN, R2_PIN, R3_PIN, R4_PIN};
static const uint16_t key_col_pins[4] = {C1_PIN, C2_PIN, C3_PIN, C4_PIN};

/* 行优先：S1-S4 第一行，S5-S8 第二行，以此类推。 */
static const uint8_t key_codes[16] = {
	1, 2, 3, 12,       /* 1  2  3  A */
	4, 5, 6, 13,       /* 4  5  6  B */
	7, 8, 9, 14,       /* 7  8  9  C */
	11, 0, 15, 10      /* *  0  #  D */
};

static uint8_t KEY_ReadMatrix(void)
{
	uint8_t row;
	uint8_t col;
	uint8_t found = KEY_RAW_NONE;
	uint8_t found_count = 0;
	GPIO_InitTypeDef GPIO_InitStruct;

	for (row = 0; row < 4; row++)
	{
		/* 未选中的行置为高阻，避免多键按下时行输出之间相互冲突。 */
		GPIO_InitStruct.GPIO_Pin = R1_PIN | R2_PIN | R3_PIN | R4_PIN;
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
		GPIO_Init(KEY_ROW_PORT, &GPIO_InitStruct);
		GPIO_InitStruct.GPIO_Pin = key_row_pins[row];
		GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_OD;
		GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
		GPIO_Init(KEY_ROW_PORT, &GPIO_InitStruct);
		GPIO_ResetBits(KEY_ROW_PORT, key_row_pins[row]);
		Delay_us(2);

		for (col = 0; col < 4; col++)
		{
			if (GPIO_ReadInputDataBit(KEY_COL_PORT, key_col_pins[col]) == Bit_RESET)
			{
				found = (uint8_t)(row * 4U + col);
				found_count++;
			}
		}
	}

	GPIO_InitStruct.GPIO_Pin = R1_PIN | R2_PIN | R3_PIN | R4_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(KEY_ROW_PORT, &GPIO_InitStruct);

	if (found_count == 0U)
	{
		return KEY_RAW_NONE;
	}
	if (found_count != 1U)
	{
		return KEY_RAW_AMBIGUOUS;
	}
	return found;
}

void KEY_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct;

	RCC_APB2PeriphClockCmd(KEY_ROW_RCC | KEY_COL_RCC, ENABLE);
	GPIO_InitStruct.GPIO_Pin = R1_PIN | R2_PIN | R3_PIN | R4_PIN;
	/* 行线扫描时切换为开漏输出；未选中行保持高阻输入。 */
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
	GPIO_Init(KEY_ROW_PORT, &GPIO_InitStruct);

	GPIO_InitStruct.GPIO_Pin = C1_PIN | C2_PIN | C3_PIN | C4_PIN;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_Init(KEY_COL_PORT, &GPIO_InitStruct);

	key_stable_raw = KEY_RAW_NONE;
	key_candidate_raw = KEY_RAW_NONE;
	key_debounce_count = 0;
	key_multi_lock = 0;
	key_last_position = 0;
}

/**
 * @brief 非阻塞扫描 4x4 矩阵键盘，稳定按下时仅返回一次键值。
 * @return KEY_NONE 表示没有新按键；数字为 0~9，A/B/C 为 12~14，
 *         # 为 15，* 清空为 11，D 确认为 10。
 */
uint8_t KEY_Scan(void)
{
	uint8_t raw_key = KEY_ReadMatrix();

	/* 多键同时按下时不猜测键值，并要求全部释放后才重新使能。 */
	if (raw_key == KEY_RAW_AMBIGUOUS)
	{
		key_multi_lock = 1;
		key_stable_raw = KEY_RAW_NONE;
		key_candidate_raw = KEY_RAW_NONE;
		key_debounce_count = 0;
		Delay_ms(1);
		return KEY_NONE;
	}
	if (key_multi_lock != 0U)
	{
		if (raw_key == KEY_RAW_NONE)
		{
			if (key_debounce_count < KEY_DEBOUNCE_SAMPLES)
			{
				key_debounce_count++;
			}
			if (key_debounce_count >= KEY_DEBOUNCE_SAMPLES)
			{
				key_multi_lock = 0;
				key_debounce_count = 0;
				key_candidate_raw = KEY_RAW_NONE;
			}
		}
		else
		{
			key_debounce_count = 0;
		}
		Delay_ms(1);
		return KEY_NONE;
	}

	if (raw_key != key_candidate_raw)
	{
		key_candidate_raw = raw_key;
		key_debounce_count = 1;
	}
	else if (key_debounce_count < KEY_DEBOUNCE_SAMPLES)
	{
		key_debounce_count++;
	}

	if ((key_debounce_count >= KEY_DEBOUNCE_SAMPLES) &&
	    (key_stable_raw != key_candidate_raw))
	{
		key_stable_raw = key_candidate_raw;
		if (key_stable_raw != KEY_RAW_NONE)
		{
			key_last_position = (uint8_t)(key_stable_raw + 1U);
			Delay_ms(1);
			return key_codes[key_stable_raw];
		}
	}

	Delay_ms(1);
	return KEY_NONE;
}

uint8_t KEY_GetLastPosition(void)
{
	return key_last_position;
}

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
	if (key == KEY_NONE)
	{
		return;
	}
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
		Pwd_ClearInput();
	}
	else if ((key == 10U) && (InputCnt == PWD_MAX_LEN))
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
