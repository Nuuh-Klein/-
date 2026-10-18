"""Run the actual migrated drivers against simulated GPIO using host GCC."""

from pathlib import Path
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]
STUB = r"""
#ifndef TEST_STM32_H
#define TEST_STM32_H
#include <stdint.h>
typedef enum {DISABLE, ENABLE} FunctionalState;
typedef enum {Bit_RESET, Bit_SET} BitAction;
typedef struct { unsigned mode[16]; uint16_t output; } GPIO_TypeDef;
extern GPIO_TypeDef gpio_a, gpio_b, gpio_c;
#define GPIOA (&gpio_a)
#define GPIOB (&gpio_b)
#define GPIOC (&gpio_c)
#define RCC_APB2Periph_GPIOA 1U
#define RCC_APB2Periph_GPIOB 2U
#define RCC_APB2Periph_GPIOC 4U
#define RCC_APB2Periph_AFIO 8U
#define RCC_APB1Periph_TIM2 16U
#define GPIO_Speed_2MHz 2U
#define GPIO_Speed_50MHz 50U
#define GPIO_Mode_IN_FLOATING 1U
#define GPIO_Mode_Out_OD 2U
#define GPIO_Mode_IPU 3U
#define GPIO_Mode_AF_PP 4U
#define GPIO_Mode_Out_PP 5U
#define GPIO_FullRemap_TIM2 0x00180300U
#define GPIO_Remap_SWJ_JTAGDisable 0x00300200U
typedef struct { uint16_t GPIO_Pin; unsigned GPIO_Mode, GPIO_Speed; } GPIO_InitTypeDef;
void GPIO_Init(GPIO_TypeDef *, GPIO_InitTypeDef *);
void GPIO_ResetBits(GPIO_TypeDef *, uint16_t);
void GPIO_SetBits(GPIO_TypeDef *, uint16_t);
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *, uint16_t);
void RCC_APB2PeriphClockCmd(uint32_t, FunctionalState);
void RCC_APB1PeriphClockCmd(uint32_t, FunctionalState);
void GPIO_PinRemapConfig(uint32_t, FunctionalState);
typedef struct { unsigned unused; } TIM_TypeDef;
extern TIM_TypeDef tim2;
#define TIM2 (&tim2)
#define TIM_Channel_1 0U
#define TIM_CKD_DIV1 0U
#define TIM_CounterMode_Up 0U
#define TIM_OCMode_PWM1 6U
#define TIM_OCPolarity_High 0U
#define TIM_OutputState_Enable 1U
#define TIM_OCPreload_Enable 1U
typedef struct {
    unsigned TIM_ClockDivision, TIM_CounterMode, TIM_Period;
    unsigned TIM_Prescaler, TIM_RepetitionCounter;
} TIM_TimeBaseInitTypeDef;
typedef struct {
    unsigned TIM_OCMode, TIM_OCPolarity, TIM_OutputState, TIM_Pulse;
} TIM_OCInitTypeDef;
void TIM_InternalClockConfig(TIM_TypeDef *);
void TIM_TimeBaseInit(TIM_TypeDef *, TIM_TimeBaseInitTypeDef *);
void TIM_OCStructInit(TIM_OCInitTypeDef *);
void TIM_OC1Init(TIM_TypeDef *, TIM_OCInitTypeDef *);
void TIM_OC1PreloadConfig(TIM_TypeDef *, unsigned);
void TIM_Cmd(TIM_TypeDef *, FunctionalState);
void TIM_SetCompare1(TIM_TypeDef *, unsigned);
#endif
"""

HARNESS = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "key.h"
#include "motor.h"
#include "buzzer.h"
#include "led.h"

GPIO_TypeDef gpio_a, gpio_b, gpio_c;
TIM_TypeDef tim2;
static uint16_t pressed;
static unsigned key_only, key_initializing, clocks, timer_clock, remap_calls, timer_enabled;
static unsigned period, prescaler, pulse, preload, low_writes, high_writes;
static unsigned elapsed_ms;

/* Independent expected wiring: old PA0..PA7 were R4/R3/R2/R1/C1/C2/C3/C4. */
static GPIO_TypeDef *const rows[4] = {GPIOA, GPIOB, GPIOB, GPIOB};
static const uint16_t row_pins[4] = {1U<<8, 1U<<15, 1U<<14, 1U<<13};
static const uint8_t codes[16] = {1,2,3,12,4,5,6,13,7,8,9,14,11,0,15,10};

static unsigned pin_number(uint16_t pin) {
    unsigned n = 0;
    assert(pin && !(pin & (pin - 1U)));
    while ((1U << n) != pin) ++n;
    return n;
}
static void check_key_pins(GPIO_TypeDef *port, uint16_t pins) {
    if (!key_only) return;
    assert((port == GPIOA && !(pins & ~0x1F00U)) ||
           (port == GPIOB && !(pins & ~0xE000U)));
}
static void check_active_rows(void) {
    unsigned count = 0;
    for (unsigned r = 0; r < 4; ++r) {
        unsigned mode = rows[r]->mode[pin_number(row_pins[r])];
        assert(mode == GPIO_Mode_IN_FLOATING || mode == GPIO_Mode_Out_OD ||
               (key_initializing && mode == 0));
        if (mode == GPIO_Mode_Out_OD) {
            assert(!(rows[r]->output & row_pins[r]));
            ++count;
        }
    }
    assert(count <= 1);
}
void GPIO_Init(GPIO_TypeDef *port, GPIO_InitTypeDef *cfg) {
    check_key_pins(port, cfg->GPIO_Pin);
    if (key_only) assert(cfg->GPIO_Speed == GPIO_Speed_2MHz);
    if (port == GPIOC) assert(cfg->GPIO_Speed == GPIO_Speed_2MHz);
    for (unsigned p = 0; p < 16; ++p)
        if (cfg->GPIO_Pin & (1U << p)) port->mode[p] = cfg->GPIO_Mode;
    if (key_only) check_active_rows();
}
void GPIO_ResetBits(GPIO_TypeDef *port, uint16_t pins) {
    check_key_pins(port, pins);
    port->output &= ~pins;
    ++low_writes;
}
void GPIO_SetBits(GPIO_TypeDef *port, uint16_t pins) {
    check_key_pins(port, pins);
    port->output |= pins;
    ++high_writes;
}
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *port, uint16_t pin) {
    assert(port == GPIOA);
    unsigned p = pin_number(pin);
    assert(p >= 9 && p <= 12 && port->mode[p] == GPIO_Mode_IPU);
    check_active_rows();
    for (unsigned r = 0; r < 4; ++r)
        if (rows[r]->mode[pin_number(row_pins[r])] == GPIO_Mode_Out_OD &&
            (pressed & (1U << (r * 4U + p - 9U)))) return Bit_RESET;
    return Bit_SET;
}
void RCC_APB2PeriphClockCmd(uint32_t mask, FunctionalState state) {
    assert(state == ENABLE);
    clocks |= mask;
}
void RCC_APB1PeriphClockCmd(uint32_t mask, FunctionalState state) {
    assert(mask == RCC_APB1Periph_TIM2 && state == ENABLE);
    timer_clock = 1;
}
void GPIO_PinRemapConfig(uint32_t map, FunctionalState state) {
    if (remap_calls++ == 0) {
        assert(map == GPIO_FullRemap_TIM2 && state == DISABLE);
    } else {
        assert(map == GPIO_Remap_SWJ_JTAGDisable && state == ENABLE);
    }
}
void TIM_InternalClockConfig(TIM_TypeDef *tim) { assert(tim == TIM2); }
void TIM_TimeBaseInit(TIM_TypeDef *tim, TIM_TimeBaseInitTypeDef *cfg) {
    assert(tim == TIM2 && cfg->TIM_CounterMode == TIM_CounterMode_Up);
    assert(cfg->TIM_ClockDivision == TIM_CKD_DIV1 && cfg->TIM_RepetitionCounter == 0);
    period = cfg->TIM_Period;
    prescaler = cfg->TIM_Prescaler;
}
void TIM_OCStructInit(TIM_OCInitTypeDef *cfg) { memset(cfg, 0, sizeof(*cfg)); }
void TIM_OC1Init(TIM_TypeDef *tim, TIM_OCInitTypeDef *cfg) {
    assert(tim == TIM2 && cfg->TIM_OCMode == TIM_OCMode_PWM1);
    assert(cfg->TIM_OCPolarity == TIM_OCPolarity_High);
    assert(cfg->TIM_OutputState == TIM_OutputState_Enable);
    pulse = cfg->TIM_Pulse;
}
void TIM_OC1PreloadConfig(TIM_TypeDef *tim, unsigned state) {
    assert(tim == TIM2 && state == TIM_OCPreload_Enable);
    preload = 1;
}
void TIM_Cmd(TIM_TypeDef *tim, FunctionalState state) {
    assert(tim == TIM2 && state == ENABLE);
    timer_enabled = 1;
}
void TIM_SetCompare1(TIM_TypeDef *tim, unsigned value) {
    assert(tim == TIM2);
    pulse = value;
}
void Delay_us(uint32_t us) { assert(us == 2); }
void Delay_ms(uint32_t ms) { elapsed_ms += ms; }
void OLED_Clear(void) {}
void OLED_ShowChar(uint8_t l, uint8_t c, char x) { (void)l; (void)c; (void)x; }
void OLED_ShowString(uint8_t l, uint8_t c, const char *s) { (void)l; (void)c; (void)s; }

static void scan_none(unsigned samples) {
    while (samples--) assert(KEY_Scan() == KEY_NONE);
}
static void press_stable(unsigned position) {
    pressed = (uint16_t)(1U << position);
    scan_none(19);
    assert(KEY_Scan() == codes[position]);
    assert(KEY_GetLastPosition() == position + 1U);
    scan_none(40); /* A held key must not repeat. */
}
static void release_stable(void) { pressed = 0; scan_none(20); }
static void test_keyboard(void) {
    key_only = 1;
    key_initializing = 1;
    KEY_Init();
    key_initializing = 0;
    assert((clocks & 3U) == 3U); /* GPIOA and GPIOB both enabled. */
    for (unsigned c = 9; c <= 12; ++c) assert(GPIOA->mode[c] == GPIO_Mode_IPU);
    scan_none(30);
    for (unsigned pos = 0; pos < 16; ++pos) {
        press_stable(pos);
        for (unsigned r = 0; r < 4; ++r)
            assert(rows[r]->mode[pin_number(row_pins[r])] == GPIO_Mode_IN_FLOATING);
        release_stable();
    }
    /* Short noise on both ports cannot emit a key event. */
    for (unsigned pos = 0; pos < 16; ++pos) {
        pressed = (uint16_t)(1U << pos); scan_none(10);
        pressed = 0; scan_none(10);
    }
    press_stable(0); release_stable();
    /* Multi-key input locks until ALL keys have been released and debounced. */
    const uint16_t pairs[] = {(1U<<0)|(1U<<1), (1U<<0)|(1U<<4), (1U<<0)|(1U<<15)};
    for (unsigned i = 0; i < 3; ++i) {
        pressed = pairs[i]; scan_none(25);
        pressed = 1U; scan_none(30);
        release_stable();
        press_stable(15); release_stable();
    }
    KEY_Init(); press_stable(0); release_stable();
    key_only = 0;
}
static void test_pwm_and_buzzer(void) {
    assert(PWM_GPIO_PORT == GPIOA && PWM_GPIO_PIN == (1U << 0));
    assert(BUZZER_GPIO_PORT == GPIOB && BUZZER_GPIO_PIN == (1U << 12));
    PWM_Init();
    assert(remap_calls == 2 && timer_clock && timer_enabled && preload);
    assert(GPIOA->mode[0] == GPIO_Mode_AF_PP && GPIOA->mode[15] == 0);
    assert(prescaler == 71 && period == 19999 && pulse == 500);
    PWM_open(); assert(pulse == 1500);
    PWM_close(); assert(pulse == 500);
    Buzzer_Init();
    assert(GPIOB->mode[12] == GPIO_Mode_Out_PP && (GPIOB->output & (1U<<12)));
    elapsed_ms = low_writes = high_writes = 0;
    buzzer1();
    assert(elapsed_ms == 50 && low_writes == 1 && high_writes == 1);
    elapsed_ms = low_writes = high_writes = 0;
    buzzer2();
    assert(elapsed_ms == 2000 && low_writes == 20 && high_writes == 20);
    /* Other assigned pins were never initialized by these drivers. */
    assert(GPIOA->mode[2] == 0 && GPIOA->mode[3] == 0);
    assert(GPIOB->mode[0] == 0 && GPIOB->mode[1] == 0 && GPIOB->mode[10] == 0);
    assert(GPIOB->mode[8] == 0 && GPIOB->mode[9] == 0);
}
static void test_leds(void) {
    LED_Init();
    assert(GPIOC->mode[14] == GPIO_Mode_Out_PP && GPIOC->mode[15] == GPIO_Mode_Out_PP);
    assert((GPIOC->output & 0xC000U) == 0xC000U);
    LED_Red_On();
    assert((GPIOC->output & 0xC000U) == 0x8000U);
    LED_Green_On();
    assert((GPIOC->output & 0xC000U) == 0);
    LED_Alloff();
    assert((GPIOC->output & 0xC000U) == 0xC000U);
}
int main(void) {
    test_keyboard();
    test_pwm_and_buzzer();
    test_leds();
    puts("PASS: 16 keys, debounce, held keys, multi-key lock, GPIO isolation, PA0 PWM, PB12 buzzer and LEDs");
    return 0;
}
"""


def main():
    gcc = shutil.which("gcc")
    if not gcc:
        raise SystemExit("Host GCC is required for this simulation.")
    build_root = ROOT / "basic/build"
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="host-pins-", dir=build_root) as directory:
        temp = Path(directory)
        pins = "\n".join(f"#define GPIO_Pin_{i} (1U << {i})" for i in range(16))
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", pins + "\n#endif"))
        (temp / "harness.c").write_text(HARNESS)
        exe = temp / "pin-migration.exe"
        subprocess.run([
            gcc, "-std=c99", "-Wall", "-Wextra", "-Werror",
            f"-I{temp}", f"-I{ROOT / 'basic/user'}",
            f"-I{ROOT / 'basic/hardware'}", f"-I{ROOT / 'basic/system'}",
            str(temp / "harness.c"),
            *(str(ROOT / "basic/hardware" / name) for name in ("key.c", "motor.c", "buzzer.c", "led.c")),
            "-o", str(exe),
        ], check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
