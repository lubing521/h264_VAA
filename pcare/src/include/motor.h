/*
 *  motor.h
 *  control DC and stepper motor
 *  zhangjunwei166@163.com
 */

#ifndef _MOTOR_H
#define _MOTOR_H

#include "types.h"


#define STEPPER_DELAY 2000
#define STEPER_PHASE_UP 225
#define STEPER_PHASE_LEFT 620
#define SMUPDOWN_CONFIG_UP 1
#define SMUPDOWN_CONFIG_DOWN 21
#define SMUPDOWN_POWER 3
#define SMUPDOWN_UP_IN_PLACE 6
#define SMUPDOWN_DOWN_IN_PLACE 8
#define SMLEFTRIGHT_CONFIG_LEFT 4
#define SMLEFTRIGHT_CONFIG_RIGHT 24
#define SMLEFTRIGHT_POWER 5
#define SMLEFTRIGHT_LEFT_IN_PLACE 9
#define SMLEFTRIGHT_RIGHT_IN_PLACE 10

#define UPDOWN_SMID 0
#define LEFTRIGHT_SMID 1


typedef struct {
	u8 opt_code;
	u8 param[2];
} motor_ctrl_t;

int init_motor(void);
int close_motor(void);
extern void DispatchMotorOp( motor_ctrl_t * );
extern int GetNextOp( int );
extern void InitMotors( );

#endif
