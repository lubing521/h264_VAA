/*
 * =====================================================================================
 *
 *       Filename:  volume_set.c
 *
 *    Description:  set volume 
 *
 *        Version:  1.0
 *        Created:  2012年09月28日 17时04分44秒
 *       Revision:  none
 *       Compiler:  unicore32-linux-gcc
 *
 *         Author:  XueJilong
 *         E-mail:  xuejilong@foxmail.com
 *   Organization:  seuic
 *
 * =====================================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <linux/input.h>
#include <unistd.h>
#include <fcntl.h>

#define VOLUME_DEV_NAME "/dev/cs3700_volume" 
#define ORG_VOLUME_DEV_NAME "/sys/devices/platform/soc-audio/speak_power" 
#define KBD_DEV_NAME "/dev/event0"
#define MAX_VOLUME '6'
#define MIN_VOLUME '3'

int main()
{
    struct input_event ev;
    int key_fd,volume_fd,org_volume_fd;
    char max = MAX_VOLUME;
    char min = MIN_VOLUME;

    char org;
    org_volume_fd = open(ORG_VOLUME_DEV_NAME,O_RDONLY);
    if(org_volume_fd < 0){
        printf("open org volume fd error !\n");
    }
    read(org_volume_fd,&org,sizeof(org));
    /*printf("org is %c\n",org);*/
    volume_fd = open(VOLUME_DEV_NAME,O_WRONLY);
    if(org != '1'){
        write(volume_fd,&max,1);
    }
    else{
        write(volume_fd,&min,1);
    }
    close(volume_fd);
    close(org_volume_fd);

    key_fd = open(KBD_DEV_NAME,O_RDONLY);
    if(key_fd < 0){
        printf("open key error !\n");
    }
    while(1){
        read(key_fd,&ev,sizeof(ev));
        printf("ev.code is %d,%d\n",ev.code,ev.value);
        if(ev.code == 139){
            volume_fd = open(VOLUME_DEV_NAME,O_WRONLY);
            if(ev.value){
                write(volume_fd,&max,1);
            }
            else{
                write(volume_fd,&min,1);
            }
            close(volume_fd);
        }/* end of if(ev.code == 139) */
    }/* end of while(1) */
    close(key_fd);
    return 0;
}

