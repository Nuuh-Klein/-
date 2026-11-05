#include "pattern.h"

void Pattern_Clear(Pattern *pattern)
{
    uint8_t i;
    if (pattern == 0) return;
    pattern->length = 0;
    for (i = 0; i < PATTERN_MAX_NODES; i++) pattern->nodes[i] = 0;
}

uint8_t Pattern_Append(Pattern *pattern, uint8_t node)
{
    uint8_t i;
    if (pattern == 0 || node < 1U || node > 9U ||
        pattern->length >= PATTERN_MAX_NODES) return 0;
    for (i = 0; i < pattern->length; i++) {
        if (pattern->nodes[i] == node) return 0;
    }
    pattern->nodes[pattern->length++] = node;
    return 1;
}

uint8_t Pattern_IsValid(const Pattern *pattern)
{
    uint8_t i, j;
    if (pattern == 0 || pattern->length < PATTERN_MIN_NODES ||
        pattern->length > PATTERN_MAX_NODES) return 0;
    for (i = 0; i < pattern->length; i++) {
        if (pattern->nodes[i] < 1U || pattern->nodes[i] > 9U) return 0;
        for (j = 0; j < i; j++) {
            if (pattern->nodes[i] == pattern->nodes[j]) return 0;
        }
    }
    return 1;
}

uint8_t Pattern_Equals(const Pattern *left, const Pattern *right)
{
    uint8_t i;
    if (!Pattern_IsValid(left) || !Pattern_IsValid(right) ||
        left->length != right->length) return 0;
    for (i = 0; i < left->length; i++) {
        if (left->nodes[i] != right->nodes[i]) return 0;
    }
    return 1;
}
