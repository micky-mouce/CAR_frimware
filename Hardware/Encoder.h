#ifndef __ENCODER_H
#define __ENCODER_H

#include "stm32f4xx.h"

void Encoder_Init(void);
int16_t Encoder_GetDelta(uint8_t encoder_index);
float Encoder_GetRPM(uint8_t encoder_index, uint16_t period_ms);

#endif
