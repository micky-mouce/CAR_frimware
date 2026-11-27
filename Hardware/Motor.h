#ifndef __MOTOR_H
#define __MOTOR_H

#include "stm32f4xx.h"

#define MOTOR_COUNT       4u
#define MOTOR_MAX_CMD     8000
#define MOTOR_DEADBAND    60

typedef enum
{
    MOTOR_A = 0, /* front left */
    MOTOR_B,     /* front right */
    MOTOR_C,     /* rear left */
    MOTOR_D      /* rear right */
} MotorId;

void Motor_Init(void);
void Motor_SetCommand(MotorId motor, int16_t command);
void Motor_StopAll(void);

#endif
