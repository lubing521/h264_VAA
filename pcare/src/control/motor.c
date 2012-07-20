/*
 *  motor.c
 *  control DC and STEPPER motor
 *  zhangjunwei166@163.com
 */

#include <stdlib.h> 
#include <stdio.h>
#include <pthread.h>
#include <fcntl.h>              
#include <unistd.h>
#include <errno.h>
#include <unistd.h>
#include <semaphore.h>
#include <sys/types.h> 
#include <sys/stat.h> 
#include <malloc.h>
#include <sys/mman.h>
#include <signal.h>
#include "motor_driver.h"
#include "motor.h"
#include "types.h"

#define DC_MOTOR_DEV  "/dev/pwm"
#define STEPPER_MOTOR_DEV  "/dev/steppermotor"


int oflags;
int dc_motor_fd;
int stepper_motor_fd;
static pthread_t  id1,id2;
int record_count;
int playback_count;
//sem_t mutex;
u8 ctrl_motor_flag;
u8 record_flag,record_full,playback_flag; 
u8 i,j,m,n;
u8 r_cmd[R_CMD_L]={0};
u8 cmd[CMD_L][CMD_W]={0};
u8 cmd_step[CMD_L][CMD_W]={0};
u8 k_step;

pthread_mutex_t mutex_dc = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mutex_step = PTHREAD_MUTEX_INITIALIZER;
/******************************************************/
/**********************open motors*********************/
/******************************************************/
int init_motor(void)
{
	int ret;
	dc_motor_fd = open(DC_MOTOR_DEV,O_RDWR);
    if (dc_motor_fd < 0)
    {
    	printf("open /dev/pwm failed.\n");
        return 0;
    }
	stepper_motor_fd = open(STEPPER_MOTOR_DEV,O_RDWR);
    if (stepper_motor_fd < 0)
    {
            printf("open /dev/steppermotor failed.\n");
            return 0;
    }
	pthread_mutex_init(&mutex_dc,NULL);
	pthread_mutex_init(&mutex_step,NULL);
    ret = pthread_create(&id1,NULL,(void *)operate,NULL);
    if (ret != 0)
            printf("create operate thread error\n");
	/* this thread for control stepper motor */
	ret = pthread_create(&id2,NULL,(void *)operate_stepper,NULL); 
    if (ret != 0)
           printf("create operate_stepper thread error\n");
	/**********************************************/
//	signal(SIGIO, stepper_down);
//	fcntl(stepper_motor_fd, F_SETOWN, getpid());	
//	oflags = fcntl(stepper_motor_fd, F_GETFL);
//	fcntl(stepper_motor_fd, F_SETFL, oflags | FASYNC);
	/***********************************************/
	return 1; 
}

/*******************************************************/
/************the interface of motor control*************/
/*******************************************************/
int ctrl_motor(motor_ctrl_t *opt)
{
	int size;
	ctrl_motor_flag = 1;
	if (pthread_mutex_lock(&mutex_dc) != 0)
		perror("pthread_lock mutex_dc");
//	else
//		printf("add mutex_dc ok !\n");
	if (pthread_mutex_lock(&mutex_step) != 0)
		perror("pthread_lock mutex_step");
	cmd[i][0] = opt->opt_code;
 	cmd[i][1] = opt->param[0];
	cmd[i][2] = opt->param[1];	
	cmd_step[i][0] = opt->opt_code;
 	cmd_step[i][1] = opt->param[0];
	cmd_step[i][2] = opt->param[1];	
	i++;	
	if (i >= CMD_L)
		i = 0;
	if (pthread_mutex_unlock(&mutex_dc) != 0)
		perror("pthread_unlock mutex_dc");
//	else
//		printf("unlock mutex_dc ok!\n");
	if (pthread_mutex_unlock(&mutex_step) != 0)
		perror("pthread_unlock mutex_step");
	return 1;	
}


/*******************************************************/
/********************close motors***********************/
/*******************************************************/
int close_motor(void)
{
	if (dc_motor_fd < 0)
		printf("this is no need close dc motor\n");
	else
		close(dc_motor_fd);
	if (stepper_motor_fd < 0)
		printf("this is no need close stepper motor\n");
	else
		close(stepper_motor_fd);
	return 1;
}
