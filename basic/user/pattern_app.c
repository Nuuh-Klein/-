#include "pattern_app.h"
#include "pattern.h"
#include "pattern_store.h"
#include "pattern_view.h"
#include "config.h"
#include "key.h"
#include "buzzer.h"
#include "led.h"
#include "motor.h"

typedef enum {
    APP_HOME, APP_UNLOCK_INPUT, APP_SET_FIRST, APP_SET_CONFIRM,
    APP_MESSAGE, APP_OPEN, APP_LOCKED
} AppState;

static AppState state, after_message;
static Pattern saved, input, first;
static PatternStoreResult storage;
static uint8_t failures, countdown;
static uint32_t state_since;

static void ShowHome(void)
{
    state = APP_HOME;
    Pattern_Clear(&input);
    Pattern_Clear(&first);
    PatternView_Message("PATTERN DEMO", "1 Unlock", "2 Set pattern",
        storage == PATTERN_STORE_OK ? "Saved / NO ADMIN" :
        storage == PATTERN_STORE_EMPTY ? "No pattern saved" : "Store invalid");
}

static void ShowInput(const char *note)
{
    PatternView_Input(&input, state == APP_UNLOCK_INPUT ? "UNLOCK" :
        state == APP_SET_FIRST ? "SET 1/2" : "SET 2/2", note);
}

static void EnterInput(AppState next)
{
    state = next;
    Pattern_Clear(&input);
    ShowInput(0);
}

static void Message(uint32_t now, AppState next, const char *title, const char *detail)
{
    state = APP_MESSAGE;
    state_since = now;
    after_message = next;
    PatternView_Message(title, detail, "", "Please wait...");
}

static void ShowCountdown(uint32_t elapsed)
{
    uint8_t remaining = (uint8_t)(LOCKING_TIME - elapsed / 1000UL);
    char line[] = "Wait: 30 seconds";
    if (remaining == countdown) return;
    countdown = remaining;
    line[6] = (char)('0' + remaining / 10U);
    line[7] = (char)('0' + remaining % 10U);
    PatternView_Message("LOCKED", line, "3 wrong patterns", "Keys disabled");
}

void PatternApp_Init(uint32_t now)
{
    storage = PatternStore_Load(&saved);
    failures = 0;
    state_since = now;
    LED_Alloff();
    Buzzer_SetActive(0);
    PWM_close();
    ShowHome();
}

void PatternApp_Update(uint32_t now, uint8_t key)
{
    uint32_t elapsed = now - state_since;
    if (state == APP_OPEN) {
        Buzzer_SetActive(elapsed < BUZZER_BEEP_MS);
        if (elapsed >= PATTERN_UNLOCK_MS) {
            PWM_close();
            LED_Alloff();
            ShowHome();
        }
        return;
    }
    if (state == APP_LOCKED) {
        if (elapsed >= LOCKING_TIME * 1000UL) {
            Buzzer_SetActive(0);
            LED_Alloff();
            failures = 0;
            ShowHome();
        } else {
            Buzzer_SetActive(elapsed < BUZZER_ALARM_TIMES * (BUZZER_BEEP_MS + BUZZER_PAUSE_MS) &&
                elapsed % (BUZZER_BEEP_MS + BUZZER_PAUSE_MS) < BUZZER_BEEP_MS);
            ShowCountdown(elapsed);
        }
        return;
    }
    if (state == APP_MESSAGE) {
        Buzzer_SetActive(elapsed < BUZZER_BEEP_MS);
        if (elapsed >= PATTERN_FEEDBACK_MS) {
            LED_Alloff();
            if (after_message == APP_HOME) ShowHome();
            else EnterInput(after_message);
        }
        return;
    }
    if (key == KEY_NONE) return;
    if (state == APP_HOME) {
        if (key == 1U) {
            if (storage == PATTERN_STORE_OK) EnterInput(APP_UNLOCK_INPUT);
            else Message(now, APP_HOME, "NO PATTERN", "Press 2 to set");
        } else if (key == 2U) {
            Pattern_Clear(&first);
            EnterInput(APP_SET_FIRST);
        }
        return;
    }
    if (key == 0U) { ShowHome(); return; }
    if (key == 11U) { Pattern_Clear(&input); ShowInput(0); return; }
    if (key >= 1U && key <= 9U) {
        ShowInput(Pattern_Append(&input, key) ? 0 : "Used!");
        return;
    }
    if (key != 15U) return; /* # (15), not the old password helper's D (10). */
    if (!Pattern_IsValid(&input)) { ShowInput("Min 4!"); return; }
    if (state == APP_SET_FIRST) {
        first = input;
        EnterInput(APP_SET_CONFIRM);
    } else if (state == APP_SET_CONFIRM) {
        if (!Pattern_Equals(&first, &input)) {
            Pattern_Clear(&first);
            Message(now, APP_SET_FIRST, "NOT SAME", "Start both again");
        } else if (PatternStore_Save(&first) != FLASH_COMPLETE) {
            storage = PatternStore_Load(&saved);
            Message(now, APP_HOME, "SAVE FAILED", "Check / retry");
        } else {
            saved = first;
            storage = PATTERN_STORE_OK;
            Pattern_Clear(&first);
            Message(now, APP_HOME, "PATTERN SAVED", "Try 1 Unlock");
        }
    } else if (state == APP_UNLOCK_INPUT) {
        if (Pattern_Equals(&input, &saved)) {
            failures = 0;
            state = APP_OPEN;
            state_since = now;
            PWM_open();
            LED_Green_On();
            Buzzer_SetActive(1);
            PatternView_Message("UNLOCKED", "Auto close: 3s", "", "Please wait...");
        } else {
            failures++;
            LED_Red_On();
            if (failures >= MAX_ERROR_COUNT) {
                state = APP_LOCKED;
                state_since = now;
                countdown = 0xFFU;
                PWM_close();
                Buzzer_SetActive(1);
                ShowCountdown(0);
            } else {
                Message(now, APP_UNLOCK_INPUT, "WRONG PATTERN",
                    failures == 1U ? "2 tries left" : "1 try left");
            }
        }
    }
}
