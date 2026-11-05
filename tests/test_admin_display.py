"""Render administrator app through the actual OLED GPIO/I2C stream."""
import tempfile
from pathlib import Path
from admin_test_support import FLASH_MODEL, UI_MODEL, DRIVERS
from test_pin_migration import ROOT, STUB
from test_pattern_lock import FLASH_STUB, run_target
from test_pattern_display import DISPLAY_TEST, EXTRA

model = UI_MODEL.replace('static char screen[4][40];', '')
model = model[:model.index('static void enroll_initial_admin(void)')]
start = model.index('void PatternView_Message(')
end = model.index('uint32_t Timebase_Millis(', start)
model = model[:start] + model[end:]

TEST = FLASH_MODEL + DISPLAY_TEST.split('int main(void) {')[0] + model + r'''
static void snapshot(const char *name) {
    char path[160];snprintf(path,sizeof(path),"basic/build/admin-%s.pbm",name);image(path);
}
int main(void) {
    CredentialConfig config=make_config(0);
    unsigned char accepted[8][128], before[8][128];
    assert(equals(&config,&config));
    OLED_Init();init_test();snapshot("home");
    memcpy(before,screen,sizeof(screen));
    OLED_FrameClear();OLED_DrawText(36,0,"LOCKING");
    OLED_DrawText(0,16,"1 Keypad PIN");OLED_DrawText(0,32,"2 Dial PIN");OLED_DrawText(0,48,"A Admin");OLED_FrameShow();
    assert(!memcmp(before,screen,sizeof(screen)));
    key(1);memcpy(before,screen,sizeof(screen));snapshot("keypad-input");
    PasswordView_Input(0,"# OK","0 Back");assert(!memcmp(before,screen,sizeof(screen)));key(0);
    key(2);memcpy(before,screen,sizeof(screen));snapshot("dial-input");
    DialSequence empty_dial;DialSequence_Clear(&empty_dial);
    DialView_Show(&empty_dial,"# OK");assert(!memcmp(before,screen,sizeof(screen)));key(0);
    key(12);snapshot("first-admin");key(15);snapshot("enroll-1");
    key(15);snapshot("enroll-2");present=0;key(15);snapshot("enroll-3");
    present=20;key(15);snapshot("enroll-ok");tick();snapshot("verify");
    key(15);snapshot("init-passwords");key(15);tick();snapshot("menu");
    key(1);snapshot("choose-normal");key(1);snapshot("number-first");digits("222222");
    snapshot("number-second");digits("222222");snapshot("password-saved");tick();
    key(2);snapshot("choose-duress");key(2);turn(2,4,4);snapshot("dial-second");turn(2,4,4);tick();
    key(4);snapshot("logs");memcpy(before,screen,sizeof(screen));int writes=operations;
    key(12);key(13);assert(!memcmp(before,screen,sizeof(screen)) && operations==writes);
    key(0);key(0);key(1);digits("222222");memcpy(accepted,screen,sizeof(screen));snapshot("numeric-pass");
    key(0);key(1);digits("123456");assert(!memcmp(accepted,screen,sizeof(screen)));
    key(0);key(12);present=10;key(15);snapshot("not-admin");
    key(0);key(12);injected_error=AS608_ERR_TIMEOUT;AS608_LastCommand=0x13;
    AS608_LastDiag.tx_bytes=16;AS608_LastDiag.tx_complete=1;
    key(15);snapshot("uart-error");key(13);snapshot("uart-bytes");key(14);snapshot("uart-registers");
    key(10);snapshot("uart-probe");
    check_isolation();
    puts("PASS: centered LOCKING home, 1 keypad / 2 dial, unchanged input frames and administrator menu/edit/log/error; normal/duress success identical");
    return 0;
}
'''

def main():
    with tempfile.TemporaryDirectory(prefix='host-admin-display-', dir=ROOT / 'basic/build') as directory:
        temp = Path(directory)
        (temp / 'stm32f10x.h').write_text(STUB.replace('#endif', FLASH_STUB + EXTRA + '\n#endif'))
        (temp / 'stm32f10x_flash.h').write_text('#include "stm32f10x.h"\n')
        run_target(temp, 'admin_display', TEST,
                   DRIVERS + ['hardware/OLED.c', 'hardware/pattern_view.c', 'hardware/pattern.c', 'hardware/dial_view.c'],
                   ['-Wno-missing-braces'])

if __name__ == '__main__':
    main()
