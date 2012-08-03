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

extern int oss_open_flag, oss_close_flag, oss_fd_record;
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
    char volume = '7';
#if 1
    char state1[]="coff";
    char state2[]="con";
    if(speak_power(state1)<0)
        printf("set codec off failed ! \n");
    if(speak_power(state2)<0)
        printf("set codec on failed ! \n");
#endif
    if(volume_set(volume)<0)
       printf("volume set failed !\n");
	printf("rate is %d,channels is %d,bit is %d\n",rate,channels,bit);
	/* set audio bit */
	arg = bit;
	status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
//	printf("status is %d   arg is %d\n",status,arg);
	if (status == -1)
		printf("SOUND_PCM_WRITE_BITS ioctl failed,status is %d\n",status);
	if (arg != bit)
    	printf("unable to set sample size\n");
    
    /* set audio channels */
	arg = channels;	
	status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
//	printf("status is %d   arg is %d\n",status,arg);
	if (status == -1)
		printf("SOUND_PCM_WRITE_CHANNELS ioctl failed,status is %d\n",status);
	if (arg != channels)
		printf("unable to set number of channels\n");
	
	/* set audio rate */
	arg	= rate;
	//arg	= 8000;
	status = ioctl(fd, SNDCTL_DSP_SPEED, &arg);
	//status = ioctl(fd, SOUND_PCM_WRITE_RATE, &arg);
//	printf("status is %d   arg is %d\n",status,arg);
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
    int tmp;
	/* TODO (FIX ME) */
	if(read(oss_fd_record, buffer, oss_buf_size) < 0){
        printf("sound  read  error  !\n");
        return -1;
    }
    //for(tmp =0;tmp<10;tmp++)
    //printf("buffer[%d]is %d \n",tmp,buffer[tmp]);
    return 0;
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
    if(record_oss_data(pcm_data_buf, oss_buf_size) < 0){
        oss_fd_record = open(OSS_AUDIO_DEV,O_RDONLY);
        if(oss_fd_record < 0){
            printf("oss_fd_record open failed !\n");
            return -1;
        }
    }

    //fprintf(stderr, "*");
#if 1
	/* convert pcm to adpcm */
	adpcm_coder((short *)pcm_data_buf, (char *)audio_data, oss_buf_size, &adpcm_state_next);

	memcpy((u8 *)(audio_data + data_buf_size), (u8 *)(&adpcm_state_curr), 3);

	adpcm_state_curr = adpcm_state_next;
#endif
    return 0;
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
    //close(oss_fd_record);
    //oss_fd_record=open(OSS_AUDIO_DEV,O_RDONLY);
    set_oss_record_config(oss_fd_record,RECORD_RATE,RECORD_CHANNELS,RECORD_BIT);
	
	while (capture_on) {

		if(store_audio_data() < 0){
            break;
        }

		//send_audio_data(pcm_data_buf, oss_buf_size);
		send_audio_data(audio_data, data_buf_size);
    //    printf("send_audio_data size is %d\n",data_buf_size);
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
#if 0
    if( oss_fd_record > 0 )
    {
		close(oss_fd_record);
		oss_fd_record = 0;
		printf("<<<Close capture audio device ...\n");
	}
#endif	
	printf("<<<Stop capture audio ...\n");
}
/* ---------------------------------------------------------- */


