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

#ifdef USE_FMT_ADPCM
#include "adpcm.h"
#endif
/* --------------------------------------------------------- */

pthread_t playback_td, runtime_playback_td, talk_playback_td;
static sem_t write_play_buf0, write_play_buf1;
static sem_t read_play_buf0, read_play_buf1;
sem_t runtime_recv, runtime_play;

extern int playback_on;
extern int audio_data_fd, music_data_fd;

#ifdef USE_FMT_ADPCM
/* double buffer for runtime playbacking */
u8 play_buf0[ADPCM_MAX_READ_LEN];
u8 play_buf1[ADPCM_MAX_READ_LEN];
static adpcm_state_t adpcm_state = {0, 0};
u8 adpcm_play_buf0[ADPCM_MAX_READ_LEN * 4];
u8 adpcm_play_buf1[ADPCM_MAX_READ_LEN * 4];
#else
/* double buffer for runtime playbacking */
u8 play_buf0[MAX_READ_LEN];
u8 play_buf1[MAX_READ_LEN];
#endif

u8 runtime_recv_buf[RUNTIME_RECV_LEN];				/* TODO */

extern int oss_open_flag, oss_close_flag, oss_fd;
/* --------------------------------------------------------- */

void init_receive(void)
{
	/* do nothing, just for compile */
}

/* --------------------------------------------------------- */

/*
 * thread function for talk playbacking, write double buffer 
 * data to alsa snd
 */
void deal_talk_play(void *arg)
{
#ifdef USE_FMT_ADPCM
	int write_len = ADPCM_MAX_READ_LEN * 4;
#else
	int write_len = MAX_READ_LEN;
#endif

	printf(">>>Start recving talking data from cellphone ...\n");
	
	while (playback_on) {
		/* buffer_0 */
		sem_wait(&read_play_buf0);
#ifdef USE_FMT_ADPCM
		adpcm_decoder(play_buf0, (short *)adpcm_play_buf0, ADPCM_MAX_READ_LEN, &adpcm_state);
		playback_buf((u8 *)adpcm_play_buf0, write_len);
		/* TODO (FIX ME) assume the two parames always 0 */
		//memset((u8 *)&adpcm_state, 0, 3);
		//adpcm_state.valprev = 0;
		//adpcm_state.index = 0;
#else
		playback_buf(play_buf0, write_len);
#endif
		sem_post(&write_play_buf0);
		//fprintf(stderr, "-");
		
		if (playback_on) {
			/* buffer_1 */
			sem_wait(&read_play_buf1);
#ifdef USE_FMT_ADPCM
			adpcm_decoder(play_buf1, (short *)adpcm_play_buf1, ADPCM_MAX_READ_LEN, &adpcm_state);
			playback_buf((u8 *)adpcm_play_buf1, write_len);
			/* TODO (FIX ME) assume the two parames always 0 */
			//memset((u8 *)&adpcm_state, 0, 3);
			//adpcm_state.valprev = 0;
			//adpcm_state.index = 0;
#else
			playback_buf(play_buf1, write_len);
#endif
			sem_post(&write_play_buf1);
			//fprintf(stderr, "-");
		}
	}
	
	pthread_exit(NULL);
}

/*
 * thread function for runtime playbacking, write double buffer 
 * data to alsa snd
 */
void deal_runtime_play(void *arg)
{
	/* TODO (FIX ME)
	 * tmp use deal_talk_play() func
	 */
	int file_size = *(int *)arg;
	
	//if (init_mp3decoder(file_size) != 0)
	//	return ;
	
	/* TODO (FIX ME)
	 * !!! sem_post(&runtime_recv) may not be called in mp3-decoder.c(file source)
	 * when we stop playback, it may case socket buffer full, then socket_write func
	 * in cellphone(java socket write thread) may be into system call sleep,
	 * bad is, this will cause that thead none return, so we call sem_post(&runtime_recv)
	 * here before deal_runtime_play thread return, so the socket buffer can save the data
	 * (next buffer size is 80KB, enough for the data in cellphone socket buffer write func
	 * then the java thread can exit normally
	 */
	sem_post(&runtime_recv);
	
	printf("<<<out of deal_runtime_play thread!\n");
	
	pthread_exit(NULL);
}

/*
 * func for talk (recording data received from cellphone) playback
 */
static int talk_playback(u32 client_fd)
{
	int error_flag, read_len;

	oss_close_flag++;
	
    printf("in talk_playback>>>>>>>>>>\n");
	if (!oss_open_flag) {
		/* open audio(oss) device for playbacking */
		oss_fd = open(OSS_AUDIO_DEV, O_RDWR);
		if (oss_fd < 0) {
			fprintf(stderr, "Open audio(oss) device failed!\n");
			return -1;
		}
		
		oss_open_flag = 1;
		
		/* set oss configuration */
		set_oss_play_config(oss_fd, AUDIO_RATE, AUDIO_CHANNELS, AUDIO_BIT);
		
		printf(">>>Open playback audio device\n");
	}
	
	/* semaphore double buffer */
	sem_init(&write_play_buf0, 0, 0);			/* read socket to buffer */
	sem_init(&write_play_buf1, 0, 0);
	sem_init(&read_play_buf0, 0, 0);			/* read from buffer to alsa driver */
	sem_init(&read_play_buf1, 0, 0);
	/* for first use of double buffer */
	sem_post(&write_play_buf0);
	sem_post(&write_play_buf1);
	
	/*
	 * TODO (FIX ME)
	 * create a thread for playbacking recording data from cellphone
	 */
	error_flag = pthread_create(&talk_playback_td, NULL, deal_talk_play, NULL);
	if (error_flag < 0) {
		printf("Create talk playback thread faild!\n");
		return -1;
	}
	
#ifdef USE_FMT_ADPCM
	read_len = ADPCM_MAX_READ_LEN;
#else
	read_len = MAX_READ_LEN;
#endif

	/* read socket until cellphone's recording stop, under the semaphore mechanism (sem_t play_buf0 \ play_buf1) */
	while (playback_on) {
		/* buffer_0 */
		sem_wait(&write_play_buf0);
#ifdef USE_FMT_ADPCM
		/* TODO (FIX ME)
		 * if not use readall method, we may use another method to fill the buffer all
		 */
		//error_flag = recv(client_fd, play_buf0, read_len, 0);	
		error_flag = read_all(client_fd, play_buf0, &read_len);
		if (error_flag < 0)
			return -1;
		if (read_len != ADPCM_MAX_READ_LEN)
			printf("###error read (adpcm): %dB###\n", error_flag);	
#else
		error_flag = read_all(client_fd, play_buf0, &read_len);
		if (error_flag < 0)
			return -1;
#endif
		sem_post(&read_play_buf0);		
		//fprintf(stderr, ".");
		
		if (playback_on) {
			/* buffer_1 */
			sem_wait(&write_play_buf1);
#ifdef USE_FMT_ADPCM
			/* TODO (FIX ME)
			 * if not use readall method, we must use another one to fill the buffer full
			 */
			//error_flag = recv(client_fd, play_buf1, read_len, 0);		
			error_flag = read_all(client_fd, play_buf1, &read_len);
			if (error_flag < 0)
				return -1;
			if (read_len != ADPCM_MAX_READ_LEN)
				printf("###error read (adpcm): %dB###\n", error_flag);
#else
			error_flag = read_all(client_fd, play_buf1, &read_len);
			if (error_flag < 0)
				return -1;
#endif
			sem_post(&read_play_buf1);		
			//fprintf(stderr, ".");
		}
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

/*
 * func for runtime playback
 */
static int runtime_playback(u32 client_fd)
{
	/* TODO (FIX ME)
	 * tmp use talk_playback() func
	 */
    printf("in runtime_playback\n");
	int error_flag, read_len;
	u8 size_buf[8];
	long *p_file_size, file_size, read_left;

	oss_close_flag++;
	
#if 1	
	/* receive file size from cellphone */
	if (recv(client_fd, size_buf, 8, 0) == -1) {
		perror("recv");
		close(client_fd);
		exit(0);
	}
	
	p_file_size = (u32 *)size_buf;
	file_size = *p_file_size;
#else
	file_size = 3785422;
#endif
	/* --------------------------------------------------------- */
	/* TODO move here */
	if (!oss_open_flag) {
		/* open audio(oss) device for playbacking */
		oss_fd = open(OSS_AUDIO_DEV, O_RDWR);
		if (oss_fd < 0) {
			fprintf(stderr, "Open audio(oss) device failed!\n");
			return -1;
		}
		
		oss_open_flag = 1;
		
		/* set oss configuration */
		set_oss_play_config(oss_fd, AUDIO_RATE, AUDIO_CHANNELS, AUDIO_BIT);
		
		printf(">>>Open playback audio device\n");
	}
	/* --------------------------------------------------------- */
	printf(">>>receive file size : %d字节\n", file_size);
	
	read_left = file_size;
	
	/* semaphore double buffer */
	sem_init(&runtime_recv, 0, 0);			/* read socket to buffer */
	sem_init(&runtime_play, 0, 0);
	/* for first use of double buffer */
	sem_post(&runtime_recv);
	
	/*
	 * TODO (FIX ME)
	 * create a thread for playbacking recording data from cellphone
	 */
	error_flag = pthread_create(&runtime_playback_td, NULL, deal_runtime_play, &file_size);
	if (error_flag < 0) {
		printf("Create talk playback thread faild!\n");
		return -1;
	}
	
	read_len = RUNTIME_RECV_LEN;

	/* read socket until cellphone's recording stop, under the semaphore mechanism (sem_t play_buf0 \ play_buf1) */
	//while (playback_on & (read_left > 0)) {
	while (playback_on) {
		/* buffer_0 */
		sem_wait(&runtime_recv);
		
		//printf("get runtime_recv post\n");
		
		/* TODO (FIX ME)
		 * if not use readall method, we may use another method to fill the buffer all
		 */
		error_flag = read_all(client_fd, runtime_recv_buf, &read_len);
		if (error_flag < 0)
			return -1;
		if (read_len != RUNTIME_RECV_LEN)
			printf("###error read (adpcm): %dB###\n", error_flag);	

		sem_post(&runtime_play);
		read_left -= read_len;
		
		fprintf(stderr, ".");
	}
}

/* TODO (FIX ME)
 * thread for receiving file from cellphone or runtime playbacking
 */
void *deal_FEdata_request(void *arg)
{
	int client_fd;
#if 0
	int file_fd;
	char *file_name = "fhcq.mp3";
	file_fd = open(file_name, O_RDONLY);
	if (file_fd < 0) {
		fprintf(stderr, "file: cannot open '%s'\n", file_name);
		return -1;
	}
	client_fd = file_fd;
#else
	//client_fd = audio_data_fd;
	client_fd = music_data_fd;
#endif

	/* seperate mode, free source when thread working is over */
	pthread_detach(pthread_self());

#if ENABLE_RUNTIME_PLAYBACK
	/* receive file from cellphone to buffer and playback */
	if (runtime_playback(client_fd) == -1)
		printf("File receive connection is error!\n");
#elif ENABLE_TALK_PLAYBACK
	/* receive talk data frome cellphone */
	if (talk_playback(client_fd) == -1)
		printf("File receive connection is error!\n");
#endif

	printf("<<<out of runtime_recv thread!\n");
	
free_buf:
	/* malloc in the main() */
	//free(arg);
	close(client_fd);
	pthread_exit(NULL);
}

/*
 * create playback audio thread
 */
void enable_playback_audio()
{
	if(pthread_create(&playback_td, NULL, deal_FEdata_request, NULL) != 0) {
		perror("pthread_create");
		return;
	}
}

