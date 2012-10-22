#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>
//#include "syswait.h"

char *name = "/sys/devices/platform/sep0611_led.1/leds/LED_Statue/brightness";
//struct itimerval tick;
int led_flag =1;
pthread_mutex_t led_mutex_lock = PTHREAD_MUTEX_INITIALIZER;

void led_on()
{
	char buf[2] = "1";
    int fd,size;
    pthread_mutex_lock(&led_mutex_lock);
    fd = open(name, O_RDWR);
    if(fd == 0)
    {
        printf("led open error\n");
        pthread_mutex_unlock(&led_mutex_lock);
        return;
    }
    write(fd, buf, strlen(buf));
    close(fd);
    pthread_mutex_unlock(&led_mutex_lock);
    return;
}

void led_off()
{
	char buf[2]="0";
	int fd,size;
    pthread_mutex_lock(&led_mutex_lock);
	fd = open(name, O_WRONLY);
    if(fd == 0)
    {
        printf("led open error\n");
        pthread_mutex_unlock(&led_mutex_lock);
        return;
    }
    write(fd, buf, strlen(buf));
    close(fd);
    pthread_mutex_unlock(&led_mutex_lock);
	return;
}

void led_flash()
{
    pthread_detach(pthread_self());
	while(1){
        if(!led_flag){
            break;
        }
        else{
            led_on();
            sleep(2);
        }
        if(!led_flag){
            break;
        }
        else{
            led_off();
            sleep(2);
        }
	}
	return;
}
