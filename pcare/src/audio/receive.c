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

extern sem_t start_music;
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

extern int oss_fd;
/* --------------------------------------------------------- */

void init_receive(void)
{
    /* do nothing, just for compile */
#ifdef USE_FMT_ADPCM
    adpcm_state.valprev = 0;
    adpcm_state.index = 0;
#endif
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
        adpcm_decoder(play_buf0, (short *)adpcm_play_buf0, ADPCM_MAX_READ_LEN, &adpcm_state);
        if(playback_buf((u8 *)adpcm_play_buf0, write_len)<0)
        {
            printf("deal_talk_play thread will exit!adpcm_play_buf0\n");
            talk_playback_td =-1;
            sem_post(&write_play_buf0);
            sem_post(&write_play_buf1);
            pthread_exit(NULL);
        }
        /* TODO (FIX ME) assume the two parames always 0 */
        //memset((u8 *)&adpcm_state, 0, 3);
        //adpcm_state.valprev = 0;
        //adpcm_state.index = 0;
        sem_post(&write_play_buf0);

        if (playback_on) {
            /* buffer_1 */
            sem_wait(&read_play_buf1);
            adpcm_decoder(play_buf1, (short *)adpcm_play_buf1, ADPCM_MAX_READ_LEN, &adpcm_state);
            if(playback_buf((u8 *)adpcm_play_buf1, write_len)<0)
            {
                printf("deal_talk_play thread will exit!adpcm_play_buf1\n");
                talk_playback_td=-1;
                sem_post(&write_play_buf1);
                pthread_exit(NULL);
            }
            /* TODO (FIX ME) assume the two parames always 0 */
            //memset((u8 *)&adpcm_state, 0, 3);
            //adpcm_state.valprev = 0;
            //adpcm_state.index = 0;
            sem_post(&write_play_buf1);
        }
    }

#ifdef USE_FMT_ADPCM
    adpcm_state.valprev = 0;
    adpcm_state.index = 0;
#endif	
    printf("deal_talk_play thread will exit!\n");
    talk_playback_td =-1;
    pthread_exit(NULL);
}


/*
 * func for talk (recording data received from cellphone) playback
 */
static int talk_playback(u32 client_fd)
{
    int error_flag, read_len,error_num=0;
    printf("in talk_playback>>>>>>>>>>\n");
    u8 size_buf[8];
    u8 oss_config_buf[3];
    unsigned rate,channels,bit,ispk;
    long *p_file_size, file_size,read_left;
    /* receive file size from cellphone */
    if (recv(client_fd, size_buf, 8, 0) == -1) {
        perror("recv");
        close(client_fd);
        exit(0);
    }
    p_file_size = (u32 *)size_buf;
    file_size = *p_file_size;
    read_left = file_size;
    printf(">>>receive file size : %d字节\n", file_size);
    if (recv(client_fd, oss_config_buf, 4, 0) == -1) {
        perror("recv");
        close(client_fd);
        exit(0);
    }
    rate=un_OSS_RATE[oss_config_buf[0]];
    channels=oss_config_buf[1];
    bit=oss_config_buf[2];
    ispk=oss_config_buf[3];
    printf("rate is %d,channels is %d,bit is %d,ispk is %d\n",rate,channels,bit,ispk);
    if(channels !=1||bit!=16||oss_config_buf[0]>7||file_size<0)
    {
        printf("received file error !!\n");
        return -1;
    }
    if(oss_fd > 0)
    {
        close(oss_fd);
        oss_fd= 0;
        printf("audio(oss) already opened! close it!\n");
    }
    oss_fd = open(OSS_AUDIO_DEV,O_RDWR);
    if (oss_fd < 0) {
        fprintf(stderr, "Open audio(oss) device failed!\n");
        return -1;
    }

    /* set oss configuration */
    if( set_oss_play_config(oss_fd, rate,AUDIO_CHANNELS, bit) < 0 )
    {
        printf("Err: set oss play config failed!\n");
        close(oss_fd);
        oss_fd = 0;
        return-1;
    }

    printf(">>>Open playback audio device\n");

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
    //while (playback_on) {

    while (playback_on && (read_left > 0)&&error_num<3&&talk_playback_td !=-1) {
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
        else
            printf(".");
        if (read_len != ADPCM_MAX_READ_LEN)
        {
            error_num++;
            printf("###error talk playback read 1 (adpcm): %dB###\n", error_flag);	
        }
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
            {
                error_num++;
                printf("###error talk playback read 2 (adpcm): %dB###\n", error_flag);
            } 
#else
            error_flag = read_all(client_fd, play_buf1, &read_len);
            if (error_flag < 0)
                return -1;
#endif
            if(!ispk)
                read_left -= read_len;
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

/* TODO (FIX ME)
 * thread for receiving file from cellphone or runtime playbacking
 */
void *deal_FEdata_request(void *arg)
{
    int client_fd;
    printf("before wait\n");
    sem_wait(&start_music);
    printf("after wait\n");
    /* tell cellphone to start sending file */
    send_talk_resp();
    client_fd = music_data_fd;
    /* seperate mode, free source when thread working is over */
    pthread_detach(pthread_self());
    /* receive talk data frome cellphone */
    if (talk_playback(client_fd) == -1)
        printf("File receive connection is error!\n");

    printf("end talk_playback*************\n");
    stop_playback();
    printf("<<<out of runtime_recv thread!\n");

free_buf:
    /* malloc in the main() */
    //free(arg);
    close(client_fd);
    music_data_fd =-1;
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

