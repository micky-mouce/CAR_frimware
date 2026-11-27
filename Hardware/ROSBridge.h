#ifndef __ROSBRIDGE_H
#define __ROSBRIDGE_H

#include "stm32f4xx.h"
#include "Motor.h"
#include "Motion.h"

#define ROS_DEFAULT_TARGET_RPM   MOTION_DEFAULT_RPM
#define ROS_MIN_TARGET_RPM       MOTION_MIN_RPM
#define ROS_MAX_TARGET_RPM       MOTION_MAX_RPM
#define ROS_COMMAND_TIMEOUT_MS   500u

typedef enum
{
    ROS_CMD_NONE = 0,
    ROS_CMD_RUN,
    ROS_CMD_STOP
} ROS_CommandType;

typedef struct
{
    ROS_CommandType command;
    MotionId motion;
    uint16_t target_rpm;
} ROS_Command;

typedef struct
{
    uint8_t  running;
    MotionId motion;
    int16_t  wheel_target[MOTOR_COUNT];
    uint16_t target_rpm;
    int16_t  actual_rpm[MOTOR_COUNT];
    int16_t  pwm[MOTOR_COUNT];
    uint32_t age_ms;
    uint8_t  command_valid;
    const char *reason;
} ROS_Status;

void ROSBridge_Init(void);
uint8_t ROSBridge_PollCommand(ROS_Command *command);
uint8_t ROSBridge_HasValidCommand(void);
uint32_t ROSBridge_GetCommandAgeMs(void);

void ROSBridge_SendReady(void);
void ROSBridge_SendStatus(const ROS_Status *status);

#endif
