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

static void *talk_playback( void *arg );
static void *talk_receive( void *arg );
extern sem_t start_talk;
extern oss_fd_play;
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
	pthread_t receive_thread;
	pthread_t playback_thread;
    pthread_mutex_t lock;
};
struct AUDIO_CFG 
{
    int rate;
    int channels;
    int bit;
    int ispk;
};
enum QueueState
{
    QUEUE_INIT,
	QUEUE_WORKING,
	QUEUE_STOPPED
};

enum TalkBufferFlag
{
	START_TALK,
	KEEP_TALK,
	END_TALK,
	STOP_TALK
};

enum TalkReceiveState
{
	TALK_INIT,
	TALK_RECEIVE,
	TALK_STOPPED
};

enum TalkPlaybackState
{
	PLAYER_INIT,
	PLAYER_PLAYBACK,
	PLAYER_STOPPED,
    PLAYER_RESET
};

struct Buffer
{
	struct Buffer *next;
	u8 data[ADPCM_MAX_READ_LEN];
    int len;
	int flag;
};

struct BufferQueue
{
	struct Buffer *buffer;
	struct Buffer *list_filled;
	struct Buffer *list_empty;
	int buffer_num;
	int state;
	sem_t is_not_full;
	sem_t is_not_empty;
    pthread_mutex_t lock;
};
pthread_t talk_playback_td, talk_receive_td;
struct Player g_player;

#define BUFFER_NUM 3
struct BufferQueue g_talk_buffer_queue;
#define TALK_QUEUE (&g_talk_buffer_queue)

u8 g_decoded_buffer[2][ADPCM_MAX_READ_LEN * 4];

extern int music_data_fd;

extern int read_client( int fd, char *buf, int len );


void init_receive()
{
    sem_init(&start_talk,0,0);
    pthread_mutex_init(&g_player.lock,NULL);
    g_player.state = STOPPED;
    g_player.next_op = NONE;
    InitQueue(TALK_QUEUE,BUFFER_NUM);
    if(pthread_create(&g_player.playback_thread, NULL, talk_playback, NULL) != 0) {
        perror("pthread_create");
        return ;
    }
    if(pthread_create(&g_player.receive_thread, NULL, talk_receive, NULL) != 0) {
        perror("pthread_create");
        return ;
    }
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


struct Buffer *GetBuffer( struct BufferQueue *queue )
{
    struct Buffer *p = NULL;
    
    pthread_mutex_lock( &queue->lock );
    if( queue->list_filled == NULL )
    {
    	pthread_mutex_unlock( &queue->lock );
   		sem_wait(&queue->is_not_empty);
   		pthread_mutex_lock( &queue->lock );
    }
	p = queue->list_filled;
    if( p != NULL && queue->state == QUEUE_STOPPED )
    {
    	p->flag = STOP_TALK;
	}
    pthread_mutex_unlock( &queue->lock );
    return p;
}

int EmptyBuffer( struct BufferQueue *queue )
{
	int result = 0;
    pthread_mutex_lock( &queue->lock );
    //printf("EmptyBuffer %x\n",queue->list_filled);
    if( queue->list_filled != NULL )
    {
	    if( queue->list_empty == NULL )
	    {
	    	queue->list_empty = queue->list_filled;
	    	sem_post( &queue->is_not_full );
	    }
   		queue->list_filled = queue->list_filled->next;
   		if( queue->list_filled == queue->list_empty )
        {
   			queue->list_filled = NULL;
            sem_trywait(&queue->is_not_empty);
        }
   		result = 1;
	}
    pthread_mutex_unlock( &queue->lock );
    return result;
}

struct Buffer *GetEmptyBuffer( struct BufferQueue *queue )
{
    struct Buffer *p = NULL;
    
    pthread_mutex_lock( &queue->lock );
    while( queue->state == QUEUE_WORKING && queue->list_empty == NULL )
    {
        //printf("#wait is not full\n");
    	pthread_mutex_unlock( &queue->lock );
   		sem_wait(&queue->is_not_full);
   		pthread_mutex_lock( &queue->lock );
    }
    if( queue->state == QUEUE_WORKING )
    {
    	p = queue->list_empty;
	}
    pthread_mutex_unlock( &queue->lock );
    //printf("GetEmptyBuffer %x\n",p);
    return p;
}

int FillBuffer( struct BufferQueue *queue )
{
	int result = 0;
    pthread_mutex_lock( &queue->lock );
    //printf("FillBuffer %x\n",queue->list_empty);
    if( queue->state == QUEUE_WORKING && queue->list_empty != NULL )
    {
	    if( queue->list_filled == NULL )
	    {
	    	queue->list_filled = queue->list_empty;
	    	sem_post( &queue->is_not_empty );
	    }
   		queue->list_empty = queue->list_empty->next;
   		if( queue->list_empty == queue->list_filled )
        {
   			queue->list_empty = NULL;
            sem_trywait(&queue->is_not_full);
        }
   		result = 1;
	}
    pthread_mutex_unlock( &queue->lock );
    return result;
}

void InitQueue( struct BufferQueue *queue, int num )
{
	int i;
	
	queue->buffer = (struct Buffer *)malloc(sizeof(struct Buffer)*num);
	queue->buffer_num = num;
	for( i = 0; i < num-1; i++ )
	{
		queue->buffer[i].next = &queue->buffer[i+1];
	}
	queue->buffer[i].next = &queue->buffer[0];
	
	queue->state = QUEUE_INIT;
	queue->list_filled = NULL;
	queue->list_empty = queue->buffer;
	
    sem_init(&queue->is_not_full, 0, 0);
    sem_init(&queue->is_not_empty, 0, 0);
	pthread_mutex_init(&queue->lock,NULL);
	
	return;
}

int SetFlag( struct BufferQueue *queue, int flag )
{
	int result = 0;
    pthread_mutex_lock( &queue->lock );
    if( queue->state == QUEUE_WORKING && queue->list_empty != NULL )
    {

   		queue->list_empty->flag = flag;
   		result = 1;
	}
    pthread_mutex_unlock( &queue->lock );
    return result;
}

void EnableBufferQueue( struct BufferQueue *queue )
{
    printf("Enable queue\n");
	pthread_mutex_lock( &queue->lock );
	queue->state = QUEUE_WORKING;
	pthread_mutex_unlock( &queue->lock );
	return;
}

void DisableBufferQueue( struct BufferQueue *queue )
{
	int i;
    printf("Disable queue\n");
	pthread_mutex_lock( &queue->lock );
	
	for( i = 0; i < queue->buffer_num-1; i++ )
	{
		queue->buffer[i].flag = STOP_TALK;
	}
	queue->state = QUEUE_STOPPED;
	if( sem_trywait(&queue->is_not_full) < 0 )
        sem_post(&queue->is_not_full);
	if( sem_trywait(&queue->is_not_empty) < 0 )
        sem_post(&queue->is_not_empty);
	pthread_mutex_unlock( &queue->lock );
	return;
}

void *talk_playback( void *arg )
{
	int state = PLAYER_INIT, i=0;
	struct AUDIO_CFG cfg;
    struct Buffer *buffer;
    adpcm_state_t adpcm_state = {0, 0};

	pthread_detach(pthread_self());
	do
	{
		switch( state )
		{
			case PLAYER_INIT:
				printf("player init\n");
                buffer = GetBuffer( TALK_QUEUE );
                if( buffer == NULL )
                    break;
				if( buffer->flag == START_TALK && buffer->len == sizeof(struct AUDIO_CFG) )
				{
					cfg = *(struct AUDIO_CFG *)(buffer->data);
                    if(oss_fd_play <= 0)
                    {
                        oss_fd_play = open(OSS_AUDIO_DEV,O_WRONLY);
                    }
                    if(oss_fd_play < 0)
                    {
                        printf("Err(talkplayback): Open audio(oss) device failed!\n");
					}
					else if( set_oss_play_config(oss_fd_play, cfg.rate,cfg.channels, cfg.bit) < 0 )
					{
						printf("Err(talkplayback): set oss play config failed!\n");
						close(oss_fd_play);
						oss_fd_play = 0;
					}
                    adpcm_state.valprev = 0;
                    adpcm_state.index = 0;
                    printf("talk start\n");
					state = PLAYER_PLAYBACK;
				}
				else if( buffer->flag == STOP_TALK )
				{
					printf("Info(talkplayback): Stop play, skip buffer!\n");
				}
				else
				{
					printf("Err(talkplayback): Bad start flag or config!\n");
				}
				EmptyBuffer( TALK_QUEUE );
				break;
            case PLAYER_RESET:
				printf("player reset\n");
                oss_fd_play = open(OSS_AUDIO_DEV,O_WRONLY);
                if(oss_fd_play < 0)
                {
                    printf("Err(talkplayback): Open audio(oss) device failed!\n");
                }
                else if( set_oss_play_config(oss_fd_play, cfg.rate,cfg.channels, cfg.bit) < 0 )
                {
                    printf("Err(talkplayback): set oss play config failed!\n");
                    close(oss_fd_play);
                    oss_fd_play = 0;
                }
                state = PLAYER_PLAYBACK;
                break;
			case PLAYER_PLAYBACK:
                //printf("playback\n");
				buffer = GetBuffer( TALK_QUEUE );
                if( buffer == NULL )
                {
                    state = PLAYER_STOPPED;
                    break;
                }
				if( buffer->flag != STOP_TALK )
				{
					if( oss_fd_play > 0 )
					{
                        if( buffer->len <= 0 || buffer->len > ADPCM_MAX_READ_LEN )
                            printf("err buffer len %d\n", buffer->len);
						adpcm_decoder( buffer->data, (short *)g_decoded_buffer[i],buffer->len, &adpcm_state);
						if( playback_buf( oss_fd_play, g_decoded_buffer[i], buffer->len*4) < 0 )
                        {
                            close(oss_fd_play);
                            oss_fd_play = 0;
                            state = PLAYER_RESET;
                        }
						i = !i;
					}
					if( buffer->flag == END_TALK )
						state = PLAYER_STOPPED;
				}
				else
				{
					state = PLAYER_STOPPED;
					printf("Info(talkplayback): talk stopped!\n");
				}
				EmptyBuffer( TALK_QUEUE );
				break;
			case PLAYER_STOPPED:
                printf("player stop\n");
				close(oss_fd_play);
                oss_fd_play = 0;
				state = PLAYER_INIT;
				break;
		}
	}while(1);
	
	pthread_exit(NULL);
}

void *talk_receive( void *arg )
{
	int state = TALK_INIT;
	struct Buffer *buffer;
    int error_flag, read_left,file_size,error_num=0;
    unsigned rate,channels,bit,ispk;
    u8 buf[12];

	pthread_detach(pthread_self());
	do
	{
		switch( state )
		{
			case TALK_INIT:
				printf("Talk init\n");
                sem_wait(&start_talk);
                printf("after wait\n");

				send_talk_resp();
			    if (read_client(music_data_fd, buf, 8) != 8) {
			        perror("recv");
			        state = TALK_STOPPED;
			    }
			    else
			    {
				    file_size = *((u32 *)&buf[0]);
				    read_left = file_size;
				    printf(">>>receive file size : %d字节\n", file_size);
				    if (read_client(music_data_fd, buf, 4) != 4) {
				        printf("Err: no file size!\n");
                        state = TALK_STOPPED;
				    }
				    else
				    {
					    rate=un_OSS_RATE[buf[0]];
					    channels=buf[1];
					    bit=buf[2];
					    ispk=buf[3];
					    printf("rate is %d,channels is %d,bit is %d,ispk is %d\n",rate,channels,bit,ispk);
					    if(channels !=1||bit!=16||buf[8]>7||file_size<0)
					    {
					        printf("Err: received file error !!\n");
					        state = TALK_STOPPED;
					    }
					    else
					    {
					    	struct AUDIO_CFG *cfg;
					    	buffer = GetEmptyBuffer( TALK_QUEUE);
                            if( buffer == NULL )
                            {
                                state = TALK_STOPPED;
                                break;
                            }
                            buffer->len = sizeof(struct AUDIO_CFG);
					    	cfg = (struct AUDIO_CFG *)(buffer->data);
						    cfg->rate=rate;
						    cfg->channels=channels;
						    cfg->bit=bit;
						    cfg->ispk=ispk;
						    SetFlag( TALK_QUEUE, START_TALK );
						    printf("start talk\n");
                            FillBuffer( TALK_QUEUE );
						    state = TALK_RECEIVE;
						}
					}
				}
				break;
			case TALK_RECEIVE:
				//printf("Talk receive\n");
		        buffer = GetEmptyBuffer(TALK_QUEUE);
                if( buffer == NULL )
                {
                    state = TALK_STOPPED;
                    break;
                }
		        int read_len = ADPCM_MAX_READ_LEN;
		        if( buffer == NULL )
		        {
		            state = TALK_STOPPED;
		        }
		        else
		        {
			        error_flag = read_all(music_data_fd, buffer->data, &read_len);
			        if (error_flag < 0)
			        {
			            state = TALK_STOPPED;
			        }
			        else
			        {
				        buffer->len = read_len;
				        if (buffer->len == 0)
				        {
				            printf("###no data read###\n");	
				            state = TALK_STOPPED;;
				        }
				        else
				        {
				        	if(!ispk)
				        	{
					            read_left -= buffer->len;
					            if( read_left <= 0 )
					            {
					                printf("music play over\n");
						    		SetFlag( TALK_QUEUE, END_TALK );
					            }
					            else
					            {
						    		SetFlag( TALK_QUEUE, KEEP_TALK );
					            }
				            }
				            else
				            {
						    	SetFlag( TALK_QUEUE, KEEP_TALK );
				            }
					        FillBuffer( TALK_QUEUE );
				        }
				    }
			    }
				break;
			case TALK_STOPPED:
				printf("Talk stop\n");
				EndPlayer();
				state = TALK_INIT;
				break;
		}
	}while(1);
	
	pthread_exit(NULL);
}

void EndPlayer()
{
    pthread_mutex_lock(&g_player.lock);
    printf("ToEndPlayer\n");
	if(music_data_fd > 0)
	{
		close(music_data_fd);
		music_data_fd = -1;
	}
	g_player.state = STOPPED;
    pthread_mutex_unlock(&g_player.lock);
	return;
}
void StartPlayer()
{
    pthread_mutex_lock(&g_player.lock);
    printf("ToStartPlayer\n");
    EnableBufferQueue(TALK_QUEUE);
	g_player.state = PLAYING;
    pthread_mutex_unlock(&g_player.lock);
	return;
}

void StopPlayer()
{
    pthread_mutex_lock(&g_player.lock);
    printf("ToStopPlayer\n");
    if( g_player.state == PLAYING )
    {
        g_player.state = STOPPING;

        DisableBufferQueue(TALK_QUEUE);
        if(music_data_fd>0)
        {
            shutdown(music_data_fd,SHUT_RDWR);
            close(music_data_fd);
            music_data_fd =-1;
        }
    }
    pthread_mutex_unlock(&g_player.lock);
	return;
}
