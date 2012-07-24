/*
 * main.c
 *  the wifi car main program.
 *  chenguangming@wxseuic.com
 *  zhangjunwei166@163.com
 *  jgfntu@163.com
 *  2012-01-20 v1.0
 */

#include <stdio.h>
#include "camera.h"
#include "audio.h"
#include "utils.h"
#include "led_control.h"
#include <pthread.h>
extern int led_flag;
pthread_t led_flash_id;
int main()
{
    int ret;
    led_flag=1;
	ret = pthread_create(&led_flash_id,NULL,(void *)led_flash,NULL); 
    if (ret != 0)
           printf("create led_flash thread error\n");
#if 1
#ifdef ENABLE_VIDEO	
	if (init_camera() < 0)
		return 0;
#endif
		
#ifdef ENABLE_AUDIO
	if (init_audio() < 0)
		return 0;
#endif	
#endif
	init_receive();
	init_motor();
#if 0
	int i=0;
	picture_t *pic;
	
	while(1){
		if(get_picture(&pic) >= 0){
			//store_jpg(pic);
			put_picture(&pic);
			//printf("%d\n", pic->index);
			if(i++ > 8000)
				return 0;
		}
	}
#endif
    while(1)
    {
        printf(">>>>>>>>>Waiting For Client<<<<<<<<<<\n");
	    network();
    }
	return 0;
}
