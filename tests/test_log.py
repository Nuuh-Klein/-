"""Actual circular audit store and auth queue with erase/program fault injection."""
import binascii
import shutil
import struct
import tempfile
from pathlib import Path

from test_pin_migration import ROOT, STUB
from test_pattern_lock import FLASH_STUB, run_target

BASE = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "log.h"
#include "audit.h"
#include "flash.h"
#include "config.h"
static uint16_t flash_image[32768];
static int fail_at = -1, operations, erases, partial_failure, commit_error;
static unsigned index_of(uint32_t address) {
    assert(address >= 0x08000000UL && address < 0x08010000UL && !(address & 1U));
    return (address - 0x08000000UL) / 2U;
}
uint16_t FLASH_ReadHalfWord(uint32_t address) { return flash_image[index_of(address)]; }
static int fail_now(void) { int op = operations++; return fail_at >= 0 && op == fail_at; }
FLASH_Status FLASH_WriteHalfWord(uint32_t address, uint16_t value) {
    unsigned i = index_of(address);
    assert(address >= 0x0800DC00UL && address < 0x0800E400UL);
    if (fail_now()) {
        if (partial_failure) flash_image[i] &= (uint16_t)(value | 0x00FFU);
        return FLASH_ERROR_PG;
    }
    assert((flash_image[i] & value) == value); flash_image[i] &= value;
    if (commit_error && (address % 16U == 0U)) return FLASH_ERROR_PG;
    return FLASH_COMPLETE;
}
FLASH_Status FLASH_EraseAPage(uint32_t address) {
    assert(address == 0x0800DC00UL || address == 0x0800E000UL);
    if (fail_now()) {
        if (partial_failure) memset(&flash_image[index_of(address)],0xFF,512);
        return FLASH_ERROR_PG;
    }
    memset(&flash_image[index_of(address)],0xFF,1024); erases++; return FLASH_COMPLETE;
}
static void reset_flash(void) {
    memset(flash_image,0xFF,sizeof(flash_image));
    fail_at = -1; operations = erases = partial_failure = commit_error = 0;
    for (uint32_t a = 0x0800E400UL; a < 0x08010000UL; a += 1024)
        flash_image[index_of(a)] = 0x1357;
    flash_image[index_of(0x0800D800UL)] = 0x2468;
}
static void check_isolation(void) {
    for (uint32_t a = 0x0800E400UL; a < 0x08010000UL; a += 1024)
        assert(flash_image[index_of(a)] == 0x1357);
    assert(flash_image[index_of(0x0800D800UL)] == 0x2468);
}
static void test_failures(unsigned fill, int partial) {
    uint16_t snapshot[32768];
    Log_Record_t record;
    reset_flash(); Log_Init();
    for (unsigned i = 0; i < fill; i++) assert(Log_WriteEvent(LOG_METHOD_PASSWORD,1,0));
    memcpy(snapshot,flash_image,sizeof(snapshot));
    for (int failure = 0; failure < 10; failure++) {
        memcpy(flash_image,snapshot,sizeof(snapshot)); Log_Init();
        operations = 0; fail_at = failure; partial_failure = partial;
        unsigned success = Log_WriteEvent(LOG_METHOD_DIAL,1,1);
        fail_at = -1;
        assert(Log_Init() && Log_Read(0,&record));
        assert(record.serial == fill + success);
        if (!success) {
            assert(record.method == LOG_METHOD_PASSWORD && !record.duress);
            assert(Log_WriteEvent(LOG_METHOD_DIAL,1,1));
            assert(Log_Read(0,&record) && record.serial == fill + 1);
        }
        assert(record.method == LOG_METHOD_DIAL && record.duress);
        assert(Log_GetCount() >= 1 && Log_GetCount() <= 128);
        check_isolation();
    }
}
"""

def wrap_records():
    rows = []
    for serial in (0xFFFFFFFE, 0xFFFFFFFF, 0, 1):
        payload = struct.pack("<5H", 1, serial & 0xFFFF, serial >> 16, 1, 1)
        crc = binascii.crc_hqx(payload, 0xFFFF)
        rows.append("{" + ",".join(str(x) for x in (0xA704, 1, serial & 0xFFFF, serial >> 16, 1, 1, crc, 0xFFFF)) + "}")
    return "static const uint16_t wrap_records[4][8] = {" + ",".join(rows) + "};\n"

TEST = BASE + wrap_records() + r"""
static CredentialConfig config(void) {
    CredentialConfig c;
    for (unsigned i = 0; i < 6; i++) { c.password[i] = 1; c.duress_password[i] = i + 1; }
    DialSequence_Clear(&c.dial); DialSequence_Clear(&c.duress_dial);
    c.dial.length = c.duress_dial.length = 3;
    for (unsigned i = 0; i < 3; i++) c.dial.direction[i] = c.duress_dial.direction[i] = i == 1 ? -1 : 1;
    c.dial.steps[0] = 3; c.dial.steps[1] = 2; c.dial.steps[2] = 4;
    c.duress_dial.steps[0] = 2; c.duress_dial.steps[1] = 3; c.duress_dial.steps[2] = 4;
    return c;
}
static void test_queue(void) {
    AuthContext auth[5];
    CredentialConfig c = config();
    AuthAuditEvent copy;
    InputEvent input;
    Log_Record_t record;
    reset_flash(); assert(Audit_Init() && !operations);
    for (unsigned i = 0; i < 5; i++) {
        assert(Auth_Init(&auth[i],&c));
        if (i & 1U) InputEvent_SubmitDial(&input,&c.dial);
        else InputEvent_SubmitPassword(&input,c.duress_password);
        assert(Auth_Evaluate(&auth[i],&input).allow_unlock);
        assert(Audit_Capture(&auth[i],&copy) == (i < 4));
    }
    assert(Audit_GetPendingCount() == 4 && auth[4].audit_pending);
    assert(Auth_Evaluate(&auth[4],&input).result == AUTH_AUDIT_PENDING);
    operations = 0; fail_at = 0;
    assert(!Audit_Process() && Audit_GetPendingCount() == 4 && Audit_HasWriteError());
    assert(!Log_GetCount()); fail_at = -1;
    assert(Audit_Process() && Audit_GetPendingCount() == 3 && !Audit_HasWriteError());
    assert(Audit_Capture(&auth[4],&copy) && !auth[4].audit_pending);
    while (Audit_GetPendingCount()) assert(Audit_Process());
    assert(Log_GetCount() == 5);
    for (unsigned i = 0; i < 5; i++) {
        assert(Log_Read(i,&record) && record.serial == 5 - i && record.result);
        assert(record.duress == !(i & 1U));
    }
    check_isolation();
}
int main(void) {
    Log_Record_t record;
    reset_flash(); assert(!Log_WriteEvent(1,1,0));
    assert(Log_Init() && !Log_GetCount() && !operations && !erases);
    assert(!Log_Read(0,&record) && !Log_Read(0,0));
    assert(!Log_WriteEvent(0,1,0) && !Log_WriteEvent(1,2,0));
    assert(!Log_WriteEvent(1,0,1) && !Log_WriteEvent(3,1,1) && !Log_WriteEvent(5,1,1));
    assert(!operations);
    for (unsigned i = 1; i <= 500; i++) {
        assert(Log_WriteEvent((i & 1U) ? LOG_METHOD_PASSWORD : LOG_METHOD_DIAL,1,(uint8_t)(i % 3 == 0)));
        unsigned retained = i <= 128 ? i : 64 + (i - 1) % 64 + 1;
        assert(Log_GetCount() == retained);
        assert(Log_Read(0,&record) && record.serial == i);
        assert(Log_Read((uint16_t)(retained-1),&record) && record.serial == i-retained+1);
        if (i % 31 == 0) { int before = operations; assert(Log_Init() && operations == before); }
    }
    assert(erases == 6); check_isolation();
    reset_flash(); Log_Init();
    assert(Log_Write(1,0) && Log_Write(LOG_METHOD_DURESS,1) && Log_Write(3,1));
    assert(Log_Read(1,&record) && record.method == 1 && record.duress);
    flash_image[index_of(0x0800DC00UL+16+10)] ^= 1;
    int before = operations; Log_Init(); assert(operations == before && Log_GetCount() == 2 && Log_GetInvalidCount() == 1);
    assert(Log_Read(0,&record) && record.serial == 3);
    assert(Log_Read(1,&record) && record.serial == 1);
    assert(Log_WriteEvent(2,1,0) && Log_Read(0,&record) && record.serial == 4);
    commit_error = 1; assert(Log_WriteEvent(1,1,1)); commit_error = 0;
    assert(Log_GetCount() == 4 && Log_Read(0,&record) && record.serial == 5);
    reset_flash(); memcpy(&flash_image[index_of(0x0800DC00UL)],wrap_records,sizeof(wrap_records));
    Log_Init(); assert(Log_GetCount() == 4);
    assert(Log_Read(0,&record) && record.serial == 1);
    assert(Log_Read(1,&record) && record.serial == 0);
    assert(Log_Read(3,&record) && record.serial == 0xFFFFFFFEUL);
    assert(Log_WriteEvent(1,1,0) && Log_Read(0,&record) && record.serial == 2);
    test_failures(5,0); test_failures(64,0); test_failures(128,0);
    test_failures(5,1); test_failures(64,1); test_failures(128,1);
    test_queue();
    puts("PASS: 500 appends, oldest-page rotation, no-erase boot, CRC skip, half writes/erases, commit-error readback, serial wrap and region isolation");
    puts("PASS: queued auth events, retry without loss/duplicates, full-queue backpressure and one-shot normal/duress capture");
    return 0;
}
"""


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    build_root = ROOT / "basic/build"
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="host-log-", dir=build_root) as directory:
        temp = Path(directory)
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", FLASH_STUB + "\n#endif"))
        (temp / "stm32f10x_flash.h").write_text('#include "stm32f10x.h"\n')
        run_target(temp, "log", TEST, ["hardware/log.c", "hardware/audit.c", "hardware/auth.c",
                   "hardware/input_event.c", "hardware/credentials.c", "hardware/dial_sequence.c"])


if __name__ == "__main__":
    main()
