#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
//#include <tgmath.h>
#include "types.h"
#include "protocol.h"
#include "t_rh.h"
#include "audio.h"

unsigned int tem_integer;
unsigned int tem_decimal;
unsigned int rh;
unsigned int tem_fix;
float tem1;
struct timeval t_rh_time;
float exp1(float x);
float exp2(float x);

int get_tem()
{
    unsigned char buf[10]={0};
    char *name = "/sys/devices/platform/sep0611-i2c.1/i2c-1/1-0040/tem";
    int fd,size;
    int tem0;
    pthread_mutex_lock(&i2c_mutex_lock);
    fd = open(name ,O_RDONLY);
    size = read(fd, buf, 5);
    if(size < 0)
    {
        tem_integer=-1;
        tem_decimal=-1;
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -1;
    }
    tem0 = atoi(buf);
    tem1 = ((float)tem0)/65536*175.72-46.85;
    tem_integer = ((int)tem1) - tem_fix;/* tem_fix */
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
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -1;
    }
    rh0 = atoi(buf);
    rh1 = ((float)rh0)/65536*125-6;
    rh1 = rh1 * exp1(4283.78 * tem_fix/(243.12 + tem1)/(243.12 + tem1 - tem_fix));
    rh = ((int)rh1);
    close(fd);
    pthread_mutex_unlock(&i2c_mutex_lock);
    return 0;
}

int start_measure(void)
{
    gettimeofday(&t_rh_time,NULL);
    /* tem_fix */
    if((t_rh_time.tv_sec/60 > 10) && (t_rh_time.tv_sec/60 <= 20))
        tem_fix = 1;
    if((t_rh_time.tv_sec/60 > 20) && (t_rh_time.tv_sec/60 <= 30))
        tem_fix = 2;
    else if(t_rh_time.tv_sec/60 > 30)
        tem_fix = 3;
    else
        tem_fix = 0;
    /* tem_fix */
    if((get_tem() < 0) | (get_rh() < 0 ))
        return -1;
    return 0;
}
float exp1(float x)
{
    float tmp=1,sum=0;
    int i;
    sum+=1;
    for (i=1;i<20;i++)
    {
        tmp*=x/i;
        sum+=tmp;
    }
    return sum;
}
float exp2(float x)
{
    float tmp=1,sum=0,sum2,i=1;
    sum+=1;
    while(tmp>1e-6)
    {
        tmp*=x/i;
        sum+=tmp;
        i++;
    }
    return sum;
}
