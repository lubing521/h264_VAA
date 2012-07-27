/*
 *  motor_driver.c
 *  interfaces
 *  zhangjunwei166@163.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>             
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>         
#include "motor_driver.h"
#include "motor.h"
#include "types.h"

#define DC_MOTOR_DEV  "/dev/pwm"
#define STEPPER_MOTOR_DEV  "/dev/steppermotor"

static int count_m1_up=0,count_m2_right=0,count_m1_down=0,count_m2_left=0;
u32 sm_phase[]={0x7,0x3,0xB,0x9,0xD,0xC,0xE,0x6}; 
static unsigned long stepper_motor_up_flag=0,stepper_motor_down_flag=0,stepper_motor_left_flag=0,stepper_motor_right_flag=0;
static int dc_motor_fd;
static int stepper_motor_fd;
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
    InitMotors( );
	return 1; 
}

/******************use sm_phase for up&down********************************/
void stepper_motor_updown()
{
    int i,get_cmd,get_cmd_pre=-1;
    while(1)
    {
        get_cmd=GetNextOp(UPDOWN_SMID);
        switch (get_cmd)
        {
        case MOTOR_FORWARD:
            get_cmd_pre=get_cmd;
            ioctl(stepper_motor_fd,SMUPDOWN_UP_IN_PLACE,&stepper_motor_up_flag);
            if (stepper_motor_up_flag == 1||count_m1_up > STEPER_PHASE_UP)
            {
                printf("up in place;\n");
                count_m1_down = 0;
                ioctl(stepper_motor_fd,SMUPDOWN_POWER,NULL);
            }
            else
            {
                count_m1_up++;
                count_m1_down--;
                for (i=8;i>0;i--)
                {
                    ioctl(stepper_motor_fd,SMUPDOWN_CONFIG_UP,&sm_phase[i-1]);
                    usleep(STEPPER_DELAY);
                }
            }
            break;
        case MOTOR_BACKWARD:
            get_cmd_pre=get_cmd;
            ioctl(stepper_motor_fd,SMUPDOWN_DOWN_IN_PLACE,&stepper_motor_down_flag);
            if (stepper_motor_down_flag == 1||count_m1_down > STEPER_PHASE_UP)
            {	
                printf("down in place!\n");
                count_m1_up = 0;
                ioctl(stepper_motor_fd,SMUPDOWN_POWER,NULL);
            }
            else
            {
                count_m1_up--;
                count_m1_down++;
                for (i=0;i<8;i++)
                {
                    ioctl(stepper_motor_fd,SMUPDOWN_CONFIG_DOWN,&sm_phase[i]);
                    usleep(STEPPER_DELAY);
                }
            }
            break;
        case MOTOR_STOP:
            if(get_cmd==get_cmd_pre)
                break;
            else
                get_cmd_pre=get_cmd;
            ioctl(stepper_motor_fd,SMUPDOWN_POWER,NULL);
            break;
        default:
            printf("GetNextOp ERROR!!!\n");
        }
    }
}
void stepper_motor_leftright()
{
    int i,get_cmd,get_cmd_pre=-1;
    while(1)
    {
        get_cmd=GetNextOp(LEFTRIGHT_SMID);
        switch (get_cmd)
        {
        case MOTOR_FORWARD:
            get_cmd_pre=get_cmd;
            ioctl(stepper_motor_fd,SMLEFTRIGHT_LEFT_IN_PLACE,&stepper_motor_left_flag);
            if (stepper_motor_left_flag == 1||count_m2_left > STEPER_PHASE_LEFT)
            {
                printf("left in place;\n");
                count_m2_right = 0;
                ioctl(stepper_motor_fd,SMLEFTRIGHT_POWER,NULL);
            }
            else
            {
                count_m2_left++;
                count_m2_right--;
                for (i=8;i>0;i--)
                {
                    ioctl(stepper_motor_fd,SMLEFTRIGHT_CONFIG_LEFT,&sm_phase[i-1]);
                    usleep(STEPPER_DELAY);
                }
            }
            break;
        case MOTOR_BACKWARD:
            get_cmd_pre=get_cmd;
            ioctl(stepper_motor_fd,SMLEFTRIGHT_RIGHT_IN_PLACE,&stepper_motor_down_flag);
            if (stepper_motor_right_flag == 1||count_m2_right > STEPER_PHASE_LEFT)
            {	
                printf("right in place!\n");
                count_m2_left = 0;
                ioctl(stepper_motor_fd,SMUPDOWN_POWER,NULL);
            }
            else
            {
                count_m2_left--;
                count_m2_right++;
                for (i=0;i<8;i++)
                {
                    ioctl(stepper_motor_fd,SMLEFTRIGHT_CONFIG_RIGHT,&sm_phase[i]);
                    usleep(STEPPER_DELAY);
                }
            }
            break;
        case MOTOR_STOP:
            if(get_cmd==get_cmd_pre)
                break;
            else
                get_cmd_pre=get_cmd;
            ioctl(stepper_motor_fd,SMLEFTRIGHT_POWER,NULL);
            break;
        default:
            printf("GetNextOp ERROR!!!\n");
        }
    }
}
/**************close led**************/
void out_high(void)
{
		//printf("%s\n",__func__);
		int i = 0;
		ioctl(dc_motor_fd,6,&i);
}

/**************open led**************/
void out_low(void)
{
		//printf("%s\n",__func__);
		int i = 1;
		ioctl(dc_motor_fd,6,&i);
}

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
