/*
 * audio.c
 *  deal with the audio data
 *  chenguangming@wxseuic.com
 *	jgfntu@163.com
 */
#include <stdio.h>
#include <pthread.h>
#include "audio.h"
#include <fcntl.h>

pthread_t audio_capture_td;
pthread_t audio_playback_td;
extern int capture_on;

int oss_fd_play=0;
int oss_fd_record=0;


int enable_capture_audio(void)
{
	int pcm_ret;
	int ret;

	//printf("%s\n", __func__);
	
    if( oss_fd_record <= 0 )
    {

        /* open audio(oss) device for capturing */
        oss_fd_record = open(OSS_AUDIO_DEV, O_RDONLY);
        if (oss_fd_record < 0) {
            fprintf(stderr, "Open audio(oss) device failed!\n");
            return -1;
        }
    }

	/* set oss configuration */
   // set_oss_record_config(oss_fd_record,RECORD_RATE,RECORD_CHANNELS,RECORD_BIT);
   // printf(">>>Open capture audio device\n");

	ret = pthread_create(&audio_capture_td, NULL, audio_capture_thread, NULL);
	if (ret < 0) {
		printf("Create audio capture thread faild!\n");
		return -1;
	}

	return 0;
}

/* TODO (FIX ME) not use now
 * Close the audio capture thread
 */
void close_audio_device(void)
{
	capture_on = 0;
	
	printf("<<<close audio device ...\n");
	//pthread_join(audio_capture_td, NULL);
}
