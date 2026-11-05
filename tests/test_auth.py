"""Credential validation, pure duress decisions and double-page storage faults."""
import shutil
import tempfile
from pathlib import Path

from test_pin_migration import ROOT, STUB
from test_pattern_lock import run_target, FLASH_STUB

BASE = r"""
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "auth.h"
#include "credential_store.h"
#include "flash.h"
static uint16_t flash_image[32768];
static int fail_at = -1, operations, erases, credential_operations, audit_operations;
#ifdef TEST_ALLOW_AUDIT
static int block_audit;
#endif
static unsigned index_of(uint32_t address) {
    assert(address >= 0x08000000UL && address < 0x08010000UL && !(address & 1U));
    return (address - 0x08000000UL) / 2U;
}
uint16_t FLASH_ReadHalfWord(uint32_t address) { return flash_image[index_of(address)]; }
static int can_write(void) { int op = operations++; return fail_at < 0 || op != fail_at; }
FLASH_Status FLASH_WriteHalfWord(uint32_t address, uint16_t value) {
    unsigned i = index_of(address);
#ifdef TEST_ALLOW_AUDIT
    assert(address >= 0x0800DC00UL && address < 0x0800EC00UL);
#else
    assert(address >= 0x0800E400UL && address < 0x0800EC00UL);
#endif
    if (address >= 0x0800E400UL) credential_operations++; else audit_operations++;
#ifdef TEST_ALLOW_AUDIT
    if (block_audit && address < 0x0800E400UL) { operations++; return FLASH_ERROR_PG; }
#endif
    if (!can_write()) return FLASH_ERROR_PG;
    assert((flash_image[i] & value) == value);
    flash_image[i] &= value; return FLASH_COMPLETE;
}
FLASH_Status FLASH_EraseAPage(uint32_t address) {
#ifdef TEST_ALLOW_AUDIT
    assert(address == 0x0800DC00UL || address == 0x0800E000UL || address == 0x0800E400UL || address == 0x0800E800UL);
#else
    assert(address == 0x0800E400UL || address == 0x0800E800UL);
#endif
    if (address >= 0x0800E400UL) credential_operations++; else audit_operations++;
    if (!can_write()) return FLASH_ERROR_PG;
    memset(&flash_image[index_of(address)], 0xFF, 1024); erases++; return FLASH_COMPLETE;
}
static void reset_flash(void) {
    memset(flash_image, 0xFF, sizeof(flash_image));
    fail_at = -1; operations = erases = credential_operations = audit_operations = 0;
#ifdef TEST_ALLOW_AUDIT
    block_audit = 0;
#endif
    for (uint32_t a = 0x0800EC00UL; a < 0x08010000UL; a += 1024)
        flash_image[index_of(a)] = 0x1357;
#ifdef TEST_ALLOW_AUDIT
    flash_image[index_of(0x0800D800UL)] = 0x2468;
#else
    flash_image[index_of(0x0800E000UL)] = 0x2468;
#endif
}
static void check_isolation(void) {
    for (uint32_t a = 0x0800EC00UL; a < 0x08010000UL; a += 1024)
        assert(flash_image[index_of(a)] == 0x1357);
#ifdef TEST_ALLOW_AUDIT
    assert(flash_image[index_of(0x0800D800UL)] == 0x2468);
#else
    assert(flash_image[index_of(0x0800E000UL)] == 0x2468);
#endif
}
static CredentialConfig make_config(unsigned variant) {
    CredentialConfig c;
    for (unsigned i = 0; i < 6; i++) { c.password[i] = 1 + variant; c.duress_password[i] = i + 1; }
    DialSequence_Clear(&c.dial); DialSequence_Clear(&c.duress_dial);
    c.dial.length = c.duress_dial.length = 3;
    for (unsigned i = 0; i < 3; i++) c.dial.direction[i] = c.duress_dial.direction[i] = i == 1 ? -1 : 1;
    c.dial.steps[0] = 3 + variant; c.dial.steps[1] = 2; c.dial.steps[2] = 4;
    c.duress_dial.steps[0] = 2; c.duress_dial.steps[1] = 3; c.duress_dial.steps[2] = 4;
    return c;
}
static int equals(const CredentialConfig *a, const CredentialConfig *b) {
    return Credentials_PasswordEquals(a->password,b->password) &&
        Credentials_PasswordEquals(a->duress_password,b->duress_password) &&
        DialSequence_Equals(&a->dial,&b->dial) && DialSequence_Equals(&a->duress_dial,&b->duress_dial);
}
"""

TEST = BASE + r"""
static void test_auth(void) {
    CredentialConfig c = make_config(0), bad;
    AuthContext auth;
    InputEvent input;
    AuthDecision normal, duress, decision;
    AuthAuditEvent audit;
    assert(Credentials_IsValid(&c));
    bad = c; memcpy(bad.duress_password,bad.password,6); assert(!Auth_Init(&auth,&bad));
    InputEvent_SubmitPassword(&input,c.password);
    assert(Auth_Evaluate(&auth,&input).result == AUTH_NOT_CONFIGURED);
    bad = c; bad.duress_dial = c.dial; assert(!Credentials_IsValid(&bad));
    bad = c; bad.password[0] = 0; assert(!Credentials_IsValid(&bad));
    bad = c; bad.duress_dial.steps[0] = 0; assert(!Credentials_IsValid(&bad));
    assert(Auth_Init(&auth,&c));
    normal = Auth_Evaluate(&auth,&input);
    assert(normal.result == AUTH_NORMAL && normal.allow_unlock && !normal.duress);
    assert(Auth_Evaluate(&auth,&input).result == AUTH_AUDIT_PENDING);
    assert(Auth_TakeAudit(&auth,&audit) && audit.success && !audit.duress);
    assert(!Auth_TakeAudit(&auth,&audit));
    InputEvent_SubmitPassword(&input,c.duress_password);
    duress = Auth_Evaluate(&auth,&input);
    assert(duress.result == AUTH_DURESS && duress.allow_unlock == normal.allow_unlock && duress.duress);
    assert(Auth_Evaluate(&auth,&input).result == AUTH_AUDIT_PENDING);
    assert(Auth_TakeAudit(&auth,&audit) && audit.method == INPUT_METHOD_PASSWORD && audit.success && audit.duress);
    InputEvent_SubmitDial(&input,&c.dial); normal = Auth_Evaluate(&auth,&input);
    assert(normal.result == AUTH_NORMAL && normal.allow_unlock); assert(Auth_TakeAudit(&auth,&audit));
    InputEvent_SubmitDial(&input,&c.duress_dial); duress = Auth_Evaluate(&auth,&input);
    assert(duress.result == AUTH_DURESS && duress.allow_unlock && duress.duress);
    assert(Auth_TakeAudit(&auth,&audit) && audit.method == INPUT_METHOD_DIAL && audit.duress);
    input.dial.steps[0]++;
    decision = Auth_Evaluate(&auth,&input); assert(decision.result == AUTH_REJECTED && !decision.allow_unlock);
    assert(Auth_TakeAudit(&auth,&audit) && !audit.success && !audit.duress);
    input.dial.direction[1] = 1;
    assert(Auth_Evaluate(&auth,&input).result == AUTH_INVALID_INPUT && !Auth_TakeAudit(&auth,&audit));
    InputEvent_SubmitPassword(&input,c.password); input.password[0] = 0;
    assert(Auth_Evaluate(&auth,&input).result == AUTH_INVALID_INPUT);
    InputEvent_SetStatus(&input,INPUT_METHOD_PASSWORD,INPUT_EVENT_CANCELLED);
    assert(Auth_Evaluate(&auth,&input).result == AUTH_IGNORED && !Auth_TakeAudit(&auth,&audit));
    InputEvent_SetStatus(&input,INPUT_METHOD_DIAL,INPUT_EVENT_FAULT);
    assert(Auth_Evaluate(&auth,&input).result == AUTH_IGNORED);
    assert(Auth_Evaluate(&auth,0).result == AUTH_IGNORED);
}
static void fault_sweep(unsigned fill) {
    uint16_t snapshot[32768];
    CredentialConfig a = make_config(0), b = make_config(1), loaded;
    CredentialStoreResult result;
    reset_flash();
    for (unsigned i = 0; i < fill; i++) assert(CredentialStore_Save(&a) == CREDENTIAL_STORE_OK);
    memcpy(snapshot,flash_image,sizeof(snapshot));
    for (int fault = 0; fault < 35; fault++) {
        memcpy(flash_image,snapshot,sizeof(snapshot)); operations = 0; fail_at = fault;
        result = CredentialStore_Save(&b); fail_at = -1;
        assert(CredentialStore_Load(&loaded) == CREDENTIAL_STORE_OK);
        assert(equals(&loaded,result == CREDENTIAL_STORE_OK ? &b : &a));
        assert(CredentialStore_Save(&b) == CREDENTIAL_STORE_OK);
        assert(CredentialStore_Load(&loaded) == CREDENTIAL_STORE_OK && equals(&b,&loaded));
        check_isolation();
    }
}
int main(void) {
    CredentialConfig a = make_config(0), b = make_config(1), loaded;
    test_auth(); reset_flash();
    assert(CredentialStore_Load(&loaded) == CREDENTIAL_STORE_EMPTY && !operations && !loaded.password[0]);
    assert(CredentialStore_Save(&a) == CREDENTIAL_STORE_OK);
    assert(CredentialStore_Save(&b) == CREDENTIAL_STORE_OK);
    assert(CredentialStore_Load(&loaded) == CREDENTIAL_STORE_OK && equals(&b,&loaded));
    flash_image[index_of(0x0800E400UL + 64 + 10)] ^= 1;
    assert(CredentialStore_Load(&loaded) == CREDENTIAL_STORE_OK && equals(&a,&loaded));
    flash_image[index_of(0x0800E400UL + 10)] ^= 1;
    assert(CredentialStore_Load(&loaded) == CREDENTIAL_STORE_INVALID && !loaded.password[0] && !erases);
    a.duress_dial = a.dial;
    int before = operations;
    assert(CredentialStore_Save(&a) == CREDENTIAL_STORE_INVALID && operations == before);
    a = make_config(0); reset_flash();
    for (unsigned i = 0; i < 70; i++) {
        const CredentialConfig *c = i & 1U ? &a : &b;
        assert(CredentialStore_Save(c) == CREDENTIAL_STORE_OK);
        assert(CredentialStore_Load(&loaded) == CREDENTIAL_STORE_OK && equals(c,&loaded));
    }
    assert(erases == 4); check_isolation();
    fault_sweep(1); fault_sweep(16); fault_sweep(32);
    puts("PASS: numeric/dial normal and duress, identical allow decision, invalid/conflicting configs, one-shot audit and no overwrite");
    puts("PASS: read-only boot, CRC, 70 saves, two-page rotations, every write/erase failure boundary and legacy-region isolation");
    return 0;
}
"""


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    build_root = ROOT / "basic/build"
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="host-auth-", dir=build_root) as directory:
        temp = Path(directory)
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", FLASH_STUB + "\n#endif"))
        (temp / "stm32f10x_flash.h").write_text('#include "stm32f10x.h"\n')
        run_target(temp, "auth", TEST, ["hardware/dial_sequence.c", "hardware/input_event.c",
                   "hardware/credentials.c", "hardware/credential_store.c", "hardware/auth.c"])


if __name__ == "__main__":
    main()
