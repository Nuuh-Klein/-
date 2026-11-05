#include "credentials.h"

uint8_t Credentials_PasswordEquals(const uint8_t *left, const uint8_t *right)
{
    uint8_t i, difference = 0;
    if (left == 0 || right == 0) return 0;
    for (i = 0; i < PASSWORD_LEN; i++) difference |= left[i] ^ right[i];
    return difference == 0;
}

uint8_t Credentials_IsValid(const CredentialConfig *config)
{
    uint8_t i;
    if (config == 0) return 0;
    for (i = 0; i < PASSWORD_LEN; i++) {
        if (config->password[i] < 1U || config->password[i] > 9U ||
            config->duress_password[i] < 1U || config->duress_password[i] > 9U) return 0;
    }
    return !Credentials_PasswordEquals(config->password, config->duress_password) &&
        DialSequence_IsValid(&config->dial) && DialSequence_IsValid(&config->duress_dial) &&
        !DialSequence_Equals(&config->dial, &config->duress_dial);
}
