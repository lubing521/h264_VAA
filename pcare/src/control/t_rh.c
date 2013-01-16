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
int tem_fix;
float tem1;
struct timeval t_rh_time;
float exp1(float x);
float exp2(float x);

int get_tem()
{
    unsigned char buf[10]={0};
    /*char *name = "/sys/devices/platform/sep0611-i2c.1/i2c-1/1-0040/tem";*/
    char *name = "/sys/bus/i2c/devices/0-0040/tem";
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
    if(tem0 < 0){
        printf("get_tem err \n");
        tem_integer = 0xDD;
        tem_decimal = 0xDD;
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -1;
    }
    tem1 = ((float)tem0)/65536*175.72-46.85;
    tem1 += (float)tem_fix;/* tem_fix */
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
    /*char *name = "/sys/devices/platform/sep0611-i2c.1/i2c-1/1-0040/rh";*/
    char *name = "/sys/bus/i2c/devices/0-0040/rh";
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
    if(rh0 < 0){
        printf("get_rh err \n");
        rh = 0xDD;
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -1;
    }
    rh1 = ((float)rh0)/65536*125-6;
    //printf("rh1 is %f\n",rh1);
    rh1 = rh1 * exp2(4283.78 * (-tem_fix)/(243.12 + tem1 - tem_fix)/(243.12 + tem1));
    //printf("fixed rh1 is %f ,fixed tem is %d\n",rh1,tem_integer);
    rh = ((int)rh1);
    close(fd);
    pthread_mutex_unlock(&i2c_mutex_lock);
    return 0;
}

int start_measure(void)
{
    /* tem_fix */
    if(tem_fix > -3)
    {
        gettimeofday(&t_rh_time,NULL);
        if((t_rh_time.tv_sec/60 > 10) && (t_rh_time.tv_sec/60 <= 20)){
            tem_fix = -1;
        }
        else if((t_rh_time.tv_sec/60 > 20) && (t_rh_time.tv_sec/60 <= 30)){
            tem_fix = -2;
        }
        else if(t_rh_time.tv_sec/60 > 30){
            tem_fix = -3;
        }
        else{
            tem_fix = 0;
        }
        //printf("tem_fix is %d\n",tem_fix);
    } /* tem_fix */
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
    double tmp=1,sum=0,sum2,i=1;
    sum+=1;
    while(tmp>1e-6)
    {
        tmp*=x/i;
        sum+=tmp;
        i++;
    }
    return (float)sum;
}
