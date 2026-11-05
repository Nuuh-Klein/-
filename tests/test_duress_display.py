"""Render the real duress UI through its actual OLED I2C stream."""
import shutil
import tempfile
from pathlib import Path

from test_auth import BASE
from test_pattern_display import DISPLAY_TEST, EXTRA
from test_pin_migration import ROOT, STUB
from test_pattern_lock import FLASH_STUB, run_target

TEST = '#define TEST_ALLOW_AUDIT 1\n' + BASE + DISPLAY_TEST.split("int main(void) {")[0] + r"""
#include "interaction_demo.h"
#include "key.h"
#include "log.h"
static int8_t steps[16];
static unsigned next_step, step_count;
void Encoder_ClearCount(void) { next_step = step_count = 0; }
uint8_t Encoder_TakeOverflow(void) { return 0; }
uint8_t Encoder_PopStep(int8_t *direction) {
    if (next_step == step_count) return 0;
    *direction = steps[next_step++]; return 1;
}
static void key(uint8_t k) { InteractionDemo_Update(k); }
int main(void) {
    unsigned char accepted[8][128];
    CredentialConfig expected = make_config(0), loaded;
    OLED_Init(); reset_flash(); InteractionDemo_Init();
    image("basic/build/modules-menu.pbm");
    key(3); image("basic/build/duress-init.pbm"); key(15);
    assert(CredentialStore_Load(&loaded) == CREDENTIAL_STORE_OK && equals(&expected,&loaded));
    key(2); for (unsigned i = 0; i < 6; i++) key(1); key(15);
    memcpy(accepted,screen,sizeof(accepted)); image("basic/build/duress-normal.pbm");
    key(12); image("basic/build/duress-normal-audit.pbm"); key(0); key(0);
    key(2); for (unsigned i = 1; i <= 6; i++) key((uint8_t)i); key(15);
    assert(!memcmp(accepted,screen,sizeof(accepted)));
    image("basic/build/duress-success.pbm");
    key(12); image("basic/build/duress-flag.pbm"); key(0); key(0);
    key(1);
    steps[step_count++] = 1; steps[step_count++] = 1;
    steps[step_count++] = -1; steps[step_count++] = -1; steps[step_count++] = -1;
    for (unsigned i = 0; i < 4; i++) steps[step_count++] = 1;
    key(15); assert(!memcmp(accepted,screen,sizeof(accepted)));
    key(12); image("basic/build/duress-dial-flag.pbm");
    key(0); key(0); key(0); key(4);
    assert(Log_GetCount() == 3);
    image("basic/build/log-newest.pbm");
    steps[step_count++] = 1; key(KEY_NONE);
    image("basic/build/log-older.pbm");
    key(12); image("basic/build/log-test.pbm");
    assert(Log_GetCount() == 4);
    key(14); image("basic/build/log-status.pbm");
    key(15); key(13); image("basic/build/log-batch-confirm.pbm"); key(0);
    int writes = operations; key(15); assert(operations == writes);
    image("basic/build/log-refreshed.pbm");
    key(0); key(1);
    for (unsigned i = 0; i < 3; i++) steps[step_count++] = 1;
    for (unsigned i = 0; i < 2; i++) steps[step_count++] = -1;
    for (unsigned i = 0; i < 4; i++) steps[step_count++] = 1;
    key(15); memcpy(accepted,screen,sizeof(accepted));
    image("basic/build/home-dial-pass.pbm");
    key(11);
    for (unsigned i = 0; i < 2; i++) steps[step_count++] = 1;
    for (unsigned i = 0; i < 3; i++) steps[step_count++] = -1;
    for (unsigned i = 0; i < 4; i++) steps[step_count++] = 1;
    key(15);
    /* The sequence display intentionally differs; the PASS label must match. */
    for (unsigned page = 6; page < 8; page++)
        for (unsigned x = 64; x < 128; x++) assert(screen[page][x] == accepted[page][x]);
    key(0); key(4); assert(Log_GetCount() == 6);
    image("basic/build/home-dial-log.pbm");
    check_isolation();
    puts("PASS: actual OLED frames identical for normal/duress numeric/dial success; diagnostic flag only on explicit A");
    puts("PASS: actual log browser OLED frames, newest/older events, marked TEST append, status and batch confirmation");
    return 0;
}
"""


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    with tempfile.TemporaryDirectory(prefix="host-duress-display-", dir=ROOT / "basic/build") as directory:
        temp = Path(directory)
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", FLASH_STUB + EXTRA + "\n#endif"))
        (temp / "stm32f10x_flash.h").write_text('#include "stm32f10x.h"\n')
        run_target(temp, "duress_display", TEST,
                   ["hardware/OLED.c", "hardware/pattern_view.c", "hardware/pattern.c", "hardware/dial_view.c",
                    "hardware/dial_sequence.c", "hardware/input_event.c", "hardware/password_input.c",
                    "hardware/credentials.c", "hardware/credential_store.c", "hardware/auth.c",
                    "hardware/log.c", "hardware/audit.c", "user/log_demo.c",
                    "user/password_demo.c", "user/interaction_demo.c", "user/duress_demo.c"],
                   ["-Wno-missing-braces"])


if __name__ == "__main__":
    main()
