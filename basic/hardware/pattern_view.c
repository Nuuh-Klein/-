#include "pattern_view.h"
#include "OLED.h"

static int16_t NodeX(uint8_t node) { return 10 + ((node - 1U) % 3U) * 20; }
static int16_t NodeY(uint8_t node) { return 10 + ((node - 1U) / 3U) * 20; }

void PatternView_Message(const char *title, const char *second,
                         const char *third, const char *fourth)
{
    OLED_FrameClear();
    OLED_DrawText(0, 0, title);
    OLED_DrawText(0, 16, second);
    OLED_DrawText(0, 32, third);
    OLED_DrawText(0, 48, fourth);
    OLED_FrameShow();
}

static void DrawNode(uint8_t node, uint8_t selected)
{
    static const uint8_t digits[9][5] = {
        {2,6,2,2,7}, {7,1,7,4,7}, {7,1,7,1,7},
        {5,5,7,1,1}, {7,4,7,1,7}, {7,4,7,5,7},
        {7,1,2,2,2}, {7,5,7,5,7}, {7,5,7,1,7}
    };
    int16_t x, y, distance, cx = NodeX(node), cy = NodeY(node);
    uint8_t row, column;
    for (y = -5; y <= 5; y++) {
        for (x = -5; x <= 5; x++) {
            distance = x * x + y * y;
            if (distance <= 25) {
                OLED_DrawPixel(cx + x, cy + y, selected || distance >= 16);
            }
        }
    }
    for (row = 0; row < 5U; row++) {
        for (column = 0; column < 3U; column++) {
            if (digits[node - 1U][row] & (1U << (2U - column)))
                OLED_DrawPixel(cx - 1 + column, cy - 2 + row, !selected);
        }
    }
}

void PatternView_Input(const Pattern *pattern, const char *title, const char *note)
{
    uint8_t i, node, selected;
    char count[] = "N:0/9";
    OLED_FrameClear();
    for (i = 1; i < pattern->length; i++) {
        OLED_DrawLine(NodeX(pattern->nodes[i - 1U]), NodeY(pattern->nodes[i - 1U]),
                      NodeX(pattern->nodes[i]), NodeY(pattern->nodes[i]));
    }
    for (node = 1; node <= 9U; node++) {
        selected = 0;
        for (i = 0; i < pattern->length; i++) {
            if (pattern->nodes[i] == node) selected = 1;
        }
        DrawNode(node, selected);
    }
    count[2] = (char)('0' + pattern->length);
    OLED_DrawText(66, 0, title);
    OLED_DrawText(66, 16, note == 0 ? count : note);
    OLED_DrawText(66, 32, "#OK *X");
    OLED_DrawText(66, 48, "0 Back");
    OLED_FrameShow();
}

void PasswordView_Input(uint8_t length, const char *confirm_hint, const char *exit_hint)
{
    uint8_t node, i;
    char masked[] = "______";
    if (length > 6U) length = 6U;
    for (i = 0; i < length; i++) masked[i] = '*';
    OLED_FrameClear();
    for (node = 1; node <= 9U; node++) DrawNode(node, 0);
    OLED_DrawText(66, 0, masked);
    OLED_DrawText(66, 16, confirm_hint);
    OLED_DrawText(66, 32, "* Del");
    OLED_DrawText(66, 48, exit_hint);
    OLED_FrameShow();
}
