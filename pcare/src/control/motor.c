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
#include <pthread.h>
#include <semaphore.h>
#include "motor_driver.h"
#include "motor.h"
#include "types.h"

struct MotorOpBuffer
{
};
enum MotorType
{
    STEPPER_MOTOR
};

#define MOTOR_NUM	2
struct Motor
{
	int type;
	int cur_op;
    int wanted;
	sem_t op_arrive;
	pthread_mutex_t op_lock;
	pthread_t control_thread;
};

struct MotorManager
{
	struct Motor motor[MOTOR_NUM];
	
};

struct MotorManager	g_motor_manager;

int GetNextOp( int id )
{
	// assert( id >= MIN_ID && id <= MAX_ID )
	struct Motor	*p_motor = &g_motor_manager.motor[id];
	return p_motor->cur_op;
}

void WaitNextOp( int id )
{
	struct Motor	*p_motor = &g_motor_manager.motor[id];

    p_motor->wanted = 1;
    sem_wait(&p_motor->op_arrive);
    return;
}

void SetNextOp( int id, int next_op )
{
	// assert( id >= MIN_ID && id <= MAX_ID )
	struct Motor	*p_motor = &g_motor_manager.motor[id];
	
	if( next_op != MOTOR_STOP && next_op != p_motor->cur_op )
	{
		if( p_motor->wanted )
		{
            p_motor->wanted = 0;
            p_motor->cur_op = next_op;
			sem_post(&p_motor->op_arrive);
            return;
		}
	}
	p_motor->cur_op = next_op;
    
	return;
}

void InitMotors( )
{
    int ret;
	struct Motor *p_motor = g_motor_manager.motor;
	p_motor[UPDOWN_SMID].type = STEPPER_MOTOR;
	p_motor[UPDOWN_SMID].cur_op = MOTOR_STOP;
    p_motor[UPDOWN_SMID].wanted = 0;
	sem_init(&p_motor[UPDOWN_SMID].op_arrive,0,0);
	pthread_mutex_init(&p_motor[UPDOWN_SMID].op_lock,NULL);
	ret = pthread_create(&p_motor[UPDOWN_SMID].control_thread,NULL,(void *)stepper_motor_updown,NULL);
    if (ret != 0)
    {
    	printf("create UPDOWN motor control thread error\n");
    }
	
	p_motor[LEFTRIGHT_SMID].type = STEPPER_MOTOR;
	p_motor[LEFTRIGHT_SMID].cur_op = MOTOR_STOP;
    p_motor[LEFTRIGHT_SMID].wanted = 0;
	sem_init(&p_motor[LEFTRIGHT_SMID].op_arrive,0,0);
	pthread_mutex_init(&p_motor[LEFTRIGHT_SMID].op_lock,NULL);
	ret = pthread_create(&p_motor[LEFTRIGHT_SMID].control_thread,NULL,(void *)stepper_motor_leftright,NULL);
    if (ret != 0)
    {
    	printf("create LEFTRIGHT motor control thread error\n");
    }
	
	return;
}

void DispatchMotorOp( motor_ctrl_t *opt )
{
	// assert( opt != NULL )
	int op_code = opt->param[0];
	int next_op;
	switch( op_code )
	{
		case 0:	// UP
		case 6:	// RIGHT
			next_op = MOTOR_FORWARD;
			break;
		case 2:	// DOWN
		case 4:	// LEFT
			next_op = MOTOR_BACKWARD;
			break;
		case 1:
		case 3:
		case 5:
		case 7:
			next_op = MOTOR_STOP;
			break;
        case 94:
            out_high();
            return;
        case 95:
            out_low();
            return;
		default:
			printf("Unsupported motor op(%d)!\n", op_code);
			return;
	}
	switch( op_code )
	{
		case 0:
		case 1:
		case 2:
		case 3:
			SetNextOp( UPDOWN_SMID, next_op );
			break;
		case 4:
		case 5:
		case 6:
		case 7:
			SetNextOp( LEFTRIGHT_SMID, next_op );
			break;
		default:
            return;			
	}
	return;
}
