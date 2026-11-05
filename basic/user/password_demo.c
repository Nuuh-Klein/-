#include "password_demo.h"
#include "pattern_view.h"
#include "key.h"

static uint8_t input_active;
static InputEvent pending_event;

static void ShowHome(void)
{
    input_active = 0;
    PatternView_Message("PASSWORD TEST", "2 Input password", "6 digits: 1-9", "Input test only");
}

static void ShowInput(const char *confirm_hint)
{
    PasswordView_Input(InputCnt, confirm_hint, "0 Exit");
}

void PasswordDemo_Init(void)
{
    InputEvent_Clear(&pending_event);
    Pwd_ClearInput();
    ShowHome();
}

void PasswordDemo_EnterInput(void)
{
    InputEvent_Clear(&pending_event);
    Pwd_ClearInput();
    input_active = 1;
    ShowInput("# OK");
}

uint8_t PasswordDemo_IsActive(void)
{
    return input_active;
}

uint8_t PasswordDemo_TakeEvent(InputEvent *event)
{
    return InputEvent_Take(&pending_event, event);
}

void PasswordDemo_Update(uint8_t key)
{
    if (key == KEY_NONE) return;
    if (!input_active) {
        if (key == 2U) {
            PasswordDemo_EnterInput();
        }
        return;
    }
    if (key == 0U) {
        InputEvent_SetStatus(&pending_event, INPUT_METHOD_PASSWORD, INPUT_EVENT_CANCELLED);
        Pwd_ClearInput();
        ShowHome();
    } else if (key >= 1U && key <= 9U) {
        if (InputCnt < PWD_MAX_LEN) {
            InputEvent_Clear(&pending_event);
            Pwd_Input_Process(key);
            ShowInput("# OK");
        } else {
            ShowInput("# Full");
        }
    } else if (key == 11U) {
        InputEvent_Clear(&pending_event);
        Pwd_Input_Process(key);
        ShowInput("# OK");
    } else if (key == 15U) {
        if (InputCnt == PWD_MAX_LEN) {
            if (!Key_SaveFlag) {
                Pwd_Input_Process(key);
                InputEvent_SubmitPassword(&pending_event, SavedPwd);
            }
            ShowInput("# Done");
        } else {
            ShowInput("#Need6");
        }
    }
}
