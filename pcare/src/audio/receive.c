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
	g_player.state = ST_STOPPED;
	g_player.next_op = OP_STOP;
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

	if( queue->state != QUEUE_STOPPING )
	{
		sem_wait(&queue->filled_buffer_ready);
		if( queue->state != QUEUE_STOPPING ) p = queue->list_filled.head;
	}
	//printf("GetBuffer %x\n",p);
	return p;
}

int EmptyBuffer( BufferQueue *queue )
{
	Buffer *p;
	pthread_mutex_lock( &queue->lock );
	if( queue->state != QUEUE_STOPPING )
	{
		p = OutBufferList( &queue->list_filled );
		InBufferList( &queue->list_empty, p );
		//printf("EmptyBuffer %x\n",p);
		sem_post(&queue->empty_buffer_ready);
	}
	pthread_mutex_unlock( &queue->lock );
	return 1;
}

Buffer *GetEmptyBuffer( BufferQueue *queue )
{
	Buffer *p = NULL;

	if( queue->state != QUEUE_STOPPING )
	{
		//printf("#wait is not full\n");
		sem_wait(&queue->empty_buffer_ready);
		if( queue->state != QUEUE_STOPPING ) p = queue->list_empty.head;
	}
	//printf("GetEmptyBuffer %x\n",p);
	return p;
}

int FillBuffer( BufferQueue *queue )
{
	Buffer *p;
	pthread_mutex_lock( &queue->lock );
	if( queue->state != QUEUE_STOPPING )
	{
		p = OutBufferList( &queue->list_empty );
		InBufferList( &queue->list_filled, p );
		//printf("FillBuffer %x\n",p);
		sem_post(&queue->filled_buffer_ready);
	}
	pthread_mutex_unlock( &queue->lock );
	return 1;
}

void InitQueue( BufferQueue *queue, int num )
{
	int i;

	printf("InitQueue\n");
	queue->buffer = (Buffer *)malloc(sizeof(Buffer)*num);
	queue->buffer_num = num;
	for( i = 0; i < num-1; i++ )
	{
		queue->buffer[i].next = &queue->buffer[i+1];
	}
	queue->buffer[i].next = NULL;

	queue->state = QUEUE_STOPPED;
	queue->request = QUEUE_STOPPED;
	queue->list_filled.head = NULL;
	queue->list_filled.tail = NULL;
	queue->list_empty.head = queue->buffer;
	queue->list_empty.tail = &queue->buffer[i];
	queue->closed_port = 0;

	sem_init(&queue->filled_buffer_ready, 0, 0);
	sem_init(&queue->empty_buffer_ready, 0, num);
	sem_init(&queue->closed, 0, 0);
	pthread_mutex_init(&queue->lock,NULL);

	return;
}

void EnableBufferQueue( BufferQueue *queue )
{
	printf("Enable queue\n");
	pthread_mutex_lock( &queue->lock );
	queue->request = QUEUE_WORKING;
	if( queue->state == QUEUE_STOPPED )
	{
		queue->state = QUEUE_WORKING;
	}
	pthread_mutex_unlock( &queue->lock );
	return;
}

void DisableBufferQueue( BufferQueue *queue )
{
	Buffer *p;
	printf("Disable queue\n");
	pthread_mutex_lock( &queue->lock );
	queue->request = QUEUE_STOPPED;
	if( queue->state == QUEUE_WORKING )
	{
		sem_post(&queue->empty_buffer_ready);
		sem_post(&queue->filled_buffer_ready);
		queue->state = QUEUE_STOPPING;
	}
	pthread_mutex_unlock( &queue->lock );
	return;
}

int CloseBufferQueue( BufferQueue *queue, int port )
{
	Buffer *p;
	int is_closed_all = 0;
	printf("Close queue (p%d)\n",port);
	pthread_mutex_lock( &queue->lock );
	queue->closed_port |= port;
	is_closed_all = (queue->closed_port == (IN_PORT|OUT_PORT));
	if( !is_closed_all )
	{
		queue->state = QUEUE_STOPPING;
		if( port == IN_PORT ) sem_post(&queue->filled_buffer_ready);
		if( port == OUT_PORT ) sem_post(&queue->empty_buffer_ready);
		pthread_mutex_unlock( &queue->lock );
		sem_wait(&queue->closed);
	}
	else
	{
		int i, num = queue->buffer_num, val = 0;
		for( i = 0; i < num-1; i++ )
		{
			queue->buffer[i].next = &queue->buffer[i+1];
		}
		queue->buffer[i].next = NULL;

		queue->list_filled.head = NULL;
		queue->list_filled.tail = NULL;
		queue->list_empty.head = queue->buffer;
		queue->list_empty.tail = &queue->buffer[i];
		while( val < num )
		{
			sem_getvalue(&queue->empty_buffer_ready,&val);
			if( val < num ) sem_post(&queue->empty_buffer_ready);
			else if( val > num ) sem_trywait(&queue->empty_buffer_ready);
		}
		while( sem_trywait(&queue->filled_buffer_ready) == 0 );
		queue->state = QUEUE_STOPPED;
		queue->closed_port = 0;
		sem_post(&queue->closed);
		pthread_mutex_unlock( &queue->lock );
	}
	return 1;
}

void *talk_playback( void *arg )
{
	int state = PLAYER_INIT, i=0;
	struct AUDIO_CFG cfg;
	Buffer *buffer;
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
				{
					state = PLAYER_STOPPED;
					break;
				}
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
					printf("player start\n");
					state = PLAYER_PLAYBACK;
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
				if( oss_fd_play > 0 )
				{
					if( buffer->len <= 0 || buffer->len > ADPCM_MAX_READ_LEN )
						printf("err buffer len %d\n", buffer->len);
					adpcm_decoder( buffer->data, (short *)g_decoded_buffer[i],buffer->len, &adpcm_state);
					if( playback_buf( oss_fd_play, g_decoded_buffer[i], buffer->len*4) < 0 )
					{
						printf("playback_buf ret error !\n");
						close(oss_fd_play);
						oss_fd_play = 0;
						state = PLAYER_RESET;
					}
					i = !i;
				}
				if( buffer->flag == END_TALK )
					state = PLAYER_STOPPED;
				EmptyBuffer( TALK_QUEUE );
				break;
			case PLAYER_STOPPED:
				printf("player stop\n");
				CloseBufferQueue( TALK_QUEUE, OUT_PORT );
				//close(oss_fd_play);
				//oss_fd_play = 0;
				state = PLAYER_INIT;
				break;
		}
	}while(1);

	pthread_exit(NULL);
}

void *talk_receive( void *arg )
{
	int state = TALK_INIT;
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
				printf("Talk init\n");
				sem_wait(&start_talk);
				printf("after wait\n");

				send_talk_resp();
				if (read_client(music_data_fd, buf, 8) != 8) {
					printf("Err: no file size!\n");
					state = TALK_STOPPED;
				}
				else
				{
					file_size = *((u32 *)&buf[0]);
					read_left = file_size;
					printf(">>>receive file size : %d字节\n", file_size);
					if (read_client(music_data_fd, buf, 4) != 4) {
						printf("Err: no audio param!\n");
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
							buffer->flag = START_TALK;
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
				printf("Talk stop\n");
				CloseBufferQueue( TALK_QUEUE, IN_PORT );
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
	g_player.state = ST_STOPPED;
	if( g_player.next_op == OP_PLAY )
	{
		EnableBufferQueue(TALK_QUEUE);
		g_player.state = ST_PLAYING;
	}
	pthread_mutex_unlock(&g_player.lock);
	return;
}
void StartPlayer()
{
	pthread_mutex_lock(&g_player.lock);
	printf("ToStartPlayer\n");
	g_player.next_op = OP_PLAY;
	if( g_player.state == ST_STOPPED )
	{
		EnableBufferQueue(TALK_QUEUE);
		g_player.state = ST_PLAYING;
	}
	pthread_mutex_unlock(&g_player.lock);
	return;
}

void StopPlayer()
{
	pthread_mutex_lock(&g_player.lock);
	printf("ToStopPlayer\n");
	g_player.next_op = OP_STOP;
	if( g_player.state == ST_PLAYING )
	{
		g_player.state = ST_STOPPING;

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
