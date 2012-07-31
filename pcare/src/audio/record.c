/*
 * record.c
 *	Audio capture
 *	jgfntu@163.com
 */

#include <linux/soundcard.h>		/* for oss  */
#include <semaphore.h>
#include "audio.h"
#include "types.h"
#include "protocol.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#include "adpcm.h"

/* ---------------------------------------------------------- */

int capture_on;

extern int oss_open_flag, oss_close_flag, oss_fd;
/* ---------------------------------------------------------- */

int oss_buf_size = MAX_READ_LEN;
int data_buf_size = MAX_READ_LEN / 4;

static u8 pcm_data_buf[MAX_READ_LEN] = {0};			/* TODO (FIX ME) can use malloc */
static u8 audio_data[MAX_READ_LEN / 4 + 3] = {0};
static adpcm_state_t adpcm_state_curr, adpcm_state_next;

/* ---------------------------------------------------------- */
/*
 * set oss record configuration
 */
void set_oss_record_config(int fd, unsigned rate, u16 channels, int bit)
{
	int status, arg;
	printf("rate is %d,channels is %d,bit is %d\n",rate,channels,bit);
	/* set audio bit */
	arg = bit;
	status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
	printf("status is %d   arg is %d\n",status,arg);
	if (status == -1)
		printf("SOUND_PCM_WRITE_BITS ioctl failed,status is %d\n",status);
	if (arg != bit)
    	printf("unable to set sample size\n");
    
    /* set audio channels */
	arg = channels;	
	status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
	printf("status is %d   arg is %d\n",status,arg);
	if (status == -1)
		printf("SOUND_PCM_WRITE_CHANNELS ioctl failed,status is %d\n",status);
	if (arg != channels)
		printf("unable to set number of channels\n");
	
	/* set audio rate */
	arg	= rate;
	//arg	= 8000;
	status = ioctl(fd, SNDCTL_DSP_SPEED, &arg);
	//status = ioctl(fd, SOUND_PCM_WRITE_RATE, &arg);
	printf("status is %d   arg is %d\n",status,arg);
	if (status == -1)
		printf("SOUND_PCM_WRITE_WRITE ioctl failed,status is %d\n",status);
	if (arg != rate)
		printf("unable to set number of rate\n");
}

/* ------------------------------------------------------------------------------ */
/*
 * Start recording data (oss) ...
 */
static int record_oss_data(u8 *buffer, u32 oss_buf_size)
{
	/* TODO (FIX ME) */
	read(oss_fd, buffer, oss_buf_size);
}

/*
 * store the record data in queue
 */
static int store_audio_data(void)
{
	/* TODO (FIX ME) */
	/* use adpcm algorithem to compress pcm data,
	 * eg: 4KB pcm data can be compressed to 1KB;
	 * struct audio_data u8  ado_len is audio actual length,
	 * intent to send sample and index of adpcm parames,
	 * put them in the ado_data[0] area
	 *
	 */
	/* convert pcm to adpcm */
	/* TODO (FIX ME) */
	record_oss_data(pcm_data_buf, oss_buf_size);
	
	//fprintf(stderr, "*");
#if 1
	/* convert pcm to adpcm */
	adpcm_coder((short *)pcm_data_buf, (char *)audio_data, oss_buf_size, &adpcm_state_next);

	memcpy((u8 *)(audio_data + data_buf_size), (u8 *)(&adpcm_state_curr), 3);

	adpcm_state_curr = adpcm_state_next;
#endif
}

/*
 * read audio data by frames
 */
static void read_audio_frame()
{
    //int pcm_fd;
    //unsigned long wr_len;
    //pcm_fd=open("/11.pcm",O_WRONLY|O_CREAT,S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
	printf(">>>Start capturing audio data ...\n");
    set_oss_record_config(oss_fd,RECORD_RATE,RECORD_CHANNELS,RECORD_BIT);
	
	while (capture_on) {

		store_audio_data();

		//send_audio_data(pcm_data_buf, oss_buf_size);
		send_audio_data(audio_data, data_buf_size);
        //write(pcm_fd,pcm_data_buf,oss_buf_size);
        //wr_len+=oss_buf_size;
        //write(pcm_fd,audio_data,data_buf_size);
        //wr_len+=data_buf_size;

	};
    //printf("wr_len is %lx\n",wr_len);
    //close(pcm_fd);
}

/* ---------------------------------------------------------- */
/*
 * The audio capture thread entry
 */
void *audio_capture_thread(void *args)
{
	pthread_detach(pthread_self());
	
	/* read audio data and send it to network */
	read_audio_frame();

	pthread_exit(0);
}

/* ---------------------------------------------------------- */
/*
 * Start capture
 */
void start_capture(void)
{	
	capture_on = 1;

	enable_capture_audio();

}

/*
 * stop capture
 */
void stop_capture(void)
{
	capture_on = 0;

    if( oss_fd > 0 )
    {
		close(oss_fd);
		oss_fd = 0;
		printf("<<<Close capture audio device ...\n");
	}
	
	printf("<<<Stop capture audio ...\n");
}
/* ---------------------------------------------------------- */


