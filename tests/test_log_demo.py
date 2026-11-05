"""Real log viewer with persistent store, browse bounds and explicit TEST data."""
import shutil
import tempfile
from pathlib import Path

from test_log import BASE
from test_pin_migration import ROOT, STUB
from test_pattern_lock import FLASH_STUB, run_target

TEST = BASE.split("static void test_failures")[0] + r"""
#include "log_demo.h"
#include "key.h"
static char screen[4][32];
static int8_t queue[256];
static unsigned read_pos, write_pos;
void PatternView_Message(const char *a, const char *b, const char *c, const char *d) {
    assert(strlen(a) <= 16 && strlen(b) <= 16 && strlen(c) <= 16 && strlen(d) <= 16);
    strcpy(screen[0],a); strcpy(screen[1],b); strcpy(screen[2],c); strcpy(screen[3],d);
}
void Encoder_ClearCount(void) { read_pos = write_pos = 0; }
uint8_t Encoder_TakeOverflow(void) { return 0; }
uint8_t Encoder_PopStep(int8_t *direction) {
    if (read_pos == write_pos) return 0;
    *direction = queue[read_pos++]; return 1;
}
static void turn(int direction, unsigned n) {
    Encoder_ClearCount();
    while (n--) queue[write_pos++] = (int8_t)direction;
    LogDemo_Update(KEY_NONE);
}
int main(void) {
    Log_Record_t record;
    reset_flash(); assert(Audit_Init()); LogDemo_Start();
    assert(LogDemo_IsActive() && !strcmp(screen[1],"No records") && !operations);
    assert(Log_WriteEvent(1,1,0) && Log_WriteEvent(2,1,1) && Log_WriteEvent(1,0,0));
    LogDemo_Start();
    assert(!strcmp(screen[0],"LOG 001/003") && !strcmp(screen[1],"ID:0000000003"));
    turn(1,1); assert(!strcmp(screen[1],"ID:0000000002") && !strcmp(screen[2],"DIAL PASS D:1"));
    assert(Log_WriteEvent(5,1,0)); LogDemo_Update(KEY_NONE);
    assert(!strcmp(screen[0],"LOG 003/004") && !strcmp(screen[1],"ID:0000000002"));
    turn(1,100); assert(!strcmp(screen[1],"ID:0000000001"));
    turn(-1,100); assert(!strcmp(screen[1],"ID:0000000004"));
    LogDemo_Update(12); assert(!strcmp(screen[1],"ID:0000000005") && !strcmp(screen[2],"TEST PASS D:0"));
    LogDemo_Update(14); assert(!strcmp(screen[0],"LOG STATUS") && !strcmp(screen[1],"N:005 Bad:000"));
    int before = operations;
    LogDemo_Update(15); assert(operations == before && !strcmp(screen[0],"LOG 001/005"));
    LogDemo_Update(13); assert(!strcmp(screen[0],"ADD 130 TEST?") && operations == before);
    LogDemo_Update(0); assert(operations == before && LogDemo_IsActive());
    LogDemo_Update(13); LogDemo_Update(15);
    assert(Log_GetCount() == 71 && !strcmp(screen[0],"LOG 001/071"));
    assert(Log_Read(0,&record) && record.serial == 135 && record.method == LOG_METHOD_TEST);
    assert(Log_Read(70,&record) && record.serial == 65);
    before = operations; Log_Init(); LogDemo_Start(); assert(operations == before);
    assert(!strcmp(screen[1],"ID:0000000135"));
    /* A damaged record is skipped individually on the viewer's read-only rescan. */
    flash_image[index_of(0x0800DC00UL + 6U * 16U + 10U)] ^= 1U;
    LogDemo_Update(15); assert(operations == before && Log_GetCount() == 70);
    assert(!strcmp(screen[1],"ID:0000000134"));
    LogDemo_Update(14); assert(!strcmp(screen[1],"N:070 Bad:001"));
    LogDemo_Update(15); fail_at = operations; LogDemo_Update(12);
    assert(!strcmp(screen[0],"LOG WRITE FAILED")); fail_at = -1;
    LogDemo_Update(15); assert(Log_Read(0,&record) && record.serial == 134);
    LogDemo_Update(12); assert(Log_Read(0,&record) && record.serial == 135);
    check_isolation(); LogDemo_Update(0); assert(!LogDemo_IsActive());
    puts("PASS: empty/record/status pages, anchored browsing and bounds, explicit TEST append/batch, reboot, corruption rescan and write failure");
    return 0;
}
"""


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    with tempfile.TemporaryDirectory(prefix="host-log-ui-", dir=ROOT / "basic/build") as directory:
        temp = Path(directory)
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", FLASH_STUB + "\n#endif"))
        (temp / "stm32f10x_flash.h").write_text('#include "stm32f10x.h"\n')
        run_target(temp, "log_ui", TEST, ["hardware/log.c", "hardware/audit.c", "hardware/auth.c",
                   "hardware/input_event.c", "hardware/credentials.c", "hardware/dial_sequence.c", "user/log_demo.c"])


if __name__ == "__main__":
    main()
