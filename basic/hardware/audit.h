#ifndef LOCKING_AUDIT_H
#define LOCKING_AUDIT_H

#include "auth.h"

#define AUDIT_QUEUE_CAPACITY 4U

/* Boot only. Scans log read-only and resets the RAM queue. */
uint8_t Audit_Init(void);
/* Takes Auth's one-shot event only if queue space is available. */
uint8_t Audit_Capture(AuthContext *context, AuthAuditEvent *diagnostic_copy);
/* Attempt one append. Failed writes keep the same event pending for retry. */
uint8_t Audit_Process(void);
/* Explicit non-password events, e.g. administrator fingerprint verification. */
uint8_t Audit_Enqueue(uint16_t method, uint8_t success, uint8_t duress);
uint8_t Audit_GetPendingCount(void);
uint8_t Audit_HasWriteError(void);

#endif
