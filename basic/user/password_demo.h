#ifndef LOCKING_PASSWORD_DEMO_H
#define LOCKING_PASSWORD_DEMO_H

#include <stdint.h>
#include "input_event.h"

/* Input/display test only. No password validation, Flash writes or unlocking. */
void PasswordDemo_Init(void);
void PasswordDemo_Update(uint8_t key);
/* Embedded in the shared test menu without drawing its standalone home page. */
void PasswordDemo_EnterInput(void);
uint8_t PasswordDemo_IsActive(void);
uint8_t PasswordDemo_TakeEvent(InputEvent *event);

#endif
