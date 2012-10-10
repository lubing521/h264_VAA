/*
 * record.c
 *	Audio capture
 *	jgfntu@163.com
 */

#include <linux/soundcard.h>		/* for oss  */
#include <semaphore.h>
#include <stdio.h>
#include <string.h>
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
extern int step_is_work;
extern int cur_sound;
extern int pre_sound;

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
	pthread_mutex_init(&g_recorder.lock,NULL);
	InitQueue(CAPTURE_QUEUE,"audio capture",BUFFER_NUM);
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
    char ch_rate_11025[]="11025";
    char ch_rate_16000[]="16000";
    char ch_rate_44100[]="44100";
    printf("   %s to %d\n",__func__,rate);
    fd = open(I2S_RATE,O_WRONLY);
    switch(rate)
    {
    case 11025:
        write(fd,ch_rate_11025,sizeof(ch_rate_11025));
        break;
    case 16000:
        write(fd,ch_rate_16000,sizeof(ch_rate_16000));
        break;
    case 44100:
        write(fd,ch_rate_44100,sizeof(ch_rate_44100));
        break;
    default:
        printf("   Unsported Rate !! Set to default 44100\n");
        write(fd,ch_rate_44100,sizeof(ch_rate_44100));
    }
    close(fd);
}
/* ---------------------------------------------------------- */
/*
 * set oss record configuration
 */
int set_oss_record_config(int fd, unsigned rate, u16 channels, int bit)
{
    int status, arg;
    char volume = '5';
    if(pre_sound == cur_sound){
        return 1;
    }
    else{
        pre_sound = cur_sound;
    }
    pthread_mutex_lock(&i2c_mutex_lock);
    if(volume_set(volume)<0)
        printf("   volume set failed !\n");
    speak_power_off();
    if(start_record == 1)
    {
        status = ioctl(fd, SOUND_PCM_SYNC, 0);
        if (status == -1)
            printf("   SOUND_PCM_WRITE_BITS ioctl failed,status is %d\n",status);
        set_i2s_rate(rate);
        pthread_mutex_unlock(&i2c_mutex_lock);
        return 1;
    }
    if(ioctl(fd,SNDCTL_DSP_RESET) != 0)
        printf("   OSS Reset Failed !\n");
    /* set audio rate */
    arg	= rate;
    //arg	= 8000;
    status = ioctl(fd, SNDCTL_DSP_SPEED, &arg);
    //status = ioctl(fd, SOUND_PCM_WRITE_RATE, &arg);
    //	printf("status is %d   arg is %d\n",status,arg);
    if (status == -1)
        printf("   SOUND_PCM_WRITE_WRITE ioctl failed,status is %d\n",status);
    if (arg != rate)
        printf("   unable to set number of rate\n");
    /* set audio bit */
    arg = bit;
    status = ioctl(fd, SOUND_PCM_WRITE_BITS, &arg);
    //	printf("status is %d   arg is %d\n",status,arg);
    if (status == -1)
        printf("   SOUND_PCM_WRITE_BITS ioctl failed,status is %d\n",status);
    if (arg != bit)
        printf("   unable to set sample size\n");

    /* set audio channels */
    arg = channels;	
    status = ioctl(fd, SOUND_PCM_WRITE_CHANNELS, &arg);
    //	printf("status is %d   arg is %d\n",status,arg);
    if (status == -1)
        printf("   SOUND_PCM_WRITE_CHANNELS ioctl failed,status is %d\n",status);
    if (arg != channels)
        printf("   unable to set number of channels\n");
    start_record = 1;
    printf("   rate is %d,channels is %d,bit is %d\n",rate,channels,bit);
    pthread_mutex_unlock(&i2c_mutex_lock);
    return 1;

}

/* ------------------------------------------------------------------------------ */
/*
 * Start recording data (oss) ...
 */
static int record_oss_data(u8 *buffer, u32 oss_buf_size)
{
	int result, t1, t2;
	result = read(oss_fd_record, buffer, oss_buf_size);
    return result;
}

/* ---------------------------------------------------------- */
/*
 * Start capture
 */
void start_capture(void)
{	
	StartRecorder();
	printf("<--Start capture audio ...\n");
//	enable_capture_audio();
}

/*
 * stop capture
 */
void stop_capture(void)
{
	StopRecorder();
	printf("<--Stop capture audio ...\n");
}


#define TIME_DIFF(t1,t2) (((t1).tv_sec-(t2).tv_sec)*1000+((t1).tv_usec-(t2).tv_usec)/1000)
void *audio_capture( void *arg )
{
	int state = RECORDER_INIT, i=0, length;
	struct AUDIO_CFG cfg = {RECORD_RATE,RECORD_CHANNELS,RECORD_BIT,0};
	Buffer *buffer;

	pthread_detach(pthread_self());
	oss_fd_record = -1;
	do
	{
		switch( state )
		{
			case RECORDER_INIT:
				printf("   Recorder Init\n");
				OpenQueueIn(CAPTURE_QUEUE);
				if( oss_fd_record < 0 )
				{
					oss_fd_record = open(OSS_AUDIO_DEV,O_RDONLY);
					if(oss_fd_record < 0)
					{
						printf("   Err(audio_capture): Open audio(oss) device failed!\n");
						break;
					}
				}
                set_oss_record_config(oss_fd_record,cfg.rate,cfg.channels,cfg.bit);
				printf("   Recorder Start\n");
				state = RECORDER_CAPTURE;
				break;
			case RECORDER_RESET:
				printf("   Recorder Reset\n");
                do{
                    oss_fd_record = open(OSS_AUDIO_DEV,O_RDONLY);
                    if(oss_fd_record < 0)
                    {
                        printf("   Err(audio_capture): Open audio(oss) [%d]device failed! Sleep 100ms And Try Again !\n",oss_fd_record);
                        usleep(100000);
                    }
                }while(oss_fd_record < 0);
				if( set_oss_record_config(oss_fd_record,cfg.rate,cfg.channels,cfg.bit) < 0 )
				{
					printf("   Err(audio_capture): set oss play config failed!\n");
					close(oss_fd_record);
					oss_fd_record = -1;
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
                length =  record_oss_data( buffer->data, RECORD_MAX_READ_LEN);
                gettimeofday(&buffer->time_stamp,NULL);
                if( length <= 0 )
				{
                    close(oss_fd_record);
					printf("   Record Ret Error ! length is %d\n",length);
					state = RECORDER_RESET;
				}
				else
				{
					buffer->len = RECORD_MAX_READ_LEN;
					FillBuffer( CAPTURE_QUEUE );
				}
				break;
			case RECORDER_STOPPED:
				printf("   Recorder Stop\n");
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
	struct timeval t1, t2, t3;
	int total_cap_time = 0, cap_cnt = 0, cap_time, max_cap_time = 0;
	int total_send_time = 0, send_cnt = 0, send_time, max_send_time = 0;
#endif

	pthread_detach(pthread_self());
	do
	{
		switch( state )
		{
			case SOCKET_INIT:
				printf("   Sender Init\n");
				OpenQueueOut(CAPTURE_QUEUE);
				adpcm_state.valprev = 0;
				adpcm_state.index = 0;
				state = SOCKET_SEND;
				break;
			case SOCKET_SEND:
				//printf("send audio\n");
#ifdef CAPTURE_PROFILE
				gettimeofday(&t1,NULL);
#endif
				buffer = GetBuffer(CAPTURE_QUEUE);
				if( buffer == NULL )
				{
					state = SOCKET_STOPPED;
					break;
				}
#ifdef CAPTURE_PROFILE
				gettimeofday(&t2,NULL);
#endif
                memcpy((u8 *)&g_raw_buffer[0][RECORD_ADPCM_MAX_READ_LEN], (u8 *)(&adpcm_state), 3);
                if(step_is_work == 1){
                    //memset(g_raw_buffer[0],0,RECORD_ADPCM_MAX_READ_LEN);
                    memset((short *)buffer->data,0,RECORD_MAX_READ_LEN);
                }
                adpcm_coder( (short *)buffer->data, g_raw_buffer[0],RECORD_MAX_READ_LEN,&adpcm_state);
				error_flag = send_audio_data(g_raw_buffer[0],RECORD_ADPCM_MAX_READ_LEN,buffer->time_stamp);
#ifdef CAPTURE_PROFILE
				gettimeofday(&t3,NULL);
				cap_time = TIME_DIFF(t2,t1);
				if( cap_time > max_cap_time ) max_cap_time = cap_time;
				total_cap_time += cap_time;
				cap_cnt++;
				if( cap_cnt == 10 )
				{
					printf("   ave_cap=%d,max_cap=%d\n",total_cap_time/cap_cnt,max_cap_time);
					total_cap_time = 0;
					cap_cnt = 0;
				}
				send_time = TIME_DIFF(t3,t2);
				if( send_time > max_send_time ) max_send_time = send_time;
				total_send_time += send_time;
				send_cnt++;
				if( send_cnt == 10 )
				{
					printf("   ave_send=%d,max_send=%d\n",total_send_time/send_cnt,max_send_time);
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
				printf("   Send Stop\n");
				state = SOCKET_INIT;
				break;
		}
	}while(1);

	pthread_exit(NULL);
}

void StartRecorder()
{
	printf("<--Start Recorder\n");
	EnableBufferQueue(CAPTURE_QUEUE);
	return;
}

void StopRecorder()
{
	printf("<--Stop Recorder\n");
	DisableBufferQueue(CAPTURE_QUEUE);
	return;
}
/* ---------------------------------------------------------- */
