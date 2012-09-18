/*
 * playback.c
 *	Audio playback
 *	jgfntu@163.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>					/* for O_WRONLY ... */
#include <linux/soundcard.h>		/* for oss  */
#include <sys/ioctl.h>
#include <pthread.h>
#include <semaphore.h>
#include "types.h"
#include "audio.h"

/* ---------------------------------------------------------- */
#define ID_RIFF 0x46464952
#define ID_WAVE 0x45564157
#define ID_FMT  0x20746d66
#define ID_DATA 0x61746164

#define FORMAT_PCM 1

struct wav_header {
	/* RIFF WAVE Chunk */
    u32 riff_id;
    u32 riff_sz;
    u32 riff_fmt;
    /* Format Chunk */
    u32 fmt_id;
    u32 fmt_sz;
    u16 audio_format;
    u16 num_channels;
    u32 sample_rate;
    u32 byte_rate;       /* sample_rate * num_channels * bps / 8 */
    u16 block_align;     /* num_channels * bps / 8 */
    u16 bits_per_sample;
    /* Data Chunk */
    u32 data_id;
    u32 data_sz;
}__attribute__((packed));
unsigned un_OSS_RATE[]={8000,11025,12000,16000,22050,24000,32000,44100,48000};
static struct wav_header hdr;
static int stat_play=0;

int playback_on = 0;		/* TODO (FIX ME) now the software on cellphone may not support talk_playback opcode command, we enable first */

extern int oss_fd_play;
extern sem_t start_music;
extern int cur_sound;
extern int pre_sound;
/* ---------------------------------------------------------- */
/* 
 * playback buffer data (double buffer, be called from ./receive.c)
 */
int playback_buf(int fd,u8 *play_buf, int len)
{
	int rc=0;
	int left_len = len;
	u8* buf = play_buf;
#if 0	
    int tmp;
    for(tmp=0;tmp<100;tmp++)
    {
        printf("play_buf[%d]=%d   ",tmp,play_buf[tmp]);
    }
    printf("\n");
#endif
	if ((fd < 0) | play_buf == NULL)
		return 0;
	
	/* TODO (FIX ME) */
   	rc = write(fd, buf, left_len);

    //printf("playback_buf>>>>>> rc= %d,left_len=%d\n",rc,left_len);
	if (rc < 0)
    {
		printf("oss write error! error num is %d\n",rc);
        return -1;
    }
	if (rc != left_len)
		printf("###short write error(oss): %dB###\n", rc);

	return 0;
}
int speak_power(char *state)
{
    int speak_power_fd;
	speak_power_fd=open(SPEAK_POWER,O_WRONLY);
    if(speak_power_fd<0)
    {
        return -1;
    }
//    printf("set speak_power to %s\n",state);
    write(speak_power_fd,state);//on/off
    close(speak_power_fd);
    return 0;
}
int speak_power_off()
{
    char state1[]="off";
    if(speak_power(state1)<0)
        printf("set speak_power off failed ! \n");
    printf("Close Speaker !!\n");
}
int speak_power_on()
{
    char state1[]="on";
    if(speak_power(state1)<0)
        printf("set speak_power off failed ! \n");
}
int volume_set(char volume)
{
    /*int volume_fd;*/
	/*volume_fd=open(CS3700_VOLUME,O_WRONLY);*/
    /*if(volume_fd<0)*/
    /*{*/
        /*//printf("volume_fd open failed !!!\n");*/
        /*return -1;*/
    /*}*/
    /*write(volume_fd,&volume);//0~7*/
    /*printf("set volume to %c\n",volume);*/
    /*close(volume_fd);*/
    return 0;
}
/*
 * set oss configuration
 */
int set_oss_play_config(int fd, unsigned rate, u16 channels, int bit)
{
    int status, arg;
    char volume = '5';
    if(pre_sound == cur_sound){
        return 0;
    }
    else{
        pre_sound = cur_sound;
    }
    pthread_mutex_lock(&i2c_mutex_lock);
    if(volume_set(volume))
        printf("volume set failed !\n");
    if(stat_play == 1)
    {
        set_i2s_rate(rate);
        pthread_mutex_unlock(&i2c_mutex_lock);
        return;
    }
    if(ioctl(fd,SNDCTL_DSP_RESET) != 0)
        printf("OSS Reset Failed !\n");
    /* set audio rate */
    //rate =8000;
    arg	= rate;
    status = ioctl(fd, SNDCTL_DSP_SPEED, &arg);
    //	printf("status is %d   arg is %d\n",status,arg);
    if (status == -1)
    {
        printf("SNDCTL_DSP_SPEED ioctl failed,status is %d\n",status);
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -5;
    }
    if (arg != rate)
    {
        printf("unable to set number of rate\n");
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -6;
    }
    /* set audio bit */
    arg = bit;
    status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
    //	printf("status is %d   arg is %d\n",status,arg);
    if (status == -1)
    {
        printf("SOUND_PCM_WRITE_BITS ioctl failed,status is %d\n",status);
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -1;
    }
    if (arg != bit)
    {
        printf("unable to set sample size\n");
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -2;
    }

    /* set audio channels */
    arg = channels;	
    //arg = channels = 2;	
    status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
    //status = ioctl(fd, SNDCTL_DSP_STEREO, &arg);
    //	printf("status is %d   arg is %d\n",status,arg);
    if (status == -1)
    {
        printf("SOUND_PCM_WRITE_CHANNELS ioctl failed,status is %d\n",status);
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -3;
    }
    if (arg != channels)
    {
        printf("unable to set number of channels\n");
        pthread_mutex_unlock(&i2c_mutex_lock);
        return -4;
    }

    stat_play = 1;
    printf("rate is %d,channels is %d,bit is %d\n",rate,channels,bit);
    pthread_mutex_unlock(&i2c_mutex_lock);
    return 0;
}

/*
 * parse the wav file header for alsa configuration
 */	
int parse_wav_header(u8 *buf, u32 fd)
{
	struct wav_header *phdr = (struct wav_header *)buf;
	
	fprintf(stderr,"playback: %d ch, %d hz, %d bit, %s, file_size %ld\n",
         phdr->num_channels, phdr->sample_rate, phdr->bits_per_sample,
         phdr->audio_format == FORMAT_PCM ? "PCM" : "unknown", phdr->data_sz);

	if( set_oss_play_config(fd, phdr->sample_rate, phdr->num_channels, phdr->bits_per_sample) < 0 )
    {
        printf("Err: set oss play config failed!\n");
        return -1;
    }

    return 0;
}

/*
 * enable talk playback audio
 */
void start_playback()
{
	playback_on = 1;
	
	
	/* TODO (FIX ME)
	 * we may create a new thread to process the receiving func here
	 */
	//enable_playback_audio();
}

static pthread_mutex_t stop_playback_lock = PTHREAD_MUTEX_INITIALIZER;
/*
 * disable talk playback audio
 */
void stop_playback()
{
    pthread_mutex_lock(&stop_playback_lock);
	playback_on = 0;

    if( oss_fd_play>0 )
    {
		close(oss_fd_play);
		oss_fd_play = 0;
		printf("<<<Close playback audio device\n");
	}
	pthread_mutex_unlock(&stop_playback_lock);

	printf("<<<Stop playback audio ...\n");
}



