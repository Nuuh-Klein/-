#ifndef LOCKING_PATTERN_VIEW_H
#define LOCKING_PATTERN_VIEW_H

#include "pattern.h"

void PatternView_Message(const char *title, const char *second,
                         const char *third, const char *fourth);
void PatternView_Input(const Pattern *pattern, const char *title, const char *note);
/* Reuse the small grid as a static keypad illustration; never reveal pressed digits. */
void PasswordView_Input(uint8_t length, const char *confirm_hint, const char *exit_hint);

#endif
