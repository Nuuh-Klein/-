#ifndef LOCKING_ADMIN_STORE_H
#define LOCKING_ADMIN_STORE_H
#include <stdint.h>
#define ADMIN_MAX_COUNT 16U
#define ADMIN_NO_ID 0xFFFFU
typedef struct {
    uint8_t count;
    uint16_t ids[ADMIN_MAX_COUNT];
    uint16_t pending_id; /* Reserved enrollment, never grants access. */
} AdminConfig;
typedef enum { ADMIN_STORE_OK, ADMIN_STORE_EMPTY, ADMIN_STORE_INVALID,
               ADMIN_STORE_IO_ERROR } AdminStoreResult;
void AdminStore_Clear(AdminConfig *config);
uint8_t AdminStore_Contains(const AdminConfig *config, uint16_t id);
AdminStoreResult AdminStore_Load(AdminConfig *config);
AdminStoreResult AdminStore_Save(const AdminConfig *config);
#endif
