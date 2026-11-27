#ifndef __PWM_H
#define __PWM_H

#include "stm32f4xx.h"

typedef enum
{
    PWM_CH_A_POS = 0,
    PWM_CH_A_NEG,
    PWM_CH_B_POS,
    PWM_CH_B_NEG,
    PWM_CH_C_POS,
    PWM_CH_C_NEG,
    PWM_CH_D_POS,
    PWM_CH_D_NEG
} PWM_Channel;

void PWM_Init(void);
void PWM_SetDuty(PWM_Channel channel, uint16_t duty);

#endif
