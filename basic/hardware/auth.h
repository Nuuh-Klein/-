#ifndef LOCKING_AUTH_H
#define LOCKING_AUTH_H

#include "credentials.h"
#include "input_event.h"

typedef enum {
    AUTH_IGNORED, AUTH_NORMAL, AUTH_DURESS, AUTH_REJECTED,
    AUTH_NOT_CONFIGURED, AUTH_INVALID_INPUT, AUTH_AUDIT_PENDING
} AuthResult;

typedef struct {
    AuthResult result;
    InputMethod method;
    uint8_t allow_unlock;
    uint8_t duress;
} AuthDecision;

/* Contains no password data. The audit adapter queues/persists this event. */
typedef struct {
    InputMethod method;
    uint8_t success;
    uint8_t duress;
} AuthAuditEvent;

typedef struct {
    CredentialConfig config;
    uint8_t configured;
    uint8_t audit_pending;
    AuthAuditEvent audit;
} AuthContext;

/* Pure logic: no OLED, LED, buzzer, servo, Flash or log calls. */
uint8_t Auth_Init(AuthContext *context, const CredentialConfig *config);
AuthDecision Auth_Evaluate(AuthContext *context, const InputEvent *event);
/* Consume once before the next evaluation; pending events are never overwritten. */
uint8_t Auth_TakeAudit(AuthContext *context, AuthAuditEvent *event);

#endif
