#ifndef LOCKING_PATTERN_STORE_H
#define LOCKING_PATTERN_STORE_H

#include "pattern.h"
#include "stm32f10x_flash.h"

typedef enum {
    PATTERN_STORE_EMPTY,
    PATTERN_STORE_OK,
    PATTERN_STORE_INVALID
} PatternStoreResult;

/* Read-only: boot never erases or initializes existing Flash. */
PatternStoreResult PatternStore_Load(Pattern *pattern);
FLASH_Status PatternStore_Save(const Pattern *pattern);

#endif
