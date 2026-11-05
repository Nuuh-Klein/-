#ifndef LOCKING_DIAL_SEQUENCE_H
#define LOCKING_DIAL_SEQUENCE_H

#include <stdint.h>

#define DIAL_SEGMENTS 3U
#define DIAL_MAX_STEPS 99U

typedef struct {
    uint8_t length;
    uint8_t invalid;
    int8_t direction[DIAL_SEGMENTS];
    uint8_t steps[DIAL_SEGMENTS];
    uint8_t position;
} DialSequence;

void DialSequence_Clear(DialSequence *sequence);
uint8_t DialSequence_Append(DialSequence *sequence, int8_t direction);
uint8_t DialSequence_IsValid(const DialSequence *sequence);
/* Compare credential directions/counts; the presentation pointer is ignored. */
uint8_t DialSequence_Equals(const DialSequence *left, const DialSequence *right);
uint8_t DialSequence_MatchesDemo(const DialSequence *sequence);

#endif
