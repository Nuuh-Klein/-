#ifndef LOCKING_INPUT_EVENT_H
#define LOCKING_INPUT_EVENT_H

#include "config.h"
#include "dial_sequence.h"

typedef enum { INPUT_METHOD_NONE, INPUT_METHOD_PASSWORD, INPUT_METHOD_DIAL } InputMethod;
typedef enum { INPUT_EVENT_NONE, INPUT_EVENT_SUBMITTED, INPUT_EVENT_CANCELLED,
               INPUT_EVENT_FAULT } InputEventKind;

typedef struct {
    InputEventKind kind;
    InputMethod method;
    uint8_t password[PASSWORD_LEN];
    DialSequence dial;
} InputEvent;

/* One pending snapshot, consumed by the caller in the same polling cycle.
 * Cancellation/fault replaces an unread submission, preventing stale unlocks. */
void InputEvent_Clear(InputEvent *event);
uint8_t InputEvent_SubmitPassword(InputEvent *event, const uint8_t *digits);
uint8_t InputEvent_SubmitDial(InputEvent *event, const DialSequence *dial);
void InputEvent_SetStatus(InputEvent *event, InputMethod method, InputEventKind kind);
uint8_t InputEvent_Take(InputEvent *pending, InputEvent *output);

#endif
