#include "credential_store.h"
#include "flash.h"

#define RECORD_WORDS 32U
#define RECORD_BYTES 64U
#define RECORDS_PER_PAGE (USER_FLASH_PAGE_SIZE / RECORD_BYTES)
#define RECORD_MARKER 0xA6D2U
#define RECORD_VERSION 1U
#define RECORD_FLAGS 15U

#if PASSWORD_LEN != 6 || DIAL_SEGMENTS != 3
#error Credential format version 1 requires six digits and three dial segments
#endif

#if CREDENTIAL_PAGE_COUNT != 2 || (CREDENTIAL_START_ADDRESS % USER_FLASH_PAGE_SIZE) != 0
#error Credential journal requires two aligned pages
#endif
#if CREDENTIAL_START_ADDRESS + CREDENTIAL_PAGE_COUNT * USER_FLASH_PAGE_SIZE > PATTERN_START_ADDRESS
#error Credential journal overlaps existing pattern data
#endif

typedef struct {
    uint8_t found, dirty, page;
    uint16_t used[CREDENTIAL_PAGE_COUNT];
    uint32_t serial;
    CredentialConfig config;
} CredentialScan;

static uint16_t RecordCRC(const uint16_t *words)
{
    uint16_t crc = 0xFFFFU;
    uint8_t i, byte, bit;
    for (i = 1; i < 29U; i++) {
        for (byte = 0; byte < 2U; byte++) {
            crc ^= (uint16_t)(((words[i] >> (byte * 8U)) & 0xFFU) << 8);
            for (bit = 0; bit < 8U; bit++)
                crc = (uint16_t)((crc & 0x8000U) ? ((uint32_t)crc << 1) ^ 0x1021U : (uint32_t)crc << 1);
        }
    }
    return crc;
}

static void ClearConfig(CredentialConfig *config)
{
    uint8_t i;
    for (i = 0; i < PASSWORD_LEN; i++) config->password[i] = config->duress_password[i] = 0;
    DialSequence_Clear(&config->dial);
    DialSequence_Clear(&config->duress_dial);
}

static uint8_t Decode(const uint16_t *words, CredentialConfig *config)
{
    uint8_t i, side;
    DialSequence *dial;
    uint16_t offset;
    if (words[0] != RECORD_MARKER || words[1] != RECORD_VERSION ||
        words[4] != RECORD_FLAGS || words[29] != RecordCRC(words)) return 0;
    ClearConfig(config);
    for (i = 0; i < PASSWORD_LEN; i++) {
        if (words[5U + i] > 9U || words[11U + i] > 9U) return 0;
        config->password[i] = (uint8_t)words[5U + i];
        config->duress_password[i] = (uint8_t)words[11U + i];
    }
    for (side = 0; side < 2U; side++) {
        dial = side ? &config->duress_dial : &config->dial;
        dial->length = DIAL_SEGMENTS;
        for (i = 0; i < DIAL_SEGMENTS; i++) {
            offset = (uint16_t)(17U + side * 6U + i * 2U);
            if ((words[offset] != 1U && words[offset] != 2U) ||
                words[offset + 1U] > DIAL_MAX_STEPS) return 0;
            dial->direction[i] = words[offset] == 1U ? 1 : -1;
            dial->steps[i] = (uint8_t)words[offset + 1U];
        }
    }
    return Credentials_IsValid(config);
}

static void Scan(CredentialScan *scan)
{
    uint8_t page, i, occupied;
    uint16_t slot, words[RECORD_WORDS];
    uint32_t address, serial;
    CredentialConfig candidate;
    scan->found = scan->dirty = scan->page = 0;
    scan->used[0] = scan->used[1] = 0;
    scan->serial = 0;
    ClearConfig(&scan->config);
    for (page = 0; page < CREDENTIAL_PAGE_COUNT; page++) {
        for (slot = 0; slot < RECORDS_PER_PAGE; slot++) {
            address = CREDENTIAL_START_ADDRESS + page * USER_FLASH_PAGE_SIZE + slot * RECORD_BYTES;
            occupied = 0;
            for (i = 0; i < RECORD_WORDS; i++) {
                words[i] = FLASH_ReadHalfWord(address + i * 2U);
                if (words[i] != 0xFFFFU) occupied = 1;
            }
            if (!occupied) continue;
            scan->dirty = 1;
            scan->used[page] = slot + 1U;
            if (!Decode(words, &candidate)) continue;
            serial = (uint32_t)words[2] | ((uint32_t)words[3] << 16);
            if (!scan->found || (serial != scan->serial &&
                (uint32_t)(serial - scan->serial) < 0x80000000UL)) {
                scan->found = 1;
                scan->serial = serial;
                scan->page = page;
                scan->config = candidate;
            }
        }
    }
}

CredentialStoreResult CredentialStore_Load(CredentialConfig *config)
{
    CredentialScan scan;
    if (config == 0) return CREDENTIAL_STORE_INVALID;
    Scan(&scan);
    *config = scan.config;
    if (scan.found) return CREDENTIAL_STORE_OK;
    return scan.dirty ? CREDENTIAL_STORE_INVALID : CREDENTIAL_STORE_EMPTY;
}

CredentialStoreResult CredentialStore_Save(const CredentialConfig *config)
{
    CredentialScan scan;
    CredentialConfig check;
    uint16_t words[RECORD_WORDS];
    uint8_t i, page, side;
    uint16_t offset;
    uint32_t address, serial;
    const DialSequence *dial;
    if (!Credentials_IsValid(config)) return CREDENTIAL_STORE_INVALID;
    Scan(&scan);
    page = scan.found ? scan.page : 0;
    if (scan.used[page] == RECORDS_PER_PAGE) {
        page ^= 1U;
        if (FLASH_EraseAPage(CREDENTIAL_START_ADDRESS + page * USER_FLASH_PAGE_SIZE) != FLASH_COMPLETE)
            return CREDENTIAL_STORE_IO_ERROR;
        scan.used[page] = 0;
    }
    address = CREDENTIAL_START_ADDRESS + page * USER_FLASH_PAGE_SIZE + scan.used[page] * RECORD_BYTES;
    serial = scan.found ? scan.serial + 1U : 1U;
    for (i = 0; i < RECORD_WORDS; i++) words[i] = 0xFFFFU;
    words[0] = RECORD_MARKER;
    words[1] = RECORD_VERSION;
    words[2] = (uint16_t)serial;
    words[3] = (uint16_t)(serial >> 16);
    words[4] = RECORD_FLAGS;
    for (i = 0; i < PASSWORD_LEN; i++) {
        words[5U + i] = config->password[i];
        words[11U + i] = config->duress_password[i];
    }
    for (side = 0; side < 2U; side++) {
        dial = side ? &config->duress_dial : &config->dial;
        for (i = 0; i < DIAL_SEGMENTS; i++) {
            offset = (uint16_t)(17U + side * 6U + i * 2U);
            words[offset] = dial->direction[i] > 0 ? 1U : 2U;
            words[offset + 1U] = dial->steps[i];
        }
    }
    words[29] = RecordCRC(words);
    for (i = 1; i <= 29U; i++) {
        if (words[i] != 0xFFFFU && FLASH_WriteHalfWord(address + i * 2U, words[i]) != FLASH_COMPLETE)
            return CREDENTIAL_STORE_IO_ERROR;
    }
    for (i = 1; i < RECORD_WORDS; i++) {
        if (FLASH_ReadHalfWord(address + i * 2U) != words[i]) return CREDENTIAL_STORE_IO_ERROR;
    }
    if (FLASH_WriteHalfWord(address, RECORD_MARKER) != FLASH_COMPLETE) return CREDENTIAL_STORE_IO_ERROR;
    if (CredentialStore_Load(&check) != CREDENTIAL_STORE_OK ||
        !Credentials_PasswordEquals(config->password, check.password) ||
        !Credentials_PasswordEquals(config->duress_password, check.duress_password) ||
        !DialSequence_Equals(&config->dial, &check.dial) ||
        !DialSequence_Equals(&config->duress_dial, &check.duress_dial)) return CREDENTIAL_STORE_IO_ERROR;
    return CREDENTIAL_STORE_OK;
}
