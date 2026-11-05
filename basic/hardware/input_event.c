#include "input_event.h"

void InputEvent_Clear(InputEvent *event)
{
    uint8_t i;
    if (event == 0) return;
    event->kind = INPUT_EVENT_NONE;
    event->method = INPUT_METHOD_NONE;
    for (i = 0; i < PASSWORD_LEN; i++) event->password[i] = 0;
    DialSequence_Clear(&event->dial);
}

uint8_t InputEvent_SubmitPassword(InputEvent *event, const uint8_t *digits)
{
    uint8_t i;
    uint8_t copy[PASSWORD_LEN];
    if (event == 0) return 0;
    if (digits == 0) { InputEvent_Clear(event); return 0; }
    for (i = 0; i < PASSWORD_LEN; i++) {
        if (digits[i] < 1U || digits[i] > 9U) { InputEvent_Clear(event); return 0; }
        copy[i] = digits[i];
    }
    InputEvent_Clear(event);
    for (i = 0; i < PASSWORD_LEN; i++) event->password[i] = copy[i];
    event->method = INPUT_METHOD_PASSWORD;
    event->kind = INPUT_EVENT_SUBMITTED;
    return 1;
}

uint8_t InputEvent_SubmitDial(InputEvent *event, const DialSequence *dial)
{
    DialSequence copy;
    if (event == 0) return 0;
    if (!DialSequence_IsValid(dial)) { InputEvent_Clear(event); return 0; }
    copy = *dial;
    InputEvent_Clear(event);
    event->dial = copy;
    event->dial.position = 0; /* Pointer position is not part of a credential. */
    event->method = INPUT_METHOD_DIAL;
    event->kind = INPUT_EVENT_SUBMITTED;
    return 1;
}

void InputEvent_SetStatus(InputEvent *event, InputMethod method, InputEventKind kind)
{
    InputEvent_Clear(event);
    if (event == 0 || (method != INPUT_METHOD_PASSWORD && method != INPUT_METHOD_DIAL) ||
        (kind != INPUT_EVENT_CANCELLED && kind != INPUT_EVENT_FAULT)) return;
    event->method = method;
    event->kind = kind;
}

uint8_t InputEvent_Take(InputEvent *pending, InputEvent *output)
{
    if (pending == 0 || output == 0 || pending == output) return 0;
    InputEvent_Clear(output);
    if (pending->kind == INPUT_EVENT_NONE) return 0;
    *output = *pending;
    InputEvent_Clear(pending);
    return 1;
}
