/*
 * network.c
 *  Used to communicate with the wificar software
 *  jgfntu@163.com
 *  
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <pthread.h>
#include <netinet/tcp.h>
#include <semaphore.h>
#include <signal.h>

#include "types.h"
#include "protocol.h" 
#include "network.h"
#include "rw.h"
#include "prase_pkt.h"
#include "int_to_array.h"
#include "camera.h"
#include "audio.h"
#include "utils.h"
#include "wifi_debug.h"
#include "t_rh.h"
#include "led_control.h"
/* ---------------------------------------------------------- */

#define SERVER_PORT 80
#define BACKLOG 	10

#define MAX_LEN 	1024*10		/* 10kb */

extern unsigned int tem_integer;
extern unsigned int tem_decimal;
extern unsigned int rh;
extern int led_flag;

int can_send = 0;				/* if AVcommand ID is same user<->camera, can_send = 1, allow send file */
int data_ID  = 7501;			/* AVdata command text[23:26] data connetion ID */

char str_ctl[]	 = "MO_O";
char str_data[]  = "MO_V";
char str_camID[] = "yuanxiang7501";
char str_camVS[] = {0, 2, 0, 0};
char str_tmp[14];

/* ---------------------------------------------------------- */

u8 *buffer;						/* TODO for opcode protocol */ 
u8 *buffer2;					/* TODO for AVdata protocol */
u8 *text;						/* TODO for opcode command text */
u8 *AVtext;						/* TODO for AVdata command text */

u32 pic_num = 1;				/* for time stamp */

int send_audio_flag = 1;

/* audio param */
static int AVcommand_fd;

/* TODO commands list */
static struct command *command1;
static struct command *command3;
static struct command *command255;
static struct command *command5;
static struct command *av_command1;
static struct command *av_command2;
static struct video_data *video_data;
static struct audio_data *audio_data;

/* send command */
/* TODO */
/* send AVcommand */
/* TODO */
/* recv command */
/* TODO */

#ifdef ENABLE_VIDEO
extern sem_t start_camera;
#endif

sem_t start_music;

/* add by zjw for feal bat_info */
FILE *bat_fp;
int bat_info[20]={0};
u32 battery_fd;

int picture_fd, audio_data_fd, music_data_fd;

pthread_mutex_t AVsocket_mutex;

/* ---------------------------------------------------------- */
/* deal bat info */
void deal_bat_info()
{
	struct command *command252;
	struct fetch_battery_power_resp *fetch_battery_power_resp;
	int bat_capacity;

	command252 = malloc(sizeof(struct command) + 1);
	fetch_battery_power_resp = &command252->text[0].fetch_battery_power_resp;
	memcpy(command3->protocol_head, str_ctl, 4);
	command252->opcode = 252;
	command252->text_len = 1;

	bat_fp = fopen("/sys/class/power_supply/battery/capacity","r");
	if (bat_fp == NULL)
		//printf("open  capacity failed\n");
		;
	else
		//printf("open capacity succed\n");
		;

	if ((fread(bat_info, 20, 1, bat_fp)) != 1)
		//printf("read from bat_fd failed\n");
		;

	//printf("bat_info[0]:%d,bat_info[1]:%d,bat_info[2]:%d\n",bat_info[0],bat_info[1],bat_info[2]);
	bat_capacity = atoi(bat_info);
	//printf("bat_capacity:%d\n",bat_capacity);
	if (fclose(bat_fp) != 0)
		printf("close bat_fp failed\n");

	fetch_battery_power_resp->battery = bat_capacity;

	if ((send(battery_fd, command252, 24, 0)) == -1) {
		perror("send");
		close(battery_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

free_buf:
	free(command252);
}

/*
 *enable temperature and relative humidity sent
 */
void enable_t_rh_sent()
{
    int tem_rh_fd = AVcommand_fd;
    struct command *command21;
    struct tem_rh_data *tem_rh_data;
	command21 = malloc(sizeof(struct command) + 4);
    memcpy(command21->protocol_head, str_ctl, 4);
    tem_rh_data = &command21->text[0].tem_rh_data;
    tem_rh_data->tem_integer = (u8)tem_integer;
    tem_rh_data->tem_decimal = (u8)tem_decimal;
    tem_rh_data->rh = (u8)rh;
    command21->opcode = 21; 
    command21->text_len = 3;
    //printf("tem_integer is %d,tem_decimal is %d,rh is %d\n",tem_integer,tem_decimal,rh);
    if (send(tem_rh_fd, command21, 26, 0) == -1){ 
	perror("send");
        close(tem_rh_fd);
		printf("========%s,%u========\n",__FILE__,__LINE__);
        exit(0);
    }   
	free(command21);
	command21 = NULL;
    return;
}                           

/*
 * enable audio_send(void)
 */
void enable_audio_send()
{
	int audio_fd = AVcommand_fd;
	
	struct command *command9;
	struct audio_start_resp *audio_start_resp;

	command9 = malloc(sizeof(struct command) + 6);
	audio_start_resp = &command1->text[0].audio_start_resp;

	memcpy(command9->protocol_head, str_ctl, 4);
	command9->opcode = 9;
	command9->text_len = 6;

	audio_start_resp->result = 0;				/* 0 : agree */
	audio_start_resp->data_con_ID = 0;			/* TODO (FIX ME) must be 0 */

	if ((send(audio_fd, command9, 29, 0)) == -1) {
		perror("send");
		close(audio_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

	free(command9);
	command9 = NULL;

}

/* enable talk data */
void send_talk_resp()
{
	int talk_fd = AVcommand_fd;
	
	struct command *command12;
	struct talk_start_resp *talk_start_resp;

	command12 = malloc(sizeof(struct command) + 6);
	talk_start_resp = &command1->text[0].talk_start_resp;

	memcpy(command12->protocol_head, str_ctl, 4);
	command12->opcode = 12;
	command12->text_len = 6;

	talk_start_resp->result = 0;				/* 0 : agree */
	talk_start_resp->data_con_ID = 0;			/* TODO (FIX ME) must be 0 */

	if ((send(talk_fd, command12, 29, 0)) == -1) {
		perror("send");
		close(talk_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

	free(command12);
	command12 = NULL;
}
	

/*
 * disable audio data send
 */
void disable_audio_send()
{
	/* do nothing */
}

/* receive AVcommand from user */ 
static int recv_AVcommand(u32 client_fd)
{
	int n;				/* actual read or write bytes number */
	int i;

	/* TODO if correct, user will send AVcommand 0 here to request for AV connection */
	memset(buffer2, 0, 100);

	/* read AVdata connection command from user(client) */
	if ((n = recv(client_fd, buffer2, 100, 0)) == -1)
	{
		perror("recv");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

	cmd_dbg("---------------------------------------\n");
	cmd_dbg("data number = %d\n", n);

	memcpy(str_tmp, buffer2, 4);
	str_tmp[4] = '\0';
	cmd_dbg("buf[0:3] = %s\n", str_tmp);
	cmd_dbg("buf[4] = %d\n", buffer2[4]);

	/* judge opcode user->camera 0:login_req 3:talk_data */
	switch (buffer2[4]) {
		case 0: 
			memcpy(AVtext, buffer2 + 23, 4);
			prase_AVpacket(buffer2[4], AVtext);
			break;
		case 3:										/* TODO talk_data*/
			memcpy(AVtext, buffer2 + 23, n - 23);
			prase_AVpacket(buffer2[4], AVtext);
			break;
		default: 
			printf("Invalid opcode from user!\n");	
			return -1;
	}

	cmd_dbg("---------------------------------------\n");
	for (i = 4; i < n; i++) {
		cmd_dbg("buf[%d] = %d\n", i, buffer2[i]);
	};
	cmd_dbg("---------------------------------------\n");

	/* ------------------------------------------- */

	return 0;
}

void send_picture(char *data, u32 length)
{
	u32 send_len = 0;

	av_command1->text_len = length + 13;			/* TODO */

	video_data->pic_len = length;
	video_data->frame_time = pic_num++;				/* test for time stamp */

	pthread_mutex_lock(&AVsocket_mutex);

	if ((send_len = send(picture_fd, av_command1, 36, 0)) == -1) {
		perror("send_header");
		close(picture_fd);
        printf("========%s,%u,%d==========\n",__FILE__,__LINE__,picture_fd);
		exit(0);
	}
	if (send_len != 36)
		printf("opcode header short send!\n");

	if ((send_len = send(picture_fd, (void *)data, length, 0)) == -1) {
		perror("send_pic");
		close(picture_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}
	if (send_len != length)
		printf("picture short send!\n");

	pthread_mutex_unlock(&AVsocket_mutex);
}

/*
 * send audio data
 */
int send_audio_data(u8 *audio_buf, u32 data_len)
{
	av_command2->text_len = data_len + 20;			/* contant sample and index */
	audio_data->ado_len = data_len;

	pthread_mutex_lock(&AVsocket_mutex);

	if ((send(audio_data_fd, av_command2, 40, 0)) == -1) {
		perror("send");
		close(audio_data_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}
	/* ------------------------------------------- */
	//printf("audio_data_size = %d\n", data_len);
	send(audio_data_fd, (void *)audio_buf, data_len + 3, 0);

	pthread_mutex_unlock(&AVsocket_mutex);

	return 1;
}

/* TODO keep opcode connection, every 1 minute, or network will be disconnected */
void keep_connect(void)
{
	int client_fd = AVcommand_fd;
	int n;
	
	command255->opcode = 255;
	command255->text_len = 0;

	/* write command to client --- keep alive */
	if ((n = send(client_fd, command255, 23, 0)) == -1) {
		perror("send");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}
}

/* ------------------------------------------- */

/* set opcode connection */
void set_opcode_connection(u32 client_fd)
{
	int m, n, offset, text_size;
	int i;
	int text_len;

	struct login_req *login_req;
	struct login_resp *login_resp;
	struct verify_req *verify_req;
	struct verify_resp *verify_resp;
	struct video_start_resp *video_start_resp;
	
	/* ------------------------------------------- */
	battery_fd = client_fd;

#ifdef WIFI_CAR_V3
	/* TODO read first command 0 from client */
	if ((n = recv(client_fd, buffer, 100, 0)) == -1) {
		perror("recv");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}
	
	login_req = (struct login_req *)(buffer + 23);

	buffer[n] = '\0';
	/* we can do something here to process connection later! */
	wifi_dbg("---------------------------------------\n");
	wifi_dbg("data number = %d\n", n);
	wifi_dbg("buf[protocol] = %s\n", buffer);
	wifi_dbg("buf[4] = %d\n", buffer[4]);
	wifi_dbg("---------------------------------------\n");

	for (i = 4; i < 23; i++) {
		text_dbg("buf[%d] = %d\n", i, buffer[i]);
	};

	printf("v1 v2 v3 v4 : %d %d %d %d\n", login_req->ch_num1, login_req->ch_num2, \
			login_req->ch_num3, login_req->ch_num4);
#else
	/* TODO read first command 0 from client */
	if ((n = recv(client_fd, buffer, 100, 0)) == -1) {
		perror("recv");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

	buffer[n] = '\0';
	/* we can do something here to process connection later! */
	wifi_dbg("---------------------------------------\n");
	wifi_dbg("data number = %d\n", n);
	wifi_dbg("buf[protocol] = %s\n", buffer);
	wifi_dbg("buf[4] = %d\n", buffer[4]);
	wifi_dbg("---------------------------------------\n");

	for (i = 4; i < 23; i++) {
		text_dbg("buf[%d] = %d\n", i, buffer[i]);
	};
#endif

	
	/* ------------------------------------------- */
#ifdef WIFI_CAR_V3
	/* TODO send login response to user by command 1, contains v1, v2, v3, v4 */
	text_size = sizeof(struct login_resp);
	command1 = malloc(sizeof(struct command) + text_size);
	login_resp = &command1->text[0].login_resp;

	memcpy(command1->protocol_head, str_ctl, 4);
	command1->opcode = 1;
	command1->text_len = text_size;

	memset(login_resp, 0, text_size);
	login_resp->result = 0;
	memcpy(login_resp->camera_ID, str_camID, 13);
	memcpy(login_resp->camera_version, str_camVS, sizeof(str_camVS));

	/* write command1 to client */
	if ((n = send(client_fd, command1, 23 + text_size, 0)) == -1) {
		perror("send");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

#else
	/* TODO send login response to user by command 1, contains camera's version and ID */
	command1 = malloc(sizeof(struct command) + 27);
	login_resp = &command1->text[0].login_resp;

	memcpy(command1->protocol_head, str_ctl, 4);
	command1->opcode = 1;
	command1->text_len = 27;

	memset(login_resp, 0, 27);
	login_resp->result = 0;
	memcpy(login_resp->camera_ID, str_camID, 13);
	memcpy(login_resp->camera_version, str_camVS, sizeof(str_camVS));

	/* write command1 to client */
	if ((n = send(client_fd, command1, 50, 0)) == -1) {
		perror("send");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}
#endif
	/* ------------------------------------------- */
	/* ------------------------------------------- */

	/* TODO receive verify request from user(command2), text will combine ID and PW */
	memset(buffer, 0, 100);
	memset(buffer2, 0, 100);

#ifdef WIFI_CAR_V3
	/* read command from client */
	if ((n = recv(client_fd, buffer, 100, 0)) == -1) {
		perror("recv");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

	wifi_dbg("---------------------------------------\n");
	wifi_dbg("data number = %d\n", n);
	wifi_dbg("buf[protocol] = %s\n", buffer);
	wifi_dbg("buf[4] = %d\n", buffer[4]);
	wifi_dbg("---------------------------------------\n");
	
	verify_req = (struct verify_req *)(buffer + 23);
	printf("a1 a2 a3 a4 : %d %d %d %d\n", verify_req->au_num1, verify_req->au_num2, \
			verify_req->au_num3, verify_req->au_num4);

#else
	/* read command from client */
	if ((n = recv(client_fd, buffer, 100, 0)) == -1) {
		perror("recv");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

	wifi_dbg("data number = %d\n", n);
	memcpy(str_tmp, buffer, 4);
	str_tmp[4] = '\0';
	wifi_dbg("buf[0:3] = %s\n", str_tmp);
	wifi_dbg("buf[4] = %d\n", buffer[4]);
	memcpy(str_tmp, buffer + 23, 13);
	str_tmp[13] = '\0';
	wifi_dbg("buf[camID] = %s\n", str_tmp);
	memcpy(str_tmp, buffer + 36, 13);
	str_tmp[13] = '\0';
	wifi_dbg("buf[camVS] = %s\n", str_tmp);
	wifi_dbg("---------------------------------------\n");
#endif
	/* TODO we can do something here to process connection later! */
	//usleep(1000);

	/* ------------------------------------------- */

	/* TODO verify respose for user by command 3, result = 0 means correct */
	command3 = malloc(sizeof(struct command) + 3);
	verify_resp = &command3->text[0].verify_resp;

	memcpy(command3->protocol_head, str_ctl, 4);
	command3->opcode = 3;
	command3->text_len = 3;

	memset(verify_resp, 0, 3);
	verify_resp->result = 0;				/* verify success */
	verify_resp->reserve = 0;				/* exist when result is 0 */

	/* write command 3 to client */
	if ((n = send(client_fd, command3, 26, 0)) == -1) {
		perror("send");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

	/* ------------------------------------------- */

	/* TODO if correct, user will send command 4 here, request for video connection */
	memset(buffer, 0, 100);

	/* read command from client */
	if ((n = recv(client_fd, buffer, 100, 0)) == -1) {
		perror("recv");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

	wifi_dbg("data number = %d\n", n);
	memcpy(str_tmp, buffer, 4);
	str_tmp[4] = '\0';
	wifi_dbg("buf[0:3] = %s\n", str_tmp);
	/* we can do something here to process connection later! */
	wifi_dbg("buf[4] = %d\n", buffer[4]);
	wifi_dbg("---------------------------------------\n");

	for (i = 4; i < n; i++) {
		text_dbg("buf[%d] = %d\n", i, buffer[i]);
	};

	/* ------------------------------------------- */
	
	/* TODO start connect video, response to user by command 5 */
	command5 = malloc(sizeof(struct command) + 6);
	video_start_resp = &command5->text[0].video_start_resp;

	memcpy(command5->protocol_head, str_ctl, 4);
	command5->opcode = 5;
	command5->text_len = 6;

	video_start_resp->result = 0;					/* agree for connection */
	video_start_resp->data_con_ID = data_ID;		/* TODO ID */

	if ((n = send(client_fd, command5, 29, 0)) == -1) {
		perror("send");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

	wifi_dbg("---------------------------------------\n");

	/* ------------------------------------------- */
	/* ------------------------------------------- */

	/* TODO go on waiting for reading in the while() */
	while (1) {
		//memset(buffer, 0, 100);
		//memset(text, 0, 100);

		/* read command from client */
		if ((n = recv(client_fd, buffer, 100, 0)) == -1) {
			perror("recv");
			close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
			exit(0);
		}
		//printf("n = %d\n", n);
		wifi_dbg("---------------------------------------\n");
		wifi_dbg("data number = %d\n", n);
		//memcpy(str_tmp, buffer, 4);
		//str_tmp[4] = '\0';
		wifi_dbg("buf[0:3] = %s\n", str_tmp);
		/* we can do something here to process connection later! */
		wifi_dbg("buf[4] = %d\n", buffer[4]);
		wifi_dbg("---------------------------------------\n");

		/* -------------------------------------------- */
		/* process command 250, ctl moto, each command size is 25 */
		if ( n % 25 == 0) {
			n /= 25;
			m = 0;
			while ((m - n) < 0) {
				offset = m * 25;
				m++;
				/* TODO prase opcode protocols */
				memcpy(text, buffer + 15 + offset, 4);
				//text_len = byteArrayToInt(text, 0, 4);
				//if (( n - 23) > text_len)
				//	printf("we have one more protocol to recive!\n");		/* TODO */
				//memset(text, 0, 100);
				//memcpy(text, buffer + 23 + offset, text_len);
				memcpy(text, buffer + 23 + offset, 2);
				if (prase_packet(buffer[4 + offset], text) == -1)
					continue;
			}
		} else {
			memcpy(text, buffer + 15, 4);
			text_len = byteArrayToInt(text, 0, 4);
			if (( n - 23) > text_len)
				printf("we have one more protocol to recive!\n");			/* TODO */
			memcpy(text, buffer + 23, text_len);
			if (prase_packet(buffer[4], text) == -1)
				continue;
		}
		/* -------------------------------------------- */

		//for (i = 4; i < n; i++) {
		//	text_dbg("buf[%d] = %d\n", i, buffer[i]);
		//};
		//	wifi_dbg("buf[23] = %d\n", buffer[23]);
		//	wifi_dbg("buf[24] = %d\n", buffer[24]);
		//wifi_dbg("---------------------------------------\n");
	}
}

/* ------------------------------------------- */

/* opcode connect working thread */
void *deal_opcode_request(void *arg)
{
	int client_fd = *((int *)arg);

	/* seperate mode, free source when thread working is over */
	pthread_detach(pthread_self());

	/* set connection */
	set_opcode_connection(client_fd);

free_buf:
	/* malloc in the connect() */
	free(command1);
	free(command3);
	free(command255);
	/* malloc in the main() */
	free(arg);
	/* buffer malloc */
	free(buffer);
	close(client_fd);
	pthread_exit(NULL);
}

/* ------------------------------------------- */

/* TODO wifi main function, later will be called in the main thread in core/main.c */
void network(void)
{
	int server_fd, *client_fd;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
	int sin_size;
	int opt = 1;							/* allow server address reuse */
	int flag = 1;							/* TODO thread flag */
	int nagle_flag = 1;						/* disable Nagle */
	
	/* malloc buffer for socket read/write */
	buffer = malloc(100);					/* TODO */
	buffer2 = malloc(100);					/* TODO */
	AVtext = malloc(200);					/* TODO */
	text = malloc(100);						/* TODO */

	sem_init(&start_music, 0, 0);

	/* create socket */
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (-1 == server_fd) {
		perror("socket");
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(1);
	}
	
	/* allow server ip address reused */
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
	/* disable the Nagle (TCP No Delay) algorithm */
	setsockopt(server_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nagle_flag, sizeof(nagle_flag));

	/* set server message */
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(SERVER_PORT);
	server_addr.sin_addr.s_addr = inet_addr("192.168.100.100");
	bzero(&(server_addr.sin_zero), 8);
	
	/* bind port */
	if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(1);
	}

	/* start listening */
	if (listen(server_fd, BACKLOG) == -1) {
		perror("listen");
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(1);
	}

	/* ---------------------------------------------------------------------------- */
	pthread_t th1;				/* main thread for opcode command */

	if (pthread_mutex_init(&AVsocket_mutex, NULL) != 0) {
		perror("Mutex initialization failed!\n");
		return;
	}
	
	while (1) {

		sin_size = sizeof(struct sockaddr_in);

		/* must request dynamicly, for diff threads */
		client_fd = (int *)malloc(sizeof(int));
		
		if ((*client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &sin_size)) == -1) {
			perror("accept");
			continue;
		}

		printf("server: got connection from %s,,,,%d\n", inet_ntoa(client_addr.sin_addr),client_fd);


		/* TODO(FIX ME): disable the Nagle (TCP No Delay) algorithm */
		setsockopt(*client_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nagle_flag, sizeof(nagle_flag));

		/* create a new thread for each client request */
		if (flag == 1) {
			flag = 2;
			AVcommand_fd = *client_fd;
			/* ----------------------------------------------------------------- */

            led_flag = 0;
            led_on();
            /* TODO keep connection ever 1 minute */
            command255 = malloc(sizeof(struct command));
            memcpy(command255->protocol_head, str_ctl, 4);
			
			/* ----------------------------------------------------------------- */
			if(pthread_create(&th1, NULL, deal_opcode_request, client_fd) != 0) {
				perror("pthread_create");
				break;
			}
		} else if (flag == 2){
			flag = 3;
			printf(">>>>>in video or record data connection<<<<<\n");
			/* ----------------------------------------------------------------- */

			picture_fd = *client_fd;					
			audio_data_fd = *client_fd;
			
			av_command1 = malloc(sizeof(struct command) + 13);
			video_data = &av_command1->text[0].video_data;
			memcpy(av_command1->protocol_head, str_data, 4);
			av_command1->opcode = 1;
			memset(video_data, 0, 13);

			av_command2 = malloc(sizeof(struct command) + 17);
			audio_data = &av_command2->text[0].audio_data;
			memcpy(av_command2->protocol_head, str_data, 4);
			av_command2->opcode = 2;
			memset(audio_data, 0, 17);
			
			/* ----------------------------------------------------------------- */
#ifdef ENABLE_VIDEO
			sem_post(&start_camera);		/* start video only when cellphone send request ! */
#endif
		}
		else if (flag == 3){
			flag = 3;
			music_data_fd = *client_fd;
			/* TODO just want to erase the no using data int the new socket buffer */
			clear_recv_buf(music_data_fd);
			/* when the new socket is connected, then tell the cellphone to start sending file */
			sem_post(&start_music);
		}
	}
	pthread_join(th1, NULL);

	pthread_mutex_destroy(&AVsocket_mutex);
	
	free(av_command1);
	free(av_command2);

	close(picture_fd);
	close(server_fd);
}














