#ifndef __SEP_IOCTL_H__
#define __SEP_IOCTL_H__

#define MAJOR_NUM 247

#define IOCTL_CKC_SET_PERI          _IO(MAJOR_NUM, 0)
#define IOCTL_CKC_GET_PERI          _IO(MAJOR_NUM, 1)
#define IOCTL_CKC_SET_PERIBUS       _IO(MAJOR_NUM, 2)
#define IOCTL_CKC_GET_PERIBUS       _IO(MAJOR_NUM, 3)
#define IOCTL_CKC_SET_PERISWRESET   _IO(MAJOR_NUM, 4)
#define IOCTL_CKC_SET_FBUSSWRESET   _IO(MAJOR_NUM, 5)
#define IOCTL_CKC_SET_CPU           _IO(MAJOR_NUM, 6) 
#define IOCTL_CKC_GET_CPU           _IO(MAJOR_NUM, 7) 
#define IOCTL_CKC_SET_SMUI2C        _IO(MAJOR_NUM, 8) 
#define IOCTL_CKC_GET_BUS           _IO(MAJOR_NUM, 9) 
#define IOCTL_CKC_GET_VALIDPLLINFO  _IO(MAJOR_NUM, 10)
#define IOCTL_CKC_SET_FBUS          _IO(MAJOR_NUM, 11)
#define IOCTL_CKC_GET_FBUS          _IO(MAJOR_NUM, 12)
#define IOCTL_CKC_SET_PMUPOWER      _IO(MAJOR_NUM, 13)
#define IOCTL_CKC_GET_PMUPOWER      _IO(MAJOR_NUM, 14)
#define IOCTL_CKC_GET_CLOCKINFO     _IO(MAJOR_NUM, 15)
#define IOCTL_CKC_SET_CHANGEFBUS    _IO(MAJOR_NUM, 16)
#define IOCTL_CKC_SET_CHANGEMEM     _IO(MAJOR_NUM, 17)
#define IOCTL_CKC_SET_CHANGECPU     _IO(MAJOR_NUM, 18)

struct storage_direct{
	void *buf;
	dma_addr_t dma;
	size_t count;
	loff_t pos;
	ssize_t actual;
	unsigned int user_space;
};
 
#define IOCTL_STORAGE_DIRECT_READ   _IO(MAJOR_NUM, 100)
#define IOCTL_STORAGE_DIRECT_WRITE  _IO(MAJOR_NUM, 101)
#define IOCTL_STORAGE_PING          _IO(MAJOR_NUM, 102)

#endif	/* __SEP_IOCTL_H__ */
