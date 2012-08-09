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
#define LED_Statue "/sys/devices/platform/sep0611_led.1/leds/LED_Statue/brightness"
#define CAMERA_DEV "/dev/video0"

int led_flag =1;
pthread_t led_flash_id;

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
        if(!led_flag)
            break;
        led_on();
        usleep(50000);
        led_off();
        usleep(500000);
    }
    return;
}

int step_motor()
{
    int n,i,stepper_motor_fd;
    printf("Open Stepper_Motor .....");
    stepper_motor_fd=open(STEPPER_MOTOR_DEV,O_WRONLY);
    if(stepper_motor_fd < 0)
    {
        printf("FAILED!!!\n");
        return -1;
    }
    printf("OK!!!\n");
    printf("Stepper_Motor Turn Down .....");
    for(n=0;n<STEPER_PHASE_UP;n++ )
    {
        for(i = 0;i<8;i++)
        {
            ioctl(stepper_motor_fd,SMUPDOWN_CONFIG_DOWN,&i);
            usleep(1000);
        }
    }
    printf("OK!!!\n");
    printf("Stepper_Motor Turn Up .....");
    for(n=0;n<STEPER_PHASE_UP;n++ )
    {
        for(i = 7;i>=0;i--)
        {
            ioctl(stepper_motor_fd,SMUPDOWN_CONFIG_UP,&i);
            usleep(1000);
        }
    }
    printf("OK!!!\n");
    printf("Stepper_Motor Turn Right .....");
    for(n=0;n<STEPER_PHASE_LEFT;n++ )
    {
        for(i = 0;i<8;i++)
        {
            ioctl(stepper_motor_fd,SMLEFTRIGHT_CONFIG_RIGHT,&i);
            usleep(1000);
        }
    }
    printf("OK!!!\n");
    printf("Stepper_Motor Turn Left .....");
    for(n=0;n<STEPER_PHASE_LEFT;n++ )
    {
        for(i = 7;i>=0;i--)
        {
            ioctl(stepper_motor_fd,SMLEFTRIGHT_CONFIG_LEFT,&i);
            usleep(1000);
        }
    }
    printf("OK!!!\n");
    printf("Close Stepper_Motor .....");
    ioctl(stepper_motor_fd,SMLEFTRIGHT_POWER,NULL);
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
    int ret,err_fd;
    printf("\n===========================================\n");
    printf("========= Now Start Check Myself ==========\n");
    printf("===========================================\n");
    led_flag=1;
	ret = pthread_create(&led_flash_id,NULL,(void *)led_flash,NULL); 
    if(camera() < 0)
        goto CAMERA_ERROR;
    if(step_motor() < 0)
        goto STEPPER_MOTOR_ERROR;
    printf("\n\n\n\n\n\n===========================================\n");
    printf("===== Check Over ,Thanks For Use ^_^ ======\n");
    printf("===========================================\n");
    return;


CAMERA_ERROR:
    printf("Camera Cannot Work !!I Won't Go On !!!!\n\n\nPlease Try to Reset Me !!\n");
    while(1)
        ;
STEPPER_MOTOR_ERROR:
    printf("Stepper_Motor Cannot Work !\n");
    err_fd=open("/err.txt",O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    close(err_fd);
    return;
}
