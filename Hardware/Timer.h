#ifndef __TIMER_H
#define __TIMER_H

#include "stm32f4xx.h"

#define CONTROL_PERIOD_MS   50u

extern volatile uint8_t  speed_update_flag;

void Timer_Init(void);
uint32_t Timer_GetMs(void);

#endif
