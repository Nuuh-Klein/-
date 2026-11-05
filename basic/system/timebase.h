#ifndef LOCKING_TIMEBASE_H
#define LOCKING_TIMEBASE_H

#include <stdint.h>

/* TIM3 supplies a monotonic millisecond clock; SysTick remains owned by Delay/AS608. */
void Timebase_Init(void);
uint32_t Timebase_Millis(void);

#endif
