#ifndef __BOARD_H
#define __BOARD_H

#include "stm32f4xx.h"

void Board_Init(void);
void Board_SetLed(uint8_t on);
uint8_t Board_IsEnabled(void);

#endif
