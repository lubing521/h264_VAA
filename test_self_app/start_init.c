#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>


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

#define STEPPER_MOTOR_DEV  "/dev/steppermotor"
#define CAMERA_DEV "/dev/video0"

pthread_t smupdown_id,smleftright_id;
#ifdef LED_FLASH //自检是否需要蓝灯闪烁
#define LED_Statue "/sys/devices/platform/sep0611_led.1/leds/LED_Statue/brightness"
pthread_t led_flash_id,smupdown_id,smleftright_id;

void led_on()
{
	char buf[2] = "1";
    int fd,size;
    fd = open(LED_Statue, O_RDWR);
    if(fd == 0)
    {
        printf("led open error\n");
        return;
    }
    write(fd, buf, strlen(buf));
    close(fd);
    return;
}

void led_off()
{
	char buf[2]="0";
	int fd,size;
	fd = open(LED_Statue, O_WRONLY);
    if(fd == 0)
    {
        printf("led open error\n");
        return;
    }
    write(fd, buf, strlen(buf));
    close(fd);
	return;
}

void led_flash()
{
    while(1)
    {
        led_on();
        sleep(2);
        led_off();
        sleep(2);
    }
    return;
}
#endif

int step_motor_leftright()
{
    int n,i,stepper_motor_fd,inplace_flag = 1;
    printf("Open Stepper_Motor LEFTRIGHT.....");
    stepper_motor_fd=open(STEPPER_MOTOR_DEV,O_WRONLY);
    if(stepper_motor_fd < 0)
    {
        printf("FAILED!!!\n");
        return -1;
    }
    printf("OK!!!\n");
#if 0
    for(n=0;n<STEPER_PHASE_LEFT && inplace_flag >= 0;n++ )
    {
        for(i = 7;i>=0;i--)
        {
            inplace_flag = ioctl(stepper_motor_fd,SMLEFTRIGHT_CONFIG_RIGHT,&i);
            usleep(1000);
        }
    }
    inplace_flag = 1;
    printf("Stepper_Motor Turn Right .....OK!!!\n");
#endif
    for(n=0;n<STEPER_PHASE_LEFT && inplace_flag >= 0;n++ )
    {
        for(i = 0;i<8;i++)
        {
            inplace_flag = ioctl(stepper_motor_fd,SMLEFTRIGHT_CONFIG_LEFT,&i);
            usleep(1000);
        }
    }
    inplace_flag = 1;
    for(n=0;n<20 && inplace_flag >= 0;n++ )
    {
        for(i = 7;i>=0;i--)
        {
            inplace_flag = ioctl(stepper_motor_fd,SMLEFTRIGHT_CONFIG_RIGHT,&i);
            usleep(1000);
        }
    }
    inplace_flag = 1;
    printf("Stepper_Motor Turn Left .....OK!!!\n");
    printf("Close Stepper_Motor LEFTRIGHT.....");
    ioctl(stepper_motor_fd,SMLEFTRIGHT_POWER,NULL);
    close(stepper_motor_fd);
    printf("OK!!!\n");
    return 0;
}
int step_motor_updown()
{
    int n,i,stepper_motor_fd,inplace_flag = 1;
    printf("Open Stepper_Motor UPDOWN.....");
    stepper_motor_fd=open(STEPPER_MOTOR_DEV,O_WRONLY);
    if(stepper_motor_fd < 0)
    {
        printf("FAILED!!!\n");
        return -1;
    }
    printf("OK!!!\n");
#if 0
    for(n=0;n<STEPER_PHASE_UP && inplace_flag >= 0;n++ )
    {
        for(i = 7;i>=0;i--)
        {
            inplace_flag = ioctl(stepper_motor_fd,SMUPDOWN_CONFIG_UP,&i);
            usleep(1000);
        }
    }
    inplace_flag = 1;
    printf("Stepper_Motor Turn Up .....OK!!!\n");
#endif
    for(n=0;n<STEPER_PHASE_UP && inplace_flag >= 0;n++ )
    {
        for(i = 0;i<8;i++)
        {
            inplace_flag = ioctl(stepper_motor_fd,SMUPDOWN_CONFIG_DOWN,&i);
            usleep(1000);
        }
    }
    inplace_flag = 1;
    for(n=0;n<20 && inplace_flag >= 0;n++ )
    {
        for(i = 7;i>=0;i--)
        {
            inplace_flag = ioctl(stepper_motor_fd,SMUPDOWN_CONFIG_UP,&i);
            usleep(1000);
        }
    }
    inplace_flag = 1;
    printf("Stepper_Motor Turn Down .....OK!!!\n");
    printf("Close Stepper_Motor UPDOWN.....");
    ioctl(stepper_motor_fd,SMUPDOWN_POWER,NULL);
    close(stepper_motor_fd);
    printf("OK!!!\n");
    return 0;
}

int camera()
{
    int camera_fd;
    printf("Open Camera .....");
    camera_fd = open(CAMERA_DEV,O_RDWR);
    if(camera_fd < 0)
    {
        printf("FAILED!!!\n");
        return -1;
    }
    else
    {
        printf("OK!!!\n");
        printf("Close Camera .....");
        close(camera_fd);
        printf("OK!!!\n");
    }
}

void main()
{
    int ret,err_fd,i;
    printf("\n===========================================\n");
    printf("========= Now Start Check Myself ==========\n");
    printf("===========================================\n");
#ifdef LED_FLASH //自检是否需要蓝灯闪烁
    ret = pthread_create(&led_flash_id,NULL,(void *)led_flash,NULL); 
#endif
    if(camera() < 0)
        goto CAMERA_ERROR;
    ret = pthread_create(&smupdown_id,NULL,(void *)step_motor_updown,NULL); 
    ret = pthread_create(&smleftright_id,NULL,(void *)step_motor_leftright,NULL); 
    //if(step_motor() < 0)
      //  goto STEPPER_MOTOR_ERROR;
	pthread_join(smupdown_id, NULL);
	pthread_join(smleftright_id, NULL);
    printf("\n\n\n\n\n\n===========================================\n");
    printf("===== Check Over ,Thanks For Use ^_^ ======\n");
    printf("===========================================\n");
    return;


CAMERA_ERROR:
    printf("Camera Cannot Work !!I Won't Go On !!!!\n\n\nI Will Reboot In 5 Seconds !!\n");
    for(i=1;i<6;i++)
    {
        printf("%d....\n",i);
        sleep(1);
    }
    system("reboot");
    while(1)
        ;
STEPPER_MOTOR_ERROR:
    printf("Stepper_Motor Cannot Work !\n");
    err_fd=open("/err.txt",O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    close(err_fd);
    return;
}
