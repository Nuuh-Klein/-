#include "interaction_demo.h"
#include "password_demo.h"
#include "dial_sequence.h"
#include "dial_view.h"
#include "pattern_view.h"
#include "encoder.h"
#include "key.h"
#include "duress_demo.h"
#include "audit.h"
#include "log_demo.h"

typedef enum { DEMO_HOME, DEMO_PASSWORD, DEMO_DIAL, DEMO_DURESS, DEMO_LOG } DemoMode;
static DemoMode mode;
static DialSequence sequence;
static uint8_t finished;
static const char *dial_status;
static InputEvent pending_event;

static void ShowHome(void)
{
    mode = DEMO_HOME;
    PatternView_Message("INPUT TEST", "1 Dial test", "2 Password test", "3Duress 4Logs");
}

static void ClearDial(void)
{
    InputEvent_Clear(&pending_event);
    DialSequence_Clear(&sequence);
    Encoder_ClearCount();
    finished = 0;
    dial_status = "# OK";
    DialView_Show(&sequence, dial_status);
}

void InteractionDemo_Init(void)
{
    Audit_Init();
    DuressDemo_Reset();
    InputEvent_Clear(&pending_event);
    Pwd_ClearInput();
    Encoder_ClearCount();
    ShowHome();
}

uint8_t InteractionDemo_TakeEvent(InputEvent *event)
{
    return InputEvent_Take(&pending_event, event);
}

void InteractionDemo_Update(uint8_t key)
{
    uint8_t overflow, changed = 0;
    int8_t direction;
    InputEvent numeric_event;
    Audit_Process();
    DuressDemo_ServiceAudit();
    if (mode == DEMO_LOG) {
        LogDemo_Update(key);
        if (!LogDemo_IsActive()) ShowHome();
        return;
    }
    if (mode == DEMO_DURESS) {
        DuressDemo_Update(key);
        if (!DuressDemo_IsActive()) ShowHome();
        return;
    }
    overflow = Encoder_TakeOverflow();
    if (mode != DEMO_DIAL) {
        /* Discard knob motion outside its test, without accumulating old events. */
        while (Encoder_PopStep(&direction)) { }
        if (mode == DEMO_PASSWORD) {
            PasswordDemo_Update(key);
            if (PasswordDemo_TakeEvent(&numeric_event)) pending_event = numeric_event;
            if (!PasswordDemo_IsActive()) ShowHome();
        } else if (key == 1U) {
            mode = DEMO_DIAL;
            ClearDial();
        } else if (key == 2U) {
            InputEvent_Clear(&pending_event);
            mode = DEMO_PASSWORD;
            PasswordDemo_EnterInput();
        } else if (key == 3U) {
            InputEvent_Clear(&pending_event);
            mode = DEMO_DURESS;
            DuressDemo_Start();
        } else if (key == 4U) {
            InputEvent_Clear(&pending_event);
            mode = DEMO_LOG;
            LogDemo_Start();
        }
        return;
    }
    if (key == 0U) {
        InputEvent_SetStatus(&pending_event, INPUT_METHOD_DIAL, INPUT_EVENT_CANCELLED);
        Encoder_ClearCount(); ShowHome(); return;
    }
    if (key == 11U) { ClearDial(); return; }
    if (overflow) {
        InputEvent_SetStatus(&pending_event, INPUT_METHOD_DIAL, INPUT_EVENT_FAULT);
        sequence.invalid = 1;
        dial_status = "Lost! *X";
        changed = 1;
    }
    while (Encoder_PopStep(&direction)) {
        if (finished || sequence.invalid) continue;
        if (!DialSequence_Append(&sequence, direction)) {
            InputEvent_SetStatus(&pending_event, INPUT_METHOD_DIAL, INPUT_EVENT_FAULT);
            dial_status = "Long! *X";
        }
        else dial_status = "# OK";
        changed = 1;
    }
    /* Overflow may occur while draining, so check again before accepting a code. */
    if (Encoder_TakeOverflow()) {
        InputEvent_SetStatus(&pending_event, INPUT_METHOD_DIAL, INPUT_EVENT_FAULT);
        sequence.invalid = 1;
        dial_status = "Lost! *X";
        changed = 1;
    }
    if (key == 15U && !finished && !sequence.invalid) { /* matrix # */
        if (sequence.length < DIAL_SEGMENTS) dial_status = "Need 3";
        else {
            AuthDecision decision;
            InputEvent_SubmitDial(&pending_event, &sequence);
            decision = DuressDemo_EvaluateInput(&pending_event);
            dial_status = decision.result == AUTH_NOT_CONFIGURED ? "NO CFG" :
                          decision.result == AUTH_AUDIT_PENDING ? "BUSY" :
                          decision.allow_unlock ? "PASS" : "NO MATCH";
            finished = 1;
        }
        changed = 1;
    }
    if (changed) DialView_Show(&sequence, dial_status);
}
