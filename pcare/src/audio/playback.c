/*
 * playback.c
 *	Audio playback
 *	jgfntu@163.com
 */

#include <stdio.h>
#include <linux/soundcard.h>		/* for oss  */
#include <sys/ioctl.h>
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

int playback_on = 0;		/* TODO (FIX ME) now the software on cellphone may not support talk_playback opcode command, we enable first */

extern int oss_open_flag, oss_close_flag, oss_fd;
extern sem_t start_music;
/* ---------------------------------------------------------- */
/* 
 * playback buffer data (double buffer, be called from ./receive.c)
 */
int playback_buf(u8 *play_buf, int len)
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
	if ((oss_fd < 0) | play_buf == NULL)
		return 0;
	
	/* TODO (FIX ME) */
   	rc = write(oss_fd, buf, left_len);

    //printf("playback_buf>>>>>> rc= %d,left_len=%d\n",rc,left_len);
	if (rc < 0)
		perror("oss write error!\n");
	if (rc != left_len)
		printf("###short write error(oss): %dB###\n", rc);

	return 0;
}

/*
 * set oss configuration
 */
int set_oss_play_config(int fd, unsigned rate, u16 channels, int bit)
{
	int status, arg;
	
	oss_fd = fd;
	
	/* set audio bit */
	arg = bit;
	status = ioctl(oss_fd, SOUND_PCM_WRITE_BITS, &arg);
	if (status == -1)
    {
		perror("SOUND_PCM_WRITE_BITS ioctl failed");
        return -1;
    }
	if (arg != bit)
    {
    	perror("unable to set sample size");
        return -2;
    }
    
    /* set audio channels */
	arg = channels;	
	status = ioctl(oss_fd, SOUND_PCM_WRITE_CHANNELS, &arg);
	if (status == -1)
    {
		perror("SOUND_PCM_WRITE_CHANNELS ioctl failed");
        return -3;
    }
	if (arg != channels)
    {
		perror("unable to set number of channels");
        return -4;
    }
	
	/* set audio rate */
	arg	= rate;
	status = ioctl(oss_fd, SOUND_PCM_WRITE_RATE, &arg);
	if (status == -1)
    {
		perror("SOUND_PCM_WRITE_WRITE ioctl failed");
        return -5;
    }
	if (arg != rate)
    {
		perror("unable to set number of rate");
        return -6;
    }

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
	
	sem_wait(&start_music);
#if 0
	/* TODO (FIX ME)
	 * clear AVdata socket recving buffer
	 */
	clear_recv_buf();
#endif
	/* tell cellphone to start sending file */
	send_talk_resp();
	
	/* TODO (FIX ME)
	 * we may create a new thread to process the receiving func here
	 */
	enable_playback_audio();
}

/*
 * disable talk playback audio
 */
void stop_playback()
{
	playback_on = 0;

	oss_close_flag--;
	
	if (oss_close_flag == 0) {
		close(oss_fd);
		oss_fd = -1;
	    oss_open_flag = 0;
		printf("<<<Close playback audio device\n");
	}
	
	printf("<<<Stop playback audio ...\n");
}



