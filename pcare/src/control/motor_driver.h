/*
 *  motor_driver.h
 *  interface of motor ctrl
 *  zhangjunwei166@163.com
 */

#ifndef _MOTOR_DRIVER_h
#define _MOTOR_DRIVER_h

#include "types.h"

/*******************STEPPER M****************************/
void stepper_motor_updown();
void stepper_motor_leftright();
void out_high(void);
void out_low(void);
enum MotorOp
{
	MOTOR_NOOP = -1,
	MOTOR_STOP = 0,
	MOTOR_FORWARD,
	MOTOR_BACKWARD,
	MOTOR_CRUISE
};
#endif

