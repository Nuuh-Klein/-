"""Run the isolated duress UI with real credential/auth/input modules."""
import shutil
import tempfile
from pathlib import Path

from test_auth import BASE
from test_pin_migration import ROOT, STUB
from test_pattern_lock import FLASH_STUB, run_target

TEST = '#define TEST_ALLOW_AUDIT 1\n' + BASE + r"""
#include "interaction_demo.h"
#include "key.h"
#include "log.h"
#include "audit.h"
static char screen[4][32];
static int8_t queue[128];
static unsigned read_pos, write_pos, overflow;
void PatternView_Message(const char *a, const char *b, const char *c, const char *d) {
    memset(screen,0,sizeof(screen));
    strcpy(screen[0],a); strcpy(screen[1],b); strcpy(screen[2],c); strcpy(screen[3],d);
}
void PasswordView_Input(uint8_t length, const char *a, const char *b) {
    (void)length; (void)a; (void)b; strcpy(screen[0],"NUMBER INPUT");
}
void DialView_Show(const DialSequence *dial, const char *status) {
    (void)dial; strcpy(screen[0],"DIAL INPUT"); strcpy(screen[1],status);
}
void OLED_Clear(void) {}
void OLED_ShowString(uint8_t a, uint8_t b, const char *s) { (void)a; (void)b; (void)s; }
void OLED_ShowChar(uint8_t a, uint8_t b, char c) { (void)a; (void)b; (void)c; }
void Encoder_ClearCount(void) { read_pos = write_pos = 0; }
uint8_t Encoder_TakeOverflow(void) { uint8_t value = overflow; overflow = 0; return value; }
uint8_t Encoder_PopStep(int8_t *direction) {
    if (read_pos == write_pos) return 0;
    *direction = queue[read_pos++]; return 1;
}
static void key(uint8_t k) { InteractionDemo_Update(k); }
static void turn_dial(unsigned r, unsigned l, unsigned last) {
    while (r--) queue[write_pos++] = 1;
    while (l--) queue[write_pos++] = -1;
    while (last--) queue[write_pos++] = 1;
    key(15);
}
static void number(const char *digits) {
    key(2); assert(!strcmp(screen[0],"NUMBER INPUT"));
    while (*digits) key((uint8_t)(*digits++ - '0'));
    key(15); assert(!strcmp(screen[0],"RESULT"));
}
static void dial(unsigned r, unsigned l, unsigned last) {
    key(1); assert(!strcmp(screen[0],"DIAL INPUT"));
    while (r--) queue[write_pos++] = 1;
    while (l--) queue[write_pos++] = -1;
    while (last--) queue[write_pos++] = 1;
    key(15); assert(!strcmp(screen[0],"RESULT"));
}
static void audit(unsigned duress, unsigned numeric, unsigned success) {
    key(12); assert(!strcmp(screen[0],"AUDIT EVENT"));
    assert(!strcmp(screen[1], numeric ? "Method: NUMBER" : "Method: DIAL"));
    assert(!strcmp(screen[2], success ? "Result: PASS" : "Result: FAIL"));
    assert(!strcmp(screen[3], duress ? "DURESS:1 0 Back" : "DURESS:0 0 Back"));
    key(0); key(0); assert(!strcmp(screen[0],"DURESS TEST"));
}
static void test_home_dial(void) {
    Log_Record_t record;
    CredentialConfig config;
    char accepted[4][32];
    unsigned count = Log_GetCount();
    int writes = credential_operations;
    InteractionDemo_Init(); /* Home dial must load saved settings without visiting 3. */
    key(1); turn_dial(3,2,4); assert(!strcmp(screen[1],"PASS"));
    memcpy(accepted,screen,sizeof(screen));
    assert(Log_GetCount() == ++count && Log_Read(0,&record));
    assert(record.method == LOG_METHOD_DIAL && record.result == LOG_RESULT_PASS && !record.duress);
    key(15); key(15); assert(Log_GetCount() == count); /* held/repeated confirmation */
    key(11); turn_dial(2,3,4); assert(!memcmp(accepted,screen,sizeof(screen)));
    assert(Log_GetCount() == ++count && Log_Read(0,&record));
    assert(record.method == LOG_METHOD_DIAL && record.result == LOG_RESULT_PASS && record.duress);
    key(11); turn_dial(1,1,1); assert(!strcmp(screen[1],"NO MATCH"));
    assert(Log_GetCount() == ++count && Log_Read(0,&record));
    assert(record.method == LOG_METHOD_DIAL && record.result == LOG_RESULT_FAIL && !record.duress);
    key(11); turn_dial(3,0,0); assert(!strcmp(screen[1],"Need 3"));
    key(0); assert(Log_GetCount() == count);
    key(1); overflow = 1; turn_dial(3,2,4);
    assert(!strcmp(screen[1],"Lost! *X") && Log_GetCount() == count); key(0);
    InteractionDemo_Init(); key(4);
    assert(Log_GetCount() == count && !strcmp(screen[2],"DIAL FAIL D:0")); key(0);
    assert(credential_operations == writes); /* Never initialize/overwrite credentials implicitly. */
    assert(CredentialStore_Load(&config) == CREDENTIAL_STORE_OK);
    config.dial.steps[0] = 4;
    assert(CredentialStore_Save(&config) == CREDENTIAL_STORE_OK);
    InteractionDemo_Init(); key(1); turn_dial(3,2,4); assert(!strcmp(screen[1],"NO MATCH"));
    key(11); turn_dial(4,2,4); assert(!strcmp(screen[1],"PASS")); key(0);
    count = Log_GetCount(); block_audit = 1;
    for (unsigned i = 0; i < 5; i++) {
        key(1); turn_dial(4,2,4); assert(!strcmp(screen[1],"PASS")); key(0);
    }
    assert(Audit_GetPendingCount() == 4 && Audit_HasWriteError());
    key(1); turn_dial(4,2,4); assert(!strcmp(screen[1],"BUSY")); key(0);
    key(3); dial(4,2,4); assert(!strcmp(screen[1],"BUSY")); key(0); key(0);
    block_audit = 0;
    for (unsigned i = 0; i < 10; i++) key(KEY_NONE);
    assert(Log_GetCount() == count + 5 && !Audit_GetPendingCount() && !Audit_HasWriteError());
    puts("PASS: home dial normal/duress/failure persist and browse after reboot; saved settings, no duplicates, faults/cancel, shared queue retry/backpressure");
}
int main(void) {
    char normal_screen[4][32];
    CredentialConfig config = make_config(0), loaded;
    assert(!equals(&config,&(CredentialConfig){0})); /* shared helper */
    reset_flash(); InteractionDemo_Init();
    key(1); turn_dial(3,2,4);
    assert(!strcmp(screen[1],"NO CFG") && !Log_GetCount() && !operations);
    key(0); key(3);
    assert(!strcmp(screen[0],"DURESS TEST") && !operations);
    key(1); assert(!strcmp(screen[0],"DURESS TEST"));
    key(15); assert(operations && !strcmp(screen[2],"A Audit event"));
    assert(CredentialStore_Load(&loaded) == CREDENTIAL_STORE_OK && equals(&config,&loaded));
    int before = credential_operations;
    key(0); key(3); key(15); assert(credential_operations == before); /* never overwrite an existing config */
    number("111111"); assert(!strcmp(screen[1],"MATCH"));
    memcpy(normal_screen,screen,sizeof(screen)); audit(0,1,1);
    number("123456"); assert(!memcmp(normal_screen,screen,sizeof(screen))); audit(1,1,1);
    number("333333"); assert(!strcmp(screen[1],"NO MATCH")); audit(0,1,0);
    dial(3,2,4); assert(!memcmp(normal_screen,screen,sizeof(screen))); audit(0,0,1);
    dial(2,3,4); assert(!memcmp(normal_screen,screen,sizeof(screen))); audit(1,0,1);
    dial(1,1,1); assert(!strcmp(screen[1],"NO MATCH")); audit(0,0,0);
    assert(Log_GetCount() == 6);
    key(0); InteractionDemo_Init(); key(3); assert(credential_operations == before);
    number("123456"); assert(!strcmp(screen[1],"MATCH")); audit(1,1,1);
    unsigned previous_count = Log_GetCount();
    block_audit = 1;
    for (unsigned i = 0; i < 5; i++) { number("111111"); assert(!strcmp(screen[1],"MATCH")); key(0); }
    assert(Audit_GetPendingCount() == 4 && Audit_HasWriteError());
    number("111111"); assert(!strcmp(screen[1],"BUSY")); key(0);
    key(0); key(3); /* leave/re-enter with a pending Auth event: do not discard it */
    number("111111"); assert(!strcmp(screen[1],"BUSY")); key(0); key(0);
    block_audit = 0;
    for (unsigned i = 0; i < 10; i++) key(KEY_NONE);
    assert(!Audit_GetPendingCount() && !Audit_HasWriteError() && Log_GetCount() == previous_count + 5);
    key(3); key(0);
    key(0); key(2); assert(!strcmp(screen[0],"NUMBER INPUT")); key(0);
    assert(!strcmp(screen[0],"INPUT TEST"));
    test_home_dial();
    check_isolation();
    /* Explicit sample initialization reports write failure and allows retry. */
    reset_flash(); InteractionDemo_Init(); key(3); fail_at = 0; key(15);
    assert(!strcmp(screen[0],"SAVE FAILED")); fail_at = -1; key(15);
    assert(!strcmp(screen[0],"DURESS TEST"));
    puts("PASS: explicit-only sample initialization, four credential cases, identical visible success, manual audit diagnostics and reboot reload");
    return 0;
}
"""


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    with tempfile.TemporaryDirectory(prefix="host-duress-", dir=ROOT / "basic/build") as directory:
        temp = Path(directory)
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", FLASH_STUB + "\n#endif"))
        (temp / "stm32f10x_flash.h").write_text('#include "stm32f10x.h"\n')
        run_target(temp, "duress", TEST,
                   ["hardware/dial_sequence.c", "hardware/input_event.c", "hardware/password_input.c",
                    "hardware/credentials.c", "hardware/credential_store.c", "hardware/auth.c",
                    "hardware/log.c", "hardware/audit.c", "user/log_demo.c",
                    "user/password_demo.c", "user/interaction_demo.c", "user/duress_demo.c"])


if __name__ == "__main__":
    main()
