#include "log.h"
#include "config.h"
#include "flash.h"

#define LOG_WORDS 8U
#define LOG_MARKER 0xA704U
#define LOG_VERSION 1U
#define SLOTS_PER_PAGE (LOG_PAGE_SIZE / LOG_RECORD_SIZE)

#if LOG_PAGE_COUNT != 2 || LOG_RECORD_SIZE != 16 || LOG_PAGE_SIZE != USER_FLASH_PAGE_SIZE
#error Log format requires two pages and 16-byte records
#endif
#if (LOG_START_ADDRESS % USER_FLASH_PAGE_SIZE) != 0 || LOG_START_ADDRESS + LOG_PAGE_SIZE * LOG_PAGE_COUNT > CREDENTIAL_START_ADDRESS
#error New audit log must be aligned and separate from credentials
#endif

typedef struct { uint32_t serial; uint16_t slot; } LogIndex;
static LogIndex index_cache[LOG_MAX_RECORDS];
static uint16_t count, invalid_count, used[LOG_PAGE_COUNT];
static uint32_t newest_serial;
static uint8_t active_page, ready;

static uint16_t LogCRC(const uint16_t *words)
{
    uint16_t crc = 0xFFFFU;
    uint8_t i, byte, bit;
    for (i = 1; i <= 5U; i++) {
        for (byte = 0; byte < 2U; byte++) {
            crc ^= (uint16_t)(((words[i] >> (byte * 8U)) & 0xFFU) << 8);
            for (bit = 0; bit < 8U; bit++)
                crc = (uint16_t)((crc & 0x8000U) ? ((uint32_t)crc << 1) ^ 0x1021U : (uint32_t)crc << 1);
        }
    }
    return crc;
}

static uint8_t ValidFields(uint16_t method, uint16_t result, uint8_t duress)
{
    return ((method >= LOG_METHOD_PASSWORD && method <= LOG_METHOD_FINGER) || method == LOG_METHOD_TEST) &&
           result <= LOG_RESULT_PASS && duress <= 1U &&
           (!duress || (result == LOG_RESULT_PASS &&
             (method == LOG_METHOD_PASSWORD || method == LOG_METHOD_DIAL)));
}

static uint8_t ReadAt(uint16_t slot, Log_Record_t *record, uint8_t *occupied)
{
    uint16_t words[LOG_WORDS];
    uint8_t i;
    uint32_t address = LOG_START_ADDRESS + (uint32_t)slot * LOG_RECORD_SIZE;
    *occupied = 0;
    for (i = 0; i < LOG_WORDS; i++) {
        words[i] = FLASH_ReadHalfWord(address + i * 2U);
        if (words[i] != 0xFFFFU) *occupied = 1;
    }
    if (words[0] != LOG_MARKER || words[1] != LOG_VERSION || words[5] > 3U ||
        words[6] != LogCRC(words) || words[7] != 0xFFFFU ||
        !ValidFields(words[4], words[5] & 1U, (uint8_t)(words[5] >> 1))) return 0;
    record->serial = (uint32_t)words[2] | ((uint32_t)words[3] << 16);
    record->method = words[4];
    record->result = words[5] & 1U;
    record->duress = (uint8_t)(words[5] >> 1);
    return 1;
}

static void Scan(void)
{
    uint16_t slot, i, j;
    uint8_t occupied, found = 0, page;
    Log_Record_t record;
    LogIndex item;
    ready = 0;
    count = invalid_count = used[0] = used[1] = 0;
    newest_serial = 0;
    active_page = 0;
    for (slot = 0; slot < LOG_MAX_RECORDS; slot++) {
        if (ReadAt(slot, &record, &occupied)) {
            index_cache[count].serial = record.serial;
            index_cache[count++].slot = slot;
            if (!found || (record.serial != newest_serial &&
                (uint32_t)(record.serial - newest_serial) < 0x80000000UL)) {
                found = 1;
                newest_serial = record.serial;
                active_page = (uint8_t)(slot / SLOTS_PER_PAGE);
            }
        } else if (occupied) invalid_count++;
        if (occupied) {
            page = (uint8_t)(slot / SLOTS_PER_PAGE);
            used[page] = (uint16_t)(slot % SLOTS_PER_PAGE + 1U);
        }
    }
    /* Newest first, including natural uint32 serial wrap. No wall-clock claims. */
    for (i = 1; i < count; i++) {
        item = index_cache[i];
        j = i;
        while (j != 0U && (uint32_t)(newest_serial - item.serial) <
               (uint32_t)(newest_serial - index_cache[j - 1U].serial)) {
            index_cache[j] = index_cache[j - 1U];
            j--;
        }
        index_cache[j] = item;
    }
    ready = 1;
}

uint8_t Log_Init(void) { Scan(); return ready; }
uint16_t Log_GetCount(void) { return ready ? count : 0; }
uint16_t Log_GetInvalidCount(void) { return ready ? invalid_count : 0; }

uint8_t Log_Read(uint16_t recent_index, Log_Record_t *record)
{
    uint8_t occupied;
    if (record == 0) return 0;
    record->serial = record->method = record->result = record->duress = 0;
    if (!ready || recent_index >= count) return 0;
    return ReadAt(index_cache[recent_index].slot, record, &occupied);
}

uint8_t Log_WriteEvent(uint16_t method, uint16_t result, uint8_t duress)
{
    uint16_t words[LOG_WORDS], slot;
    uint8_t i, page, occupied, verified;
    uint32_t address, serial;
    Log_Record_t check;
    FLASH_Status status = FLASH_COMPLETE;
    if (!ready || !ValidFields(method, result, duress)) return 0;
    page = active_page;
    serial = count ? newest_serial + 1U : 1U;
    if (used[page] == SLOTS_PER_PAGE) {
        page ^= 1U;
        if (used[page] != 0U) {
            status = FLASH_EraseAPage(LOG_START_ADDRESS + page * LOG_PAGE_SIZE);
            Scan(); /* Recover even if erase was interrupted or reported an error. */
            if (status != FLASH_COMPLETE) return 0;
        }
    }
    slot = (uint16_t)(page * SLOTS_PER_PAGE + used[page]);
    address = LOG_START_ADDRESS + (uint32_t)slot * LOG_RECORD_SIZE;
    for (i = 0; i < LOG_WORDS; i++) words[i] = 0xFFFFU;
    words[0] = LOG_MARKER;
    words[1] = LOG_VERSION;
    words[2] = (uint16_t)serial;
    words[3] = (uint16_t)(serial >> 16);
    words[4] = method;
    words[5] = (uint16_t)(result | (uint16_t)duress << 1);
    words[6] = LogCRC(words);
    for (i = 1; i <= 6U; i++) {
        if (words[i] == 0xFFFFU) continue;
        status = FLASH_WriteHalfWord(address + i * 2U, words[i]);
        if (status != FLASH_COMPLETE) break;
    }
    if (status == FLASH_COMPLETE) {
        for (i = 1; i < LOG_WORDS; i++) {
            if (FLASH_ReadHalfWord(address + i * 2U) != words[i]) { status = FLASH_ERROR_PG; break; }
        }
    }
    if (status == FLASH_COMPLETE) status = FLASH_WriteHalfWord(address, LOG_MARKER);
    /* A controller may report an error after programming the commit word.
     * Return success only when the actual committed record can be verified. */
    verified = ReadAt(slot, &check, &occupied) && check.serial == serial &&
               check.method == method && check.result == result && check.duress == duress;
    Scan();
    return verified;
}

uint8_t Log_Write(uint16_t method, uint16_t result)
{
    if (method == LOG_METHOD_DURESS) return Log_WriteEvent(LOG_METHOD_PASSWORD, result, 1);
    return Log_WriteEvent(method, result, 0);
}
