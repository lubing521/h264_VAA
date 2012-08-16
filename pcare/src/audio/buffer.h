#ifndef _BUFFER_H
#define _BUFFER_H

enum QueueState
{
	QUEUE_INIT = 0,
	QUEUE_WORKING,
	QUEUE_STOPPING,
	QUEUE_STOPPED
};

#define MAX_BUF_LEN	(MAX_READ_LEN)
typedef struct s_Buffer
{
	struct s_Buffer *next;
	u8 data[MAX_BUF_LEN];
	int len;
	int flag;
}Buffer;

typedef struct
{
	Buffer *head;
	Buffer *tail;
}BufferList;

#define IN_PORT		1
#define OUT_PORT	2

typedef struct
{
	Buffer *buffer;
	BufferList list_filled;
	BufferList list_empty;
	int buffer_num;
	int state;
	int request;
	int closed_port;
	sem_t filled_buffer_ready;
	sem_t empty_buffer_ready;
	sem_t closed;
	pthread_mutex_t lock;
}BufferQueue;

Buffer *GetBuffer( BufferQueue *queue );
int EmptyBuffer( BufferQueue *queue );
Buffer *GetEmptyBuffer( BufferQueue *queue );
int FillBuffer( BufferQueue *queue );
void InitQueue( BufferQueue *queue, int num );
void EnableBufferQueue( BufferQueue *queue );
void DisableBufferQueue( BufferQueue *queue );
int CloseBufferQueue( BufferQueue *queue, int port );

#endif
