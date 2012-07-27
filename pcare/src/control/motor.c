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
	int cur_op;
	int next_op;
    int wanted;
	sem_t op_arrive;
	pthread_mutex_t op_lock;
};
enum MotorType
{
    STEPPER_MOTOR
};

#define MOTOR_NUM	2
struct Motor
{
	int type;
	struct MotorOpBuffer	buffer;
	pthread_t control_thread;
};

struct MotorManager
{
	struct Motor motor[MOTOR_NUM];
	
};

struct MotorManager	g_motor_manager;

void InitMotorOpBuffer( struct MotorOpBuffer *p_buffer )
{
	// assert( p_buffer != 0 )
	
	p_buffer->cur_op = MOTOR_STOP;
	p_buffer->next_op = MOTOR_NOOP;
    p_buffer->wanted = 0;
	sem_init(&p_buffer->op_arrive,0,0);
	pthread_mutex_init(&p_buffer->op_lock,NULL);
	
	return;
}

int GetNextOp( int id )
{
	// assert( id >= MIN_ID && id <= MAX_ID )
	int next_op;
	struct Motor	*p_motor = &g_motor_manager.motor[id];
	struct MotorOpBuffer	*p_buffer = &p_motor->buffer;

	pthread_mutex_lock(&p_buffer->op_lock);
	if( p_buffer->next_op == MOTOR_NOOP )
	{
		if( p_buffer->cur_op == MOTOR_STOP )
		{
            p_buffer->wanted = 1;
            pthread_mutex_unlock(&p_buffer->op_lock);
			sem_wait(&p_buffer->op_arrive);
            pthread_mutex_lock(&p_buffer->op_lock);
			p_buffer->cur_op = p_buffer->next_op;
            p_buffer->next_op = MOTOR_NOOP;
		}
		
	}
	else
	{
		p_buffer->cur_op = p_buffer->next_op;
        p_buffer->next_op = MOTOR_NOOP;
	}
	next_op = p_buffer->cur_op;
	pthread_mutex_unlock(&p_buffer->op_lock);
	return next_op;
}

void SetNextOp( int id, int next_op )
{
	// assert( id >= MIN_ID && id <= MAX_ID )
	struct Motor	*p_motor = &g_motor_manager.motor[id];
	struct MotorOpBuffer	*p_buffer = &p_motor->buffer;
	
	pthread_mutex_lock(&p_buffer->op_lock);
	p_buffer->next_op = next_op;
	if( p_buffer->cur_op == MOTOR_STOP )
	{
		//int op_wait;
		//sem_getvalue(&p_buffer->op_arrive,&op_wait);
		if( p_buffer->wanted )
		{
            p_buffer->wanted = 0;
			sem_post(&p_buffer->op_arrive);
		}
	}
	pthread_mutex_unlock(&p_buffer->op_lock);
	
	return;
}

void InitMotors( )
{
    int ret;
	struct Motor *p_motor = g_motor_manager.motor;
	p_motor[UPDOWN_SMID].type = STEPPER_MOTOR;
	InitMotorOpBuffer( &p_motor[UPDOWN_SMID].buffer );
	ret = pthread_create(&p_motor[UPDOWN_SMID].control_thread,NULL,(void *)stepper_motor_updown,NULL);
    if (ret != 0)
    {
    	printf("create UPDOWN motor control thread error\n");
    }
	
	p_motor[LEFTRIGHT_SMID].type = STEPPER_MOTOR;
	InitMotorOpBuffer( &p_motor[LEFTRIGHT_SMID].buffer );
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
			next_op = MOTOR_BACKWARD;
			break;
		case 2:	// DOWN
		case 4:	// LEFT
			next_op = MOTOR_FORWARD;
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
