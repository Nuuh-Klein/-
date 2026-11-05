#ifndef LOCKING_INTERACTION_DEMO_H
#define LOCKING_INTERACTION_DEMO_H

#include <stdint.h>
#include "input_event.h"

void InteractionDemo_Init(void);
void InteractionDemo_Update(uint8_t key);
/* Drain after Update, then pass SUBMITTED snapshots to the future auth module. */
uint8_t InteractionDemo_TakeEvent(InputEvent *event);

#endif
