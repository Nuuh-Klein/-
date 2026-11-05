#include "dial_view.h"
#include "OLED.h"

void DialView_Show(const DialSequence *sequence, const char *status)
{
    static const int16_t pointer[12][2] = {
        {28,6}, {37,8}, {44,15}, {46,24}, {44,33}, {37,40},
        {28,42}, {19,40}, {12,33}, {10,24}, {12,15}, {19,8}
    };
    int16_t x, y, distance;
    uint8_t i;
    char segment[] = "1:---";
    OLED_FrameClear();
    for (y = -21; y <= 21; y++) {
        for (x = -21; x <= 21; x++) {
            distance = x * x + y * y;
            if (distance >= 401 && distance <= 441) OLED_DrawPixel(28 + x, 24 + y, 1);
        }
    }
    OLED_DrawLine(28, 3, 28, 7);
    OLED_DrawLine(28, 41, 28, 45);
    OLED_DrawLine(7, 24, 11, 24);
    OLED_DrawLine(45, 24, 49, 24);
    OLED_DrawLine(28, 24, pointer[sequence->position][0], pointer[sequence->position][1]);
    OLED_DrawPixel(27, 24, 1);
    OLED_DrawPixel(29, 24, 1);
    OLED_DrawText(0, 48, "*X 0Out");
    for (i = 0; i < DIAL_SEGMENTS; i++) {
        segment[0] = (char)('1' + i);
        segment[2] = '-'; segment[3] = '-'; segment[4] = '-';
        if (i < sequence->length) {
            segment[2] = sequence->direction[i] > 0 ? 'R' : 'L';
            segment[3] = (char)('0' + sequence->steps[i] / 10U);
            segment[4] = (char)('0' + sequence->steps[i] % 10U);
        }
        OLED_DrawText(64, (uint8_t)(i * 16U), segment);
    }
    OLED_DrawText(64, 48, status);
    OLED_FrameShow();
}
