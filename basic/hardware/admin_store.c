#include "admin_store.h"
#include "flash.h"
#include "config.h"
#define WORDS 32U
#define BYTES 64U
#define SLOTS (USER_FLASH_PAGE_SIZE / BYTES)
#define MARKER 0xA8D1U
#if ADMIN_PAGE_COUNT != 2 || ADMIN_START_ADDRESS + 2048UL != LOG_START_ADDRESS
#error Administrator journal must occupy two pages before audit storage
#endif
typedef struct {
    uint8_t found, dirty, page;
    uint16_t used[2];
    uint32_t serial;
    AdminConfig config;
} ScanState;
void AdminStore_Clear(AdminConfig *c)
{
    uint8_t i;
    c->count = 0; c->pending_id = ADMIN_NO_ID;
    for (i = 0; i < ADMIN_MAX_COUNT; i++) c->ids[i] = ADMIN_NO_ID;
}
uint8_t AdminStore_Contains(const AdminConfig *c, uint16_t id)
{
    uint8_t i;
    if (!c || c->count > ADMIN_MAX_COUNT || id == ADMIN_NO_ID) return 0;
    for (i = 0; i < c->count; i++) if (c->ids[i] == id) return 1;
    return 0;
}
static uint8_t Valid(const AdminConfig *c)
{
    uint8_t i, j;
    if (!c || c->count > ADMIN_MAX_COUNT) return 0;
    for (i = 0; i < c->count; i++) {
        if (c->ids[i] == ADMIN_NO_ID) return 0;
        for (j = 0; j < i; j++) if (c->ids[j] == c->ids[i]) return 0;
    }
    return c->pending_id == ADMIN_NO_ID ||
           (c->count < ADMIN_MAX_COUNT && !AdminStore_Contains(c, c->pending_id));
}
static uint16_t RecordCRC(const uint16_t *w)
{
    uint16_t crc = 0xFFFFU;
    uint8_t i, b, bit;
    for (i = 1; i < 22; i++) for (b = 0; b < 2; b++) {
        crc ^= (uint16_t)(((w[i] >> (8U * b)) & 255U) << 8);
        for (bit = 0; bit < 8; bit++) crc = (uint16_t)((crc & 0x8000U) ? ((uint32_t)crc << 1) ^ 0x1021U : (uint32_t)crc << 1);
    }
    return crc;
}
static uint8_t Decode(const uint16_t *w, AdminConfig *c)
{
    uint8_t i;
    if (w[0] != MARKER || w[1] != 1 || w[4] > ADMIN_MAX_COUNT || w[22] != RecordCRC(w)) return 0;
    AdminStore_Clear(c); c->count = (uint8_t)w[4]; c->pending_id = w[5];
    for (i = 0; i < c->count; i++) c->ids[i] = w[6U + i];
    return Valid(c);
}
static void Scan(ScanState *s)
{
    uint8_t page, i, used;
    uint16_t slot, w[WORDS];
    uint32_t a, serial;
    AdminConfig c;
    s->found = s->dirty = s->page = 0; s->used[0] = s->used[1] = 0; s->serial = 0;
    AdminStore_Clear(&s->config);
    for (page = 0; page < 2; page++) for (slot = 0; slot < SLOTS; slot++) {
        a = ADMIN_START_ADDRESS + page * USER_FLASH_PAGE_SIZE + slot * BYTES;
        used = 0;
        for (i = 0; i < WORDS; i++) { w[i] = FLASH_ReadHalfWord(a + i * 2U); if (w[i] != 0xFFFFU) used = 1; }
        if (!used) continue;
        s->dirty = 1; s->used[page] = slot + 1U;
        if (!Decode(w, &c)) continue;
        serial = (uint32_t)w[2] | ((uint32_t)w[3] << 16);
        if (!s->found || (serial != s->serial && (uint32_t)(serial - s->serial) < 0x80000000UL)) {
            s->found = 1; s->page = page; s->serial = serial; s->config = c;
        }
    }
}
AdminStoreResult AdminStore_Load(AdminConfig *c)
{
    ScanState s;
    if (!c) return ADMIN_STORE_INVALID;
    Scan(&s); *c = s.config;
    return s.found ? ADMIN_STORE_OK : s.dirty ? ADMIN_STORE_INVALID : ADMIN_STORE_EMPTY;
}
static uint8_t Equal(const AdminConfig *a, const AdminConfig *b)
{
    uint8_t i;
    if (a->count != b->count || a->pending_id != b->pending_id) return 0;
    for (i = 0; i < a->count; i++) if (a->ids[i] != b->ids[i]) return 0;
    return 1;
}
AdminStoreResult AdminStore_Save(const AdminConfig *c)
{
    ScanState s;
    AdminConfig check;
    uint8_t page, i;
    uint16_t w[WORDS];
    uint32_t a, serial;
    if (!Valid(c)) return ADMIN_STORE_INVALID;
    Scan(&s);
    if (!s.found && s.dirty) return ADMIN_STORE_INVALID;
    page = s.found ? s.page : 0;
    if (s.used[page] == SLOTS) {
        page ^= 1U;
        if (FLASH_EraseAPage(ADMIN_START_ADDRESS + page * USER_FLASH_PAGE_SIZE) != FLASH_COMPLETE) return ADMIN_STORE_IO_ERROR;
        s.used[page] = 0;
    }
    a = ADMIN_START_ADDRESS + page * USER_FLASH_PAGE_SIZE + s.used[page] * BYTES;
    serial = s.found ? s.serial + 1U : 1U;
    for (i = 0; i < WORDS; i++) w[i] = 0xFFFFU;
    w[0] = MARKER; w[1] = 1; w[2] = (uint16_t)serial; w[3] = (uint16_t)(serial >> 16);
    w[4] = c->count; w[5] = c->pending_id;
    for (i = 0; i < c->count; i++) w[6U + i] = c->ids[i];
    w[22] = RecordCRC(w);
    for (i = 1; i <= 22; i++) if (w[i] != 0xFFFFU && FLASH_WriteHalfWord(a + i * 2U, w[i]) != FLASH_COMPLETE) return ADMIN_STORE_IO_ERROR;
    for (i = 1; i < WORDS; i++) if (FLASH_ReadHalfWord(a + i * 2U) != w[i]) return ADMIN_STORE_IO_ERROR;
    (void)FLASH_WriteHalfWord(a, MARKER);
    return AdminStore_Load(&check) == ADMIN_STORE_OK && Equal(c, &check) ? ADMIN_STORE_OK : ADMIN_STORE_IO_ERROR;
}
