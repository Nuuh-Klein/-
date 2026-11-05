#ifndef LOCKING_CREDENTIAL_EDITOR_H
#define LOCKING_CREDENTIAL_EDITOR_H
#include "credential_store.h"
#include "input_event.h"
typedef struct {
    CredentialConfig base;
    InputEvent first;
    InputMethod method;
    uint8_t duress, second, active;
} CredentialEditor;
typedef enum { EDIT_INVALID, EDIT_FIRST_ACCEPTED, EDIT_MISMATCH, EDIT_CONFLICT,
               EDIT_SAVE_FAILED, EDIT_SAVED } EditResult;
uint8_t CredentialEditor_Begin(CredentialEditor *editor, uint8_t duress, InputMethod method);
EditResult CredentialEditor_Submit(CredentialEditor *editor, const InputEvent *event);
void CredentialEditor_Cancel(CredentialEditor *editor);
#endif
