#ifndef __FTL_UTIL__
#define __FTL_UTIL__

extern unsigned int comp_f(unsigned char *buf,unsigned int offset,unsigned char data,unsigned int len);
extern unsigned int comp32_f(unsigned int *buf,unsigned int offset,unsigned int data,unsigned int len);
extern void memset_f(unsigned char *addr,unsigned char value,unsigned int len);
extern void memset16_f(unsigned short *addr,unsigned short value,unsigned int len);
extern void memset32_f(unsigned int *addr,unsigned int value,unsigned int len);
extern void memcp_f(unsigned char *dest,unsigned char *sour,unsigned int len);
extern void memcp32_f(unsigned int *ptrDest,unsigned int *ptrSrc,unsigned int len);

#endif
