#include "pattern_store.h"
#include "config.h"
#include "flash.h"

/* Two append-only pages. The previous page survives until the new record commits.
 * Halfwords: marker, version, serial low/high, length, nodes[9], CRC16, reserved.
 * CRC covers version through nodes. Marker is programmed last. */
#define RECORD_WORDS 16U
#define RECORD_BYTES (RECORD_WORDS * 2U)
#define RECORDS_PER_PAGE (USER_FLASH_PAGE_SIZE / RECORD_BYTES)
#define RECORD_MARKER 0xA761U
#define RECORD_VERSION 1U

typedef struct {
    uint8_t found;
    uint8_t dirty;
    uint8_t page;
    uint16_t used[2];
    uint32_t serial;
    Pattern pattern;
} StoreScan;

static uint16_t RecordCRC(const uint16_t *words)
{
    uint16_t crc = 0xFFFFU;
    uint8_t i, byte, bit;
    for (i = 1; i < 14U; i++) {
        for (byte = 0; byte < 2U; byte++) {
            crc ^= (uint16_t)(((words[i] >> (byte * 8U)) & 0xFFU) << 8);
            for (bit = 0; bit < 8U; bit++) {
                crc = (uint16_t)((crc & 0x8000U) ? ((uint32_t)crc << 1) ^ 0x1021U : (uint32_t)crc << 1);
            }
        }
    }
    return crc;
}

static void Scan(StoreScan *scan)
{
    uint8_t page, i, occupied;
    uint16_t slot, words[RECORD_WORDS];
    uint32_t address, serial;
    Pattern candidate;
    scan->found = scan->dirty = scan->page = 0;
    scan->used[0] = scan->used[1] = 0;
    scan->serial = 0;
    Pattern_Clear(&scan->pattern);
    for (page = 0; page < 2U; page++) {
        for (slot = 0; slot < RECORDS_PER_PAGE; slot++) {
            address = PATTERN_START_ADDRESS + page * USER_FLASH_PAGE_SIZE + slot * RECORD_BYTES;
            occupied = 0;
            for (i = 0; i < RECORD_WORDS; i++) {
                words[i] = FLASH_ReadHalfWord(address + i * 2U);
                if (words[i] != 0xFFFFU) occupied = 1;
            }
            if (!occupied) continue;
            scan->dirty = 1;
            scan->used[page] = slot + 1U;
            if (words[0] != RECORD_MARKER || words[1] != RECORD_VERSION ||
                words[4] > PATTERN_MAX_NODES || words[14] != RecordCRC(words)) continue;
            Pattern_Clear(&candidate);
            candidate.length = (uint8_t)words[4];
            for (i = 0; i < candidate.length; i++) {
                if (words[i + 5U] > 9U) break;
                candidate.nodes[i] = (uint8_t)words[i + 5U];
            }
            if (i != candidate.length || !Pattern_IsValid(&candidate)) continue;
            serial = (uint32_t)words[2] | ((uint32_t)words[3] << 16);
            if (!scan->found || (serial != scan->serial &&
                (uint32_t)(serial - scan->serial) < 0x80000000UL)) {
                scan->found = 1;
                scan->page = page;
                scan->serial = serial;
                scan->pattern = candidate;
            }
        }
    }
}

PatternStoreResult PatternStore_Load(Pattern *pattern)
{
    StoreScan scan;
    if (pattern == 0) return PATTERN_STORE_INVALID;
    Scan(&scan);
    *pattern = scan.pattern;
    if (scan.found) return PATTERN_STORE_OK;
    return scan.dirty ? PATTERN_STORE_INVALID : PATTERN_STORE_EMPTY;
}

FLASH_Status PatternStore_Save(const Pattern *pattern)
{
    StoreScan scan;
    FLASH_Status status;
    uint16_t words[RECORD_WORDS];
    uint8_t i, page;
    uint32_t address, serial;
    Pattern check;
    if (!Pattern_IsValid(pattern)) return FLASH_ERROR_PG;
    Scan(&scan);
    page = scan.found ? scan.page : 0;
    if (scan.used[page] >= RECORDS_PER_PAGE) {
        page ^= 1U;
        status = FLASH_EraseAPage(PATTERN_START_ADDRESS + page * USER_FLASH_PAGE_SIZE);
        if (status != FLASH_COMPLETE) return status;
        scan.used[page] = 0;
    }
    address = PATTERN_START_ADDRESS + page * USER_FLASH_PAGE_SIZE + scan.used[page] * RECORD_BYTES;
    serial = scan.found ? scan.serial + 1U : 1U;
    for (i = 0; i < RECORD_WORDS; i++) words[i] = 0xFFFFU;
    words[0] = RECORD_MARKER;
    words[1] = RECORD_VERSION;
    words[2] = (uint16_t)serial;
    words[3] = (uint16_t)(serial >> 16);
    words[4] = pattern->length;
    for (i = 0; i < pattern->length; i++) words[i + 5U] = pattern->nodes[i];
    words[14] = RecordCRC(words);
    for (i = 1; i < 15U; i++) {
        if (words[i] == 0xFFFFU) continue;
        status = FLASH_WriteHalfWord(address + i * 2U, words[i]);
        if (status != FLASH_COMPLETE) return status;
    }
    /* Read back all payload before exposing it as committed. */
    for (i = 1; i < RECORD_WORDS; i++) {
        if (FLASH_ReadHalfWord(address + i * 2U) != words[i]) return FLASH_ERROR_PG;
    }
    status = FLASH_WriteHalfWord(address, RECORD_MARKER);
    if (status != FLASH_COMPLETE) return status;
    if (PatternStore_Load(&check) != PATTERN_STORE_OK || !Pattern_Equals(&check, pattern))
        return FLASH_ERROR_PG;
    return FLASH_COMPLETE;
}
