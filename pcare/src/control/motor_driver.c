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
#include "types.h"
#include "motor.h"
#include "motor_driver.h"
#include "network.h"

#define MOTOR1_V  0xF0
#define MOTOR2_V  0x0F

#define DCM     100
#define DCM1_ADVANCE  0
#define DCM1_RETREAT  1
#define DCM2_ADVANCE  0
#define DCM2_RETREAT  1
#define STEPPER_DELAY 1000
#define STEPPER_LIMIT 50
#define STEPER_PHASE_UP 225
#define STEPER_PHASE_LEFT 620
extern pthread_mutex_t mutex_dc;
extern pthread_mutex_t mutex_step;
extern int oflags;
extern int dc_motor_fd;
extern int stepper_motor_fd;
extern int record_count;
extern int playback_count;
extern u8 ctrl_motor_flag; 
extern u8 record_flag,record_full,playback_flag;
extern u8 i,j,m,n,k_step;
extern u8 r_cmd[R_CMD_L];
extern u8 cmd[CMD_L][CMD_W];
extern u8 cmd_step[CMD_L][CMD_W];
/***count_m1 up ++ down --*******
 ***count_m2 right ++ left --****
 ********************************/
static int count_m1_up=0;
static int count_m2_right=0;
static int count_m1_down=0;
static int count_m2_left=0;
static int count_m1=0;
static int count_m2=0;
FILE *fp;
u8 unfull_write_flag = 0;
u8 read_count = 0;
u8 record_count_flag = 0;
//u32 sm1[]={0x7000,0x6000,0x6800,0x4800,0x5800,0x1800,0x3800,0x3000};
u32 sm_phase[]={0x7,0x3,0xB,0x9,0xD,0xC,0xE,0x6}; 
char stepper_motor_up_flag[1]={0};
char stepper_motor_down_flag[1]={0};
char stepper_motor_left_flag[1]={0};
char stepper_motor_right_flag[1]={0};
u32 Record_Count = 0;
char protocol_playback_flag = 0;
/***********STEPPER MOTOR FLAG*************/
u8  upflag,downflag,leftflag,rightflag,updownflag,rightleftflag;
//FILE *bat_fp;
//int bat_info[1]={0};


int operate(void *arg)
{
		int size,ret;
		while(1)
		{
				ret = pthread_mutex_trylock(&mutex_dc);
				if (ret == EBUSY)
						printf("mutex_dc handled by ctr_motor");
				if (ret != 0)
						perror("pthread_trylock mutex_dc");	
				/* IF WE NEED PLAYBACK TRACK  GO THIS WAY */
				if (playback_flag && (record_count >= 0))
				{
						//printf("mode playback;r_cmd[%d]:%d;%d;%d;record_count:%d \n",read_count,r_cmd[read_count],r_cmd[read_count+1],r_cmd[read_count+2],record_count);
						if (r_cmd[read_count] == OPT_DC)
								operate_motor(r_cmd[read_count+1],r_cmd[read_count+2]);
						else if (r_cmd[read_count] == OPT_STEP)
								operate_stepper_motor(r_cmd[read_count+1]);
						else
								printf("cmd err :%d\n",r_cmd[read_count]);
						read_count += 3;
						if ((read_count >= (j-3)) && (record_count == 0))
						{
								printf("read the last\n");
								playback_flag = 0;
						}
						if (read_count >= R_CMD_L)
						{
								memset(r_cmd,0,sizeof(r_cmd));
								if ((unfull_write_flag) == 1 && (record_count_flag == 1)) //this for the last but not full
								{
										printf("read again not full;j:%d\n",j);
										if ((fread(r_cmd,j,1,fp))!=1) 
										{
												printf("read from fd(&j&) failed\n");
										}
										record_count_flag = 0;
										record_count--;
										read_count = 0;
								}
								else 
								{
										printf("read again\n");
										if ((fread(r_cmd,R_CMD_L,1,fp))!=1) 
										{
												printf("read from fd failed\n");
												//return 0;
										}
										record_count--;
										if (record_count == 1)
												record_count_flag = 1;
										read_count = 0;
										if ((r_cmd[R_CMD_L-3] == 250) && (r_cmd[R_CMD_L-2] == 6) && (r_cmd[R_CMD_L-1] == 0))
										{
												r_cmd[R_CMD_L-3] = 0;	 	
										}
								}
						}
				}
				else if (ctrl_motor_flag)//this function mean do operate() after ctrl_motor()
				{
						//printf("m:%d; i:%d; cmd[%d][0]:%d;cmd[%d][1]:%d;cmd[%d][2]:%d\n",m,i,m,cmd[m][0],m,cmd[m][1],m,cmd[m][2]);
						if (m != i)
						{
								if (m >= CMD_L)
										m = 0;
								if ((cmd[m][0]==0) && (cmd[m][1]==0) && (cmd[m][2]==0))
								{
										printf("%s both 0;m:%d;i:%d\n",__func__,m,i);
								}
							//	printf("m:%d;i:%d\n",m,i);
								if (m != i)
								{
									if (cmd[m][0] == OPT_DC)
											operate_motor(cmd[m][1],cmd[m][2]);
									else if (cmd[m][0] == OPT_STEP)
											operate_stepper_motor(cmd[m][1]);
									else
											printf("cmd opt_cord error; \n");
									for (n=0;n<CMD_W;n++)
									{
											cmd[m][n] = 0;
									}
									m++;
								}
						}
				}
				pthread_mutex_unlock(&mutex_dc);
				//printf("pthread_tryunlock mutrx_dc\n");
				usleep(50000);//50ms

		}
		return 1;
}

/*************************THIS FUNCTION FOR STEPPER MOTOR*******************************/
int operate_stepper(void *arg)
{
		int size;
		while(1)
		{
				/* IF WE NEED PLAYBACK TRACK  GO THIS WAY */
				if (playback_flag && (record_count >= 0))
				{
						if (r_cmd[read_count] == OPT_STEP)
								control_stepper_motor(r_cmd[read_count+1]);	
				}
				else if (ctrl_motor_flag)//this function mean do operate_motor() after control_stepper_motor()
				{
						if (k_step != i)
						{
								if (k_step >= CMD_L)
										k_step = 0;
								if ((cmd_step[k_step][0]==0) && (cmd_step[k_step][1]==0) && (cmd_step[k_step][2]==0))
										printf("%s both 0\n",__func__);
								if (cmd_step[k_step][0] == OPT_STEP)
										control_stepper_motor(cmd_step[k_step][1]);
								k_step++;	
						}
				}
				usleep(50000);//50ms
		}
		return 1;
}

/*************************DC MOTOR****************************/
void operate_motor(u8 param1,u8 param2)
{
		u8 i,j;
		i = param1;
		j = param2;
		switch(i)
		{
				case 0:
						motor1_stop();
						break;
				case 1:
						motor1_advance(j);
						break;
				case 2:
						motor1_retreat(j);
						break;
				case 3:
						motor2_stop();
						break;
				case 4:
						motor2_advance(j);
						break;
				case 5:
						motor2_retreat(j);
						break;
				case 6:
						track_record(j);
						break;
				case 7:
						track_playback(j);
						break;
				case 8:
						led_darts_on(j);
						break;
				case 9:
						led_darts_off(j);
						break;	
				case 10:
						m1advance_m2advance(j);
						break;
				case 11:
						m1advance_m2retreat(j);
						break;
				case 12:
						m1retreat_m2advance(j);
						break;
				case 13:
						m1retreat_m2retreat(j);
						break;
				default:
						printf("cmd error \n");
		}
}

/*************************DC M1********************************/
void motor1_stop()
{
		/*we can not write 0 to data reg*/
		//printf("%s\n",__func__);
		int b1 = 1;
		int i = DCM1_RETREAT;
		ioctl(dc_motor_fd,1,&b1);
		ioctl(dc_motor_fd,4,&i);
}


void motor1_advance(u8 param1)
{
		/*if we write 1000 mean all high; if we write 500 mean 50%*/
		int i,b1;
		//printf("%s\n",__func__);
		if (10 == param1)
				param1 = 9;
		i = DCM1_ADVANCE;
		b1 = 1000 - DCM*param1;
		if (!b1)
				b1 = 1;
		ioctl(dc_motor_fd,1,&b1);
		ioctl(dc_motor_fd,4,&i);
}

void motor1_retreat(u8 param1)
{
		int i,b1;
		//printf("%s\n",__func__);
		if (10 == param1)
				param1 = 9;
		i = DCM1_RETREAT;
		b1 = DCM*param1;
		if (!b1)
				b1 = 1;
		ioctl(dc_motor_fd,1,&b1);
		ioctl(dc_motor_fd,4,&i);
}

/***************************DC M2***********************************/
void motor2_stop()
{
		/*we can not write 0 to data reg*/
		//printf("%s\n",__func__);
		int b1 = 1;
		int i = DCM2_ADVANCE;
		ioctl(dc_motor_fd,3,&b1);
		ioctl(dc_motor_fd,5,&i);
}


void motor2_advance(u8 param1)
{
		/*if we write 1000 mean all high; if we write 500 mean 50%*/
		int i,b1;
		//printf("%s\n",__func__);
		if (10 == param1)
				param1 = 9;
		i = DCM2_ADVANCE;
		b1 = DCM*param1;
		if (!b1)
				b1 = 1;
		ioctl(dc_motor_fd,3,&b1);
		ioctl(dc_motor_fd,5,&i);
}

void motor2_retreat(u8 param1)
{
		int i,b1;
		//printf("%s\n",__func__);
		if (10 == param1)
				param1 = 9;
		i = DCM2_RETREAT;
		b1 = 1000 - DCM*param1;
		if (!b1)
				b1 = 1;
		ioctl(dc_motor_fd,3,&b1);
		ioctl(dc_motor_fd,5,&i);
}


void track_record(u8 param1)
{
#if 0
		int size;
		//printf("%s param1 :%d\n",__func__,param1);
		if (param1)
		{
				j = 0;
				record_flag = 1;
				record_full = 0;	
				if((fd  =  open("record.txt",  O_CREAT  |  O_TRUNC  |  O_WRONLY  ,  0600 ))<0)
						printf("create record.txt failed\n");
				else
						printf("create record.txt succsed\n");
		}
		else
		{
				//printf("%s j:%d\n",__func__,j);
				if ((j>0) && (j<R_CMD_L))
				{
						if ((size = write(fd, r_cmd, j)) < 0)
						{		
								printf("write to fd(&j&) failed\n");
						}
						record_count++;
						unfull_write_flag = 1;
						//printf("**** unfull write***** record_count:%d\n",record_count);
				}
				record_flag = 0;
				record_full = 0;
				Record_Count = record_count;
				//printf("Record_Count :%drecord_count:%d\n",Record_Count,record_count);
				if (close(fd) < 0)
				{
						printf("close record.txt failed\n");
				}	
		}
#endif
}

void track_playback(u8 param1)
{
#if 0
		int size;
		//printf("%s , param1 :%d record_count :%d Record_Count :%d\n",__func__,param1,record_count,Record_Count);
		record_count = Record_Count;
		//printf("after ^^^^ record_count:%d\n",record_count);
		if (param1)
		{
				protocol_playback_flag = 1;
				fp = fopen("record.txt","r+");
				if (fp == NULL)
						printf("open record.txt failed\n");
				else
						printf("open record.txt succed\n");
				memset(r_cmd,0,sizeof(r_cmd));
				if ((fread(r_cmd,R_CMD_L,1,fp))!=1)
				{
						printf("read from fd failed\n");
				}
				record_count--;
				//printf("&& record_count --&&%d\n",record_count);
				playback_flag = 1;
				read_count = 0;
				//printf("playback_flag:%d,record_count:%d\n",playback_flag,record_count);
		}
		else
		{
				if (protocol_playback_flag == 1)
				{
						protocol_playback_flag = 0;
						printf("we will close fp\n");
						if (fclose(fp) != 0)
						{
								printf("close record.txt failed\n");
						}
						playback_flag = 0;
						playback_count = 0;
				}
		}
#endif

}

void led_darts_on(u8 param1)
{
		//printf("%s\n",__func__);
		int i=1;
		if (!param1) //led
				ioctl(dc_motor_fd,6,&i);
		else
		{
				/*for open darts*/
				ioctl(dc_motor_fd,7,&i);
		}
}

void led_darts_off(u8 param1)
{
		//printf("%s\n",__func__);
		int i=0;
		if (!param1) //led
				ioctl(dc_motor_fd,6,&i);
		else
		{
				printf("we do not do anything\n");
				/* for close darts */
				//ioctl(dc_motor_fd,7,&i);
		}

}
/***************************DC M1&M2******************************/
void m1advance_m2advance(u8 param1)
{
		//	printf("%s\n",__func__);
		int m1_v,m2_v,m1_i,m2_i;
		m1_i = DCM1_ADVANCE;
		m2_i = DCM2_ADVANCE;
		m1_v = MOTOR1_V & param1;
		m2_v = MOTOR2_V & param1;
		ioctl(dc_motor_fd,1,&m1_v);
		ioctl(dc_motor_fd,4,&m1_i);
		ioctl(dc_motor_fd,3,&m2_v);
		ioctl(dc_motor_fd,5,&m2_i);
}

void m1advance_m2retreat(u8 param1)
{
		//	printf("%s\n",__func__);
		int m1_v,m2_v,m1_i,m2_i;
		m1_i = DCM1_ADVANCE;
		m2_i = DCM2_RETREAT;
		m1_v = MOTOR1_V & param1;
		m2_v = MOTOR2_V & param1;
		m2_v = 1000 - m2_v;
		ioctl(dc_motor_fd,1,&m1_v);
		ioctl(dc_motor_fd,4,&m1_i);
		ioctl(dc_motor_fd,3,&m2_v);
		ioctl(dc_motor_fd,5,&m2_i);
}

void m1retreat_m2advance(u8 param1)
{
		//	printf("%s\n",__func__);
		int m1_v,m2_v,m1_i,m2_i;
		m1_i = DCM1_RETREAT;
		m2_i = DCM2_ADVANCE;
		m1_v = MOTOR1_V & param1;
		m1_v = 1000 - m1_v;
		m2_v = MOTOR2_V & param1;
		ioctl(dc_motor_fd,1,&m1_v);
		ioctl(dc_motor_fd,4,&m1_i);
		ioctl(dc_motor_fd,3,&m2_v);
		ioctl(dc_motor_fd,5,&m2_i);
}

void m1retreat_m2retreat(u8 param1)
{
		//	printf("%s\n",__func__);
		int m1_v,m2_v,m1_i,m2_i;
		m1_i = DCM1_RETREAT;
		m2_i = DCM2_RETREAT;
		m1_v = MOTOR1_V & param1;
		m2_v = MOTOR2_V & param1;
		m1_v = 1000 - m1_v;
		m2_v = 1000 - m2_v;
		ioctl(dc_motor_fd,1,&m1_v);
		ioctl(dc_motor_fd,4,&m1_i);
		ioctl(dc_motor_fd,3,&m2_v);
		ioctl(dc_motor_fd,5,&m2_i);
}

/**************************************************************/
/*********************STEPPER MOTOR****************************/
/**************************************************************/
void control_stepper_motor(u8 param1)
{
		u8 i;
		i = param1;	
		int j,k;
		j = STEPPER_DELAY;
		k = STEPPER_LIMIT;
		//printf("i:%d;%s\n",i,__func__);
		switch(i)
		{
				case 0:
						stepper_up(j);
						break;
				case 2:
						stepper_down(j);
						break;
				case 4:
						stepper_left(j);
						break;
				case 6:
						stepper_right(j);
						break;
				case 25:
						stepper_mid(j);
						break;
				case 26:
						cruise_updown(k,j);
						break;
				case 28:
						cruise_rightleft(k,j);
						break;
				default :
						printf("we do not need process others\n");

		}
}
/**************************************************************/
void operate_stepper_motor(u8 param1)
{
		u8 i;
		int j,k;
		i = param1;
		j = STEPPER_DELAY;
		k = STEPPER_LIMIT;
		//printf("i:%d;%s\n",i,__func__);
		switch(i)
		{
				case 0:
						printf("this mode we will do in another thread;0\n");
						break;
				case 1:
						stepper_upstop();
						break;
				case 2:
						printf("this mode we will do in another thread;2\n");
						break;
				case 3:
						stepper_downstop();
						break;
				case 4:
						printf("this mode we will do in another thread;4\n");
						break;
				case 5:
						stepper_leftstop();
						break;
				case 6:
						printf("this mode we will do in another thread;6\n");
						break;
				case 7:
						stepper_rightstop();
						break;
				case 25:
						printf("this mode we will do in another thread;25\n");
						break;
				case 26:
						printf("this mode we will do in another thread;26\n");
						break;
				case 27:
						stop_updown_cruise();
						break;
				case 28:
						printf("this mode we will do in another thread;28\n");
						break;
				case 29:
						stop_rightleft_cruise();
						break;
				case 90:
						left_up(j);
						break;
				case 91:
						right_up(j);
						break;
				case 92:
						left_down(j);
						break;
				case 93:
						right_down(j);
						break;
				case 94:
						out_high();
						break;
				case 95:
						out_low();
						break;
				default:
						printf("opcord is error\n");

		}

}
/******************use sm_phase for up&down********************************/
void stepper_up(int param)
{
		u8 i,k;
		int j;
		k = 1;
		j = param;
		upflag = 1;
		//printf("%s\n",__func__);
		while(k)
		{
				ioctl(stepper_motor_fd,6,&stepper_motor_up_flag[0]);
				//printf("%s;upflag:%d;stepper_motor_up_flag[0]:%d;stepper_motor_down_flag[0]:%d\n",__func__,upflag,stepper_motor_up_flag[0],stepper_motor_down_flag[0]);
				if (stepper_motor_up_flag[0] == 1||count_m1_up > STEPER_PHASE_UP)
				{
						printf("up in place;count_m1=%d\n",count_m1);
						/* UP IN PLACE */
						k = 0;
                        count_m1_down = 0;
						ioctl(stepper_motor_fd,3,&k);
				}
				else
				{
					//printf("upflag:%d\n",upflag);
					if (upflag)
                    {
                        count_m1_up++;
                        count_m1_down--;
                        for (i=8;i>0;i--)
                        {
                            ioctl(stepper_motor_fd,1,&sm_phase[i-1]);
                            usleep(2*j);
						}
					}
				}
				if (upflag == 0)
				{
						//printf("we will set k = 0\n");
						k = 0;
				}
		}
}

void stepper_upstop(void)
{
		u8 i;
		upflag = 0;
		stepper_motor_up_flag[0] = 0;
		//printf("%s;upflag:%d\n",__func__,upflag);
		ioctl(stepper_motor_fd,3,&i);
        confirm_stop();
}

void stepper_down(int param)
{
		u8 i,k;
		int j;
		k = 1;
		j = param;
		downflag = 1;
		//printf("%s\n",__func__);
		while(k)
		{
				ioctl(stepper_motor_fd,8,&stepper_motor_down_flag[0]);
				//printf("%s;downflag:%d;stepper_motor_up_flag[0]:%d;stepper_motor_down_flag[0]:%d\n",__func__,upflag,stepper_motor_up_flag[0],stepper_motor_down_flag[0]);
				if (stepper_motor_down_flag[0] == 1||count_m1_down > STEPER_PHASE_UP)
				{	
						/* DOWN IN PLACE */
						printf("down in place!count_m1 is %d\n",count_m1);
						k = 0;
						ioctl(stepper_motor_fd,3,&k);
                        count_m1_up = 0;
				}
				else
				{
					//printf("downflag:%d\n",downflag);
					if(downflag)
					{
                        count_m1_up--;
                        count_m1_down++;
						for (i=0;i<8;i++)
						{
								ioctl(stepper_motor_fd,21,&sm_phase[i]);
								usleep(2*j);
						}
					}
				}
				if (downflag == 0)
				{
						//printf("we will set k = 0\n");
						k = 0;		
				}
		}
}

void stepper_downstop(void)
{
		u8 i;
		downflag = 0;
		stepper_motor_down_flag[0] = 0;
		//printf("%s;downflag:%d\n",__func__,downflag);
		ioctl(stepper_motor_fd,3,&i);
        confirm_stop();
}

/*********************use sm_phase for right&left*************************/
void stepper_right(int param)
{
    u8 i,k;
		int j;
		k = 1;
		j = param;
		rightflag = 1;
		//printf("%s\n",__func__);
		while(k)
		{
				ioctl(stepper_motor_fd,10,&stepper_motor_right_flag[0]);
				//printf("%s;downflag:%d;stepper_motor_left_flag[0]:%d;stepper_motor_right_flag[0]:%d\n",__func__,rightflag,stepper_motor_left_flag[0],stepper_motor_right_flag[0]);
				if (stepper_motor_right_flag[0] == 1||count_m2_right > STEPER_PHASE_LEFT)
				{	
						/* RIGHT IN PLACE */
						printf("right in place!\n count_m2 is %d",count_m2);
						k = 0;
                        count_m2=0;
						ioctl(stepper_motor_fd,5,&k);
                        count_m2_left = 0;
				}
				else
				{
					//printf("rightflag:%d\n",rightflag);
					if(rightflag)
					{
				        count_m2_left--;
                        count_m2_right++;
						for (i=8;i>0;i--)
						{
								ioctl(stepper_motor_fd,24,&sm_phase[i-1]);
								usleep(2*j);
						}
					}
				}
				if (rightflag == 0)
				{
						//printf("we will set k = 0\n");
						k = 0;		
				}
		}
}

void stepper_rightstop(void)
{
    u8 i;
		rightflag = 0;
		stepper_motor_right_flag[0] = 0;
		//printf("%s;rightflag:%d\n",__func__,rightflag);
		ioctl(stepper_motor_fd,5,&i);
        confirm_stop();
}

void stepper_left(int param)
{
    u8 i,k;
		int j;
		k = 1;
		j = param;
		leftflag = 1;
		//printf("%s\n",__func__);
		while(k)
		{
				ioctl(stepper_motor_fd,9,&stepper_motor_left_flag[0]);
				//printf("%s;leftflag:%d;stepper_motor_left_flag[0]:%d;stepper_motor_right_flag[0]:%d\n",__func__,leftflag,stepper_motor_left_flag[0],stepper_motor_right_flag[0]);
				if (stepper_motor_left_flag[0] == 1||count_m2_left > STEPER_PHASE_LEFT)
				{
						printf("left in place;count_m2 is %d\n",count_m2);
						/* left IN PLACE */
						k = 0;
						ioctl(stepper_motor_fd,5,&k);
                        count_m2_right = 0;
				}
				else
				{
					//printf("leftflag:%d\n",leftflag);
					if (leftflag)
					{
				        count_m2_left++;
                        count_m2_right--;
						for (i=0;i<8;i++)
						{
								ioctl(stepper_motor_fd,4,&sm_phase[i]);
								usleep(2*j);
						}
					}
				}
				if (leftflag == 0)
				{
						//printf("we will set k = 0\n");
						k = 0;
				}
		}
}

void stepper_leftstop(void)
{
		u8 i;
		leftflag = 0;
		stepper_motor_left_flag[0] = 0;
		ioctl(stepper_motor_fd,5,&i);
		//printf("%s;%d\n",__func__,leftflag);
        confirm_stop();
}

/*****************************camera in mid***************************************/
void stepper_mid(int param)
{
		u8 i,j;
		int k;
		k = param;
		printf("%s\n",__func__);
		if (count_m1 > 0)
		{
				for (j=count_m1; j>0; j--)
				{
						for (i=7;i>=0;i--)
						{ 
								ioctl(stepper_motor_fd,1,&sm_phase[i]);
								usleep(k);
						}
				}
				count_m1 = 0;
		}
		else if (count_m1 < 0)
		{
				for (j=count_m1; j<0; j++)
				{
						for (i=0; i<8; i++)
						{
								ioctl(stepper_motor_fd,1,&sm_phase[i]);
						}
				}
				count_m1 = 0;
		}

		if (count_m2 > 0)
		{
				for (j=count_m2; j>0; j--)
				{
						for (i=7;i<0;i--)
						{ 
								ioctl(stepper_motor_fd,4,&sm_phase[i]);
								usleep(k);
						}
				}
				count_m2 = 0;
		}
		else if (count_m2 < 0)
		{
				for (j=count_m2; j<0; j++)
				{
						for (i=0; i<8; i++)
						{
								ioctl(stepper_motor_fd,4,&sm_phase[i]);
						}
				}
				count_m2 = 0;
		}
}

/****************************cruise up&down*************************/
void cruise_updown(u8 param1,int param2)
{
		u8 i,k;
		k = 1;
		printf("%s\n",__func__);
		int n = param1;
		int m = param2;
		updownflag = 1;
		while (k)
		{
				ioctl(stepper_motor_fd,6,&stepper_motor_up_flag[0]);
				ioctl(stepper_motor_fd,8,&stepper_motor_down_flag[0]);
				if ((stepper_motor_up_flag[0] == 1) && (stepper_motor_down_flag[1] == 0))
				{
						/* UP IN PLACE */
						for (i=7;i>0;i--)
						{
								ioctl(stepper_motor_fd,1,&sm_phase[i]);
								usleep(2*STEPPER_DELAY);
						}
				}
				else if ((stepper_motor_up_flag[0] == 0) && (stepper_motor_down_flag[1] == 1))
				{
						/* DOWN IN PLACE */	
						for (i=0;i<8;i++)
						{
								ioctl(stepper_motor_fd,1,&sm_phase[i]);
								usleep(2*STEPPER_DELAY);
						}
				}
				else
				{
						for (i=0;i<8;i++)
						{
								ioctl(stepper_motor_fd,1,&sm_phase[i]);
								usleep(2*STEPPER_DELAY);
						}
				}
				if (!updownflag)
						k = 0;
		}
}

void stop_updown_cruise()
{
		printf("%s\n",__func__);
		u8 i;
		updownflag = 0;
		stepper_motor_up_flag[0] = 0;
		stepper_motor_down_flag[0] = 0;
		ioctl(stepper_motor_fd,3,&i);
}

/****************************cruise right&left*************************/
void cruise_rightleft(u8 param1,int param2)
{
		u8 i,j,k;
		k = 1;
		int n = param1;
		int m = param2;
		rightleftflag = 1;
		while(k)
		{
				for (i=0; i<n; i++)
				{
						for(j=0;j<8;j++)
						{
								ioctl(stepper_motor_fd,4,&sm_phase[i]);
								usleep(m);
						}
				}	
				for (i=0; i<2*n; i++)
				{
						for(j=7;j<0;j++)
						{
								ioctl(stepper_motor_fd,4,&sm_phase[i]);
								usleep(m);
						}
				}
				for (i=0; i<n; i++)
				{
						for(j=0;j<8;j++)
						{
								ioctl(stepper_motor_fd,4,&sm_phase[i]);
								usleep(m);
						}
				}
				if (rightleftflag == 0)
						k = 0;		
		}	
}

void stop_rightleft_cruise()
{
		u8 i;
		rightleftflag = 0;
		ioctl(stepper_motor_fd,5,&i);
}


void left_up(param)
{
		u8 i,j;
		j = param;
		count_m2--;/*left*/
		for (i=8;i>0;i--)
		{
				ioctl(stepper_motor_fd,4,&sm_phase[i]);
				usleep(j);
		}
		count_m1++;/*up*/
		for (i=0;i<8;i++)
		{
				ioctl(stepper_motor_fd,1,&sm_phase[i]);
				usleep(j);
		}
}


void right_up(param)
{
		u8 i,j;
		j = param;
		count_m2++;/*right*/
		for (i=0;i<8;i++)
		{
				ioctl(stepper_motor_fd,4,&sm_phase[i]);
				usleep(j);
		}
		count_m1++;/*up*/
		for (i=0;i<8;i++)
		{
				ioctl(stepper_motor_fd,1,&sm_phase[i]);
				usleep(j);
		}
}


void left_down(param)
{
		u8 i,j;
		j = param;
		count_m2--;/*left*/
		for (i=8;i>0;i--)
		{
				ioctl(stepper_motor_fd,4,&sm_phase[i]);
				usleep(j);
		}
		count_m1--;/*down*/
		for (i=8;i>0;i--)
		{
				ioctl(stepper_motor_fd,1,&sm_phase[i]);
				usleep(j);
		}
}

void right_down(param)
{
		u8 i,j;
		j = param;
		count_m2++;/*right*/
		for (i=0;i<8;i++)
		{
				ioctl(stepper_motor_fd,4,&sm_phase[i]);
				usleep(j);
		}
		count_m1--;/*down*/
		for (i=8;i>0;i--)
		{
				ioctl(stepper_motor_fd,1,&sm_phase[i]);
				usleep(j);
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

