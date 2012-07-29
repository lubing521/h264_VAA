/*
 * receive.c
 *	Receive audio file from cellphone(Android)
 *	jgfntu@163.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <sys/socket.h>
#include <sys/select.h> 
#include <fcntl.h>					/* for O_WRONLY ... */
#include <semaphore.h>

#include "types.h"
#include "audio.h"
#include "rw.h"
#include "adpcm.h"

extern sem_t start_music;
/* --------------------------------------------------------- */
enum PlayerState
{
	PLAYING,
	STOPPING,
	STOPPED
};

enum PlayerOp
{
	NONE,
	PLAY,
	STOP
};

struct Player
{
	enum PlayerState state;
	enum PlayerOp next_op;
	pthread_t playback_thread;
};

pthread_t playback_td, talk_playback_td;
struct Player g_player;

extern int music_data_fd;
extern int oss_fd;

extern int read_client( int fd, char *buf, int len );

struct Buffer
{
    u8 data[ADPCM_MAX_READ_LEN];
    int len;
    sem_t is_empty;
    sem_t is_full;
    int wanted;
    pthread_mutex_t lock;
};

#define BUFFER_NUM 2
struct Buffer g_talk_buffer_queue[BUFFER_NUM];
int g_full_head = 0, g_empty_head = 0,queue_state=0;

u8 g_decoded_buffer[2][ADPCM_MAX_READ_LEN * 4];

void init_receive()
{
    sem_init(&start_music,0,0);
    g_player.state = STOPPED;
    g_player.next_op = NONE;
}

/*
 * clear AVdata socket buffer
 */
void clear_recv_buf(int client_fd)
{
    int recv_size = 0;
    int  nret;
    char tmp[2];
    struct timeval tm_out;
    fd_set  fds;

    tm_out.tv_sec = 0;
    tm_out.tv_usec = 0;

    FD_ZERO(&fds);
    FD_SET(client_fd, &fds);
#if 0	
    if ((recv_size = recv(client_fd, runtime_recv_buf, RUNTIME_RECV_LEN, 0)) == -1) {
        perror("recv");
        close(client_fd);
        exit(0);
    }
    printf("first * clear recv socket buffer : %d字节!\n", recv_size);
    recv_size = 0;
#endif
    while(1) {
        nret = select(FD_SETSIZE, &fds, NULL, NULL, &tm_out);
        if(nret == 0)
            break;
        recv(client_fd, tmp, 1, 0);
        recv_size++;
    }
    printf("second * clear recv socket buffer : %d字节!\n", recv_size);
#if 0
    do {
        if ((recv_size = recv(client_fd, runtime_recv_buf, RUNTIME_RECV_LEN, 0)) == -1) {
            perror("recv");
            close(client_fd);
            exit(0);
        }
        printf("clear recv socket buffer : %d字节!\n", recv_size);
    } while (recv_size > 0);
#endif
}

void init_buffer()
{
    int i;
    for( i = 0; i < BUFFER_NUM; i++ )
    {
        struct Buffer *p_buf = &g_talk_buffer_queue[i];
        p_buf->len = 0;
        //p_buf->wanted = WANTED_NONE;
        sem_init(&p_buf->is_full, 0, 0);			/* read socket to buffer */
        sem_init(&p_buf->is_empty, 0, 1);
        pthread_mutex_init(&p_buf->lock,NULL);
    }
    g_full_head = 0;
    g_empty_head = 0;
    queue_state=1;
    return;
}

struct Buffer *GetBuffer( void )
{
    struct Buffer *p = &g_talk_buffer_queue[g_full_head];
    //pthread_mutex_lock( &p->lock );
    //p->wanted = WANTED_FULL;
    //pthread_mutex_unlock( &p->lock );
    if( sem_wait(&p->is_full) < 0 )
        return NULL;
    pthread_mutex_lock( &p->lock );
    g_full_head++;
    if( g_full_head >= BUFFER_NUM )
        g_full_head = 0;
    pthread_mutex_unlock( &p->lock );
    return p;
}

int EmptyBuffer(struct Buffer *p_buf)
{
    //pthread_mutex_lock( &p_buf->lock );
    return sem_post(&p_buf->is_empty);
    //p_buf->wanted = WANTED_NONE;
    //pthread_mutex_unlock( &p_buf->lock );
}

struct Buffer *GetEmptyBuffer()
{
    struct Buffer *p = &g_talk_buffer_queue[g_empty_head];
    //pthread_mutex_lock( &p->lock );
    //p->wanted = WANTED_EMPTY;
    //pthread_mutex_unlock( &p->lock );
    if( sem_wait(&p->is_empty) < 0 )
        return NULL;
    pthread_mutex_lock( &p->lock );
    g_empty_head++;
    if( g_empty_head >= BUFFER_NUM ) g_empty_head = 0;
    pthread_mutex_unlock( &p->lock );
    return p;
}

int AddBuffer( struct Buffer *p_buf )
{
    //pthread_mutex_lock( &p_buf->lock );
    return sem_post(&p_buf->is_full);
    //p_buf->wanted = WANTED_NONE;
    //pthread_mutex_unlock( &p_buf->lock );
}

void ResetBufferQueue()
{
    int i;
    for( i = 0; i < BUFFER_NUM; i++ )
    {
        struct Buffer *p_buf = &g_talk_buffer_queue[i];
        pthread_mutex_lock( &p_buf->lock );
        sem_destroy( &p_buf->is_full );
        sem_destroy( &p_buf->is_empty );
        pthread_mutex_unlock( &p_buf->lock );
    }
    queue_state=0;

}
void *talk_play(void *arg)
{
    u8 *buf = (u8 *)arg;
    unsigned int rate,channels,bit,ispk,i;
    rate=un_OSS_RATE[buf[0]];
    channels=buf[1];
    bit=buf[2];
    ispk=buf[3];

    if(oss_fd > 0)
    {
        close(oss_fd);
        oss_fd= 0;
        printf("audio(oss) already opened! close it!\n");
    }
    oss_fd = open(OSS_AUDIO_DEV,O_RDWR);
    if (oss_fd < 0) {
        printf("Err: Open audio(oss) device failed!\n");
        goto exit;
    }

    /* set oss configuration */
    if( set_oss_play_config(oss_fd, rate,AUDIO_CHANNELS, bit) < 0 )
    {
        printf("Err: set oss play config failed!\n");
        goto exit;
    }

    printf(">>>Open playback audio device\n");
    adpcm_state_t adpcm_state = {0, 0};

    i = 0;
    while(1)
    {
        struct Buffer *buffer=GetBuffer();
        if( buffer == NULL )
            break;
        adpcm_decoder( buffer->data, (short *)g_decoded_buffer[i],buffer->len, &adpcm_state);
        if( EmptyBuffer(buffer) < 0 )
            break;
        if( playback_buf(g_decoded_buffer[i],buffer->len*4) < 0 )
        {
            printf("Err(talk):playback failed!\n");
            break;
        }
        usleep(1000);
        i = !i;
    }

exit:
    printf("talk play thread exit\n");
    EndPlayer(-1);
    pthread_exit(NULL);
}

static int receive_talk(u32 client_fd)
{
    int error_flag, read_left,file_size,error_num=0;
    unsigned rate,channels,bit,ispk;
    u8 buf[12];
    
    printf("in talk_playback>>>>>>>>>>\n");
    /* receive file size from cellphone */
    if (read_client(client_fd, buf, 8) != 8) {
        perror("recv");
        return -1;
    }
    file_size = *((u32 *)&buf[0]);
    read_left = file_size;
    printf(">>>receive file size : %d字节\n", file_size);
    if (read_client(client_fd, buf, 4) != 4) {
        perror("recv");
        return -1;
    }
    rate=un_OSS_RATE[buf[0]];
    channels=buf[1];
    bit=buf[2];
    ispk=buf[3];
    printf("rate is %d,channels is %d,bit is %d,ispk is %d\n",rate,channels,bit,ispk);
    if(channels !=1||bit!=16||buf[8]>7||file_size<0)
    {
        printf("Err: received file error !!\n");
        return -1;
    }
   
    init_buffer();

    /*
     * TODO (FIX ME)
     * create a thread for playbacking recording data from cellphone
     */
    error_flag = pthread_create(&talk_playback_td, NULL, talk_play, buf);
    if (error_flag < 0) {
        printf("Create talk playback thread faild!\n");
        return -1;
    }
    
    while(1)
    {
        struct Buffer *p_buf = GetEmptyBuffer();
        int read_len = ADPCM_MAX_READ_LEN;
        if( p_buf == NULL )
            return -1;
        error_flag = read_all(client_fd, p_buf->data, &read_len);
        if (error_flag < 0)
            return -1;
        p_buf->len = read_len;
        if (p_buf->len == 0)
        {
            printf("###no data read###\n");	
            return -1;
        }
        AddBuffer(p_buf);
        if(!ispk)
        {
            read_left -= p_buf->len;
            if( read_left <= 0 )
            {
                printf("music play over\n");
                break;
            }
        }
    }

    return 0;
}

/* TODO (FIX ME)
 * thread for receiving file from cellphone or runtime playbacking
 */
void *deal_FEdata_request(void *arg)
{
    int client_fd,exit_flag=-1;
    printf("before wait\n");
    if(sem_wait(&start_music)<0)
        goto free_buf;
    printf("after wait\n");
    /* tell cellphone to start sending file */
    send_talk_resp();
    client_fd = music_data_fd;
    /* seperate mode, free source when thread working is over */
    pthread_detach(pthread_self());
    /* receive talk data frome cellphone */
    if (receive_talk(client_fd) == -1)
    {
        printf("File receive connection is error!\n");
    }
    else
        exit_flag =0;



free_buf:
    EndPlayer(exit_flag);
    printf("deal_FEdata_request thread exit!\n");
    pthread_exit(NULL);
}

/*
 * create playback audio thread
 */
int enable_playback_audio()
{
    if(pthread_create(&playback_td, NULL, deal_FEdata_request, NULL) != 0) {
        perror("pthread_create");
        return -1;
    }
    return 0;
}


void StartPlayer()
{
	if( g_player.state == STOPPED )
	{
        printf("ToStartPlayer\n");
		int ret =enable_playback_audio(); 
	    if (ret != 0)
    	{
	    	printf("ERR(Player):create playback thread error\n");
	    }
	    else
	    {
			g_player.state = PLAYING;
		}
		g_player.next_op = NONE;
	}
	else
	{
        printf("Wait for play\n");
		g_player.next_op = PLAY;
	}
	return;
}

void StopPlayer()
{
    printf("ToStopPlayer\n");
    if( g_player.state == PLAYING )
        g_player.state = STOPPING;
    if(oss_fd > 0)
    {
        close(oss_fd);
        oss_fd = -1;
    }
    if(queue_state)
        ResetBufferQueue();
    if(music_data_fd>0)
    {
        close(music_data_fd);
        music_data_fd =-1;
    }
    g_player.next_op = NONE;
    //	}
    /*	else
        {
        printf("Wait for stop\n");
        g_player.next_op = STOP;
        }*/
	return;
}

void EndPlayer( int exit_flag )
{    
	if( g_player.state == STOPPING || g_player.state == PLAYING )
	{
        printf("End player\n");
        if(exit_flag == -1)
        {
            if(oss_fd > 0)
            {
                close(oss_fd);
                oss_fd = -1;
            }
            if(queue_state)
                ResetBufferQueue();
        }
        if(music_data_fd>0)
        {
            close(music_data_fd);
            music_data_fd =-1;
        }
		g_player.state = STOPPED;
		if( g_player.next_op == PLAY )
		{
            printf("ReStartPlayer\n");
			StartPlayer();
		}
		g_player.next_op = NONE;
	}
	else
	{
		printf("ERR(Player):wrong state(%d) for end player!\n", g_player.state );
	}

	return;
}
