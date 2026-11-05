#ifndef LOCKING_ADMIN_FINGERPRINT_H
#define LOCKING_ADMIN_FINGERPRINT_H
#include "admin_store.h"
#include "AS608.h"
/* UI owns stage gating; successful reservation is not authorization. */
uint8_t AdminFinger_Prepare(void);
uint8_t AdminFinger_Begin(AdminConfig *config, uint8_t *recover);
uint8_t AdminFinger_First(void);
uint8_t AdminFinger_Second(void);
uint8_t AdminFinger_Commit(AdminConfig *config);
uint8_t AdminFinger_Recover(AdminConfig *config);
#endif
