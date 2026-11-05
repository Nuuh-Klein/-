#ifndef LOCKING_LOG_DEMO_H
#define LOCKING_LOG_DEMO_H

#include <stdint.h>

void LogDemo_Start(void);
void LogDemo_StartReadOnly(void);
void LogDemo_Update(uint8_t key);
uint8_t LogDemo_IsActive(void);

#endif
