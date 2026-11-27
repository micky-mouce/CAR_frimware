#ifndef __BOARDCONFIG_H
#define __BOARDCONFIG_H

/*
 * Radial 4-omni layout, TOP VIEW (front is BETWEEN A and B):
 *                  FRONT
 *              A           B
 *                    +
 *              C           D
 * A/B/C/D connect to the correspondingly labelled board motor sockets.
 * Positive wheel RPM drives the chassis along the CCW tangent at that wheel.
 * Raw electrical polarities cannot be inferred from a chassis photo.
 * These +1 values are starting values; verify motor AND encoder polarity.
 * If positive PWM drives a wheel along the CW tangent, change MOTOR_SIGN.
 * If CCW-chassis wheel rotation reads negative, change ENCODER_SIGN.
 * The two signs are calibrated independently, not blindly flipped together.
 * MOTOR_SIGN reverses the bridge output, ENCODER_SIGN corrects the measured
 * speed polarity used by PID.
 */
#define MOTOR_SIGN_A       1
#define MOTOR_SIGN_B       1
#define MOTOR_SIGN_C       1
#define MOTOR_SIGN_D       1

#define ENCODER_SIGN_A     1
#define ENCODER_SIGN_B     1
#define ENCODER_SIGN_C     1
#define ENCODER_SIGN_D     1

#define ENCODER_PPR        11.0f
#define GEAR_RATIO         34.0f

#endif
