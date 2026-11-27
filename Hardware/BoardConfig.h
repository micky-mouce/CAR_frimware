#ifndef __BOARDCONFIG_H
#define __BOARDCONFIG_H

/*
 * Change one sign if a wheel rotates opposite to the required direction.
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
