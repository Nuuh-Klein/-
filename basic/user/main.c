#include "stm32f10x.h"
#include "buzzer.h"
#include "motor.h"
#include "led.h"
#include "admin_fingerprint.h"
#include "credential_editor.h"
#include "auth.h"
#include "audit.h"
#include "log.h"
#include "log_demo.h"
#include "pattern_view.h"
#include "dial_view.h"
#include "password_input.h"
#include "encoder.h"
#include "key.h"
#include "timebase.h"
#include "OLED.h"
#include <string.h>

typedef enum {
    STATE_HOME,
    STATE_KEYPAD_INPUT,
    STATE_DIAL_INPUT,
    STATE_ADMIN_VERIFY,
    STATE_FIRST_ADMIN,
    STATE_ADMIN_MENU,
    STATE_PASSWORD_SELECT,
    STATE_ENROLL_FIRST,
    STATE_ENROLL_REMOVE,
    STATE_ENROLL_SECOND,
    STATE_ENROLL_RECOVER,
    STATE_LOGS,
    STATE_INIT_CREDENTIALS,
    STATE_ERROR,
    STATE_SUCCESS
} AppState;
static AppState state, retry_state;
static AdminConfig admins;
static AdminStoreResult admin_state;
static AuthContext auth;
static CredentialEditor editor;
static DialSequence dial;
static uint8_t authorized, first_admin, editing, edit_duress, second_input, finished, cancelled;
static InputMethod edit_method;
static uint32_t success_time;
static uint8_t finger_diagnostics, failed_command;
static AS608_UARTDiagnostics failed_uart;
static AS608_HardwareState failed_hw;

static void Home(void);
static void Menu(void);
static void BeginEnroll(void);
static void ShowError(const char *message, AppState retry);

static uint8_t CancelFinger(void)
{
    if (KEY_Scan() == 0U) cancelled = 1;
    return cancelled;
}
static void ClearInput(void)
{
    Pwd_ClearInput(); DialSequence_Clear(&dial); Encoder_ClearCount(); finished = 0;
}
static uint8_t ReloadCredentials(void)
{
    CredentialConfig config;
    if (auth.audit_pending) return 0;
    if (CredentialStore_Load(&config) != CREDENTIAL_STORE_OK) { Auth_Init(&auth, 0); return 0; }
    return Auth_Init(&auth, &config);
}
static void Home(void)
{
    authorized = first_admin = editing = 0;
    PWM_close();
    LED_Alloff();
    CredentialEditor_Cancel(&editor); ClearInput(); state = STATE_HOME;
    /* 8-pixel text cells: center the title on the 128-pixel OLED. */
    OLED_FrameClear();
    OLED_DrawText((uint8_t)((128U - (sizeof("LOCKING") - 1U) * 8U) / 2U), 0, "LOCKING");
    OLED_DrawText(0, 16, "1 Keypad PIN");
    OLED_DrawText(0, 32, "2 Dial PIN");
    OLED_DrawText(0, 48, "A Admin");
    OLED_FrameShow();
}
static void Menu(void)
{
    if (!authorized) { Home(); return; }
    editing = 0; CredentialEditor_Cancel(&editor); ClearInput();
    if (!auth.configured) {
        state = STATE_INIT_CREDENTIALS;
        PatternView_Message("NO PASSWORDS", "# Init samples", "Explicit write", "0 Admin menu");
        return;
    }
    state = STATE_ADMIN_MENU;
    PatternView_Message("1Normal 2Duress", "3 Add admin FP", "4 Audit logs", "0 Logout");
}
static void MenuWithoutInit(void)
{
    state = STATE_ADMIN_MENU; editing = 0; CredentialEditor_Cancel(&editor); ClearInput();
    PatternView_Message("1Normal 2Duress", "3 Add admin FP", "4 Audit logs", "0 Logout");
}
static void ShowError(const char *message, AppState retry)
{
    finger_diagnostics = 0;
    state = STATE_ERROR; retry_state = retry;
    Pwd_ClearInput(); Encoder_ClearCount();
    PatternView_Message("NOT COMPLETED", message, "# Retry", authorized ? "0 Admin menu" : "0 Home");
}
static void FingerError(uint8_t code, AppState retry)
{
    char value[] = "Code:00 Cmd:00";
    static const char hex[] = "0123456789ABCDEF";
    if (code == AS608_ERR_CANCELLED || cancelled) { if (authorized) Menu(); else Home(); return; }
    ShowError(code == AS608_ERR_DUPLICATE ? "Finger exists" : code == AS608_ERR_NO_FINGER ? "No finger" :
        code == AS608_ERR_FINGER_PRESENT ? "Lift finger now" : code == AS608_ERR_MERGE ? "Different finger" :
        code == AS608_ERR_SAVE_CONFIG ? "Role save failed" : code == AS608_ERR_LIB_FULL ? "Admin/FP full" :
        code == AS608_ERR_NO_MATCH ? "No match" : "Fingerprint fail", retry);
    finger_diagnostics = 1; failed_command = AS608_LastCommand;
    failed_uart = AS608_LastDiag; AS608_GetHardwareState(&failed_hw);
    value[5] = hex[code >> 4]; value[6] = hex[code & 15];
    value[12] = hex[AS608_LastCommand >> 4]; value[13] = hex[AS608_LastCommand & 15];
    /* Replace the retry hint with diagnostic code; # still retries. */
    OLED_ShowString(3, 1, value);
    OLED_ShowString(4, 1, "B/C/D #Try 0Bk");
}
static void Hex(char *out, uint32_t value, uint8_t digits)
{
    static const char alphabet[] = "0123456789ABCDEF";
    while (digits) { out[--digits] = alphabet[value & 15U]; value >>= 4; }
}
static void Count3(char *out, uint16_t value)
{
    uint8_t digits = 3;
    if (value > 999U) value = 999U;
    while (digits) { out[--digits] = (char)('0' + value % 10U); value /= 10U; }
}
static void ShowFingerDiagnostics(uint8_t registers)
{
    if (registers) {
        char line1[] = "BRR0000 S0000";
        char line2[] = "CR1:0000 I0 P0";
        char line3[] = "PA:00 M0 G0 Q00";
        Hex(line1 + 3, failed_hw.brr, 4); Hex(line1 + 9, failed_hw.cr2, 4);
        Hex(line2 + 4, failed_hw.cr1, 4); line2[10] = '0' + failed_hw.irq_enabled; line2[13] = '0' + failed_hw.irq_masked;
        Hex(line3 + 3, failed_hw.gpio_modes, 2); line3[7] = '0' + failed_hw.remapped;
        line3[10] = '0' + failed_hw.priority_group; Hex(line3 + 13, failed_hw.irq_priority, 2);
        PatternView_Message(line1, line2, line3, "B:Bytes #Try 0Bk");
    } else {
        uint8_t i;
        char line1[] = "C00 TX000 TC0";
        char line2[] = "RX000 E00 D000";
        char bytes[17];
        Hex(line1 + 1, failed_command, 2); Count3(line1 + 6, failed_uart.tx_bytes); line1[12] = '0' + failed_uart.tx_complete;
        Count3(line2 + 2, failed_uart.rx_bytes); Hex(line2 + 7, failed_uart.uart_errors, 2); Count3(line2 + 11, failed_uart.rx_dropped);
        for (i = 0; i < 8; i++) Hex(bytes + 2U * i, failed_uart.first_rx[i], 2);
        bytes[16] = 0;
        PatternView_Message(line1, line2, bytes, "C:Regs #Try 0Bk");
    }
}
static void ProbeFingerprintLink(void)
{
    char irq[] = "I:00 R000 E00";
    char poll[] = "P:00 R000 E00";
    uint8_t result;
    PatternView_Message("LINK TEST ONLY", "IRQ / poll RX", "Checking...", "No enrollment");
    result = AS608_VerifyPassword();
    Hex(irq + 2, result, 2); Count3(irq + 6, AS608_LastDiag.rx_bytes); Hex(irq + 11, AS608_LastDiag.uart_errors, 2);
    result = AS608_VerifyPasswordPolled();
    Hex(poll + 2, result, 2); Count3(poll + 6, AS608_LastDiag.rx_bytes); Hex(poll + 11, AS608_LastDiag.uart_errors, 2);
    PatternView_Message("LINK TEST ONLY", irq, poll, "D Again #Try0Bk");
}
static void Success(const char *message)
{
    state = STATE_SUCCESS; success_time = Timebase_Millis();
    CredentialEditor_Cancel(&editor); ClearInput();
    PatternView_Message("COMPLETED", message, "Returning...", authorized ? "0 Admin menu" : "0 Home");
}
static void ShowEnroll(AppState next)
{
    state = next; Encoder_ClearCount();
    if (next == STATE_ENROLL_FIRST) PatternView_Message("ENROLL 1/3", "Place NEW finger", "# Capture first", "0 Cancel");
    else if (next == STATE_ENROLL_REMOVE) PatternView_Message("ENROLL 2/3", "Lift your finger", "# Check removed", "0 Cancel");
    else if (next == STATE_ENROLL_SECOND) PatternView_Message("ENROLL 3/3", "Same finger back", "# Capture + save", "0 Cancel");
    else PatternView_Message("PENDING TEMPLATE", "Use saved finger", "# Verify & save", "0 Cancel");
}
static void BeginEnroll(void)
{
    uint8_t code, recover;
    if (!authorized && !first_admin) { Home(); return; }
    cancelled = 0;
    code = AdminFinger_Begin(&admins, &recover);
    if (code != AS608_OK) { FingerError(code, STATE_ENROLL_FIRST); return; }
    ShowEnroll(recover ? STATE_ENROLL_RECOVER : STATE_ENROLL_FIRST);
}
static void EnterInput(InputMethod method)
{
    ClearInput();
    if (method == INPUT_METHOD_PASSWORD) {
        state = STATE_KEYPAD_INPUT;
        PasswordView_Input(0, editing ? (second_input ? "2nd #OK" : "1st #OK") : "# OK", "0 Back");
    } else {
        state = STATE_DIAL_INPUT;
        DialView_Show(&dial, editing ? (second_input ? "2nd #OK" : "1st #OK") : "# OK");
    }
}
static void StartEdit(InputMethod method)
{
    if (!authorized) { Home(); return; }
    if (auth.audit_pending) { ShowError("Audit busy", STATE_PASSWORD_SELECT); return; }
    if (!CredentialEditor_Begin(&editor, edit_duress, method)) {
        state = STATE_INIT_CREDENTIALS;
        PatternView_Message("NO PASSWORDS", "# Init samples", "Explicit write", "0 Admin menu"); return;
    }
    editing = 1; second_input = 0; edit_method = method; EnterInput(method);
}
static void Submitted(const InputEvent *event)
{
    AuthDecision decision;
    EditResult result;
    if (editing) {
        result = CredentialEditor_Submit(&editor, event);
        if (result == EDIT_FIRST_ACCEPTED) { second_input = 1; EnterInput(edit_method); return; }
        if (result == EDIT_SAVED) { ReloadCredentials(); Success("Password saved"); return; }
        ShowError(result == EDIT_MISMATCH ? "Inputs differ" : result == EDIT_CONFLICT ? "Normal=duress" :
                  result == EDIT_SAVE_FAILED ? "Save failed" : "Invalid input", STATE_PASSWORD_SELECT);
    } else {
        decision = Auth_Evaluate(&auth, event);
        Audit_Capture(&auth, 0); Audit_Process();
        finished = 1;
        if (state == STATE_KEYPAD_INPUT) PasswordView_Input(InputCnt,
            decision.result == AUTH_AUDIT_PENDING ? "BUSY" : decision.result == AUTH_NOT_CONFIGURED ? "NO CFG" : decision.allow_unlock ? "PASS" : "FAIL", "0 Back");
        else DialView_Show(&dial, decision.result == AUTH_AUDIT_PENDING ? "BUSY" :
            decision.result == AUTH_NOT_CONFIGURED ? "NO CFG" : decision.allow_unlock ? "PASS" : "NO MATCH");
        /* Normal and duress success deliberately share all visible outputs.
         * Reuse the original blocking sounds; only a rejected credential alarms. */
        if (decision.allow_unlock) {
            LED_Red_Off();
            LED_Green_On();
            PWM_open();
            buzzer1();
        } else if (decision.result == AUTH_REJECTED) {
            PWM_close();
            LED_Green_Off();
            LED_Red_On();
            buzzer2();
        }
    }
}
static void InitSamples(void)
{
    CredentialConfig config, check;
    uint8_t i;
    if (CredentialStore_Load(&check) == CREDENTIAL_STORE_OK) { ReloadCredentials(); Menu(); return; }
    if (auth.audit_pending) { ShowError("Audit busy", STATE_INIT_CREDENTIALS); return; }
    memset(&config, 0, sizeof(config));
    for (i = 0; i < PASSWORD_LEN; i++) { config.password[i] = 1; config.duress_password[i] = i + 1U; }
    config.dial.length = config.duress_dial.length = 3;
    for (i = 0; i < 3; i++) config.dial.direction[i] = config.duress_dial.direction[i] = i == 1 ? -1 : 1;
    config.dial.steps[0] = 3; config.dial.steps[1] = 2; config.dial.steps[2] = 4;
    config.duress_dial.steps[0] = 2; config.duress_dial.steps[1] = 3; config.duress_dial.steps[2] = 4;
    (void)CredentialStore_Save(&config);
    if (!ReloadCredentials()) { ShowError("Save failed", STATE_INIT_CREDENTIALS); return; }
    Success("Samples saved");
}
static void Login(void)
{
    uint16_t id, score;
    uint8_t code, accepted;
    if (Audit_GetPendingCount() == AUDIT_QUEUE_CAPACITY) { ShowError("Audit busy", STATE_ADMIN_VERIFY); return; }
    cancelled = 0;
    PatternView_Message("CHECK FINGER", "Place admin FP", "Waiting...", "0 Cancel");
    code = AdminFinger_Prepare();
    if (code == AS608_OK) code = AS608_JM101Search(&id, &score);
    if (cancelled || code == AS608_ERR_CANCELLED) { Home(); return; }
    accepted = code == AS608_OK && AdminStore_Contains(&admins, id);
    if (code == AS608_OK || code == AS608_ERR_NO_MATCH) {
        if (!Audit_Enqueue(LOG_METHOD_FINGER, accepted, 0)) { ShowError("Audit busy", STATE_ADMIN_VERIFY); return; }
        Audit_Process();
    }
    if (accepted) { authorized = 1; Menu(); }
    else if (code == AS608_OK) ShowError("Not admin finger", STATE_ADMIN_VERIFY);
    else FingerError(code, STATE_ADMIN_VERIFY);
}
static void App_Init(void)
{
    Audit_Init(); Auth_Init(&auth, 0); ReloadCredentials();
    admin_state = AdminStore_Load(&admins);
    AS608_SetStatusCallback(0); AS608_SetCancelCallback(CancelFinger);
    Home();
}

/* All application transitions live here in main.c. Input/storage/driver modules
 * supply data and hardware operations; they do not own the application state. */
static void App_Update(uint8_t key)
{
    InputEvent event;
    int8_t direction;
    uint8_t code, changed = 0;
    const char *hint;
    Audit_Process();
    Audit_Capture(&auth, 0);
    if (state != STATE_DIAL_INPUT && state != STATE_LOGS) {
        while (Encoder_PopStep(&direction)) { }
        Encoder_TakeOverflow();
    }
    if (key == 0 && state != STATE_HOME) {
        CredentialEditor_Cancel(&editor);
        ClearInput();
        if (authorized && state != STATE_ADMIN_MENU) MenuWithoutInit();
        else Home();
        return;
    }
    /* A page transition alone must never grant administrator access. */
    if (!authorized && (state == STATE_ADMIN_MENU || state == STATE_PASSWORD_SELECT || state == STATE_INIT_CREDENTIALS || state == STATE_LOGS ||
        (editing && (state == STATE_KEYPAD_INPUT || state == STATE_DIAL_INPUT)))) {
        Home();
        return;
    }
    switch (state) {
    case STATE_HOME:
        if (key == 1 || key == 2) {
            editing = 0;
            EnterInput(key == 1 ? INPUT_METHOD_PASSWORD : INPUT_METHOD_DIAL);
        } else if (key == 12) {
            authorized = 0;
            admin_state = AdminStore_Load(&admins);
            if (admin_state == ADMIN_STORE_INVALID) {
                ShowError("Admin data bad", STATE_HOME);
                break;
            }
            if (!admins.count) {
                first_admin = 1;
                state = STATE_FIRST_ADMIN;
                PatternView_Message("FIRST ADMIN", "New admin finger", "# Begin 3 steps", "0 Home");
            } else {
                first_admin = 0;
                state = STATE_ADMIN_VERIFY;
                PatternView_Message("ADMIN VERIFY", "Place admin FP", "# Start matching", "0 Home");
            }
        }
        break;
    case STATE_ADMIN_VERIFY:
        if (key == 15) Login();
        break;
    case STATE_FIRST_ADMIN:
        if (key == 15) BeginEnroll();
        break;
    case STATE_ADMIN_MENU:
        if (key == 1 || key == 2) {
            edit_duress = key == 2;
            state = STATE_PASSWORD_SELECT;
            PatternView_Message(edit_duress ? "SET DURESS" : "SET NORMAL", "1 Number 2 Dial", "Enter twice", "0 Admin menu");
        } else if (key == 3) BeginEnroll();
        else if (key == 4) {
            state = STATE_LOGS;
            LogDemo_StartReadOnly();
        }
        break;
    case STATE_PASSWORD_SELECT:
        if (key == 1 || key == 2) StartEdit(key == 1 ? INPUT_METHOD_PASSWORD : INPUT_METHOD_DIAL);
        break;
    case STATE_INIT_CREDENTIALS:
        if (key == 15) InitSamples();
        break;
    case STATE_ENROLL_FIRST:
    case STATE_ENROLL_REMOVE:
    case STATE_ENROLL_SECOND:
    case STATE_ENROLL_RECOVER:
        if (key != 15) break;
        if (!authorized && !first_admin) { Home(); break; }
        cancelled = 0;
        if (state == STATE_ENROLL_FIRST) {
            code = AdminFinger_First();
            if (code == AS608_OK) ShowEnroll(STATE_ENROLL_REMOVE);
            else FingerError(code, STATE_ENROLL_FIRST);
        } else if (state == STATE_ENROLL_REMOVE) {
            code = AS608_CheckRemoved();
            if (code == AS608_OK) ShowEnroll(STATE_ENROLL_SECOND);
            else FingerError(code, STATE_ENROLL_REMOVE);
        } else {
            if (state == STATE_ENROLL_RECOVER) code = AdminFinger_Recover(&admins);
            else {
                code = AdminFinger_Second();
                if (code == AS608_OK) code = AS608_StoreBuffer(admins.pending_id);
                if (code == AS608_OK) code = AdminFinger_Commit(&admins);
            }
            if (code == AS608_OK) {
                admin_state = ADMIN_STORE_OK;
                Success("Admin FP saved");
            } else FingerError(code, STATE_ENROLL_FIRST);
        }
        break;
    case STATE_LOGS:
        LogDemo_Update(key);
        break;
    case STATE_KEYPAD_INPUT:
        hint = editing ? (second_input ? "2nd #OK" : "1st #OK") : "# OK";
        if (key >= 1 && key <= 9) {
            if (finished) break;
            Pwd_Input_Process(key);
            PasswordView_Input(InputCnt, hint, "0 Back");
        } else if (key == 11) {
            finished = 0;
            Pwd_Input_Process(key);
            PasswordView_Input(InputCnt, hint, "0 Back");
        } else if (key == 15 && !finished) {
            if (InputCnt != PASSWORD_LEN) PasswordView_Input(InputCnt, "# Need6", "0 Back");
            else if (InputEvent_SubmitPassword(&event, InputBuf)) Submitted(&event);
        }
        break;
    case STATE_DIAL_INPUT:
        if (key == 11) { EnterInput(INPUT_METHOD_DIAL); break; }
        if (Encoder_TakeOverflow()) { dial.invalid = 1; changed = 1; }
        while (Encoder_PopStep(&direction)) {
            if (!finished && !dial.invalid) DialSequence_Append(&dial, direction);
            changed = 1;
        }
        if (Encoder_TakeOverflow()) { dial.invalid = 1; changed = 1; }
        if (key == 15 && !finished && !dial.invalid) {
            if (InputEvent_SubmitDial(&event, &dial)) { Submitted(&event); break; }
            DialView_Show(&dial, "Need 3");
            break;
        }
        if (changed && !finished) DialView_Show(&dial, dial.invalid ? "Lost! *X" :
            editing ? (second_input ? "2nd #OK" : "1st #OK") : "# OK");
        break;
    case STATE_SUCCESS:
        if ((uint32_t)(Timebase_Millis() - success_time) >= 1500U) {
            if (first_admin) {
                Home();
                state = STATE_ADMIN_VERIFY;
                PatternView_Message("ADMIN VERIFY", "Use NEW finger", "# Start matching", "0 Home");
                first_admin = 0;
            } else Menu();
        }
        break;
    case STATE_ERROR:
        if (finger_diagnostics && key == 10) { ProbeFingerprintLink(); break; }
        if (finger_diagnostics && (key == 13 || key == 14)) { ShowFingerDiagnostics(key == 14); break; }
        if (key != 15) break;
        switch (retry_state) {
        case STATE_ADMIN_VERIFY:
            state = STATE_ADMIN_VERIFY;
            Login();
            break;
        case STATE_ENROLL_FIRST:
            BeginEnroll();
            break;
        case STATE_ENROLL_REMOVE:
        case STATE_ENROLL_SECOND:
        case STATE_ENROLL_RECOVER:
            ShowEnroll(retry_state);
            break;
        case STATE_INIT_CREDENTIALS:
            if (authorized) { state = STATE_INIT_CREDENTIALS; InitSamples(); }
            else Home();
            break;
        case STATE_HOME:
            Home();
            break;
        default:
            if (!authorized) { Home(); break; }
            state = STATE_PASSWORD_SELECT;
            PatternView_Message(edit_duress ? "SET DURESS" : "SET NORMAL", "1 Number 2 Dial", "Enter twice", "0 Admin menu");
            break;
        }
        break;
    default:
        Home();
        break;
    }
}

#ifndef LOCKING_HOST_TEST
int main(void)
{
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    OLED_Init();
    LED_Init();
    Buzzer_Init();
    PWM_Init();
    KEY_Init();
    Encoder_Init();
    Timebase_Init();
    AS608_Init();
    App_Init();
    while (1) {
        App_Update(KEY_Scan());
    }
}
#endif
