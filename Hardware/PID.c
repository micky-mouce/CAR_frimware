#include "PID.h"

void PID_Init(PID_Controller *pid)
{
    pid->kp = 1.0f;
    pid->ki = 0.0f;
    pid->kd = 0.0f;
    pid->target = 0.0f;
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
    pid->out_max = 8000.0f;
    pid->integral_max = 5000.0f;
    pid->integral_zone = 100.0f;
}

void PID_SetParam(PID_Controller *pid, float kp, float ki, float kd)
{
    pid->kp = kp;
    pid->ki = ki;
    pid->kd = kd;
    PID_Reset(pid);
}

void PID_SetTarget(PID_Controller *pid, float target)
{
    pid->target = target;
    PID_Reset(pid);
}

void PID_Reset(PID_Controller *pid)
{
    pid->integral = 0.0f;
    pid->last_error = 0.0f;
}

float PID_Calc(PID_Controller *pid, float actual)
{
    float error = pid->target - actual;
    float derivative = error - pid->last_error;
    float output;

    if ((error > -pid->integral_zone) && (error < pid->integral_zone))
    {
        pid->integral += error;
        if (pid->integral > pid->integral_max)
        {
            pid->integral = pid->integral_max;
        }
        if (pid->integral < -pid->integral_max)
        {
            pid->integral = -pid->integral_max;
        }
    }

    pid->last_error = error;
    output = (pid->kp * error) + (pid->ki * pid->integral) +
             (pid->kd * derivative);

    if (output > pid->out_max)
    {
        output = pid->out_max;
    }
    if (output < -pid->out_max)
    {
        output = -pid->out_max;
    }

    return output;
}
