#ifndef LOCKING_DURESS_DEMO_H
#define LOCKING_DURESS_DEMO_H

#include <stdint.h>
#include "auth.h"

/* Separate integration harness. Audit adapter persists events; no lock outputs. */
void DuressDemo_Start(void);
void DuressDemo_Update(uint8_t key);
uint8_t DuressDemo_IsActive(void);
void DuressDemo_Reset(void);
void DuressDemo_ServiceAudit(void);
/* Shared test authentication path; lazily loads saved credentials, queues audit. */
AuthDecision DuressDemo_EvaluateInput(const InputEvent *event);

#endif
