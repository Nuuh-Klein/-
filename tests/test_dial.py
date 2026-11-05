"""Exercise the actual encoder IRQ, ordered queue, debounce, sequence and test menu."""
import shutil
import tempfile
from pathlib import Path

from test_pin_migration import ROOT, STUB
from test_extended_peripherals import EXTRA
from test_pattern_lock import run_target

TEST = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "encoder.h"
#include "dial_sequence.h"
#include "dial_view.h"
#include "interaction_demo.h"
#include "duress_demo.h"
#include "key.h"

GPIO_TypeDef gpio_a, gpio_b, gpio_c;
static unsigned phase = 3, button, mask, pending;
static uint32_t now;
void EXTI0_IRQHandler(void);
void EXTI1_IRQHandler(void);
void EXTI15_10_IRQHandler(void);
void RCC_APB2PeriphClockCmd(uint32_t c, FunctionalState on) { (void)c; assert(on == ENABLE); }
void GPIO_Init(GPIO_TypeDef *p, GPIO_InitTypeDef *c) {
    assert(p == GPIOB && c->GPIO_Mode == GPIO_Mode_IPU);
    assert(c->GPIO_Pin == 3U || c->GPIO_Pin == (1U << 10));
}
uint8_t GPIO_ReadInputDataBit(GPIO_TypeDef *p, uint16_t pin) {
    assert(p == GPIOB);
    if (pin == 1U) return phase & 2U ? Bit_SET : Bit_RESET;
    if (pin == 2U) return phase & 1U ? Bit_SET : Bit_RESET;
    assert(pin == (1U << 10)); return button ? Bit_RESET : Bit_SET;
}
void GPIO_EXTILineConfig(uint8_t p, uint8_t pin) { assert(p == 1 && (pin < 2 || pin == 10)); }
void EXTI_Init(EXTI_InitTypeDef *e) { assert(e->EXTI_LineCmd == ENABLE); }
void EXTI_ClearITPendingBit(uint32_t bit) { pending &= ~bit; }
ITStatus EXTI_GetITStatus(uint32_t bit) { return pending & bit ? SET : RESET; }
void NVIC_Init(NVIC_InitTypeDef *n) { assert(n->NVIC_IRQChannelCmd == ENABLE); }
uint32_t __get_PRIMASK(void) { return mask; }
void __disable_irq(void) { mask = 1; }
void __enable_irq(void) { assert(mask); mask = 0; }
static void edge(unsigned next) {
    unsigned changed = phase ^ next;
    assert(!mask);
    phase = next;
    if (changed & 2U) pending |= EXTI_Line0;
    if (changed & 1U) pending |= EXTI_Line1;
    if (pending & EXTI_Line0) EXTI0_IRQHandler();
    else if (pending & EXTI_Line1) EXTI1_IRQHandler();
    assert(!pending);
}
static void turn(int direction, unsigned count) {
    static const unsigned right[4] = {1,3,0,2}, left[4] = {2,0,3,1};
    while (count--) {
        for (unsigned i = 0; i < 4; i++) edge(direction > 0 ? right[phase] : left[phase]);
    }
}
static void init_driver(void) {
    phase = 3; button = mask = pending = 0;
    Encoder_Init(); Encoder_Key_EXTI_Init();
    Encoder_Service(0); Encoder_Service(25);
}
static void test_driver(void) {
    int8_t direction;
    unsigned i;
    init_driver();
    edge(2); edge(3); /* contact bounce must not generate a full detent */
    assert(!Encoder_PopStep(&direction));
    turn(1,3); turn(-1,2); turn(1,4); /* all nine events before any reader */
    assert(Encoder_GetDelta() == 5 && Encoder_GetDelta() == 0);
    for (i = 0; i < 9; i++) {
        assert(Encoder_PopStep(&direction));
        assert(direction == ((i < 3 || i >= 5) ? 1 : -1));
    }
    assert(!Encoder_PopStep(&direction) && !Encoder_PopStep(0));
    edge(0); assert(!Encoder_PopStep(&direction)); /* invalid two-bit jump */
    turn(-1,1); assert(Encoder_PopStep(&direction) && direction == -1);
    turn(1,3); Encoder_ClearCount(); assert(!Encoder_PopStep(&direction) && !Encoder_GetDelta());
    turn(1,70); assert(Encoder_TakeOverflow() && !Encoder_TakeOverflow());
    for (i = 0; Encoder_PopStep(&direction); i++) assert(direction == 1);
    assert(i == 63); Encoder_ClearCount();
    turn(-1,1); mask = 1;
    assert(Encoder_PopStep(&direction) && direction == -1 && mask == 1);
    assert(!Encoder_TakeOverflow() && mask == 1); mask = 0;
    button = 1; Encoder_Service(26); Encoder_Service(50);
    assert(!Encoder_GetKeyPressed()); Encoder_Service(51);
    assert(Encoder_GetKeyPressed() && !Encoder_GetKeyPressed());
    Encoder_Service(900); assert(!Encoder_GetKeyPressed()); /* hold */
    button = 0; Encoder_Service(901);
    button = 1; Encoder_Service(910); /* release bounce cannot re-arm */
    assert(!Encoder_GetKeyPressed());
    button = 0; Encoder_Service(920); Encoder_Service(945);
    button = 1; Encoder_Service(946); Encoder_Service(971);
    assert(Encoder_GetKeyPressed());
    button = 0; Encoder_Service(972); Encoder_Service(997);
    pending = EXTI_Line10; EXTI15_10_IRQHandler(); assert(!Encoder_GetKeyPressed());
    button = 1; Encoder_Service(0xFFFFFFF0U); Encoder_Service(8);
    assert(!Encoder_GetKeyPressed()); Encoder_Service(9); assert(Encoder_GetKeyPressed());
    /* Boot-held button does not count as a confirmation. */
    Encoder_Key_EXTI_Init(); Encoder_Service(1000); Encoder_Service(1100);
    assert(!Encoder_GetKeyPressed());
}
static void test_sequence(void) {
    DialSequence s;
    unsigned i;
    DialSequence_Clear(&s); assert(!DialSequence_MatchesDemo(&s));
    assert(!DialSequence_Append(&s, 0) && !s.length);
    for (i = 0; i < 3; i++) assert(DialSequence_Append(&s, 1));
    for (i = 0; i < 2; i++) assert(DialSequence_Append(&s, -1));
    for (i = 0; i < 4; i++) assert(DialSequence_Append(&s, 1));
    assert(DialSequence_MatchesDemo(&s) && s.position == 5);
    {
        DialSequence other = s;
        other.position = 11;
        assert(DialSequence_Equals(&s, &other));
        other.steps[0]++;
        assert(!DialSequence_Equals(&s, &other));
        other.direction[1] = 1;
        assert(!DialSequence_IsValid(&other));
        assert(!DialSequence_IsValid(0));
    }
    assert(!DialSequence_Append(&s, -1) && s.invalid && !DialSequence_MatchesDemo(&s));
    DialSequence_Clear(&s); assert(DialSequence_Append(&s, -1) && s.position == 11);
    assert(!DialSequence_MatchesDemo(&s));
    DialSequence_Clear(&s);
    for (i = 0; i < 99; i++) assert(DialSequence_Append(&s, 1));
    assert(!DialSequence_Append(&s, 1) && s.invalid && s.steps[0] == 99);
}

static DialSequence shown;
static char status[32];
static unsigned view_count, home, numeric;
void DialView_Show(const DialSequence *s, const char *text) {
    shown = *s; strcpy(status, text); view_count++; home = 0;
}
void PatternView_Message(const char *a, const char *b, const char *c, const char *d) {
    assert(!strcmp(a,"INPUT TEST") && !strcmp(b,"1 Dial test"));
    assert(!strcmp(c,"2 Password test") && !strcmp(d,"3Duress 4Logs")); home = 1;
}
void Pwd_ClearInput(void) { numeric = 0; }
void PasswordDemo_EnterInput(void) { numeric = 1; home = 0; }
uint8_t PasswordDemo_IsActive(void) { return (uint8_t)numeric; }
void PasswordDemo_Update(uint8_t key) { if (key == 0) numeric = 0; }
uint8_t PasswordDemo_TakeEvent(InputEvent *event) { InputEvent_Clear(event); return 0; }
void DuressDemo_Start(void) {}
void DuressDemo_Update(uint8_t key) { (void)key; }
uint8_t DuressDemo_IsActive(void) { return 0; }
void DuressDemo_Reset(void) {}
void DuressDemo_ServiceAudit(void) {}
AuthDecision DuressDemo_EvaluateInput(const InputEvent *event) {
    AuthDecision decision = {AUTH_REJECTED, INPUT_METHOD_DIAL, 0, 0};
    assert(event->kind == INPUT_EVENT_SUBMITTED && event->method == INPUT_METHOD_DIAL);
    if (DialSequence_MatchesDemo(&event->dial)) {
        decision.result = AUTH_NORMAL; decision.allow_unlock = 1;
    }
    return decision;
}
uint8_t Audit_Init(void) { return 1; }
uint8_t Audit_Process(void) { return 0; }
void LogDemo_Start(void) {}
void LogDemo_Update(uint8_t key) { (void)key; }
uint8_t LogDemo_IsActive(void) { return 0; }
static void update(uint8_t key) { ++now; InteractionDemo_Update(key); }
static void click(void) {
    button = 1;
    Encoder_Key_Flag = 1; /* Even a legacy PB10 press event cannot submit. */
    update(KEY_NONE);
    button = 0; update(KEY_NONE);
}
int main(void) {
    unsigned before;
    InputEvent event;
    test_driver(); test_sequence();
    init_driver(); now = 30; InteractionDemo_Init(); assert(home);
    turn(1,2); click(); update(1); assert(!shown.length); /* stale home inputs discarded */
    before = view_count; click(); assert(view_count == before && !shown.length);
    update(15); assert(!strcmp(status,"Need 3") && !shown.length);
    turn(1,3); turn(-1,2); turn(1,4); update(KEY_NONE);
    assert(DialSequence_MatchesDemo(&shown));
    before = view_count; click(); assert(view_count == before && !strcmp(status,"# OK"));
    update(10); assert(view_count == before); /* D is not #. */
    update(15); assert(!strcmp(status,"PASS"));
    assert(InteractionDemo_TakeEvent(&event) && event.kind == INPUT_EVENT_SUBMITTED &&
           event.method == INPUT_METHOD_DIAL && event.dial.steps[0] == 3 && !event.dial.position);
    assert(!InteractionDemo_TakeEvent(&event));
    before = view_count; turn(-1,1); click(); update(15); assert(view_count == before);
    update(11); assert(!shown.length && !strcmp(status,"# OK"));
    turn(-1,3); turn(1,2); turn(-1,4); update(KEY_NONE); update(15);
    assert(!strcmp(status,"NO MATCH"));
    assert(InteractionDemo_TakeEvent(&event) && event.kind == INPUT_EVENT_SUBMITTED);
    update(11); turn(1,1); turn(-1,1); turn(1,1); turn(-1,1); update(KEY_NONE); update(15);
    assert(shown.invalid && !strcmp(status,"Long! *X"));
    assert(InteractionDemo_TakeEvent(&event) && event.kind == INPUT_EVENT_FAULT);
    update(11); turn(1,70); update(KEY_NONE); update(15);
    assert(shown.invalid && !strcmp(status,"Lost! *X"));
    update(11); turn(1,3); update(0); assert(home);
    assert(InteractionDemo_TakeEvent(&event) && event.kind == INPUT_EVENT_CANCELLED);
    update(1); assert(!shown.length); update(0); update(2); assert(numeric);
    turn(1,3); click(); assert(numeric); update(0); assert(home);
    update(1); assert(!shown.length);
    puts("PASS: actual encoder IRQ and ordered reversals, bounce, overflow, masked IRQ state, press/release debounce and timer wrap");
    puts("PASS: keypad # submission, PB10/D ignored, R3/L2/R4 matching, wrong/extra turns, fixed result, clear/exit and numeric routing");
    return 0;
}
"""


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    build_root = ROOT / "basic/build"
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="host-dial-", dir=build_root) as directory:
        temp = Path(directory)
        pins = "\n".join(f"#define GPIO_Pin_{i} (1U << {i})" for i in range(16))
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", EXTRA + pins + "\n#endif"))
        run_target(temp, "dial", TEST,
                   ["hardware/encoder.c", "hardware/dial_sequence.c", "hardware/input_event.c",
                    "user/interaction_demo.c"])
        # Exercise the two calibration parameters independently of the default build.
        config = (ROOT / "basic/user/config.h").read_text(encoding="utf-8")
        config = config.replace("ENCODER_DIRECTION          1", "ENCODER_DIRECTION          -1")
        config = config.replace("ENCODER_EDGES_PER_STEP     4", "ENCODER_EDGES_PER_STEP     2")
        (temp / "config.h").write_text(config, encoding="utf-8")
        variant = TEST.split("static void test_driver(void)")[0] + r'''
int main(void) {
    int8_t direction;
    (void)now;
    init_driver();
    turn(1, 1);
    assert(Encoder_PopStep(&direction) && direction == -1);
    assert(Encoder_PopStep(&direction) && direction == -1);
    assert(!Encoder_PopStep(&direction) && Encoder_GetDelta() == -2);
    turn(-1, 1);
    assert(Encoder_PopStep(&direction) && direction == 1);
    assert(Encoder_PopStep(&direction) && direction == 1);
    puts("PASS: configurable reversed direction and two-edge detents");
    return 0;
}
'''
        run_target(temp, "calibration", variant, ["hardware/encoder.c"])


if __name__ == "__main__":
    main()
