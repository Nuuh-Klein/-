#ifndef LOCKING_PATTERN_APP_H
#define LOCKING_PATTERN_APP_H

#include <stdint.h>

void PatternApp_Init(uint32_t now);
/* Call frequently; all timed feedback and lockout use elapsed milliseconds. */
void PatternApp_Update(uint32_t now, uint8_t key);

#endif
