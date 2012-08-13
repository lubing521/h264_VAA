#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
//#include "syswait.h"

char *name = "/sys/devices/platform/sep0611_led.1/leds/LED_Statue/brightness";
//struct itimerval tick;
int led_flag =1;

void led_on()
{
	char buf[2] = "1";
    int fd,size;
    fd = open(name, O_RDWR);
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
	fd = open(name, O_WRONLY);
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
    pthread_detach(pthread_self());
	while(1){
		led_on();
		sleep(2);
		if(!led_flag)
			break;
		led_off();
		sleep(2);
		if(!led_flag)
			break;
	}
	return;
}
