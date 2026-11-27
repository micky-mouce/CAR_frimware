#ifndef __PID_H
#define __PID_H

#include "stm32f4xx.h"

typedef struct
{
    float kp;
    float ki;
    float kd;
    float target;
    float integral;
    float last_error;
    float out_max;
    float integral_max;
    float integral_zone;
} PID_Controller;

void PID_Init(PID_Controller *pid);
void PID_SetParam(PID_Controller *pid, float kp, float ki, float kd);
void PID_SetTarget(PID_Controller *pid, float target);
void PID_Reset(PID_Controller *pid);
float PID_Calc(PID_Controller *pid, float actual);

#endif
