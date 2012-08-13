/*
 * prase-pkt.c
 *  prase packet
 *  jgfntu@163.com
 *
 */

#include "stdio.h"
#include "motor.h"
#include "prase_pkt.h"
#include "wifi_debug.h"
#include "audio.h"
#include "network.h"
//#include "t_rh.h"

motor_ctrl_t opt;
extern int start_measure();
extern void enable_t_rh_sent();
extern u32 audio_num;
/* prase opcode command text */
int prase_packet(int opcode, u8 *buf)
{
	//printf(">>>opcode = %d\n", opcode);
	
	switch (opcode) {
		case 250:
			opt.opt_code = 250;
			opt.param[0] = buf[0];
			opt.param[1] = buf[1];
            DispatchMotorOp( &opt );
			break;
		case 255:
			keep_connect();
			break;
		case 14:
			opt.opt_code = 14;
			opt.param[0] = buf[0];
			opt.param[1] = 0;
            DispatchMotorOp( &opt );
			break;
		case 8:
#ifdef ENABLE_CAPTURE_AUDIO
            audio_num = 0;
			start_capture();			/* !!!must be called first */
			enable_audio_send();
#else
			printf(">>>>If you want to use the capture audio, plese enable ENABLE_CAPTURE_AUDIO in the /wificar/src/include/audio.h\n");
#endif
			break;
		case 10:
#ifdef ENABLE_CAPTURE_AUDIO
            printf("send audio packages num is %lu\n",audio_num);
			stop_capture();
			disable_audio_send();
#else
			printf("<<<<Disable the capture audio\n");
#endif
			break;
		case 11:
			/* TODO enable talk audio */
			//start_playback();
            StartPlayer();
			printf(">>>Start playbacking ...\n");
			break;
		case 13:
			/* TODO disable talk audio */
			//stop_playback();
            StopPlayer();
			break;
		case 251:
			deal_bat_info();
			break;
		case 20:
			start_measure();
			enable_t_rh_sent();
			break;
		default:
			//printf(">>>opcode = %d\n", opcode);
			printf("Unsupported opcode from user!\n");
			return -1;
	}
	return 0;
}

/* prase AVdata command text */
int prase_AVpacket(int opcode, u8 *buf)
{
	int id = 0;

	if (opcode == 0) 
		if ((id = byteArrayToInt(buf, 0, 4)) == data_ID) {
			can_send = 1;
			printf("------data_ID = %d------\n", id); 
			wifi_dbg("video data can send (same data_ID)\n");
		};
	return 0;
}





