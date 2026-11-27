#include "stm32f4xx.h"
#include "Board.h"
#include "Timer.h"
#include "Motor.h"
#include "Encoder.h"
#include "PID.h"
#include "ROSBridge.h"
#include "OLED.h"

#define STATUS_PERIOD_MS        100u

#define PID_KP                  25.0f
#define PID_KI                  0.8f
#define PID_KD                  2.0f

typedef enum
{
    DRIVE_STOP = 0,
    DRIVE_RUN
} DriveState;

static DriveState drive_state = DRIVE_STOP;
static const char *stop_reason = "BOOT";
static uint16_t target_rpm = ROS_DEFAULT_TARGET_RPM;
static MotionId motion = MOTION_STOP;
static int16_t wheel_target[MOTOR_COUNT] = {0, 0, 0, 0};
static uint8_t settle_ticks = 0u;
static int16_t actual_rpm[MOTOR_COUNT] = {0, 0, 0, 0};
static int16_t motor_pwm[MOTOR_COUNT] = {0, 0, 0, 0};
static PID_Controller wheel_pid[MOTOR_COUNT];
static uint8_t status_counter = 0u;
static uint16_t led_counter = 0u;
static uint8_t oled_counter = 0u;

static void Drive_EnterState(DriveState new_state, const char *reason);
static void Drive_ApplyTarget(MotionId new_motion, uint16_t new_target);
static void Drive_UpdateLeds(void);
static void OLED_UpdateDisplay(void);

int main(void)
{
    ROS_Command command;
    uint8_t index;

    Board_Init();
    Timer_Init();
    Motor_Init();
    OLED_Init();
    Encoder_Init();
    ROSBridge_Init();

    for (index = 0u; index < MOTOR_COUNT; index++)
    {
        PID_Init(&wheel_pid[index]);
        PID_SetParam(&wheel_pid[index], PID_KP, PID_KI, PID_KD);
        PID_SetTarget(&wheel_pid[index], (float)wheel_target[index]);
    }

    Motor_StopAll();
    drive_state = DRIVE_STOP;
    stop_reason = "BOOT";
    Board_SetLed(0u);
    OLED_UpdateDisplay();
    ROSBridge_SendReady();

    while (1)
    {
        while (ROSBridge_PollCommand(&command))
        {
            if (command.command == ROS_CMD_RUN)
            {
                if (Board_IsEnabled() == 0u)
                {
                    if (drive_state != DRIVE_STOP)
                    {
                        Drive_EnterState(DRIVE_STOP, "HW_DISABLED");
                    }
                    else
                    {
                        stop_reason = "HW_DISABLED";
                    }
                }
                else
                {
                    Drive_ApplyTarget(command.motion, command.target_rpm);
                    if (drive_state != DRIVE_RUN)
                    {
                        Drive_EnterState(DRIVE_RUN, "ROS_RUN");
                    }
                }
            }
            else if (command.command == ROS_CMD_STOP)
            {
                if (drive_state != DRIVE_STOP)
                {
                    Drive_EnterState(DRIVE_STOP, "ROS_STOP");
                }
                else
                {
                    stop_reason = "ROS_STOP";
                }
            }
        }

        if (speed_update_flag)
        {
            speed_update_flag = 0u;

            if (drive_state == DRIVE_RUN)
            {
                uint8_t fault = 0u;
                if (Board_IsEnabled() == 0u)
                {
                    Drive_EnterState(DRIVE_STOP, "HW_DISABLED");
                    fault = 1u;
                }
                else if ((ROSBridge_HasValidCommand() == 0u) ||
                         (ROSBridge_GetCommandAgeMs() >=
                          ROS_COMMAND_TIMEOUT_MS))
                {
                    Drive_EnterState(DRIVE_STOP, "TIMEOUT");
                    fault = 1u;
                }

                if (fault == 0u)
                {
                    if (settle_ticks > 0u)
                    {
                        Motor_StopAll();
                    }
                    for (index = 0u; index < MOTOR_COUNT; index++)
                    {
                        float rpm = Encoder_GetRPM(index, CONTROL_PERIOD_MS);
                        float output;

                        actual_rpm[index] = (int16_t)(rpm +
                            (rpm >= 0.0f ? 0.5f : -0.5f));
                        output = (settle_ticks != 0u) ? 0.0f :
                                 PID_Calc(&wheel_pid[index], rpm);
                        motor_pwm[index] = (int16_t)output;
                        Motor_SetCommand((MotorId)index, motor_pwm[index]);
                    }
                    if (settle_ticks > 0u) settle_ticks--;
                }
            }

            if (drive_state == DRIVE_STOP)
            {
                for (index = 0u; index < MOTOR_COUNT; index++)
                {
                    float rpm = Encoder_GetRPM(index, CONTROL_PERIOD_MS);
                    actual_rpm[index] = (int16_t)(rpm + (rpm >= 0.0f ? 0.5f : -0.5f));
                    motor_pwm[index] = 0;
                }
                Motor_StopAll();
            }

            status_counter++;
            if (status_counter >= (STATUS_PERIOD_MS / CONTROL_PERIOD_MS))
            {
                ROS_Status feedback;

                status_counter = 0u;
                feedback.running = (uint8_t)(drive_state == DRIVE_RUN);
                feedback.target_rpm = (drive_state == DRIVE_RUN) ? target_rpm : 0u;
                feedback.motion = (drive_state == DRIVE_RUN) ? motion : MOTION_STOP;
                feedback.age_ms = ROSBridge_GetCommandAgeMs();
                feedback.command_valid = ROSBridge_HasValidCommand();
                feedback.reason = (drive_state == DRIVE_RUN) ?
                                  "ROS_RUN" : stop_reason;

                for (index = 0u; index < MOTOR_COUNT; index++)
                {
                    feedback.wheel_target[index] = (drive_state == DRIVE_RUN) ? wheel_target[index] : 0;
                    feedback.actual_rpm[index] = actual_rpm[index];
                    feedback.pwm[index] = motor_pwm[index];
                }
                ROSBridge_SendStatus(&feedback);
            }

            Drive_UpdateLeds();

            oled_counter++;
            if (oled_counter >= 4u)
            {
                oled_counter = 0u;
                OLED_UpdateDisplay();
            }
        }
    }
}

static void Drive_EnterState(DriveState new_state, const char *reason)
{
    uint8_t index;

    drive_state = new_state;

    if (drive_state == DRIVE_RUN)
    {
        for (index = 0u; index < MOTOR_COUNT; index++)
        {
            PID_SetTarget(&wheel_pid[index], (float)wheel_target[index]);
        }
        Board_SetLed(1u);
        stop_reason = "ROS_RUN";
    }
    else
    {
        Motor_StopAll();
        motion = MOTION_STOP;
        settle_ticks = 0u;
        for (index = 0u; index < MOTOR_COUNT; index++)
        {
            PID_Reset(&wheel_pid[index]);
            wheel_target[index] = 0;
            motor_pwm[index] = 0;
        }
        stop_reason = reason;
    }
}

static void Drive_ApplyTarget(MotionId new_motion, uint16_t new_target)
{
    uint8_t index;
    if ((new_motion == motion) && (new_target == target_rpm)) return;

    /* Stop before switching direction; leave one complete control tick off.
     * Repeated identical heartbeat commands do not reset PID or this guard.
     */
    if (new_motion != motion)
    {
        Motor_StopAll();
        settle_ticks = 1u;
        for (index = 0u; index < MOTOR_COUNT; index++) motor_pwm[index] = 0;
    }
    motion = new_motion;
    target_rpm = new_target;
    Motion_GetTargets(motion, target_rpm, wheel_target);
    for (index = 0u; index < MOTOR_COUNT; index++)
    {
        PID_SetTarget(&wheel_pid[index], (float)wheel_target[index]);
    }
}

static void Drive_UpdateLeds(void)
{
    if (drive_state == DRIVE_RUN)
    {
        Board_SetLed(1u);
        led_counter = 0u;
        return;
    }

    led_counter++;
    if (led_counter >= 10u)
    {
        led_counter = 0u;
        Board_SetLed((uint8_t)((Timer_GetMs() / 500u) & 1u));
    }
}

static void OLED_UpdateDisplay(void)
{
    const char *state_text;
    const char *reason_text;
    uint32_t age_ms = ROSBridge_GetCommandAgeMs();

    if (drive_state == DRIVE_RUN)
    {
        state_text = "RUN";
    }
    else if ((stop_reason[0] == 'T') && (stop_reason[1] == 'I'))
    {
        state_text = "TMO";
    }
    else if (stop_reason[0] == 'H')
    {
        state_text = "HLT";
    }
    else
    {
        state_text = "STP";
    }

    if (age_ms > 999u)
    {
        age_ms = 999u;
    }

    if (stop_reason[0] == 'T')
    {
        reason_text = "TIMEOUT";
    }
    else if (stop_reason[0] == 'H')
    {
        reason_text = "HW DISABLED";
    }
    else if (stop_reason[0] == 'B')
    {
        reason_text = "BOOT";
    }
    else
    {
        reason_text = "ROS STOP";
    }

    OLED_Clear();

    OLED_ShowString(1, 1, "4WD ");
    OLED_ShowString(1, 5, (char *)state_text);

    OLED_ShowString(2, 1, (char *)Motion_Name(motion));
    OLED_ShowNum(2, 8, target_rpm, 3);

    OLED_ShowString(3, 1, "RPM A ");
    OLED_ShowSignedNum(3, 7, actual_rpm[0], 2);
    OLED_ShowString(4, 1, "RPM B ");
    OLED_ShowSignedNum(4, 7, actual_rpm[1], 2);
    OLED_ShowString(5, 1, "RPM C ");
    OLED_ShowSignedNum(5, 7, actual_rpm[2], 2);
    OLED_ShowString(6, 1, "RPM D ");
    OLED_ShowSignedNum(6, 7, actual_rpm[3], 2);

    OLED_ShowString(7, 1, "CMD AGE ");
    OLED_ShowNum(7, 9, age_ms, 3);

    OLED_ShowString(8, 1, "REASON ");
    OLED_ShowString(8, 8, (char *)((drive_state == DRIVE_RUN) ?
                                   "ROS RUN" : reason_text));

    OLED_Refresh();
}
