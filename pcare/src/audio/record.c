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
	
	/* set audio bit */
	arg = bit;
	status = ioctl(oss_fd, SOUND_PCM_WRITE_BITS, &arg);
	if (status == -1)
		perror("SOUND_PCM_WRITE_BITS ioctl failed");
	if (arg != bit)
    	perror("unable to set sample size");
    
    /* set audio channels */
	arg = channels;	
	status = ioctl(oss_fd, SOUND_PCM_WRITE_CHANNELS, &arg);
	if (status == -1)
		perror("SOUND_PCM_WRITE_CHANNELS ioctl failed");
	if (arg != channels)
		perror("unable to set number of channels");
	
	/* set audio rate */
	arg	= rate;
	status = ioctl(oss_fd, SOUND_PCM_WRITE_RATE, &arg);
	if (status == -1)
		perror("SOUND_PCM_WRITE_WRITE ioctl failed");
	if (arg != rate)
		perror("unable to set number of rate");
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

	/* convert pcm to adpcm */
	adpcm_coder((short *)pcm_data_buf, (char *)audio_data, oss_buf_size, &adpcm_state_next);

	memcpy((u8 *)(audio_data + data_buf_size), (u8 *)(&adpcm_state_curr), 3);

	adpcm_state_curr = adpcm_state_next;

}

/*
 * read audio data by frames
 */
static void read_audio_frame()
{
	printf(">>>Start capturing audio data ...\n");
	
	while (capture_on) {

		store_audio_data();

		send_audio_data(audio_data, data_buf_size);

	};
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

	oss_close_flag--;
	
	if (oss_close_flag == 0) {
		close(oss_fd);
		oss_open_flag = 0;
		oss_fd = -1;
		printf("<<<Close capture audio device ...\n");
	}
	
	printf("<<<Stop capture audio ...\n");
}
/* ---------------------------------------------------------- */


