#ifndef LOCKING_CREDENTIALS_H
#define LOCKING_CREDENTIALS_H

#include "config.h"
#include "dial_sequence.h"

typedef struct {
    uint8_t password[PASSWORD_LEN];
    uint8_t duress_password[PASSWORD_LEN];
    DialSequence dial;
    DialSequence duress_dial;
} CredentialConfig;

uint8_t Credentials_IsValid(const CredentialConfig *config);
uint8_t Credentials_PasswordEquals(const uint8_t *left, const uint8_t *right);

#endif
