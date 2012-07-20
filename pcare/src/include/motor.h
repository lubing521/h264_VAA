/*
 *  motor.h
 *  control DC and stepper motor
 *  zhangjunwei166@163.com
 */

#ifndef _MOTOR_H
#define _MOTOR_H

#include "types.h"

#define CMD_L 100
#define CMD_W  3
#define OPT_DC 250
#define OPT_STEP 14
#define R_CMD_L  99

typedef struct {
	u8 opt_code;
	u8 param[2];
} motor_ctrl_t;

int init_motor(void);
int ctrl_motor(motor_ctrl_t *opt);
int close_motor(void);

#endif
