#include "Motor.h"
#include "PWM.h"
#include "BoardConfig.h"

static int8_t Motor_GetSign(MotorId motor)
{
    switch (motor)
    {
        case MOTOR_A: return MOTOR_SIGN_A;
        case MOTOR_B: return MOTOR_SIGN_B;
        case MOTOR_C: return MOTOR_SIGN_C;
        case MOTOR_D: return MOTOR_SIGN_D;
        default:      return 1;
    }
}

static void Motor_GetChannels(MotorId motor,
                              PWM_Channel *positive,
                              PWM_Channel *negative)
{
    switch (motor)
    {
        case MOTOR_A:
            *positive = PWM_CH_A_POS;
            *negative = PWM_CH_A_NEG;
            break;
        case MOTOR_B:
            *positive = PWM_CH_B_POS;
            *negative = PWM_CH_B_NEG;
            break;
        case MOTOR_C:
            *positive = PWM_CH_C_POS;
            *negative = PWM_CH_C_NEG;
            break;
        default:
            *positive = PWM_CH_D_POS;
            *negative = PWM_CH_D_NEG;
            break;
    }
}

void Motor_Init(void)
{
    PWM_Init();
    Motor_StopAll();
}

void Motor_SetCommand(MotorId motor, int16_t input_command)
{
    PWM_Channel positive;
    PWM_Channel negative;
    int32_t command = input_command;

    Motor_GetChannels(motor, &positive, &negative);
    command *= Motor_GetSign(motor);

    if (command > MOTOR_MAX_CMD)
    {
        command = MOTOR_MAX_CMD;
    }
    if (command < -MOTOR_MAX_CMD)
    {
        command = -MOTOR_MAX_CMD;
    }

    if ((command > -MOTOR_DEADBAND) && (command < MOTOR_DEADBAND))
    {
        command = 0;
    }

    if (command > 0)
    {
        PWM_SetDuty(positive, (uint16_t)command);
        PWM_SetDuty(negative, 0u);
    }
    else if (command < 0)
    {
        PWM_SetDuty(positive, 0u);
        PWM_SetDuty(negative, (uint16_t)(-command));
    }
    else
    {
        PWM_SetDuty(positive, 0u);
        PWM_SetDuty(negative, 0u);
    }
}

void Motor_StopAll(void)
{
    MotorId motor;

    for (motor = MOTOR_A; motor <= MOTOR_D; motor++)
    {
        Motor_SetCommand(motor, 0);
    }
}
