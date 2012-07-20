/*
 *  motor_driver.h
 *  interface of motor ctrl
 *  zhangjunwei166@163.com
 */

#ifndef _MOTOR_DRIVER_h
#define _MOTOR_DRIVER_h

#include "types.h"
/***************DC M********************************/
int operate(void *arg);
int operate_stepper(void *arg);
void operate_motor(u8 param1, u8 param2);
//void operate_stepper_motor(u8 param1);
void motor1_stop(void);
void motor1_advance(u8 param1);
void motor1_retreat(u8 param1);
void motor2_stop(void);
void motor2_advance(u8 param1);
void motor2_retreat(u8 param1);
void track_record(u8 param1);
void track_playback(u8 param1);
void led_darts_on(u8 param1);
void led_darts_off(u8 param1);
void m1advance_m2advance(u8 param1);
void m1advance_m2retreat(u8 param1);
void m1retreat_m2advance(u8 param1);
void m1retreat_m2retreat(u8 param1);

/*******************STEPPER M****************************/
void operate_stepper_motor(u8 param1);
void control_stepper_motor(u8 param1);
void stepper_up(int param);
void stepper_upstop(void);
void stepper_down(int param);
void stepper_downstop(void);
void stepper_right(int param);
void stepper_rightstop(void);
void stepper_left(int param);
void stepper_leftstop(void);
void stepper_mid(int param);
void cruise_updown(u8 param1,int param2);
void stop_updown_cruise();
void cruise_rightleft(u8 param1,int param2);
void stop_rightleft_cruise(void);
void left_up(int param);
void right_up(int param);
void left_down(int param);
void right_down(int param);
void out_high(void);
void out_low(void);
#endif

