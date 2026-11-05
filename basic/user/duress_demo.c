#include "duress_demo.h"
#include "auth.h"
#include "credential_store.h"
#include "password_demo.h"
#include "pattern_view.h"
#include "dial_view.h"
#include "encoder.h"
#include "key.h"
#include "audit.h"

typedef enum { DURESS_HUB, DURESS_NUMBER, DURESS_DIAL, DURESS_RESULT, DURESS_AUDIT } DemoPage;
static DemoPage page, audit_return;
static uint8_t active, ready, have_audit;
static InputMethod last_method;
static AuthContext auth;
static AuthAuditEvent last_audit;
static DialSequence dial;
static const char *last_result;

static void ShowHub(void)
{
    page = DURESS_HUB;
    Encoder_ClearCount();
    PatternView_Message("DURESS TEST", "1 Dial 2 Number",
        ready ? "A Audit event" : "# Init samples", "0 Back");
}

static void Reload(void)
{
    CredentialConfig config;
    ready = CredentialStore_Load(&config) == CREDENTIAL_STORE_OK && Auth_Init(&auth, &config);
    if (!ready) Auth_Init(&auth, 0);
}

static void InitSamples(void)
{
    CredentialConfig config;
    uint8_t i;
    for (i = 0; i < PASSWORD_LEN; i++) {
        config.password[i] = 1;
        config.duress_password[i] = i + 1U;
    }
    DialSequence_Clear(&config.dial);
    DialSequence_Clear(&config.duress_dial);
    config.dial.length = config.duress_dial.length = DIAL_SEGMENTS;
    for (i = 0; i < DIAL_SEGMENTS; i++) {
        config.dial.direction[i] = config.duress_dial.direction[i] = i == 1U ? -1 : 1;
    }
    config.dial.steps[0] = 3; config.dial.steps[1] = 2; config.dial.steps[2] = 4;
    config.duress_dial.steps[0] = 2; config.duress_dial.steps[1] = 3; config.duress_dial.steps[2] = 4;
    if (CredentialStore_Save(&config) != CREDENTIAL_STORE_OK) {
        PatternView_Message("SAVE FAILED", "No unlock", "", "0 Back / #Retry");
        return;
    }
    Reload();
    ShowHub();
}

static void EnterInput(InputMethod method)
{
    last_method = method;
    Encoder_ClearCount();
    if (method == INPUT_METHOD_PASSWORD) {
        page = DURESS_NUMBER;
        PasswordDemo_EnterInput();
    } else {
        page = DURESS_DIAL;
        DialSequence_Clear(&dial);
        DialView_Show(&dial, "# OK");
    }
}

AuthDecision DuressDemo_EvaluateInput(const InputEvent *event)
{
    AuthDecision decision;
    if (!ready && !auth.audit_pending) Reload();
    decision = Auth_Evaluate(&auth, event);
    have_audit = Audit_Capture(&auth, &last_audit);
    Audit_Process();
    return decision;
}

static void Evaluate(const InputEvent *event)
{
    AuthDecision decision = DuressDemo_EvaluateInput(event);
    page = DURESS_RESULT;
    last_result = decision.result == AUTH_AUDIT_PENDING ? "BUSY" :
                  decision.allow_unlock ? "MATCH" : "NO MATCH";
    /* Both accepted cases use exactly the same visible text and no output effects. */
    PatternView_Message("RESULT", last_result,
        "* Retry 0 Back", "A Audit event");
}

static void ShowAudit(void)
{
    audit_return = page;
    page = DURESS_AUDIT;
    if (!have_audit) {
        PatternView_Message("AUDIT EVENT", "No event yet", "", "0 Back");
        return;
    }
    PatternView_Message("AUDIT EVENT",
        last_audit.method == INPUT_METHOD_PASSWORD ? "Method: NUMBER" : "Method: DIAL",
        last_audit.success ? "Result: PASS" : "Result: FAIL",
        last_audit.duress ? "DURESS:1 0 Back" : "DURESS:0 0 Back");
}

void DuressDemo_Start(void)
{
    active = 1;
    have_audit = 0;
    if (auth.audit_pending) ready = auth.configured;
    else Reload();
    ShowHub();
}

uint8_t DuressDemo_IsActive(void) { return active; }

void DuressDemo_Reset(void)
{
    active = ready = have_audit = 0;
    Auth_Init(&auth, 0);
}

void DuressDemo_ServiceAudit(void)
{
    if (auth.audit_pending && Audit_Capture(&auth, &last_audit)) have_audit = 1;
}

void DuressDemo_Update(uint8_t key)
{
    int8_t direction;
    uint8_t changed = 0, overflow = Encoder_TakeOverflow();
    InputEvent event;
    const char *status = "# OK";
    if (page != DURESS_DIAL) while (Encoder_PopStep(&direction)) { }
    if (page == DURESS_HUB) {
        if (key == 0U) active = 0;
        else if (!ready && key == 15U) InitSamples();
        else if (ready && key == 1U) EnterInput(INPUT_METHOD_DIAL);
        else if (ready && key == 2U) EnterInput(INPUT_METHOD_PASSWORD);
        else if (key == 12U) ShowAudit();
    } else if (page == DURESS_NUMBER) {
        PasswordDemo_Update(key);
        if (PasswordDemo_TakeEvent(&event)) {
            if (event.kind == INPUT_EVENT_CANCELLED) ShowHub();
            else if (event.kind == INPUT_EVENT_SUBMITTED) Evaluate(&event);
        }
    } else if (page == DURESS_DIAL) {
        if (key == 0U) { ShowHub(); return; }
        if (key == 11U) { EnterInput(INPUT_METHOD_DIAL); return; }
        if (overflow) dial.invalid = 1;
        while (Encoder_PopStep(&direction)) {
            if (!dial.invalid) DialSequence_Append(&dial, direction);
            changed = 1;
        }
        if (Encoder_TakeOverflow()) overflow = 1;
        if (overflow) { dial.invalid = 1; status = "Lost! *X"; changed = 1; }
        else if (dial.invalid) status = "Long! *X";
        if (key == 15U && !dial.invalid) {
            if (InputEvent_SubmitDial(&event, &dial)) { Evaluate(&event); return; }
            status = "Need 3";
            changed = 1;
        }
        if (changed) DialView_Show(&dial, status);
    } else if (page == DURESS_RESULT) {
        if (key == 0U) ShowHub();
        else if (key == 11U) EnterInput(last_method);
        else if (key == 12U) ShowAudit();
    } else if (page == DURESS_AUDIT && key == 0U) {
        if (audit_return == DURESS_RESULT) {
            page = DURESS_RESULT;
            PatternView_Message("RESULT", last_result,
                "* Retry 0 Back", "A Audit event");
        } else ShowHub();
    }
}
