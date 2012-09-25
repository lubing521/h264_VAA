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
#include "buffer.h"

static void *talk_playback( void *arg );
static void *talk_receive( void *arg );
extern sem_t start_talk;
extern oss_fd_play;
int receiver_err_num;
static char * receiver_err_string[]={
    "No Fileszie Received !\n",
    "No Audio Param Received !\n",
    "Received Error Audio Param !\n",
    "GetEmptyBuffer's result is NULL Pointer in Receiver Init !\n",
    "GetEmptyBuffer's result is NULL Pointer When Receiving !\n",
    "Read_all From Socket Error !\n",
    "Music Played Over !\n"
};
/* --------------------------------------------------------- */
enum PlayerState
{
	ST_PLAYING,
	ST_STOPPING,
	ST_STOPPED
};

enum PlayerOp
{
	OP_PLAY,
	OP_STOP
};

struct Player
{
	enum PlayerState state;
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

pthread_t talk_playback_td, talk_receive_td;
struct Player g_player;

#define BUFFER_NUM 3
BufferQueue g_talk_buffer_queue;
#define TALK_QUEUE (&g_talk_buffer_queue)

u8 g_decoded_buffer[2][ADPCM_MAX_READ_LEN * 4];

extern int music_data_fd;

extern int read_client( int fd, char *buf, int len );


void init_receive()
{
	sem_init(&start_talk,0,0);
	pthread_mutex_init(&g_player.lock,NULL);
	InitQueue(TALK_QUEUE,"talk", BUFFER_NUM);
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
	printf("first * clear recv socket buffer : %d×Ö½Ú!\n", recv_size);
	recv_size = 0;
#endif
	while(1) {
		nret = select(FD_SETSIZE, &fds, NULL, NULL, &tm_out);
		if(nret == 0)
			break;
		recv(client_fd, tmp, 1, 0);
		recv_size++;
	}
	printf("   second * clear recv socket buffer : %d×Ö½Ú!\n", recv_size);
#if 0
	do {
		if ((recv_size = recv(client_fd, runtime_recv_buf, RUNTIME_RECV_LEN, 0)) == -1) {
			perror("recv");
			close(client_fd);
			exit(0);
		}
		printf("clear recv socket buffer : %d×Ö½Ú!\n", recv_size);
	} while (recv_size > 0);
#endif
}

static void InitBufferList( BufferList *list, Buffer *head, int len )
{
	Buffer *p = head;
	int i;
	for( i = 0; i < len-1; i++ )
	{
		p[i].next = &p[i+1];
	}
	if( len > 0 )
	{
		p[i].next = NULL;
		list->head = head;
		list->tail = &p[i];
	}
	else
	{
		list->head = NULL;
		list->tail = NULL;
	}
	sem_init( &list->ready, 0, 0 );
	list->wanted = 0;
	return;
}

static void ResetBufferList( BufferList *list, Buffer *head, int len )
{
	Buffer *p = head;
	int i;
	for( i = 0; i < len-1; i++ )
	{
		p[i].next = &p[i+1];
	}
	if( len > 0 )
	{
		p[i].next = NULL;
		list->head = head;
		list->tail = &p[i];
	}
	else
	{
		list->head = NULL;
		list->tail = NULL;
	}
	if( list->wanted )
	{
		sem_post( &list->ready );
		list->wanted = 0;
	}
	return;
}

static inline void InBufferList( BufferList *list, Buffer *node )
{
	if( list->head == NULL )
	{
		list->head = node;
		list->tail = list->head;
	}
	else
	{
		list->tail->next = node;
		list->tail = node;
	}
	node->next = NULL;
	return;
}

static inline Buffer *OutBufferList( BufferList *list )
{
	Buffer *node = NULL;

	if( list->head != NULL )
	{
		node = list->head;
		list->head = node->next;
		node->next = NULL;
		if( node == list->tail )
		{
			list->tail = NULL;
		}
	}
	return node;
}


Buffer *GetBuffer( BufferQueue *queue )
{
	Buffer *p = NULL;

	pthread_mutex_lock( &queue->lock );
	if( queue->out_state == QUEUE_WORKING )
	{
	p = queue->list_filled.head;
	if( p == NULL )
	{
		queue->list_filled.wanted = 1;
		pthread_mutex_unlock( &queue->lock );

		sem_wait(&queue->list_filled.ready);
		if( queue->out_state != QUEUE_WORKING )
		{
			//printf("GetBuffer Cancelled\n");
			return NULL;
		}

		pthread_mutex_lock( &queue->lock );
		p = queue->list_filled.head;
	}
	}
	pthread_mutex_unlock( &queue->lock );
	//printf("GetBuffer %x\n",p);
	return p;
}

int EmptyBuffer( BufferQueue *queue )
{
	Buffer *p;
	pthread_mutex_lock( &queue->lock );
	if( queue->out_state == QUEUE_WORKING )
	{
		p = OutBufferList( &queue->list_filled );
		//printf("EmptyBuffer %x\n",p);
		if( p != NULL )
		{
			InBufferList( &queue->list_empty, p );
			if( queue->list_empty.wanted )
			{
				sem_post(&queue->list_empty.ready);
				queue->list_empty.wanted = 0;
			}
		}
	}
	pthread_mutex_unlock( &queue->lock );
	return 1;
}

Buffer *GetEmptyBuffer( BufferQueue *queue )
{
	Buffer *p = NULL;

	pthread_mutex_lock( &queue->lock );
	if( queue->in_state == QUEUE_WORKING )
	{
		p = queue->list_empty.head;
		if( p == NULL )
		{
			queue->list_empty.wanted = 1;
			pthread_mutex_unlock( &queue->lock );
			sem_wait(&queue->list_empty.ready);
			if( queue->in_state != QUEUE_WORKING )
			{
				//printf("GetEmptyBuffer Cancelled\n");
				return NULL;
			}
			pthread_mutex_lock( &queue->lock );
			p = queue->list_empty.head;
		}
	}
	pthread_mutex_unlock( &queue->lock );
	//printf("GetEmptyBuffer %x\n",p);
	return p;
}

int FillBuffer( BufferQueue *queue )
{
	Buffer *p;
	pthread_mutex_lock( &queue->lock );
	if( queue->in_state == QUEUE_WORKING )
	{
		p = OutBufferList( &queue->list_empty );
		//printf("FillBuffer %x\n",p);
		if( p != NULL )
		{
			InBufferList( &queue->list_filled, p );
			if( queue->list_filled.wanted )
			{
				sem_post(&queue->list_filled.ready);
				queue->list_filled.wanted = 0;
			}
		}
	}
	pthread_mutex_unlock( &queue->lock );
	return 1;
}

void InitQueue( BufferQueue *queue, const char *name, int num )
{
	printf("   InitQueue(%s)\n",name);
	queue->buffer = (Buffer *)malloc(sizeof(Buffer)*num);
	queue->buffer_num = num;
	queue->name = name;

	queue->in_state = QUEUE_STOPPED;
	queue->out_state = QUEUE_STOPPED;
	InitBufferList( &queue->list_filled, NULL, 0 );
	InitBufferList( &queue->list_empty, queue->buffer, num );

	pthread_mutex_init(&queue->lock,NULL);
	sem_init(&queue->in_ready,0,0);
	sem_init(&queue->out_ready,0,0);

	return;
}

int OpenQueueIn( BufferQueue *queue )
{
	sem_wait( &queue->in_ready );
	printf("   Open Queue(%s) In\n", queue->name);
	return ( queue->in_state == QUEUE_WORKING );
}

int OpenQueueOut( BufferQueue *queue )
{
	sem_wait( &queue->out_ready );
	printf("   Open Queue(%s) Out\n", queue->name);
	return ( queue->out_state == QUEUE_WORKING );
}

void EnableBufferQueue( BufferQueue *queue )
{
	printf("   Enable queue(%s)\n", queue->name);
	queue->in_state = QUEUE_WORKING;
	queue->out_state = QUEUE_WORKING;
	sem_trywait( &queue->in_ready );
	sem_post( &queue->in_ready );
	sem_trywait( &queue->out_ready );
	sem_post( &queue->out_ready );
	return;
}

void DisableBufferQueue( BufferQueue *queue )
{
	printf("   Disable queue(%s)\n",queue->name);
	pthread_mutex_lock( &queue->lock );
	ResetBufferList( &queue->list_filled, NULL, 0 );
	ResetBufferList( &queue->list_empty, queue->buffer, queue->buffer_num );
	queue->in_state = QUEUE_STOPPED;
	queue->out_state = QUEUE_STOPPED;
	sem_trywait( &queue->in_ready );
	sem_trywait( &queue->out_ready );
	pthread_mutex_unlock( &queue->lock );
	return;
}

void *talk_playback( void *arg )
{
	int state = PLAYER_STOPPED, i=0;
	struct AUDIO_CFG cfg;
	Buffer *buffer;
	adpcm_state_t adpcm_state = {0, 0};

	pthread_detach(pthread_self());
	do
	{
		buffer = GetBuffer( TALK_QUEUE );
		if( buffer == NULL )
		{
			state = PLAYER_STOPPED;
		}
		switch( state )
		{
			case PLAYER_INIT:
				printf("   player init\n");
				if( buffer->flag == START_TALK && buffer->len == sizeof(struct AUDIO_CFG) )
				{
					cfg = *(struct AUDIO_CFG *)(buffer->data);
					if(oss_fd_play <= 0)
					{
						oss_fd_play = open(OSS_AUDIO_DEV,O_WRONLY);
					}
					if(oss_fd_play < 0)
					{
						printf("   Err(talkplayback): Open audio(oss) device failed!\n");
					}
					else if( set_oss_play_config(oss_fd_play, cfg.rate,cfg.channels, cfg.bit) < 0 )
					{
						printf("   Err(talkplayback): set oss play config failed!\n");
						close(oss_fd_play);
						oss_fd_play = 0;
					}
					adpcm_state.valprev = 0;
					adpcm_state.index = 0;
					printf("   Init OK ! Player Started !\n");
					state = PLAYER_PLAYBACK;
				}
				else
				{
					printf("   Err(talkplayback): Bad start flag or config!\n");
				}
				EmptyBuffer( TALK_QUEUE );
				break;
			case PLAYER_RESET:
				printf("   Error in OSS ! Player Will Reset\n");
				oss_fd_play = open(OSS_AUDIO_DEV,O_WRONLY);
				if(oss_fd_play < 0)
				{
					printf("   Error Serious !: Open audio(oss) device failed!\n");
				}
				else if( set_oss_play_config(oss_fd_play, cfg.rate,cfg.channels, cfg.bit) < 0 )
				{
					printf("   Err(talkplayback): set oss play config failed!\n");
					close(oss_fd_play);
					oss_fd_play = 0;
				}
				state = PLAYER_PLAYBACK;
				break;
			case PLAYER_PLAYBACK:
				//printf("playback\n");
				if( oss_fd_play > 0 && g_player.state == ST_PLAYING )
				{
					if( buffer->len <= 0 || buffer->len > ADPCM_MAX_READ_LEN )
						printf("   Error: buffer len %d is not equal to %d\n", buffer->len,ADPCM_MAX_READ_LEN);
					adpcm_decoder( buffer->data, (short *)g_decoded_buffer[i],buffer->len, &adpcm_state);
					if( playback_buf( oss_fd_play, g_decoded_buffer[i], buffer->len*4) < 0 )
					{
						printf("   Error: Player Ret Rrror !\n");
						close(oss_fd_play);
						oss_fd_play = 0;
						state = PLAYER_RESET;
					}
					i = !i;
				}
				if( buffer->flag == END_TALK )
                {
                    state = PLAYER_STOPPED;
                    send_music_played_over();
                }
				EmptyBuffer( TALK_QUEUE );
				break;
			case PLAYER_STOPPED:
				printf("   Player stop\n");
				//close(oss_fd_play);
				//oss_fd_play = 0;
                speak_power_off();
				OpenQueueOut( TALK_QUEUE );
				state = PLAYER_INIT;
				break;
		}
	}while(1);

	pthread_exit(NULL);
}

void *talk_receive( void *arg )
{
	int state = TALK_STOPPED;
	Buffer *buffer;
	int error_flag, read_left,file_size,error_num=0;
	unsigned rate,channels,bit,ispk;
	u8 buf[12];

	pthread_detach(pthread_self());
	do
	{
		switch( state )
		{
			case TALK_INIT:
				printf("   Receiver Init\n");
				
				if (read_client(music_data_fd, buf, 8) != 8) {
//					printf("Err: no file size!\n");
                    receiver_err_num = 0;
					state = TALK_STOPPED;
				}
				else
				{
					file_size = *((u32 *)&buf[0]);
					read_left = file_size - 4;
					printf("   receive file size : %d Bytes\n", file_size);
					if (read_client(music_data_fd, buf, 4) != 4) {
//						printf("Err: no audio param!\n");
                        receiver_err_num = 1;
						state = TALK_STOPPED;
					}
					else
					{
						rate=un_OSS_RATE[buf[0]];
						channels=buf[1];
						bit=buf[2];
						ispk=buf[3];
						//printf("rate is %d,channels is %d,bit is %d,ispk is %d\n",rate,channels,bit,ispk);
						if(channels !=1||bit!=16||buf[8]>7||file_size<0)
						{
//							printf("Err: received audio param error !!\n");
                            receiver_err_num = 2;
							state = TALK_STOPPED;
						}
						else
						{
							struct AUDIO_CFG *cfg;
							OpenQueueIn( TALK_QUEUE );
							buffer = GetEmptyBuffer( TALK_QUEUE);
							if( buffer == NULL )
							{
								state = TALK_STOPPED;
                                receiver_err_num = 3;
								break;
							}
							buffer->len = sizeof(struct AUDIO_CFG);
							cfg = (struct AUDIO_CFG *)(buffer->data);
							cfg->rate=rate;
							cfg->channels=channels;
							cfg->bit=bit;
							cfg->ispk=ispk;
							buffer->flag = START_TALK;
                            if(ispk)
                                printf("   Start Talk Receiver\n");
                            else
                                printf("   Start Music Receiver\n");
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
                    receiver_err_num = 4;
					break;
				}
				int read_len = ADPCM_MAX_READ_LEN;
				error_flag = read_all(music_data_fd, buffer->data, &read_len);
				if (error_flag < 0)
				{
					state = TALK_STOPPED;
                    receiver_err_num = 5;
				}
				else
				{
					buffer->len = read_len;
					if (buffer->len == 0)
					{
//                        printf("###no data read###,read_left is %d\n",read_left);	
                        receiver_err_num = 6;
                        state = TALK_STOPPED;;
					}
					else
					{
						if(!ispk)
						{
							read_left -= buffer->len;
							if( read_left <= 0 )
							{
                                printf("   music play over\n");
                                buffer->flag = END_TALK;
                            }
							else
							{
								buffer->flag = KEEP_TALK;
							}
						}
						else
						{
							buffer->flag = KEEP_TALK;
						}
						FillBuffer( TALK_QUEUE );
					}
				}
				break;
			case TALK_STOPPED:
                if(ispk)
                    printf("   Stop Talk Receiver\n");
                else
                    printf("   Stop Music Receiver\n");
				EndPlayer();
				sem_wait(&start_talk);
				state = TALK_INIT;
				break;
		}
	}while(1);

	pthread_exit(NULL);
}

void EndPlayer()
{
	pthread_mutex_lock(&g_player.lock);
//	printf("End Player .... music over or something wrong !\n");
    printf("   Error[%d] %s",receiver_err_num,receiver_err_string[receiver_err_num]);
	if(music_data_fd > 0)
	{
		close(music_data_fd);
		music_data_fd = -1;
	}
	pthread_mutex_unlock(&g_player.lock);
	return;
}
void StartPlayer()
{
	pthread_mutex_lock(&g_player.lock);
	printf("<--Start Player\n");
	EnableBufferQueue(TALK_QUEUE);
	send_talk_resp();
	pthread_mutex_unlock(&g_player.lock);
	return;
}

void StopPlayer()
{
	pthread_mutex_lock(&g_player.lock);
	printf("<--Stop Player\n");
	DisableBufferQueue(TALK_QUEUE);
	if(music_data_fd>0)
	{
		shutdown(music_data_fd,SHUT_RDWR);
		close(music_data_fd);
		music_data_fd =-1;
	}
    send_talk_end_resp();
	pthread_mutex_unlock(&g_player.lock);
	return;
}
