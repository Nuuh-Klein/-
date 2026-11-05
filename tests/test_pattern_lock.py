"""Exercise actual pattern/storage/application C modules with Flash fault injection."""
from pathlib import Path
import shutil
import subprocess
import tempfile

from test_pin_migration import ROOT, STUB

FLASH_STUB = r"""
typedef unsigned FLASH_Status;
#define FLASH_COMPLETE 0U
#define FLASH_ERROR_PG 1U
"""

FLASH_MODEL = r"""
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include "config.h"
#include "pattern.h"
#include "pattern_store.h"
#include "flash.h"

static uint16_t flash_image[0x10000U / 2U];
static int fail_after = -1, operations, erase_count;
static unsigned index_of(uint32_t address) {
    assert(address >= 0x08000000U && address < 0x08010000U && !(address & 1U));
    return (address - 0x08000000U) / 2U;
}
uint16_t FLASH_ReadHalfWord(uint32_t address) { return flash_image[index_of(address)]; }
static int may_write(void) {
    if (fail_after >= 0 && operations++ == fail_after) return 0;
    return 1;
}
FLASH_Status FLASH_WriteHalfWord(uint32_t address, uint16_t value) {
    unsigned i = index_of(address);
    assert(address >= 0x0800EC00U && address < 0x0800F400U);
    if (!may_write()) return FLASH_ERROR_PG;
    assert((flash_image[i] & value) == value);
    flash_image[i] &= value;
    return FLASH_COMPLETE;
}
FLASH_Status FLASH_EraseAPage(uint32_t address) {
    assert(address == 0x0800EC00U || address == 0x0800F000U);
    if (!may_write()) return FLASH_ERROR_PG;
    memset(&flash_image[index_of(address)], 0xFF, 1024);
    erase_count++;
    return FLASH_COMPLETE;
}
static Pattern make(const char *digits) {
    Pattern p;
    Pattern_Clear(&p);
    while (*digits) assert(Pattern_Append(&p, (uint8_t)(*digits++ - '0')));
    return p;
}
static void reset_flash(void) {
    memset(flash_image, 0xFF, sizeof(flash_image));
    fail_after = -1; operations = erase_count = 0;
    /* Sentinels in application code, audit and numeric password regions. */
    flash_image[index_of(0x0800E800U)] = 0x1357;
    flash_image[index_of(0x0800F400U)] = 0x2468;
    flash_image[index_of(0x0800F800U)] = 0xABCD;
    flash_image[index_of(0x0800FC00U)] = 0xA5A5;
}
static void check_regions(void) {
    assert(FLASH_ReadHalfWord(0x0800E800U) == 0x1357);
    assert(FLASH_ReadHalfWord(0x0800F400U) == 0x2468);
    assert(FLASH_ReadHalfWord(0x0800F800U) == 0xABCD);
    assert(FLASH_ReadHalfWord(0x0800FC00U) == 0xA5A5);
}
"""

STORAGE_TEST = FLASH_MODEL + r"""
static void test_rules(void) {
    Pattern p = make("1397"), q = make("123697");
    unsigned i;
    assert(p.length == 4 && p.nodes[1] == 3); /* no automatic midpoint */
    assert(!Pattern_Equals(&p, &q));
    assert(!Pattern_Append(&p, 1) && p.length == 4);
    assert(!Pattern_Append(&p, 0) && !Pattern_Append(&p, 10));
    assert(Pattern_IsValid(&p));
    p.nodes[1] = 1; assert(!Pattern_IsValid(&p));
    p = make("123"); assert(!Pattern_IsValid(&p));
    p = make("123456789"); assert(Pattern_IsValid(&p) && !Pattern_Append(&p, 1));
    Pattern_Clear(&p); assert(!Pattern_IsValid(&p));
    assert(!Pattern_IsValid(0) && !Pattern_Equals(0, &q));
    for (i = 0; i < 9; i++) assert(!p.nodes[i]);
}
static void test_power_failure(unsigned fill) {
    uint16_t snapshot[sizeof(flash_image) / sizeof(flash_image[0])];
    Pattern a = make("1397"), b = make("147896"), loaded;
    FLASH_Status status;
    unsigned i;
    int fail;
    reset_flash();
    for (i = 0; i < fill; i++) assert(PatternStore_Save(&a) == FLASH_COMPLETE);
    memcpy(snapshot, flash_image, sizeof(snapshot));
    /* Every erase/program boundary, including the commit write and page reuse. */
    for (fail = 0; fail < 18; fail++) {
        memcpy(flash_image, snapshot, sizeof(snapshot));
        fail_after = fail; operations = 0;
        status = PatternStore_Save(&b);
        fail_after = -1;
        assert(PatternStore_Load(&loaded) == PATTERN_STORE_OK);
        assert(Pattern_Equals(&loaded, status == FLASH_COMPLETE ? &b : &a));
        /* A partial slot must not poison the next retry. */
        assert(PatternStore_Save(&b) == FLASH_COMPLETE);
        assert(PatternStore_Load(&loaded) == PATTERN_STORE_OK && Pattern_Equals(&b, &loaded));
        check_regions();
    }
}
int main(void) {
    Pattern a = make("1397"), b = make("987654321"), loaded;
    unsigned i;
    test_rules();
    reset_flash();
    assert(PatternStore_Load(&loaded) == PATTERN_STORE_EMPTY && !loaded.length);
    assert(erase_count == 0);
    assert(PatternStore_Save(&a) == FLASH_COMPLETE);
    assert(PatternStore_Load(&loaded) == PATTERN_STORE_OK && Pattern_Equals(&a, &loaded));
    assert(PatternStore_Save(&b) == FLASH_COMPLETE);
    assert(PatternStore_Load(&loaded) == PATTERN_STORE_OK && Pattern_Equals(&b, &loaded));
    /* Payload corruption is rejected; latest older valid commit remains usable. */
    flash_image[index_of(0x0800EC00U + 32 + 10)] ^= 1U;
    assert(PatternStore_Load(&loaded) == PATTERN_STORE_OK && Pattern_Equals(&a, &loaded));
    flash_image[index_of(0x0800EC00U + 10)] ^= 1U;
    assert(PatternStore_Load(&loaded) == PATTERN_STORE_INVALID && !loaded.length);
    assert(!erase_count);
    assert(PatternStore_Save(&b) == FLASH_COMPLETE);
    assert(PatternStore_Load(&loaded) == PATTERN_STORE_OK && Pattern_Equals(&b, &loaded));
    reset_flash();
    for (i = 0; i < 140; i++) {
        const Pattern *p = i & 1U ? &a : &b;
        assert(PatternStore_Save(p) == FLASH_COMPLETE);
        assert(PatternStore_Load(&loaded) == PATTERN_STORE_OK && Pattern_Equals(p, &loaded));
    }
    assert(erase_count == 4);
    loaded = make("123");
    assert(PatternStore_Save(&loaded) != FLASH_COMPLETE);
    check_regions();
    test_power_failure(1); test_power_failure(32); test_power_failure(64);
    puts("PASS: direct-node rules, CRC, 140 saves, both page rotations, all write-failure boundaries and region isolation");
    return 0;
}
"""

APP_TEST = FLASH_MODEL + r"""
#include "pattern_app.h"
#include "pattern_view.h"
#include "key.h"

static uint32_t clock_ms;
static unsigned opened, green, red, buzz, open_count;
static Pattern shown;
static char title[64], detail[64];
void PatternView_Message(const char *a, const char *b, const char *c, const char *d) {
    (void)c; (void)d;
    strcpy(title, a); strcpy(detail, b);
}
void PatternView_Input(const Pattern *p, const char *a, const char *b) {
    shown = *p; strcpy(title, a); strcpy(detail, b ? b : "");
}
void PWM_open(void) { opened = 1; open_count++; }
void PWM_close(void) { opened = 0; }
void LED_Alloff(void) { green = red = 0; }
void LED_Green_On(void) { green = 1; }
void LED_Red_On(void) { red = 1; }
void Buzzer_SetActive(uint8_t on) { buzz = on; }
static void key(uint8_t k) { clock_ms += 25; PatternApp_Update(clock_ms, k); }
static void wait_ms(uint32_t ms) { clock_ms += ms; PatternApp_Update(clock_ms, KEY_NONE); }
static void enter(const char *digits) {
    while (*digits) key((uint8_t)(*digits++ - '0'));
    key(15);
}
static void save_pattern(const char *digits) {
    key(2); assert(!strcmp(title, "SET 1/2"));
    enter(digits); assert(!strcmp(title, "SET 2/2"));
    enter(digits); assert(!strcmp(title, "PATTERN SAVED"));
    wait_ms(1200); assert(!strcmp(title, "PATTERN DEMO"));
}
int main(void) {
    unsigned i;
    Pattern expected = make("1397"), restored;
    reset_flash(); PatternApp_Init(0);
    assert(!opened && !buzz && !green && !red && !erase_count);
    key(1); assert(!strcmp(title, "NO PATTERN")); wait_ms(1200);
    key(2); enter("123"); assert(!strcmp(detail, "Min 4!"));
    assert(shown.length == 3);
    key(11); assert(!shown.length);
    key(1); key(3); key(1); assert(shown.length == 2 && !strcmp(detail, "Used!"));
    key(0); assert(!strcmp(title, "PATTERN DEMO"));
    key(2); enter("1397"); enter("1234");
    assert(!strcmp(title, "NOT SAME")); wait_ms(1200);
    assert(!strcmp(title, "SET 1/2")); key(0);
    save_pattern("1397");
    assert(PatternStore_Load(&restored) == PATTERN_STORE_OK && Pattern_Equals(&expected, &restored));
    key(1); key(10); key(12); key(13); key(14); assert(!shown.length); /* A-D ignored */
    enter("1397"); assert(opened && green && buzz && open_count == 1);
    wait_ms(49); assert(opened && buzz);
    wait_ms(1); assert(!buzz);
    wait_ms(2949); assert(opened);
    wait_ms(1); assert(!opened && !green && !strcmp(title, "PATTERN DEMO"));
    /* Return/clear do not reset the wrong-attempt counter. */
    key(1); enter("1234"); assert(!strcmp(title, "WRONG PATTERN") && red);
    wait_ms(1200); key(0); key(1); enter("1234"); wait_ms(1200);
    key(11); enter("1234"); assert(!strcmp(title, "LOCKED") && red && buzz);
    wait_ms(50); assert(!buzz);
    wait_ms(50); assert(buzz);
    for (i = 0; i < 16; i++) PatternApp_Update(clock_ms, (uint8_t)i);
    assert(!strcmp(title, "LOCKED") && !opened);
    wait_ms(1900); assert(!buzz && !strcmp(detail, "Wait: 28 seconds"));
    wait_ms(27999); assert(!strcmp(title, "LOCKED"));
    wait_ms(1); assert(!strcmp(title, "PATTERN DEMO") && !red && !buzz);
    /* Boot reloads persisted pattern without writing Flash. */
    PatternApp_Init(clock_ms); key(1); enter("1397"); assert(opened); wait_ms(3000);
    /* Failed setting cannot open or overwrite the last saved pattern. */
    key(2); enter("147896"); fail_after = 0; operations = 0; enter("147896");
    fail_after = -1; assert(!strcmp(title, "SAVE FAILED")); wait_ms(1200);
    key(1); enter("1397"); assert(opened); wait_ms(3000);
    /* Millisecond wrap must preserve the full three-second open period. */
    clock_ms = 0xFFFFFF00U; PatternApp_Init(clock_ms);
    key(1); enter("1397"); assert(opened); wait_ms(2999); assert(opened);
    wait_ms(1); assert(!opened);
    check_regions();
    puts("PASS: setup twice, cancel/clear, no-pattern, no auto-fill, correct # mapping, save failure, LEDs/buzzer, 3s close, 30s lockout and timer wrap");
    return 0;
}
"""


def run_target(temp, name, harness, drivers, extra_flags=()):
    source = temp / f"{name}.c"
    source.write_text(harness)
    exe = temp / f"{name}.exe"
    subprocess.run([
        shutil.which("gcc"), "-std=c99", "-Wall", "-Wextra", "-Werror", *extra_flags,
        f"-I{temp}", *(f"-I{ROOT / 'basic' / part}" for part in ("user", "hardware", "system")),
        str(source), *(str(ROOT / "basic" / p) for p in drivers), "-o", str(exe),
    ], check=True)
    subprocess.run([str(exe)], check=True, timeout=20)


def main():
    if not shutil.which("gcc"):
        raise SystemExit("Host GCC is required.")
    build_root = ROOT / "basic/build"
    build_root.mkdir(exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="host-pattern-", dir=build_root) as directory:
        temp = Path(directory)
        (temp / "stm32f10x.h").write_text(STUB.replace("#endif", FLASH_STUB + "\n#endif"))
        (temp / "stm32f10x_flash.h").write_text('#include "stm32f10x.h"\n')
        drivers = ["hardware/pattern.c", "hardware/pattern_store.c"]
        run_target(temp, "storage", STORAGE_TEST, drivers)
        run_target(temp, "app", APP_TEST, drivers + ["user/pattern_app.c"])


if __name__ == "__main__":
    main()
