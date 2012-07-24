#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <fcntl.h>
//#include "syswait.h"

char *name = "/sys/devices/platform/sep0611_led.1/leds/LED_Statue/brightness";
//struct itimerval tick;
static int led_flag;

void led_on()
{
//	char buf[2] = {'49' , '\0'};
	char buf[2] = "1";
	char s[3];
//	buf = "1";
	int fd,size;
	fd = open(name, O_RDWR);
	write(fd, buf, strlen(buf));
	printf("%d\n",fd);
	if(fd == 0)
		{
		printf("error\n");
		return;
		}
	read(fd, s, 2);
	printf("%s\n",s);
	printf("led is on\n");
	close(fd);
	return;
}

void led_off()
{
	led_flag = 0;
	char buf[2]="0";
//	buf = "0";
	int fd,size;
	fd = open(name, O_WRONLY);
	write(fd, buf, strlen(buf));
	close(fd);
	return;
}

/*void led_flash_true()
{
//	led_on();
	led_off();
	led_on();
	return;
}

void led_flash_on() 
{
	int ret = 0;
	signal(SIGALRM, led_flash_true);
	struct itimerval tick;
	memset(&tick, 0, sizeof(tick));
			
	tick.it_value.tv_sec = 1;
	tick.it_value.tv_usec = 0;
	
	tick.it_interval.tv_sec = 1;
	tick.it_interval.tv_usec = 0;
	
	ret = setitimer(ITIMER_REAL, &tick, NULL);
	if(ret) {
		printf("Set timer failed!!\n");
	}
	
	while(1) {
		if(!led_flag)
			break;
		pause();
	}
	
	return<F9><F8>;
}

void led_flash_off()
{
	int ret;
	led_flag =0;
	ret = setitimer(0, NULL, NULL);
	return;
}*/

void led_flash()
{
	while(1){
		if(!led_flag)
			break;
		led_on();
		usleep(10);
		led_off();
		usleep(500);		
	}
	return;
}









