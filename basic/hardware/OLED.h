#ifndef __OLED_H
#define __OLED_H

#include "stm32f10x.h"

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, const char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);

/* Buffered graphics. Compose a frame, then flush once (1024 bytes of RAM).
 * Existing text functions retain their immediate behavior. */
void OLED_FrameClear(void);
void OLED_DrawPixel(int16_t x, int16_t y, uint8_t on);
void OLED_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1);
void OLED_DrawText(uint8_t x, uint8_t y, const char *text);
void OLED_FrameShow(void);

#endif
