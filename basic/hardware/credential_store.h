#ifndef LOCKING_CREDENTIAL_STORE_H
#define LOCKING_CREDENTIAL_STORE_H

#include "credentials.h"

typedef enum { CREDENTIAL_STORE_OK, CREDENTIAL_STORE_EMPTY,
               CREDENTIAL_STORE_INVALID, CREDENTIAL_STORE_IO_ERROR } CredentialStoreResult;

/* Separate double-page journal. Load never initializes/erases existing data. */
CredentialStoreResult CredentialStore_Load(CredentialConfig *config);
CredentialStoreResult CredentialStore_Save(const CredentialConfig *config);

#endif
