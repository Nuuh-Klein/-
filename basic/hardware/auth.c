#include "auth.h"
#include <string.h>

uint8_t Auth_Init(AuthContext *context, const CredentialConfig *config)
{
    CredentialConfig copy;
    if (context == 0) return 0;
    if (!Credentials_IsValid(config)) {
        memset(context, 0, sizeof(*context));
        return 0;
    }
    copy = *config;
    memset(context, 0, sizeof(*context));
    context->config = copy;
    context->configured = 1;
    return 1;
}

AuthDecision Auth_Evaluate(AuthContext *context, const InputEvent *event)
{
    AuthDecision decision = {AUTH_IGNORED, INPUT_METHOD_NONE, 0, 0};
    uint8_t normal, duress, i;
    if (event == 0 || event->kind != INPUT_EVENT_SUBMITTED) return decision;
    decision.method = event->method;
    if (context == 0 || !context->configured) {
        decision.result = AUTH_NOT_CONFIGURED;
        return decision;
    }
    if (context->audit_pending) {
        decision.result = AUTH_AUDIT_PENDING;
        return decision;
    }
    if (event->method == INPUT_METHOD_PASSWORD) {
        for (i = 0; i < PASSWORD_LEN; i++) {
            if (event->password[i] < 1U || event->password[i] > 9U) {
                decision.result = AUTH_INVALID_INPUT;
                return decision;
            }
        }
        normal = Credentials_PasswordEquals(event->password, context->config.password);
        duress = Credentials_PasswordEquals(event->password, context->config.duress_password);
    } else if (event->method == INPUT_METHOD_DIAL && DialSequence_IsValid(&event->dial)) {
        normal = DialSequence_Equals(&event->dial, &context->config.dial);
        duress = DialSequence_Equals(&event->dial, &context->config.duress_dial);
    } else {
        decision.result = AUTH_INVALID_INPUT;
        return decision;
    }
    decision.result = normal ? AUTH_NORMAL : duress ? AUTH_DURESS : AUTH_REJECTED;
    decision.allow_unlock = normal || duress;
    decision.duress = duress;
    context->audit.method = event->method;
    context->audit.success = decision.allow_unlock;
    context->audit.duress = decision.duress;
    context->audit_pending = 1;
    return decision;
}

uint8_t Auth_TakeAudit(AuthContext *context, AuthAuditEvent *event)
{
    if (context == 0 || event == 0 || !context->audit_pending) return 0;
    *event = context->audit;
    context->audit_pending = 0;
    context->audit.method = INPUT_METHOD_NONE;
    context->audit.success = context->audit.duress = 0;
    return 1;
}
