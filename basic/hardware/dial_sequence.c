#include "dial_sequence.h"

void DialSequence_Clear(DialSequence *sequence)
{
    uint8_t i;
    sequence->length = sequence->invalid = sequence->position = 0;
    for (i = 0; i < DIAL_SEGMENTS; i++) {
        sequence->direction[i] = 0;
        sequence->steps[i] = 0;
    }
}

uint8_t DialSequence_Append(DialSequence *sequence, int8_t direction)
{
    uint8_t index;
    if (sequence->invalid) return 0;
    if (direction != 1 && direction != -1) return 0;
    if (sequence->length == 0 || sequence->direction[sequence->length - 1U] != direction) {
        if (sequence->length == DIAL_SEGMENTS) { sequence->invalid = 1; return 0; }
        index = sequence->length++;
        sequence->direction[index] = direction;
    } else index = sequence->length - 1U;
    if (sequence->steps[index] == DIAL_MAX_STEPS) { sequence->invalid = 1; return 0; }
    sequence->steps[index]++;
    sequence->position = (uint8_t)((sequence->position + (direction > 0 ? 1U : 11U)) % 12U);
    return 1;
}

uint8_t DialSequence_MatchesDemo(const DialSequence *sequence)
{
    return DialSequence_IsValid(sequence) &&
        sequence->direction[0] == 1 && sequence->steps[0] == 3U &&
        sequence->direction[1] == -1 && sequence->steps[1] == 2U &&
        sequence->direction[2] == 1 && sequence->steps[2] == 4U;
}

uint8_t DialSequence_IsValid(const DialSequence *sequence)
{
    uint8_t i;
    if (sequence == 0 || sequence->invalid || sequence->length != DIAL_SEGMENTS) return 0;
    for (i = 0; i < DIAL_SEGMENTS; i++) {
        if ((sequence->direction[i] != 1 && sequence->direction[i] != -1) ||
            sequence->steps[i] == 0U || sequence->steps[i] > DIAL_MAX_STEPS) return 0;
        if (i != 0U && sequence->direction[i] == sequence->direction[i - 1U]) return 0;
    }
    return 1;
}

uint8_t DialSequence_Equals(const DialSequence *left, const DialSequence *right)
{
    uint8_t i;
    if (!DialSequence_IsValid(left) || !DialSequence_IsValid(right)) return 0;
    for (i = 0; i < DIAL_SEGMENTS; i++) {
        if (left->direction[i] != right->direction[i] || left->steps[i] != right->steps[i]) return 0;
    }
    return 1;
}
