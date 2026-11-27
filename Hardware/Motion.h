#ifndef __MOTION_H
#define __MOTION_H

#include <stdint.h>

#define MOTION_DEFAULT_RPM 40u
#define MOTION_MIN_RPM     20u
#define MOTION_MAX_RPM     80u

typedef enum
{
    MOTION_STOP = 0,
    MOTION_FORWARD,
    MOTION_BACKWARD,
    MOTION_LEFT,
    MOTION_RIGHT,
    MOTION_CW,
    MOTION_CCW
} MotionId;

/* Top view: A=front left, B=front right, C=rear left, D=rear right.
 * Wheel axles point radially; front bisects A/B. Positive wheel speed
 * drives the chassis along the counterclockwise tangent at that wheel.
 * The wheel RPM parameter is a magnitude, not chassis linear/angular speed.
 */
void Motion_GetTargets(MotionId motion, uint16_t rpm, int16_t targets[4]);
const char *Motion_Name(MotionId motion);
/* Movement commands only. No output mutation on invalid input. */
uint8_t Motion_Parse(const char *line, MotionId *motion, uint16_t *rpm);

#endif
