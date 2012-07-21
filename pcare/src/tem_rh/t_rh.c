#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "protocal.h"
#include "Types.h"

unsigned int tem_integer;
unsigned int tem_decimal;
unsigned int rh;

int get_tem()
{
	unsigned char buf[10];
	char *name = "/sys/devices/platform/sep0611-i2c.1/i2c-1/1-0040/tem";
	int fd,size;
	int tem0;
	float tem1;
	fd = open(name ,O_RDONLY);
	size = read(fd, buf, 5);
	printf("%s\n",buf);
	tem0 = atoi(buf);
	tem1 = ((float)tem0)/65536*175.72-46.85;
	tem_integer = ((int)tem1);
//	printf("%f",tem1-tem_integer);
	tem_decimal = (int)((tem1 - tem_integer)*10);
//	printf("%d %d\n",tem_integer,tem_decimal);
	close(fd);
	return 0;
}

int get_rh()
{
	unsigned char buf[10];
	char *name = "/sys/devices/platform/sep0611-i2c.1/i2c-1/1-0040/rh";
	int fd,size;
	int rh0;
	float rh1;
	fd = open(name , O_RDONLY);
	size = read(fd, buf, 5);
	printf("%s\n",buf);
	rh0 = atoi(buf);
	rh1 = ((float)rh0)/65536*125-6;
	rh = ((int)rh1);
	close(fd);
	return 0;
}

void start_measure()
{
	get_tem();
	get_rh();
	return;
}











