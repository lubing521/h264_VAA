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
#ifdef BLOWFISH
extern void BlowfishEncrption(unsigned long * ,int len);
extern void BlowfishDecrption(unsigned long * ,int len);
#define SBOX_BEGIN 18
const unsigned long sbox[4][256]=
{
    {
        0xd1310ba6, 0x98dfb5ac, 0x2ffd72db, 0xd01adfb7, 0xb8e1afed, 0x6a267e96, 0xba7c9045, 0xf12c7f99, 
        0x24a19947, 0xb3916cf7, 0x0801f2e2, 0x858efc16, 0x636920d8, 0x71574e69, 0xa458fea3, 0xf4933d7e, 
        0x0d95748f, 0x728eb658, 0x718bcd58, 0x82154aee, 0x7b54a41d, 0xc25a59b5, 0x9c30d539, 0x2af26013, 
        0xc5d1b023, 0x286085f0, 0xca417918, 0xb8db38ef, 0x8e79dcb0, 0x603a180e, 0x6c9e0e8b, 0xb01e8a3e, 
        0xd71577c1, 0xbd314b27, 0x78af2fda, 0x55605c60, 0xe65525f3, 0xaa55ab94, 0x57489862, 0x63e81440, 
        0x55ca396a, 0x2aab10b6, 0xb4cc5c34, 0x1141e8ce, 0xa15486af, 0x7c72e993, 0xb3ee1411, 0x636fbc2a, 
        0x2ba9c55d, 0x741831f6, 0xce5c3e16, 0x9b87931e, 0xafd6ba33, 0x6c24cf5c, 0x7a325381, 0x28958677, 
        0x3b8f4898, 0x6b4bb9af, 0xc4bfe81b, 0x66282193, 0x61d809cc, 0xfb21a991, 0x487cac60, 0x5dec8032, 
        0xef845d5d, 0xe98575b1, 0xdc262302, 0xeb651b88, 0x23893e81, 0xd396acc5, 0x0f6d6ff3, 0x83f44239, 
        0x2e0b4482, 0xa4842004, 0x69c8f04a, 0x9e1f9b5e, 0x21c66842, 0xf6e96c9a, 0x670c9c61, 0xabd388f0, 
        0x6a51a0d2, 0xd8542f68, 0x960fa728, 0xab5133a3, 0x6eef0b6c, 0x137a3be4, 0xba3bf050, 0x7efb2a98, 
        0xa1f1651d, 0x39af0176, 0x66ca593e, 0x82430e88, 0x8cee8619, 0x456f9fb4, 0x7d84a5c3, 0x3b8b5ebe, 
        0xe06f75d8, 0x85c12073, 0x401a449f, 0x56c16aa6, 0x4ed3aa62, 0x363f7706, 0x1bfedf72, 0x429b023d, 
        0x37d0d724, 0xd00a1248, 0xdb0fead3, 0x49f1c09b, 0x075372c9, 0x80991b7b, 0x25d479d8, 0xf6e8def7, 
        0xe3fe501a, 0xb6794c3b, 0x976ce0bd, 0x04c006ba, 0xc1a94fb6, 0x409f60c4, 0x5e5c9ec2, 0x196a2463, 
        0x68fb6faf, 0x3e6c53b5, 0x1339b2eb, 0x3b52ec6f, 0x6dfc511f, 0x9b30952c, 0xcc814544, 0xaf5ebd09, 
        0xbee3d004, 0xde334afd, 0x660f2807, 0x192e4bb3, 0xc0cba857, 0x45c8740f, 0xd20b5f39, 0xb9d3fbdb, 
        0x5579c0bd, 0x1a60320a, 0xd6a100c6, 0x402c7279, 0x679f25fe, 0xfb1fa3cc, 0x8ea5e9f8, 0xdb3222f8, 
        0x3c7516df, 0xfd616b15, 0x2f501ec8, 0xad0552ab, 0x323db5fa, 0xfd238760, 0x53317b48, 0x3e00df82, 
        0x9e5c57bb, 0xca6f8ca0, 0x1a87562e, 0xdf1769db, 0xd542a8f6, 0x287effc3, 0xac6732c6, 0x8c4f5573, 
        0x695b27b0, 0xbbca58c8, 0xe1ffa35d, 0xb8f011a0, 0x10fa3d98, 0xfd2183b8, 0x4afcb56c, 0x2dd1d35b, 
        0x9a53e479, 0xb6f84565, 0xd28e49bc, 0x4bfb9790, 0xe1ddf2da, 0xa4cb7e33, 0x62fb1341, 0xcee4c6e8, 
        0xef20cada, 0x36774c01, 0xd07e9efe, 0x2bf11fb4, 0x95dbda4d, 0xae909198, 0xeaad8e71, 0x6b93d5a0, 
        0xd08ed1d0, 0xafc725e0, 0x8e3c5b2f, 0x8e7594b7, 0x8ff6e2fb, 0xf2122b64, 0x8888b812, 0x900df01c, 
        0x4fad5ea0, 0x688fc31c, 0xd1cff191, 0xb3a8c1ad, 0x2f2f2218, 0xbe0e1777, 0xea752dfe, 0x8b021fa1, 
        0xe5a0cc0f, 0xb56f74e8, 0x18acf3d6, 0xce89e299, 0xb4a84fe0, 0xfd13e0b7, 0x7cc43b81, 0xd2ada8d9, 
        0x165fa266, 0x80957705, 0x93cc7314, 0x211a1477, 0xe6ad2065, 0x77b5fa86, 0xc75442f5, 0xfb9d35cf, 
        0xebcdaf0c, 0x7b3e89a0, 0xd6411bd3, 0xae1e7e49, 0x00250e2d, 0x2071b35e, 0x226800bb, 0x57b8e0af, 
        0x2464369b, 0xf009b91e, 0x5563911d, 0x59dfa6aa, 0x78c14389, 0xd95a537f, 0x207d5ba2, 0x02e5b9c5, 
        0x83260376, 0x6295cfa9, 0x11c81968, 0x4e734a41, 0xb3472dca, 0x7b14a94a, 0x1b510052, 0x9a532915, 
        0xd60f573f, 0xbc9bc6e4, 0x2b60a476, 0x81e67400, 0x08ba6fb5, 0x571be91f, 0xf296ec6b, 0x2a0dd915, 
        0xb6636521, 0xe7b9f9b6, 0xff34052e, 0xc5855664, 0x53b02d5d, 0xa99f8fa1, 0x08ba4799, 0x6e85076a
    },
    {
        0x4b7a70e9, 0xb5b32944, 0xdb75092e, 0xc4192623, 0xad6ea6b0, 0x49a7df7d, 0x9cee60b8, 0x8fedb266, 
        0xecaa8c71, 0x699a17ff, 0x5664526c, 0xc2b19ee1, 0x193602a5, 0x75094c29, 0xa0591340, 0xe4183a3e, 
        0x3f54989a, 0x5b429d65, 0x6b8fe4d6, 0x99f73fd6, 0xa1d29c07, 0xefe830f5, 0x4d2d38e6, 0xf0255dc1, 
        0x4cdd2086, 0x8470eb26, 0x6382e9c6, 0x021ecc5e, 0x09686b3f, 0x3ebaefc9, 0x3c971814, 0x6b6a70a1, 
        0x687f3584, 0x52a0e286, 0xb79c5305, 0xaa500737, 0x3e07841c, 0x7fdeae5c, 0x8e7d44ec, 0x5716f2b8, 
        0xb03ada37, 0xf0500c0d, 0xf01c1f04, 0x0200b3ff, 0xae0cf51a, 0x3cb574b2, 0x25837a58, 0xdc0921bd, 
        0xd19113f9, 0x7ca92ff6, 0x94324773, 0x22f54701, 0x3ae5e581, 0x37c2dadc, 0xc8b57634, 0x9af3dda7, 
        0xa9446146, 0x0fd0030e, 0xecc8c73e, 0xa4751e41, 0xe238cd99, 0x3bea0e2f, 0x3280bba1, 0x183eb331, 
        0x4e548b38, 0x4f6db908, 0x6f420d03, 0xf60a04bf, 0x2cb81290, 0x24977c79, 0x5679b072, 0xbcaf89af, 
        0xde9a771f, 0xd9930810, 0xb38bae12, 0xdccf3f2e, 0x5512721f, 0x2e6b7124, 0x501adde6, 0x9f84cd87, 
        0x7a584718, 0x7408da17, 0xbc9f9abc, 0xe94b7d8c, 0xec7aec3a, 0xdb851dfa, 0x63094366, 0xc464c3d2, 
        0xef1c1847, 0x3215d908, 0xdd433b37, 0x24c2ba16, 0x12a14d43, 0x2a65c451, 0x50940002, 0x133ae4dd, 
        0x71dff89e, 0x10314e55, 0x81ac77d6, 0x5f11199b, 0x043556f1, 0xd7a3c76b, 0x3c11183b, 0x5924a509, 
        0xf28fe6ed, 0x97f1fbfa, 0x9ebabf2c, 0x1e153c6e, 0x86e34570, 0xeae96fb1, 0x860e5e0a, 0x5a3e2ab3, 
        0x771fe71c, 0x4e3d06fa, 0x2965dcb9, 0x99e71d0f, 0x803e89d6, 0x5266c825, 0x2e4cc978, 0x9c10b36a, 
        0xc6150eba, 0x94e2ea78, 0xa5fc3c53, 0x1e0a2df4, 0xf2f74ea7, 0x361d2b3d, 0x1939260f, 0x19c27960, 
        0x5223a708, 0xf71312b6, 0xebadfe6e, 0xeac31f66, 0xe3bc4595, 0xa67bc883, 0xb17f37d1, 0x018cff28, 
        0xc332ddef, 0xbe6c5aa5, 0x65582185, 0x68ab9802, 0xeecea50f, 0xdb2f953b, 0x2aef7dad, 0x5b6e2f84, 
        0x1521b628, 0x29076170, 0xecdd4775, 0x619f1510, 0x13cca830, 0xeb61bd96, 0x0334fe1e, 0xaa0363cf, 
        0xb5735c90, 0x4c70a239, 0xd59e9e0b, 0xcbaade14, 0xeecc86bc, 0x60622ca7, 0x9cab5cab, 0xb2f3846e, 
        0x648b1eaf, 0x19bdf0ca, 0xa02369b9, 0x655abb50, 0x40685a32, 0x3c2ab4b3, 0x319ee9d5, 0xc021b8f7, 
        0x9b540b19, 0x875fa099, 0x95f7997e, 0x623d7da8, 0xf837889a, 0x97e32d77, 0x11ed935f, 0x16681281, 
        0x0e358829, 0xc7e61fd6, 0x96dedfa1, 0x7858ba99, 0x57f584a5, 0x1b227263, 0x9b83c3ff, 0x1ac24696, 
        0xcdb30aeb, 0x532e3054, 0x8fd948e4, 0x6dbc3128, 0x58ebf2ef, 0x34c6ffea, 0xfe28ed61, 0xee7c3c73, 
        0x5d4a14d9, 0xe864b7e3, 0x42105d14, 0x203e13e0, 0x45eee2b6, 0xa3aaabea, 0xdb6c4f15, 0xfacb4fd0, 
        0xc742f442, 0xef6abbb5, 0x654f3b1d, 0x41cd2105, 0xd81e799e, 0x86854dc7, 0xe44b476a, 0x3d816250, 
        0xcf62a1f2, 0x5b8d2646, 0xfc8883a0, 0xc1c7b6a3, 0x7f1524c3, 0x69cb7492, 0x47848a0b, 0x5692b285, 
        0x095bbf00, 0xad19489d, 0x1462b174, 0x23820e00, 0x58428d2a, 0x0c55f5ea, 0x1dadf43e, 0x233f7061, 
        0x3372f092, 0x8d937e41, 0xd65fecf1, 0x6c223bdb, 0x7cde3759, 0xcbee7460, 0x4085f2a7, 0xce77326e, 
        0xa6078084, 0x19f8509e, 0xe8efd855, 0x61d99735, 0xa969a7aa, 0xc50c06c2, 0x5a04abfc, 0x800bcadc, 
        0x9e447a2e, 0xc3453484, 0xfdd56705, 0x0e1e9ec9, 0xdb73dbd3, 0x105588cd, 0x675fda79, 0xe3674340, 
        0xc5c43465, 0x713e38d8, 0x3d28f89e, 0xf16dff20, 0x153e21e7, 0x8fb03d4a, 0xe6e39f2b, 0xdb83adf7 
    },
    {
        0xe93d5a68, 0x948140f7, 0xf64c261c, 0x94692934, 0x411520f7, 0x7602d4f7, 0xbcf46b2e, 0xd4a20068, 
        0xd4082471, 0x3320f46a, 0x43b7d4b7, 0x500061af, 0x1e39f62e, 0x97244546, 0x14214f74, 0xbf8b8840, 
        0x4d95fc1d, 0x96b591af, 0x70f4ddd3, 0x66a02f45, 0xbfbc09ec, 0x03bd9785, 0x7fac6dd0, 0x31cb8504, 
        0x96eb27b3, 0x55fd3941, 0xda2547e6, 0xabca0a9a, 0x28507825, 0x530429f4, 0x0a2c86da, 0xe9b66dfb, 
        0x68dc1462, 0xd7486900, 0x680ec0a4, 0x27a18dee, 0x4f3ffea2, 0xe887ad8c, 0xb58ce006, 0x7af4d6b6, 
        0xaace1e7c, 0xd3375fec, 0xce78a399, 0x406b2a42, 0x20fe9e35, 0xd9f385b9, 0xee39d7ab, 0x3b124e8b, 
        0x1dc9faf7, 0x4b6d1856, 0x26a36631, 0xeae397b2, 0x3a6efa74, 0xdd5b4332, 0x6841e7f7, 0xca7820fb, 
        0xfb0af54e, 0xd8feb397, 0x454056ac, 0xba489527, 0x55533a3a, 0x20838d87, 0xfe6ba9b7, 0xd096954b, 
        0x55a867bc, 0xa1159a58, 0xcca92963, 0x99e1db33, 0xa62a4a56, 0x3f3125f9, 0x5ef47e1c, 0x9029317c, 
        0xfdf8e802, 0x04272f70, 0x80bb155c, 0x05282ce3, 0x95c11548, 0xe4c66d22, 0x48c1133f, 0xc70f86dc, 
        0x07f9c9ee, 0x41041f0f, 0x404779a4, 0x5d886e17, 0x325f51eb, 0xd59bc0d1, 0xf2bcc18f, 0x41113564, 
        0x257b7834, 0x602a9c60, 0xdff8e8a3, 0x1f636c1b, 0x0e12b4c2, 0x02e1329e, 0xaf664fd1, 0xcad18115, 
        0x6b2395e0, 0x333e92e1, 0x3b240b62, 0xeebeb922, 0x85b2a20e, 0xe6ba0d99, 0xde720c8c, 0x2da2f728, 
        0xd0127845, 0x95b794fd, 0x647d0862, 0xe7ccf5f0, 0x5449a36f, 0x877d48fa, 0xc39dfd27, 0xf33e8d1e, 
        0x0a476341, 0x992eff74, 0x3a6f6eab, 0xf4f8fd37, 0xa812dc60, 0xa1ebddf8, 0x991be14c, 0xdb6e6b0d, 
        0xc67b5510, 0x6d672c37, 0x2765d43b, 0xdcd0e804, 0xf1290dc7, 0xcc00ffa3, 0xb5390f92, 0x690fed0b, 
        0x667b9ffb, 0xcedb7d9c, 0xa091cf0b, 0xd9155ea3, 0xbb132f88, 0x515bad24, 0x7b9479bf, 0x763bd6eb, 
        0x37392eb3, 0xcc115979, 0x8026e297, 0xf42e312d, 0x6842ada7, 0xc66a2b3b, 0x12754ccc, 0x782ef11c, 
        0x6a124237, 0xb79251e7, 0x06a1bbe6, 0x4bfb6350, 0x1a6b1018, 0x11caedfa, 0x3d25bdd8, 0xe2e1c3c9, 
        0x44421659, 0x0a121386, 0xd90cec6e, 0xd5abea2a, 0x64af674e, 0xda86a85f, 0xbebfe988, 0x64e4c3fe, 
        0x9dbc8057, 0xf0f7c086, 0x60787bf8, 0x6003604d, 0xd1fd8346, 0xf6381fb0, 0x7745ae04, 0xd736fccc, 
        0x83426b33, 0xf01eab71, 0xb0804187, 0x3c005e5f, 0x77a057be, 0xbde8ae24, 0x55464299, 0xbf582e61, 
        0x4e58f48f, 0xf2ddfda2, 0xf474ef38, 0x8789bdc2, 0x5366f9c3, 0xc8b38e74, 0xb475f255, 0x46fcd9b9, 
        0x7aeb2661, 0x8b1ddf84, 0x846a0e79, 0x915f95e2, 0x466e598e, 0x20b45770, 0x8cd55591, 0xc902de4c, 
        0xb90bace1, 0xbb8205d0, 0x11a86248, 0x7574a99e, 0xb77f19b6, 0xe0a9dc09, 0x662d09a1, 0xc4324633, 
        0xe85a1f02, 0x09f0be8c, 0x4a99a025, 0x1d6efe10, 0x1ab93d1d, 0x0ba5a4df, 0xa186f20f, 0x2868f169, 
        0xdcb7da83, 0x573906fe, 0xa1e2ce9b, 0x4fcd7f52, 0x50115e01, 0xa70683fa, 0xa002b5c4, 0x0de6d027, 
        0x9af88c27, 0x773f8641, 0xc3604c06, 0x61a806b5, 0xf0177a28, 0xc0f586e0, 0x006058aa, 0x30dc7d62, 
        0x11e69ed7, 0x2338ea63, 0x53c2dd94, 0xc2c21634, 0xbbcbee56, 0x90bcb6de, 0xebfc7da1, 0xce591d76, 
        0x6f05e409, 0x4b7c0188, 0x39720a3d, 0x7c927c24, 0x86e3725f, 0x724d9db9, 0x1ac15bb4, 0xd39eb8fc, 
        0xed545578, 0x08fca5b5, 0xd83d7cd3, 0x4dad0fc4, 0x1e50ef5e, 0xb161e6f8, 0xa28514d9, 0x6c51133c, 
        0x6fd5c7e7, 0x56e14ec4, 0x362abfce, 0xddc6c837, 0xd79a3234, 0x92638212, 0x670efa8e, 0x406000e0
    },
    {
        0x3a39ce37, 0xd3faf5cf, 0xabc27737, 0x5ac52d1b, 0x5cb0679e, 0x4fa33742, 0xd3822740, 0x99bc9bbe, 
        0xd5118e9d, 0xbf0f7315, 0xd62d1c7e, 0xc700c47b, 0xb78c1b6b, 0x21a19045, 0xb26eb1be, 0x6a366eb4, 
        0x5748ab2f, 0xbc946e79, 0xc6a376d2, 0x6549c2c8, 0x530ff8ee, 0x468dde7d, 0xd5730a1d, 0x4cd04dc6, 
        0x2939bbdb, 0xa9ba4650, 0xac9526e8, 0xbe5ee304, 0xa1fad5f0, 0x6a2d519a, 0x63ef8ce2, 0x9a86ee22, 
        0xc089c2b8, 0x43242ef6, 0xa51e03aa, 0x9cf2d0a4, 0x83c061ba, 0x9be96a4d, 0x8fe51550, 0xba645bd6, 
        0x2826a2f9, 0xa73a3ae1, 0x4ba99586, 0xef5562e9, 0xc72fefd3, 0xf752f7da, 0x3f046f69, 0x77fa0a59, 
        0x80e4a915, 0x87b08601, 0x9b09e6ad, 0x3b3ee593, 0xe990fd5a, 0x9e34d797, 0x2cf0b7d9, 0x022b8b51, 
        0x96d5ac3a, 0x017da67d, 0xd1cf3ed6, 0x7c7d2d28, 0x1f9f25cf, 0xadf2b89b, 0x5ad6b472, 0x5a88f54c, 
        0xe029ac71, 0xe019a5e6, 0x47b0acfd, 0xed93fa9b, 0xe8d3c48d, 0x283b57cc, 0xf8d56629, 0x79132e28, 
        0x785f0191, 0xed756055, 0xf7960e44, 0xe3d35e8c, 0x15056dd4, 0x88f46dba, 0x03a16125, 0x0564f0bd, 
        0xc3eb9e15, 0x3c9057a2, 0x97271aec, 0xa93a072a, 0x1b3f6d9b, 0x1e6321f5, 0xf59c66fb, 0x26dcf319, 
        0x7533d928, 0xb155fdf5, 0x03563482, 0x8aba3cbb, 0x28517711, 0xc20ad9f8, 0xabcc5167, 0xccad925f, 
        0x4de81751, 0x3830dc8e, 0x379d5862, 0x9320f991, 0xea7a90c2, 0xfb3e7bce, 0x5121ce64, 0x774fbe32, 
        0xa8b6e37e, 0xc3293d46, 0x48de5369, 0x6413e680, 0xa2ae0810, 0xdd6db224, 0x69852dfd, 0x09072166, 
        0xb39a460a, 0x6445c0dd, 0x586cdecf, 0x1c20c8ae, 0x5bbef7dd, 0x1b588d40, 0xccd2017f, 0x6bb4e3bb, 
        0xdda26a7e, 0x3a59ff45, 0x3e350a44, 0xbcb4cdd5, 0x72eacea8, 0xfa6484bb, 0x8d6612ae, 0xbf3c6f47, 
        0xd29be463, 0x542f5d9e, 0xaec2771b, 0xf64e6370, 0x740e0d8d, 0xe75b1357, 0xf8721671, 0xaf537d5d, 
        0x4040cb08, 0x4eb4e2cc, 0x34d2466a, 0x0115af84, 0xe1b00428, 0x95983a1d, 0x06b89fb4, 0xce6ea048, 
        0x6f3f3b82, 0x3520ab82, 0x011a1d4b, 0x277227f8, 0x611560b1, 0xe7933fdc, 0xbb3a792b, 0x344525bd, 
        0xa08839e1, 0x51ce794b, 0x2f32c9b7, 0xa01fbac9, 0xe01cc87e, 0xbcc7d1f6, 0xcf0111c3, 0xa1e8aac7, 
        0x1a908749, 0xd44fbd9a, 0xd0dadecb, 0xd50ada38, 0x0339c32a, 0xc6913667, 0x8df9317c, 0xe0b12b4f, 
        0xf79e59b7, 0x43f5bb3a, 0xf2d519ff, 0x27d9459c, 0xbf97222c, 0x15e6fc2a, 0x0f91fc71, 0x9b941525, 
        0xfae59361, 0xceb69ceb, 0xc2a86459, 0x12baa8d1, 0xb6c1075e, 0xe3056a0c, 0x10d25065, 0xcb03a442, 
        0xe0ec6e0e, 0x1698db3b, 0x4c98a0be, 0x3278e964, 0x9f1f9532, 0xe0d392df, 0xd3a0342b, 0x8971f21e, 
        0x1b0a7441, 0x4ba3348c, 0xc5be7120, 0xc37632d8, 0xdf359f8d, 0x9b992f2e, 0xe60b6f47, 0x0fe3f11d, 
        0xe54cda54, 0x1edad891, 0xce6279cf, 0xcd3e7e6f, 0x1618b166, 0xfd2c1d05, 0x848fd2c5, 0xf6fb2299, 
        0xf523f357, 0xa6327623, 0x93a83531, 0x56cccd02, 0xacf08162, 0x5a75ebb5, 0x6e163697, 0x88d273cc, 
        0xde966292, 0x81b949d0, 0x4c50901b, 0x71c65614, 0xe6c6c7bd, 0x327a140a, 0x45e1d006, 0xc3f27b9a, 
        0xc9aa53fd, 0x62a80f00, 0xbb25bfe2, 0x35bdd2f6, 0x71126905, 0xb2040222, 0xb6cbcf7c, 0xcd769c2b, 
        0x53113ec0, 0x1640e3d3, 0x38abbd60, 0x2547adf0, 0xba38209c, 0xf746ce76, 0x77afa1c5, 0x20756060, 
        0x85cbfe4e, 0x8ae88dd8, 0x7aaaf9b0, 0x4cf9aa7e, 0x1948c25c, 0x02fb8a8c, 0x01c36ae4, 0xd6ebe1f9, 
        0x90d4f869, 0xa65cdea0, 0x3f09252d, 0xc208e69f, 0xb74e6132, 0xce77e25b, 0x578fdfe3, 0x3ac372e6 
    }
};
const unsigned long pbox[18]=
{
    0x243f6a88, 0x85a308d3, 0x13198a2e, 0x03707344, 0xa4093822, 0x299f31d0, 0x082efa98, 0xec4e6c89, 
    0x452821e6, 0x38d01377, 0xbe5466cf, 0x34e90c6c, 0xc0ac29b7, 0xc97c50dd, 0x3f84d5b5, 0xb5470917, 
    0x9216d5d9, 0x8979fb1b
};
unsigned long keybox[18+4*256];
unsigned long Receive_num[4];
unsigned long Send_num[4]={2,0,1,2};
unsigned long Send_num_En[4]={2,0,1,2};
#endif
extern unsigned int tem_integer;
extern unsigned int tem_decimal;
extern unsigned int rh;
extern int led_flag;

int can_send = 0;				/* if AVcommand ID is same user<->camera, can_send = 1, allow send file */
int data_ID  = 7501;			/* AVdata command text[23:26] data connetion ID */

char str_ctl[]	 = "MO_O";
char str_data[]  = "MO_V";
char str_camID[] = "yuanxiang7501";
char str_SSID[16];
char str_camVS[] = {0, 2, 2, 7};
char str_tmp[14];

/* ---------------------------------------------------------- */

u8 *buffer;						/* TODO for opcode protocol */ 
u8 *buffer2;					/* TODO for AVdata protocol */
u8 *text;						/* TODO for opcode command text */
u8 *AVtext;						/* TODO for AVdata command text */

u32 pic_num = 1;				/* for time stamp */
u32 audio_num = 1;

int send_audio_flag = 1;

/* audio param */
static int AVcommand_fd;
static struct timeval av_start_time={0,0};

/* TODO commands list */
static struct command *command1;
static struct command *command3;
static struct command *command254;
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

sem_t start_talk;

/* add by zjw for feal bat_info */
FILE *bat_fp;
int bat_info[20]={0};
u32 battery_fd;

int picture_fd, audio_data_fd, music_data_fd=-1;

pthread_mutex_t AVsocket_mutex;

#ifdef BLOWFISH
void swap(unsigned long * x,unsigned long * y)
{
    unsigned long temp;
    temp=*x;
    *x=*y;
    *y=temp;
}
void BlowfishKeyInit(char *,int);
unsigned long F(unsigned char Bytes[4]);
void BlowfishEncipher(unsigned long * XL,unsigned long * XR);
void BlowfishDecipher(unsigned long * XL,unsigned long * XR);

void BlowfishKeyInit(char* strKey,int nLen)
{
    /////////////////////////////////////
    //把pbox,sbox中的内容复制到keybox中
    unsigned long* pKey=keybox;
    int i,oldlen=nLen;
    char *cp=NULL;
    unsigned long xl=0,xr=0;
    memcpy(pKey,pbox,sizeof(long)*18);
    pKey+=18;
    memcpy(pKey,sbox,sizeof(long)*4*256);

    //////////////////////////////////////
    //处理字符串key如果长度不够18*4=72bytes就扩展。
    if(nLen<72)
    {
        cp=malloc(sizeof(char)*72);
        memset(cp,0,sizeof(char)*72);
        nLen=0;
        while(nLen<72)
        {
            strcat(cp,strKey);
            nLen+=oldlen;
        }
    }
    else
        cp=strKey;

    ///////////////////////////////
    //用已处理的key和keybox中的前18个元素异或(也就是pbox的内容)
    pKey=(unsigned long*)cp;
    for (i=0;i<18;i++)
    {
        keybox[i]^=pKey[i];
    }

    ///////////////////////////////////////
    //用函数BlowfishEncipher迭代521=((18+256*4)/2)次，初始xl=xr=0,输出用来填充keybox;
    for (i=0;i<521;i++)
    {
        BlowfishEncipher(&xl,&xr);
        keybox[2*i]=xl;
        keybox[2*i+1]=xr;
    }
    free(cp);
}
void BlowfishEncipher(unsigned long * XL,unsigned long * XR)
{
    unsigned long xl=*XL,xr=*XR;
    int i;
    for (i=0;i<16;i++)
    {
        xl^=keybox[i];
        xr^=F((unsigned char*)&xl);
        swap(&xl,&xr);
    }
    swap(&xl,&xr);
    xr^=keybox[16];
    xl^=keybox[17];
    *XR=xr;
    *XL=xl;
}
unsigned long F(unsigned char Bytes[4])
{
    unsigned long temp;
    temp=keybox[SBOX_BEGIN+Bytes[3]]+keybox[SBOX_BEGIN+256+Bytes[2]];
    temp^=keybox[SBOX_BEGIN+256*2+Bytes[1]];
    temp+=keybox[SBOX_BEGIN+256*3+Bytes[0]];
    return temp;
}
void BlowfishDecipher(unsigned long * XL,unsigned long * XR)
{
    unsigned long xl=*XL,xr=*XR,temp=0;
    int i;
    for (i=17;i>1;i--)
    {
        xl^=keybox[i];
        xr^=F((unsigned char*)&xl);
        swap(&xl,&xr);
    }
    swap(&xl,&xr);
    xr^=keybox[1];
    xl^=keybox[0];
    *XR=xr;
    *XL=xl;
}
void BlowfishEncrption(unsigned long *szMessage,int len)
{
    int i;
    for (i=0;i<len;i+=2)
    {
        BlowfishEncipher(&szMessage[i+1],&szMessage[i]);
    }
}
void BlowfishDecrption(unsigned long *szMessage,int len)
{
    int i;
    for (i=0;i<len;i+=2)
    {
        BlowfishDecipher(&szMessage[i+1],&szMessage[i]);
    }
}
#endif

static void keep_alive_timeout(int signo )
{
    printf("Err:keep alive timeout!\n");
    close(AVcommand_fd);
    exit(0);
}
#if 0
static void reset_keep_alive_timer( void )
{
#if 0
    struct itimerval t;
    t.it_interval.tv_sec = 10;
    t.it_interval.tv_usec = 0;
    //t.it_value.tv_sec = 10;
    //t.it_value.tv_usec = 0;
    setitimer(SIGALRM, &t, NULL);
    printf("My Heart Is Beated !\n");
    //setitimer(ITIMER_REAL, &t, NULL);
    //setitimer(SIGALRM, &value, &ovalue); 
#endif
}
#endif
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
    int send_len;
	command21 = malloc(sizeof(struct command) + 4);
    memcpy(command21->protocol_head, str_ctl, 4);
    tem_rh_data = &command21->text[0].tem_rh_data;
    tem_rh_data->tem_integer = (u8)tem_integer;
    tem_rh_data->tem_decimal = (u8)tem_decimal;
    tem_rh_data->rh = (u8)rh;
    command21->opcode = 21; 
    command21->text_len = 3;
    //printf("tem_integer is %d,tem_decimal is %d,rh is %d\n",tem_integer,tem_decimal,rh);
    send_len = send(tem_rh_fd, command21, 26, 0); 
    if (send_len == -1){ 
	perror("send");
        close(tem_rh_fd);
		printf("========%s,%u========\n",__FILE__,__LINE__);
        exit(0);
    }
    else if(send_len < 26){
        printf("-->temperature and relative send short");
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
    int send_len;

	command9 = malloc(sizeof(struct command) + 6);
	audio_start_resp = &command9->text[0].audio_start_resp;

	memcpy(command9->protocol_head, str_ctl, 4);
	command9->opcode = 9;
	command9->text_len = 6;

	audio_start_resp->result = 0;				/* 0 : agree */
	audio_start_resp->data_con_ID = 0;			/* TODO (FIX ME) must be 0 */

	send_len = send(audio_fd, command9, 29, 0);
	if (send_len == -1) {
		perror("send");
		close(audio_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
        printf("-->Send Audio Resp ... Failed !!\n");
		exit(0);
	}
    else if(send_len < 29){
        printf("-->Send Audio Resp ... Short\n");
    }
    else {
        printf("-->Send Audio Resp !\n");
    }

	free(command9);
	command9 = NULL;

}

/*
 * send music player over
 */
void send_music_played_over()
{
    int music_played_over_fd = AVcommand_fd;
    struct command *command15;
    struct music_played_over *music_played_over;
    int send_len;
    command15 = malloc(sizeof(struct command));
    memcpy(command15->protocol_head, str_ctl, 4);
    music_played_over = &command15->text[0].music_played_over;
    command15->opcode = 15; 
    command15->text_len = 0;
    send_len = send(music_played_over_fd, command15, 23, 0);
    if (send_len == -1){ 
        perror("send");
        close(music_played_over_fd);
        printf("========%s,%u========\n",__FILE__,__LINE__);
        printf("-->Send Music Is Played Over ... Failed \n");
        exit(0);
    }
    else if(send_len < 23){
        printf("-->Send Music Is Player Over ... Short \n");
    }
    else{
        printf("-->Send Music Is Played Over !\n");
    }
    free(command15);
    command15 = NULL;
    return;
}                           
/*
 *confirm_stop 
 */
void send_talk_end_resp()
{
    int talk_end_fd = AVcommand_fd;
    struct command *command22;
    struct talk_end_resp *talk_end_resp;
    int send_len;
    command22 = malloc(sizeof(struct command));
    memcpy(command22->protocol_head, str_ctl, 4);
    talk_end_resp = &command22->text[0].talk_end_resp;
    command22->opcode = 22; 
    command22->text_len = 0;
    send_len = send(talk_end_fd, command22, 23, 0); 
    if (send_len == -1){ 
        perror("send");
        close(talk_end_fd);
        printf("========%s,%u========\n",__FILE__,__LINE__);
        printf("-->Send Talk Stop Resp ... Failed \n");
        exit(0);
    }
    else if(send_len < 23){
        printf("-->Send Talk Stop Resp ... Short\n");
    }
    else{
        printf("-->Send Talk Stop Resp !\n");
    }
    free(command22);
    command22 = NULL;
    return;
}                           
/* enable talk data */
void send_talk_resp()
{
	int talk_fd = AVcommand_fd;
	
	struct command *command12;
	struct talk_start_resp *talk_start_resp;
    int send_len;

	command12 = malloc(sizeof(struct command) + 6);
	talk_start_resp = &command12->text[0].talk_start_resp;

	memcpy(command12->protocol_head, str_ctl, 4);
	command12->opcode = 12;
	command12->text_len = 6;

	talk_start_resp->result = 0;				/* 0 : agree */
	talk_start_resp->data_con_ID = 0;			/* TODO (FIX ME) must be 0 */

	send_len = send(talk_fd, command12, 29, 0);
	if (send_len == -1) {
		perror("send");
		close(talk_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
        printf("-->Send Talk Start Resp ... Failed\n");
		exit(0);
	}
    else if(send_len < 29){
        printf("-->Send Talk Start Resp ... Short\n");
    }
    else{
        printf("-->Send Talk Start Resp !\n");
    }

	free(command12);
	command12 = NULL;
}
/* when step motor to the end ,send cmd to client */
void send_alarm_notify(u8 alarm_kind)
{
	int alarm_fd = AVcommand_fd;
	
	struct command *command25;
	struct alarm_notify *alarm_notify_r;
    int send_len;

	command25 = malloc(sizeof(struct alarm_notify)+sizeof(struct command));
	alarm_notify_r = &command25->text[0].alarm_notify;

	memcpy(command25->protocol_head, str_ctl, 4);
	command25->opcode = 25;
	command25->text_len = sizeof(struct alarm_notify);

	alarm_notify_r->alarm_type = alarm_kind;
    alarm_notify_r->reserve1 = 0;
    alarm_notify_r->reserve2 = 0;
    alarm_notify_r->reserve3 = 0;
    alarm_notify_r->reserve4 = 0;
	send_len = send(alarm_fd, command25, sizeof(struct command) + sizeof(struct alarm_notify), 0);
	if (send_len == -1) {
		perror("send");
		close(alarm_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
        printf("-->Send Alarm_Notify ... Failed\n");
		exit(0);
	}
    else if(send_len < (sizeof(struct command) + sizeof(struct alarm_notify))){
        printf("-->Send Alarm_Notify ... Short\n");
    }
    else{
        printf("-->Send Alarm_Notify !\n");
    }

	free(command25);
	command25 = NULL;
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
#define MAX_RETRY_NUM	60
#define TIME_DIFF(t1,t2) (((t1).tv_sec-(t2).tv_sec)*1000+((t1).tv_usec-(t2).tv_usec)/1000)
void send_picture(char *data, u32 length,struct timeval t1)
{
	long send_len = 0;
	long total_send_len = 0;
	/*struct timeval t2;*/
	char *p;
	static int retry_num=0;

	//gettimeofday(&t1,NULL);
	av_command1->text_len = length + 13;			/* TODO */

	video_data->pic_len = length;
	video_data->time_stamp = TIME_DIFF(t1,av_start_time);
	video_data->frame_time = pic_num++;				/* test for time stamp */

	pthread_mutex_lock(&AVsocket_mutex);
	p = (char *)av_command1;
	while( total_send_len < 36 )
	{
		/*gettimeofday(&t1,NULL);*/
		send_len = send(picture_fd, (void *)&p[total_send_len], 36-total_send_len, 0);
		/*gettimeofday(&t2,NULL);*/
		if (send_len <= 0) {
			perror("#send_header");
			if( errno != EAGAIN || retry_num >= MAX_RETRY_NUM )
			{
				goto err_exit;
			}
			else
			{
				retry_num++;
				usleep(500000);
			}
		}
		else
		{
			total_send_len += send_len;
			if (total_send_len < 36)
			{
				printf("#opcode header short send(%d/36)!\n",total_send_len);
			}
			retry_num = 0;
		}
	}
	total_send_len = 0;
	p = data;
	while( total_send_len < length )
	{
		/*gettimeofday(&t1,NULL);*/
		send_len = send(picture_fd, (void *)&p[total_send_len], length-total_send_len, 0);
		/*gettimeofday(&t2,NULL);*/
		if (send_len <= 0) {
			perror("#send_pic");
			if( errno != EAGAIN || retry_num >= MAX_RETRY_NUM )
			{
				goto err_exit;
			}
			else
			{
				retry_num++;
				usleep(500000);
			}
		}
		else
		{
			total_send_len += send_len;
			if (total_send_len < length)
			{
				printf("#picture short send(%d/%d)!\n",total_send_len,length);
			}
			retry_num = 0;
		}
	}
	pthread_mutex_unlock(&AVsocket_mutex);
	return;
err_exit:
	close(picture_fd);
	printf("#send picture failed, exit to restart!\n");
	exit(0);
	return;
}

/*
 * send audio data
 */
static unsigned long pre_time_stamp = 0;
int send_audio_data(u8 *audio_buf, u32 data_len,struct timeval t1)
{
	long send_len = 0;
	long total_send_len = 0;
	long length = data_len+3;
	struct timeval t2;
	char *p;
	static int retry_num=0;

	//gettimeofday(&t1,NULL);
	av_command2->text_len = data_len + 20;			/* contant sample and index */
	audio_data->ado_len = data_len;
    audio_data->time_stamp = TIME_DIFF(t1,av_start_time);
    if(audio_data->time_stamp - pre_time_stamp > 100){
        printf("   Audio Cost Time is %d\n",audio_data->time_stamp - pre_time_stamp);
    }
    pre_time_stamp = audio_data->time_stamp;
	p = (char *)av_command2;
    //printf("audio_data time_stamp is %lu\n",audio_data->time_stamp);

    audio_num++;
	pthread_mutex_lock(&AVsocket_mutex);

	while( total_send_len < 40 )
	{
        /*gettimeofday(&t1,NULL);*/
		send_len = send(audio_data_fd, (void *)&p[total_send_len], 40-total_send_len, 0);
		/*gettimeofday(&t2,NULL);*/
		if (send_len <= 0) {
			perror("#send_audio_header");
			if( errno != EAGAIN || retry_num >= MAX_RETRY_NUM )
			{
				goto err_exit;
			}
			else
			{
				retry_num++;
				usleep(500000);
			}
		}
		else
		{
			total_send_len += send_len;
			if (total_send_len < 40)
			{
				printf("#opcode audio header short send(%d/40)!\n",total_send_len);
			}
			retry_num = 0;
		}
	}
	total_send_len = 0;
	p = audio_buf;
	while( total_send_len < length )
	{
		send_len = send(audio_data_fd, (void *)&p[total_send_len], length-total_send_len, 0);
		if (send_len <= 0) {
			perror("#send_audio");
			if( errno != EAGAIN || retry_num >= MAX_RETRY_NUM )
			{
				goto err_exit;
			}
			else
			{
				retry_num++;
				usleep(500000);
			}
		}
		else
		{
			total_send_len += send_len;
			if (total_send_len < length)
			{
				printf("#audio short send(%d/%d)!\n",total_send_len,length);
			}
			retry_num = 0;
		}
	}

	pthread_mutex_unlock(&AVsocket_mutex);

	return 1;
err_exit:
	close(audio_data_fd);
    printf("#errno is %d, retry_num is %d\n",errno,retry_num);
	printf("#send audio data failed, exit to restart!\n");
	exit(0);
	return;
}

/* TODO keep opcode connection, every 1 minute, or network will be disconnected */
void keep_connect(void)
{
	int client_fd = AVcommand_fd;
    struct keep_alive_resp *keep_alive_resp;
	
    keep_alive_resp = &command254->text[0].keep_alive_resp;
	command254->opcode = 254;
	command254->text_len = 0;

	/* write command to client --- keep alive */
	if (send(client_fd, command254, 23, 0) == -1) {
		perror("send");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}
}

int read_client( int fd, char *buf, int len )
{
    int n;
    int recv_len = 0;
    char *p = buf;
	while ( recv_len < len )
    {
           n =recv(fd, p, len-recv_len, 0);
           if( n <= 0 )
           {
                printf("==recv err(%d): n=%d recv_len=%d======\n",errno,n,recv_len);
		    	break;
            }
            else
            {
                p += n;
                recv_len += n;
             
            }
	}
    return recv_len;
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

#ifdef BLOWFISH
    BlowfishKeyInit(BLOWFISH_KEY,strlen(BLOWFISH_KEY));
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
    Receive_num[0]=login_req->ch_num1;
    Receive_num[1]=login_req->ch_num2;
	Receive_num[2]=login_req->ch_num3;
    Receive_num[3]=login_req->ch_num4;
    //for(i=0;i<4;i++)
    //    printf("Receive_num[%d] is %ld\n",i,Receive_num[i]);
    BlowfishEncrption(Receive_num,sizeof(Receive_num)/4);
    //for(i=0;i<4;i++)
    //    printf("Receive_num[%d] is %ld\n",i,Receive_num[i]);
	/* ------------------------------------------- */
	/* TODO send login response to user by command 1, contains v1, v2, v3, v4 */
	text_size = sizeof(struct login_resp);
	command1 = malloc(sizeof(struct command) + text_size);
	login_resp = &command1->text[0].login_resp;

	memcpy(command1->protocol_head, str_ctl, 4);
	command1->opcode = 1;
	command1->text_len = text_size;

	memset(login_resp, 0, text_size);
	login_resp->result = 0;
	memcpy(login_resp->camera_ID, str_SSID, sizeof(str_SSID));
	memcpy(login_resp->camera_version, str_camVS, sizeof(str_camVS));
    login_resp->au_num1=Receive_num[0];
    login_resp->au_num2=Receive_num[1];
    login_resp->au_num3=Receive_num[2];
    login_resp->au_num4=Receive_num[3];
    login_resp->ch_num1=Send_num[0];
    login_resp->ch_num2=Send_num[1];
    login_resp->ch_num3=Send_num[2];
    login_resp->ch_num4=Send_num[3];

	/* write command1 to client */
	if ((n = send(client_fd, command1, sizeof(struct command) + text_size, 0)) == -1) {
		perror("send");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}

#else
	/* TODO read first command 0 from client */
	if ((n = recv(client_fd, buffer, 100, 0)) == -1) {
		perror("recv");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
	}
    //printf("######### n is %d\n",n);
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
	
	/* TODO send login response to user by command 1, contains camera's version and ID */
	command1 = malloc(sizeof(struct command) + 30);
	login_resp = &command1->text[0].login_resp;

	memcpy(command1->protocol_head, str_ctl, 4);
	command1->opcode = 1;
	command1->text_len = 30;

	memset(login_resp, 0, 30);
	login_resp->result = 0;
	memcpy(login_resp->camera_ID, str_SSID, sizeof(str_SSID));
	memcpy(login_resp->camera_version, str_camVS, sizeof(str_camVS));

	/* write command1 to client */
	if ((n = send(client_fd, command1, 53, 0)) == -1) {
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

#ifdef BLOWFISH
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
    Receive_num[0]=verify_req->au_num1;
    Receive_num[1]=verify_req->au_num2;
    Receive_num[2]=verify_req->au_num3;
    Receive_num[3]=verify_req->au_num4;
    BlowfishEncrption(Send_num,sizeof(Send_num_En)/4);

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
    printf("Send My ID and Version OK\n");
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
#ifdef BLOWFISH
    for(i=0;i<4;i++)
    {
        if(Receive_num[i]==Send_num[i])
            continue;
        else
        {
            verify_resp->result = 1;				/* verify failed */
            printf("ERROR!! Receive_num[%d]= %ld,Send_num[%d]=%ld\n",i,i,Receive_num[i],Send_num[i]);
            break;
        }

    }
#endif
	verify_resp->reserve = 0;				/* exist when result is 0 */

	/* write command 3 to client */
	if ((n = send(client_fd, command3, sizeof(struct command) + 3, 0)) == -1) {
		perror("send");
		close(client_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
		exit(0);
    }
    if(verify_resp->result != 0)
    {
        printf("Verify Failed !!!!\n");
        exit(0);
    }
    else
    {
        printf("Verify Success !!!!\n");
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
    printf("Video RESP Send OK !\nNEXT START PHRASING COMMANDS!\n");

	wifi_dbg("---------------------------------------\n");

	/* ------------------------------------------- */
	/* ------------------------------------------- */

	/* TODO go on waiting for reading in the while() */
    while (1) {
        int opcode;
        int sync_len = strlen(str_ctl);
        int sync_ok = 0;
        /* read command from client */
        do
        {
            if( read_client( client_fd, buffer, sync_len ) != sync_len )
            {
                printf("phone closed command socket! restart!\n");
                exit(0);
            }
            if(strncmp(buffer,str_ctl,sync_len))
            {
                printf("command's head is %lx\nstr_ctl is %s\n",(*(unsigned long *)buffer),str_ctl);
                //clear_recv_buf(client_fd);
            }
            else
                sync_ok = 1;
        }while(!sync_ok);
        if( read_client( client_fd, buffer, 23-sync_len ) != 23-sync_len )
        {
            printf("phone closed command socket! restart!\n");
            exit(0);
        }
        opcode = buffer[0];
        text_len = byteArrayToInt(&buffer[11], 0, 4);
        if( text_len > 200 )
        {
            printf("bad text_len!\n");
            continue;
        }
        if( read_client( client_fd, buffer, text_len ) != text_len )
        {
            printf("phone closed command socket! restart!\n");
            exit(0);
        }
        if (prase_packet(opcode, buffer) == -1)
            continue;
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
	free(command254);
	/* malloc in the main() */
	free(arg);
	/* buffer malloc */
	free(buffer);
	close(client_fd);
	pthread_exit(NULL);
}

#if 0
int pic_cnt=0;

void sigalarm_handler(int sig)
{
	printf("fps=%d\n", pic_cnt);
	pic_cnt = 0;
}


int init_timer(void)
{
	struct itimerval itv;
	int ret;

	signal(SIGALRM, sigalrm_handler);
	itv.it_interval.tv_sec = 1;
	itv.it_interval.tv_usec = 0;
	itv.it_value.tv_sec = 1;
	itv.it_value.tv_usec = 0;
	
	ret = setitimer(ITIMER_REAL, &itv, NULL);
	if ( ret != 0){
		printf("Set timer error. %s \n", strerror(errno) );
		return -1;
	}
	printf("add timer\n");
}
#endif

/* ------------------------------------------- */

/* TODO wifi main function, later will be called in the main thread in core/main.c */
void network(void)
{
	int server_fd, *client_fd;
	struct sockaddr_in server_addr;
	struct sockaddr_in client_addr;
    in_addr_t current_client_address =0;
    int sin_size;
    int opt = 1;							/* allow server address reuse */
	int flag = 1;							/* TODO thread flag */
	int nagle_flag = 1;						/* disable Nagle */
	
	/* malloc buffer for socket read/write */
	buffer = malloc(100);					/* TODO */
	buffer2 = malloc(100);					/* TODO */
	AVtext = malloc(200);					/* TODO */
	text = malloc(100);						/* TODO */


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
	server_addr.sin_addr.s_addr = inet_addr("192.168.1.100");
	bzero(&(server_addr.sin_zero), 8);
	
	/* bind port */
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        close(server_fd);
        printf("========%s,%u==========\n",__FILE__,__LINE__);
        system("reboot");
        exit(1);
    }

    /* start listening */
    if (listen(server_fd, BACKLOG) == -1) {
        perror("listen");
        close(server_fd);
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

		printf("<--Got Connection From %s\n", inet_ntoa(client_addr.sin_addr));
        if(current_client_address == 0)
            current_client_address =client_addr.sin_addr.s_addr;
        else if (current_client_address != client_addr.sin_addr.s_addr)
        {
            printf("-->Rejected !!!! Already a client conected!\n");
            free(client_fd);
            continue;
        }


		/* TODO(FIX ME): disable the Nagle (TCP No Delay) algorithm */
		setsockopt(*client_fd, IPPROTO_TCP, TCP_NODELAY, (char *)&nagle_flag, sizeof(nagle_flag));

        /* create a new thread for each client request */
        if (flag == 1) {
            int timeout = 10000;
            flag = 2;
            AVcommand_fd = *client_fd;
            free(client_fd);
            setsockopt(AVcommand_fd, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout));
            /* ----------------------------------------------------------------- */

            led_flag = 0;
            led_on();
            command254 = malloc(sizeof(struct command));
            memcpy(command254->protocol_head, str_ctl, 4);
            /* ----------------------------------------------------------------- */
            if(pthread_create(&th1, NULL, deal_opcode_request, &AVcommand_fd) != 0) {
                perror("pthread_create");
                break;
            }
        } else if (flag == 2){
		struct timeval timeout = {0,500000};
            flag = 3;
            printf(">>>>>in video or record data connection<<<<<\n");
            /* ----------------------------------------------------------------- */

			picture_fd = *client_fd;					
			audio_data_fd = *client_fd;
			free(client_fd);
			
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
			
			setsockopt(picture_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
			gettimeofday(&av_start_time,NULL);
			/* ----------------------------------------------------------------- */
#ifdef ENABLE_VIDEO
			sem_post(&start_camera);		/* start video only when cellphone send request ! */
#endif
		}
		else if (flag == 3){
            flag = 3;
            // close(music_data_fd);
            if(music_data_fd != -1)
            {
                close(*client_fd);
                free(client_fd);
                printf("-->Error: Muisc Is Working Now ! Reject Request!\n");
            }
            else{
                music_data_fd = *client_fd;
		free(client_fd);
                /* TODO just want to erase the no using data int the new socket buffer */
                //clear_recv_buf(music_data_fd);
                /* when the new socket is connected, then tell the cellphone to start sending file */
                sem_post(&start_talk);
            }
		}
	}
	pthread_join(th1, NULL);

	pthread_mutex_destroy(&AVsocket_mutex);
	
	free(av_command1);
	free(av_command2);

	close(picture_fd);
	close(server_fd);
}
