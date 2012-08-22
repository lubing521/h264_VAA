/*
 * protocol.h
 *  the wileless commuicate protocol
 *  jgfntu@163.com
 */

#ifndef __NETWORK_H
#define __NETWORK_H
void network(void);
void enable_t_rh_sent(void);
void confirm_stop(void);
//#define BLOWFISH
#ifdef BLOWFISH
#define BLOWFISH_KEY "seuic"
void BlowfishKeyInit(char* strKey,int nLen);
#endif

#endif

