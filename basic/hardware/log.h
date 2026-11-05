#ifndef LOCKING_LOG_H
#define LOCKING_LOG_H

#include <stdint.h>

typedef enum {
    LOG_METHOD_PASSWORD = 1,
    LOG_METHOD_DIAL = 2,
    LOG_METHOD_FINGER = 3,
    LOG_METHOD_TEST = 5,    /* Explicit diagnostics only; never emitted by auth. */
    LOG_METHOD_PATTERN = 2, /* Legacy source alias; new records use DIAL. */
    LOG_METHOD_DURESS = 4   /* Legacy Log_Write alias: numeric + duress. */
} Log_Method_t;

typedef enum { LOG_RESULT_FAIL = 0, LOG_RESULT_PASS = 1 } Log_Result_t;

typedef struct {
    uint32_t serial;
    uint16_t method;
    uint16_t result;
    uint8_t duress;
} Log_Record_t;

/* Read-only scan. Bad/partial records are skipped, never erased at boot. */
uint8_t Log_Init(void);
uint8_t Log_WriteEvent(uint16_t method, uint16_t result, uint8_t duress);
/* Compatibility wrapper. The persistent format is always the new format. */
uint8_t Log_Write(uint16_t method, uint16_t result);
uint16_t Log_GetCount(void);
uint16_t Log_GetInvalidCount(void);
uint8_t Log_Read(uint16_t recent_index, Log_Record_t *record);

#endif
