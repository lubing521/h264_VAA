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

#include "buffer.h"
#include "adpcm.h"

static void *audio_capture( void *arg );
static void *audio_send( void *arg );

enum RecordState
{
	ST_RECORDING,
	ST_STOPPING,
	ST_STOPPED
};

enum RecordOp
{
	OP_NONE,
	OP_RECORD,
	OP_STOP
};

struct Recorder
{
	enum RecordState state;
	enum RecordOp next_op;
	pthread_t send_thread;
	pthread_t capture_thread;
	pthread_mutex_t lock;
};
struct AUDIO_CFG 
{
	int rate;
	int channels;
	int bit;
	int ispk;
};

enum CaptureBufferFlag
{
	START_CAPTURE,
	KEEP_CAPTURE,
	END_CAPTURE,
	STOP_CAPTURE
};

enum SendState
{
	SOCKET_INIT,
	SOCKET_SEND,
	SOCKET_STOPPED
};

enum RecorderState
{
	RECORDER_INIT,
	RECORDER_CAPTURE,
	RECORDER_STOPPED,
	RECORDER_RESET
};

//pthread_t capture_td, send_td;
struct Recorder g_recorder;

#define BUFFER_NUM 3
BufferQueue g_capture_buffer_queue;
#define CAPTURE_QUEUE (&g_capture_buffer_queue)

u8 g_raw_buffer[2][RECORD_MAX_READ_LEN];

extern int send_data_fd;
extern int oss_fd_record;
int start_record = 0;
sem_t capture_is_start;

void init_send()
{
	sem_init(&capture_is_start,0,0);
	pthread_mutex_init(&g_recorder.lock,NULL);
	g_recorder.state = ST_STOPPED;
	g_recorder.next_op = OP_NONE;
	InitQueue(CAPTURE_QUEUE,BUFFER_NUM);
	if(pthread_create(&g_recorder.capture_thread, NULL, audio_capture, NULL) != 0) {
		perror("pthread_create");
		return ;
	}
	if(pthread_create(&g_recorder.send_thread, NULL, audio_send, NULL) != 0) {
		perror("pthread_create");
		return ;
	}
}

int set_i2s_rate(unsigned int rate)
{
    int fd,ret;
    char ch_rate1[]="11025";
    char ch_rate2[]="44100";
    printf("%s to %d\n",__func__,rate);
    fd = open(I2S_RATE,O_WRONLY);
    if(rate == 11025)
        write(fd,ch_rate1,sizeof(ch_rate1));
    else
        write(fd,ch_rate2,sizeof(ch_rate2));

}
/* ---------------------------------------------------------- */
/*
 * set oss record configuration
 */
void set_oss_record_config(int fd, unsigned rate, u16 channels, int bit)
{
	int status, arg;
    char volume = '7';
    char state1[]="off";
    if(speak_power(state1)<0)
        printf("set speak_power off failed ! \n");
    if(start_record == 1)
    {
        set_i2s_rate(rate);
        return;
    }
#if 0
    char state1[]="coff";
    char state2[]="con";
    if(speak_power(state1)<0)
        printf("set codec off failed ! \n");
    if(speak_power(state2)<0)
        printf("set codec on failed ! \n");
#endif
    if(ioctl(fd,SNDCTL_DSP_RESET) != 0)
        printf("OSS Reset Failed !\n");
    if(volume_set(volume)<0)
       printf("volume set failed !\n");
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
    start_record = 1;
	printf("rate is %d,channels is %d,bit is %d\n",rate,channels,bit);
	
}

/* ------------------------------------------------------------------------------ */
/*
 * Start recording data (oss) ...
 */
static int record_oss_data(u8 *buffer, u32 oss_buf_size)
{
    return read(oss_fd_record, buffer, oss_buf_size);
}

/* ---------------------------------------------------------- */
/*
 * Start capture
 */
void start_capture(void)
{	
	StartRecorder();
//	enable_capture_audio();
}

/*
 * stop capture
 */
void stop_capture(void)
{
	char state1[]="on";
	if(speak_power(state1)<0)
		printf("set speak_power on failed ! \n");
	StopRecorder();
	printf("<<<Stop capture audio ...\n");
}


void *audio_capture( void *arg )
{
	int state = RECORDER_INIT, i=0;
	struct AUDIO_CFG cfg = {RECORD_RATE,RECORD_CHANNELS,RECORD_BIT,0};
	Buffer *buffer;

	pthread_detach(pthread_self());
	oss_fd_record = open(OSS_AUDIO_DEV,O_RDONLY);
	if(oss_fd_record < 0)
	{
		printf("Err(audio_capture): Open audio(oss) device failed!\n");
		pthread_exit(0);
	}

	do
	{
		switch( state )
		{
			case RECORDER_INIT:
				printf("recorder init\n");
				sem_wait(&capture_is_start);
    				set_oss_record_config(oss_fd_record,cfg.rate,cfg.channels,cfg.bit);
				printf("recorder start\n");
				state = RECORDER_CAPTURE;
				break;
			case RECORDER_RESET:
				printf("recorder reset\n");
				oss_fd_record = open(OSS_AUDIO_DEV,O_RDONLY);
				if(oss_fd_record < 0)
				{
					printf("Err(audio_capture): Open audio(oss) device failed!\n");
				}
				else if( set_oss_play_config(oss_fd_record,cfg.rate,cfg.channels,cfg.bit) < 0 )
				{
					printf("Err(audio_capture): set oss play config failed!\n");
					close(oss_fd_record);
					break;
				}
				state = RECORDER_CAPTURE;
				break;
			case RECORDER_CAPTURE:
				//printf("capture\n");
				buffer = GetEmptyBuffer( CAPTURE_QUEUE );
				if( buffer == NULL )
				{
					state = RECORDER_STOPPED;
					break;
				}
				if( record_oss_data( buffer->data, RECORD_MAX_READ_LEN) <= 0 )
				{
					printf("record ret error !\n");
					state = RECORDER_RESET;
				}
				else
				{
					buffer->len = RECORD_MAX_READ_LEN;
					FillBuffer( CAPTURE_QUEUE );
				}
				break;
			case RECORDER_STOPPED:
				printf("recorder stop\n");
				CloseBufferQueue( CAPTURE_QUEUE, IN_PORT );
				//close(oss_fd_record);
				//oss_fd_record = 0;
				state = RECORDER_INIT;
				break;
		}
	}while(1);

	pthread_exit(NULL);
}

//#define CAPTURE_PROFILE
void *audio_send( void *arg )
{
	int state = SOCKET_INIT;
	Buffer *buffer;
	int error_flag;
	adpcm_state_t adpcm_state = {0, 0};
#ifdef CAPTURE_PROFILE
	int t1, t2, t3;
	int total_cap_time = 0, cap_cnt = 0, cap_time, max_cap_time = 0;
	int total_send_time = 0, send_cnt = 0, send_time, max_send_time = 0;
#endif

	pthread_detach(pthread_self());
	do
	{
		switch( state )
		{
			case SOCKET_INIT:
				printf("start send\n");
				adpcm_state.valprev = 0;
				adpcm_state.index = 0;
				state = SOCKET_SEND;
				break;
			case SOCKET_SEND:
				//printf("send audio\n");
#ifdef CAPTURE_PROFILE
				t1 = times(NULL);
#endif
				buffer = GetBuffer(CAPTURE_QUEUE);
				if( buffer == NULL )
				{
					state = SOCKET_STOPPED;
					break;
				}
#ifdef CAPTURE_PROFILE
				t2 = times(NULL);
#endif
				memcpy((u8 *)&g_raw_buffer[0][RECORD_ADPCM_MAX_READ_LEN], (u8 *)(&adpcm_state), 3);
				adpcm_coder( (short *)buffer->data, g_raw_buffer[0],RECORD_MAX_READ_LEN,&adpcm_state);
				error_flag = send_audio_data(g_raw_buffer[0],RECORD_ADPCM_MAX_READ_LEN);
#ifdef CAPTURE_PROFILE
				t3 = times(NULL);
				cap_time = t2 - t1;
				if( cap_time > max_cap_time ) max_cap_time = cap_time;
				total_cap_time += cap_time;
				cap_cnt++;
				if( cap_cnt == 10 )
				{
					printf("ave_cap=%d,max_cap=%d\n",total_cap_time/cap_cnt,max_cap_time);
					total_cap_time = 0;
					cap_cnt = 0;
				}
				send_time = t3 - t2;
				if( send_time > max_send_time ) max_send_time = send_time;
				total_send_time += send_time;
				send_cnt++;
				if( send_cnt == 10 )
				{
					printf("ave_send=%d,max_send=%d\n",total_send_time/send_cnt,max_send_time);
					total_send_time = 0;
					send_cnt = 0;
				}
#endif
				if (error_flag < 0)
				{
					state = SOCKET_STOPPED;
				}
				EmptyBuffer(CAPTURE_QUEUE);
				break;
			case SOCKET_STOPPED:
				printf("send stop\n");
				CloseBufferQueue( CAPTURE_QUEUE, OUT_PORT );
				EndRecorder();
				state = SOCKET_INIT;
				break;
		}
	}while(1);

	pthread_exit(NULL);
}

void EndRecorder()
{
	pthread_mutex_lock(&g_recorder.lock);
	printf("ToEndRecorder\n");
	g_recorder.state = ST_STOPPED;
	pthread_mutex_unlock(&g_recorder.lock);
	if(g_recorder.next_op == OP_RECORD) StartRecorder();
	return;
}
void StartRecorder()
{
	pthread_mutex_lock(&g_recorder.lock);
	printf("ToStartRecorder\n");
	g_recorder.next_op = OP_RECORD;
	if(g_recorder.state == ST_STOPPED)
	{ 
		EnableBufferQueue(CAPTURE_QUEUE);
		g_recorder.state = ST_RECORDING;
		sem_post(&capture_is_start);
	}
	pthread_mutex_unlock(&g_recorder.lock);
	return;
}

void StopRecorder()
{
	pthread_mutex_lock(&g_recorder.lock);
	printf("ToStopRecorder\n");
	g_recorder.next_op = OP_STOP;
	if( g_recorder.state == ST_RECORDING )
	{
		g_recorder.state = ST_STOPPING;
		sem_trywait(&capture_is_start);
		DisableBufferQueue(CAPTURE_QUEUE);
	}
	pthread_mutex_unlock(&g_recorder.lock);
	return;
}
/* ---------------------------------------------------------- */
