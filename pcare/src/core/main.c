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

int main()
{
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
    printf(">>>>>>>>>up in while(1) network<<<<<<<<<<\n");
	    network();
    printf(">>>>>>>>>down in while(1) network<<<<<<<<<<\n");
    }
    printf(">>>>>>>>>exit while(1) network<<<<<<<<<<\n");
	return 0;
}
