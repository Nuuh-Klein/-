#ifndef LOCKING_PASSWORD_INPUT_H
#define LOCKING_PASSWORD_INPUT_H

#include "config.h"

/* Input buffers only: SavedPwd is the last submitted RAM snapshot, not Flash. */
extern uint8_t InputBuf[PWD_MAX_LEN];
extern uint8_t SavedPwd[PWD_MAX_LEN];
extern uint8_t InputCnt;
extern uint8_t Key_SaveFlag;

/* Digits append, * backspaces, # submits. UI reserves 0 for Exit. */
void Pwd_Input_Process(uint8_t key);
void Pwd_ClearInput(void);
void Pwd_Save(void);
/* Legacy text renderer; the current UI uses PasswordView_Input. */
void Pwd_PrintInputToOLED(void);

#endif
