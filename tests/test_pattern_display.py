"""Decode actual OLED GPIO I2C output and check TIM3 clock configuration/IRQ."""
import shutil
import tempfile
from pathlib import Path

from test_pin_migration import ROOT, STUB
from test_pattern_lock import run_target

EXTRA = r"""
void GPIO_WriteBit(GPIO_TypeDef *, uint16_t, BitAction);
#define GPIO_Pin_8 (1U<<8)
#define GPIO_Pin_9 (1U<<9)
"""

DISPLAY_TEST = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "OLED.h"
#include "pattern_view.h"

GPIO_TypeDef gpio_a, gpio_b, gpio_c;
static unsigned scl = 1, sda = 1, active, phase, value, count;
static unsigned page, column, frames, data_bytes;
static unsigned char transaction[140], screen[8][128];
static void stop(void) {
    unsigned i;
    if (count < 2) return;
    assert(transaction[0] == 0x78);
    if (transaction[1] == 0) {
        assert(count == 3);
        value = transaction[2];
        if ((value & 0xF8U) == 0xB0U) page = value & 7U;
        else if ((value & 0xF0U) == 0x10U) column = (column & 15U) | ((value & 15U) << 4);
        else if ((value & 0xF0U) == 0) column = (column & 0xF0U) | (value & 15U);
    } else {
        assert(transaction[1] == 0x40);
        for (i = 2; i < count; i++) {
            assert(page < 8 && column < 128);
            screen[page][column++] = transaction[i];
            data_bytes++;
        }
        if (count == 130) frames++;
    }
}
void GPIO_WriteBit(GPIO_TypeDef *port, uint16_t pin, BitAction bit) {
    unsigned level = bit == Bit_SET;
    assert(port == GPIOB);
    if (pin == GPIO_Pin_9) {
        if (scl && sda && !level) {
            active = 1; phase = value = count = 0;
        } else if (scl && !sda && level && active) {
            stop(); active = 0;
        }
        sda = level;
    } else {
        assert(pin == GPIO_Pin_8);
        if (active && !scl && level) {
            if (phase < 8) {
                value = (value << 1) | sda;
                if (phase == 7) {
                    assert(count < sizeof(transaction));
                    transaction[count++] = (unsigned char)value;
                    value = 0;
                }
            }
            phase = (phase + 1) % 9;
        }
        scl = level;
    }
}
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *cfg) { (void)port; (void)cfg; }
void RCC_APB2PeriphClockCmd(uint32_t mask, FunctionalState enabled) { (void)mask; (void)enabled; }
void Delay_ms(uint32_t ms) { (void)ms; }
static unsigned pixel(unsigned x, unsigned y) { return !!(screen[y / 8][x] & (1U << (y % 8))); }
static void image(const char *path) {
    FILE *f = fopen(path, "wb");
    unsigned x, y;
    assert(f);
    fprintf(f, "P1\n128 64\n");
    for (y = 0; y < 64; y++) {
        for (x = 0; x < 128; x++) fprintf(f, "%u ", pixel(x, y));
        fputc('\n', f);
    }
    fclose(f);
}
int main(void) {
    Pattern p;
    unsigned i, x, y, changed;
    unsigned char empty[8][128], full[8][128];
    OLED_Init();
    OLED_FrameClear();
    OLED_DrawLine(0, 0, 127, 63);
    OLED_DrawLine(127, 0, 0, 63);
    OLED_DrawPixel(-1, 2, 1); OLED_DrawPixel(128, 2, 1);
    OLED_DrawPixel(1, -1, 1); OLED_DrawPixel(1, 64, 1);
    OLED_FrameShow();
    assert(pixel(0, 0) && pixel(127, 63) && pixel(127, 0) && pixel(0, 63));
    assert(frames == 8 && data_bytes == 2048);
    OLED_FrameClear(); OLED_FrameShow();
    for (i = 0; i < sizeof(screen); i++) assert(((unsigned char *)screen)[i] == 0);
    PatternView_Message("PATTERN DEMO", "1 Unlock", "2 Set pattern", "No pattern saved");
    image("basic/build/pattern-home.pbm");
    Pattern_Clear(&p); PatternView_Input(&p, "SET 1/2", 0);
    image("basic/build/pattern-empty.pbm");
    Pattern_Append(&p, 1); Pattern_Append(&p, 3);
    PatternView_Input(&p, "UNLOCK", 0);
    assert(pixel(13, 10) && pixel(53, 10) && !pixel(33, 10)); /* skipped 2 remains unselected */
    Pattern_Append(&p, 9); Pattern_Append(&p, 7);
    PatternView_Input(&p, "UNLOCK", 0);
    assert(pixel(20, 10) && pixel(50, 20) && pixel(40, 50));
    image("basic/build/pattern-1397.pbm");
    for (i = 2; i <= 8; i++) Pattern_Append(&p, (uint8_t)i);
    PatternView_Input(&p, "SET 2/2", "Used!");
    image("basic/build/pattern-full.pbm");
    PasswordView_Input(0, "# OK", "0 Exit");
    memcpy(empty, screen, sizeof(empty));
    image("basic/build/password-empty.pbm");
    PasswordView_Input(6, "# OK", "0 Exit");
    memcpy(full, screen, sizeof(full));
    image("basic/build/password-full.pbm");
    changed = 0;
    for (y = 0; y < 8; y++) {
        for (x = 0; x < 128; x++) {
            if (x < 64 || y >= 2) assert(empty[y][x] == full[y][x]);
            else if (empty[y][x] != full[y][x]) changed++;
        }
    }
    assert(changed > 0); /* Only the masked line changes, never the keypad. */
    PasswordView_Input(5, "# OK", "0 Exit");
    for (y = 0; y < 2; y++) {
        for (x = 66; x < 114; x++) {
            assert(screen[y][x] == (x >= 106 ? empty[y][x] : full[y][x]));
        }
    }
    image("basic/build/password-delete.pbm");
    PasswordView_Input(6, "# Done", "0 Exit");
    image("basic/build/password-done.pbm");
    puts("PASS: static password grid, star masking, delete restores one slot, UI preview frames");
    puts("PASS: actual OLED I2C frames decoded, bounded pixels, full refresh, text and direct-node trajectory");
    return 0;
}
"""

TIMER_EXTRA = r"""
typedef enum {RESET, SET} ITStatus;
typedef struct {uint32_t CFGR;} RCC_TypeDef;
extern RCC_TypeDef rcc;
#define RCC (&rcc)
#define RCC_CFGR_PPRE1 0x700U
#define RCC_APB1Periph_TIM3 32U
#define TIM3 (&tim3)
#define TIM3_IRQn 29U
#define TIM_IT_Update 1U
extern TIM_TypeDef tim3;
typedef struct {uint32_t PCLK1_Frequency;} RCC_ClocksTypeDef;
typedef struct {unsigned NVIC_IRQChannel, NVIC_IRQChannelCmd;
unsigned NVIC_IRQChannelPreemptionPriority, NVIC_IRQChannelSubPriority;} NVIC_InitTypeDef;
void RCC_GetClocksFreq(RCC_ClocksTypeDef *);
void TIM_TimeBaseStructInit(TIM_TimeBaseInitTypeDef *);
void TIM_ClearITPendingBit(TIM_TypeDef *, unsigned);
void TIM_ITConfig(TIM_TypeDef *, unsigned, FunctionalState);
ITStatus TIM_GetITStatus(TIM_TypeDef *, unsigned);
void NVIC_Init(NVIC_InitTypeDef *);
"""

TIMER_TEST = r"""
#include <assert.h>
#include <stdio.h>
#include "stm32f10x.h"
#include "timebase.h"
RCC_TypeDef rcc;
TIM_TypeDef tim3;
static uint32_t pclk1;
static unsigned period, prescaler, enabled, pending, irq_enabled;
void TIM3_IRQHandler(void);
void RCC_APB1PeriphClockCmd(uint32_t mask, FunctionalState on) {
    assert(mask == RCC_APB1Periph_TIM3 && on == ENABLE);
}
void RCC_GetClocksFreq(RCC_ClocksTypeDef *c) { c->PCLK1_Frequency = pclk1; }
void TIM_Cmd(TIM_TypeDef *t, FunctionalState on) { assert(t == TIM3); enabled = on; }
void TIM_InternalClockConfig(TIM_TypeDef *t) { assert(t == TIM3); }
void TIM_TimeBaseStructInit(TIM_TimeBaseInitTypeDef *t) { t->TIM_CounterMode = 0; }
void TIM_TimeBaseInit(TIM_TypeDef *t, TIM_TimeBaseInitTypeDef *cfg) {
    assert(t == TIM3); period = cfg->TIM_Period; prescaler = cfg->TIM_Prescaler;
}
void TIM_ClearITPendingBit(TIM_TypeDef *t, unsigned bit) {
    assert(t == TIM3 && bit == TIM_IT_Update); pending = 0;
}
void TIM_ITConfig(TIM_TypeDef *t, unsigned bit, FunctionalState on) {
    assert(t == TIM3 && bit == TIM_IT_Update && on == ENABLE); irq_enabled = 1;
}
ITStatus TIM_GetITStatus(TIM_TypeDef *t, unsigned bit) {
    assert(t == TIM3 && bit == TIM_IT_Update); return pending ? SET : RESET;
}
void NVIC_Init(NVIC_InitTypeDef *n) {
    assert(n->NVIC_IRQChannel == TIM3_IRQn && n->NVIC_IRQChannelCmd == ENABLE);
}
int main(void) {
    unsigned i;
    pclk1 = 36000000; rcc.CFGR = 0x400;
    Timebase_Init(); assert(enabled && irq_enabled && period == 999 && prescaler == 71);
    assert(!Timebase_Millis()); TIM3_IRQHandler(); assert(!Timebase_Millis());
    for (i = 0; i < 30000; i++) { pending = 1; TIM3_IRQHandler(); assert(!pending); }
    assert(Timebase_Millis() == 30000);
    pclk1 = 24000000; rcc.CFGR = 0;
    Timebase_Init(); assert(prescaler == 23 && !Timebase_Millis());
    puts("PASS: TIM3 APB1 clock doubling, 1ms update IRQ, pending flag clearing and independent timebase");
    return 0;
}
"""


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    with tempfile.TemporaryDirectory(prefix="host-display-", dir=ROOT / "basic/build") as directory:
        temp = Path(directory)
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", EXTRA + TIMER_EXTRA + "\n#endif"))
        run_target(temp, "display", DISPLAY_TEST,
                   ["hardware/pattern.c", "hardware/pattern_view.c", "hardware/OLED.c"],
                   ["-Wno-missing-braces"])  # Existing flat initializer in OLED_Font.h.
        run_target(temp, "timer", TIMER_TEST, ["system/timebase.c"])


if __name__ == "__main__":
    main()
