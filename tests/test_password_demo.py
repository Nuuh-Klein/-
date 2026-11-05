"""Run the password UI against the actual debounced matrix and input helper."""
import shutil
import tempfile
from pathlib import Path

from test_pin_migration import ROOT, STUB, HARNESS
from test_pattern_lock import run_target

TEST = HARNESS.split("int main(void) {")[0] + r"""
#include "password_demo.h"
#include "pattern_view.h"

static unsigned input_page, displayed_count, updates;
static char confirm[32], exit_hint[32];
void PasswordView_Input(uint8_t length, const char *ok, const char *exit_text) {
    input_page = 1;
    displayed_count = length;
    strcpy(confirm, ok);
    strcpy(exit_hint, exit_text);
    updates++;
}
void PatternView_Message(const char *a, const char *b, const char *c, const char *d) {
    assert(!strcmp(a, "PASSWORD TEST") && !strcmp(b, "2 Input password"));
    (void)c; (void)d;
    input_page = 0;
    updates++;
}
static void scan_demo(unsigned samples) {
    while (samples--) PasswordDemo_Update(KEY_Scan());
}
static void tap(unsigned position) {
    pressed = (uint16_t)(1U << position); scan_demo(25);
    pressed = 0; scan_demo(25);
}
int main(void) {
    unsigned i, before;
    InputEvent event;
    test_keyboard(); test_pwm_and_buzzer(); test_leds();
    PasswordDemo_Init(); assert(!input_page && !InputCnt && !Key_SaveFlag);
    tap(0); assert(!input_page); /* 1 does not open this test. */
    tap(1); assert(input_page && !InputCnt && !displayed_count);
    assert(!strcmp(confirm, "# OK") && !strcmp(exit_hint, "0 Exit"));
    tap(12); assert(!InputCnt); /* * on empty is harmless. */
    tap(14); assert(!Key_SaveFlag && !strcmp(confirm, "#Need6"));
    tap(0); tap(0); tap(4); /* 1,1,4: duplicate digits are valid. */
    assert(InputCnt == 3 && displayed_count == 3);
    assert(InputBuf[0] == 1 && InputBuf[1] == 1 && InputBuf[2] == 4);
    tap(12); assert(InputCnt == 2 && !InputBuf[2] && displayed_count == 2);
    tap(12); assert(InputCnt == 1 && InputBuf[0] == 1);
    tap(1); tap(2); tap(4); tap(5); tap(6); /* now 123456 */
    assert(InputCnt == 6 && displayed_count == 6 && !Key_SaveFlag);
    tap(10); assert(InputCnt == 6 && InputBuf[5] == 6 && !strcmp(confirm, "# Full"));
    before = updates;
    tap(3); tap(7); tap(11); tap(15); /* A/B/C/D never submit or edit. */
    assert(updates == before && !Key_SaveFlag);
    tap(14); assert(Key_SaveFlag && !strcmp(confirm, "# Done"));
    assert(PasswordDemo_TakeEvent(&event) && event.kind == INPUT_EVENT_SUBMITTED &&
           event.method == INPUT_METHOD_PASSWORD && event.password[5] == 6);
    assert(!PasswordDemo_TakeEvent(&event));
    tap(14); assert(!PasswordDemo_TakeEvent(&event)); /* no duplicate submission */
    for (i = 0; i < 6; i++) assert(SavedPwd[i] == i + 1U);
    tap(12); assert(!Key_SaveFlag && InputCnt == 5 && displayed_count == 5);
    tap(9); tap(14); assert(Key_SaveFlag && SavedPwd[5] == 8);
    /* A held key produces exactly one digit, including after entering the page. */
    tap(13); assert(!input_page && !InputCnt && !Key_SaveFlag); /* 0 exits. */
    assert(PasswordDemo_TakeEvent(&event) && event.kind == INPUT_EVENT_CANCELLED);
    assert(!event.password[0] && !event.dial.length && !PasswordDemo_TakeEvent(&event));
    for (i = 0; i < 6; i++) assert(!InputBuf[i] && !SavedPwd[i]);
    tap(1);
    pressed = 1U; scan_demo(200);
    assert(InputCnt == 1 && displayed_count == 1);
    pressed = 0; scan_demo(25);
    /* Multi-key input is rejected until every key is released. */
    pressed = 3U; scan_demo(30);
    pressed = 2U; scan_demo(30); assert(InputCnt == 1);
    pressed = 0; scan_demo(25); tap(1); assert(InputCnt == 2);
    tap(13); tap(1); assert(input_page && !InputCnt);
    assert(pulse == 500); /* Input confirmation never opens the servo. */
    puts("PASS: actual keypad -> UI, masked count, repeat digits, one-step delete, six-digit # confirmation, 0 exit, long press and multi-key isolation");
    return 0;
}
"""


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    build_root = ROOT / "basic/build"
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="host-password-", dir=build_root) as directory:
        temp = Path(directory)
        pins = "\n".join(f"#define GPIO_Pin_{i} (1U << {i})" for i in range(16))
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", pins + "\n#endif"))
        run_target(temp, "password", TEST,
                   ["hardware/key.c", "hardware/password_input.c", "hardware/motor.c", "hardware/buzzer.c", "hardware/led.c",
                    "hardware/input_event.c", "hardware/dial_sequence.c", "user/password_demo.c"])


if __name__ == "__main__":
    main()
