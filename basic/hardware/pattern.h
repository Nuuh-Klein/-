#ifndef LOCKING_PATTERN_H
#define LOCKING_PATTERN_H

#include <stdint.h>

#define PATTERN_MIN_NODES 4U
#define PATTERN_MAX_NODES 9U

typedef struct {
    uint8_t length;
    uint8_t nodes[PATTERN_MAX_NODES];
} Pattern;

void Pattern_Clear(Pattern *pattern);
/* Only the pressed node is appended. Repeated/out-of-range nodes are ignored. */
uint8_t Pattern_Append(Pattern *pattern, uint8_t node);
uint8_t Pattern_IsValid(const Pattern *pattern);
uint8_t Pattern_Equals(const Pattern *left, const Pattern *right);

#endif
