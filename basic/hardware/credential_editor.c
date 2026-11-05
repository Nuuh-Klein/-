#include "credential_editor.h"
#include <string.h>
void CredentialEditor_Cancel(CredentialEditor *e) { memset(e, 0, sizeof(*e)); }
uint8_t CredentialEditor_Begin(CredentialEditor *e, uint8_t duress, InputMethod method)
{
    CredentialEditor_Cancel(e);
    if (duress > 1 || (method != INPUT_METHOD_PASSWORD && method != INPUT_METHOD_DIAL) ||
        CredentialStore_Load(&e->base) != CREDENTIAL_STORE_OK) return 0;
    e->duress = duress; e->method = method; e->active = 1;
    return 1;
}
static uint8_t Equal(const CredentialConfig *a, const CredentialConfig *b)
{
    return Credentials_PasswordEquals(a->password, b->password) &&
           Credentials_PasswordEquals(a->duress_password, b->duress_password) &&
           DialSequence_Equals(&a->dial, &b->dial) && DialSequence_Equals(&a->duress_dial, &b->duress_dial);
}
EditResult CredentialEditor_Submit(CredentialEditor *e, const InputEvent *event)
{
    CredentialConfig next, check;
    uint8_t i, same;
    if (!e->active || !event || event->kind != INPUT_EVENT_SUBMITTED || event->method != e->method) return EDIT_INVALID;
    if (e->method == INPUT_METHOD_PASSWORD) {
        for (i = 0; i < PASSWORD_LEN; i++) if (event->password[i] < 1 || event->password[i] > 9) return EDIT_INVALID;
    } else if (!DialSequence_IsValid(&event->dial)) return EDIT_INVALID;
    if (!e->second) { e->first = *event; e->second = 1; return EDIT_FIRST_ACCEPTED; }
    same = e->method == INPUT_METHOD_PASSWORD ? Credentials_PasswordEquals(e->first.password, event->password) :
           DialSequence_Equals(&e->first.dial, &event->dial);
    if (!same) { CredentialEditor_Cancel(e); return EDIT_MISMATCH; }
    next = e->base;
    if (e->method == INPUT_METHOD_PASSWORD) memcpy(e->duress ? next.duress_password : next.password, event->password, PASSWORD_LEN);
    else if (e->duress) next.duress_dial = event->dial;
    else next.dial = event->dial;
    if (!Credentials_IsValid(&next)) { CredentialEditor_Cancel(e); return EDIT_CONFLICT; }
    (void)CredentialStore_Save(&next);
    /* Readback is authoritative, including controller errors after final commit. */
    same = CredentialStore_Load(&check) == CREDENTIAL_STORE_OK && Equal(&next, &check);
    CredentialEditor_Cancel(e);
    return same ? EDIT_SAVED : EDIT_SAVE_FAILED;
}
