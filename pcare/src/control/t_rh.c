#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "types.h"
#include "protocol.h"
#include "t_rh.h"
#include "audio.h"

unsigned int tem_integer;
unsigned int tem_decimal;
unsigned int rh;

int get_tem()
{
    unsigned char buf[10]={0};
    char *name = "/sys/devices/platform/sep0611-i2c.1/i2c-1/1-0040/tem";
    int fd,size;
    int tem0;
    float tem1;
    pthread_mutex_lock(&i2c_mutex_lock);
    fd = open(name ,O_RDONLY);
    size = read(fd, buf, 5);
    if(size < 0)
    {
        tem_integer=-1;
        tem_decimal=-1;
        return -1;
    }
    tem0 = atoi(buf);
    tem1 = ((float)tem0)/65536*175.72-46.85;
    tem_integer = ((int)tem1);
    //	printf("%f",tem1-tem_integer);
    tem_decimal = (int)((tem1 - tem_integer)*10);
    //	printf("%d %d\n",tem_integer,tem_decimal);
    close(fd);
    pthread_mutex_unlock(&i2c_mutex_lock);
    return 0;
}

int get_rh()
{
    unsigned char buf[10]={0};
    char *name = "/sys/devices/platform/sep0611-i2c.1/i2c-1/1-0040/rh";
    int fd,size;
    int rh0;
    float rh1;
    pthread_mutex_lock(&i2c_mutex_lock);
    fd = open(name , O_RDONLY);
    size = read(fd, buf, 5);
    if(size < 0)
    {
        rh = -1;
        return -1;
    }
    rh0 = atoi(buf);
    rh1 = ((float)rh0)/65536*125-6;
    rh = ((int)rh1);
    close(fd);
    pthread_mutex_unlock(&i2c_mutex_lock);
    return 0;
}

int start_measure(void)
{
    if((get_tem() < 0)| (get_rh() < 0 ))
        return -1;
    return 0;
}
