/*
 * v4l2.h
 *  V4L2 video capture. 
 *		flow mothods: open -> init -> start -> ... -> stop -> uninit -> close
 *  chenguangming@wxseuic.com
 */
#ifndef _V4L2_h
#define _V4L2_h

#include <camera.h>

typedef struct {
	char *data;
	unsigned long length;
} frame_t;

int open_device (char *dev_name);
int init_device (int fd, int width, int heigth, int fps);
void start_capturing (int fd);
void stop_capturing (int fd);
void uninit_device (void);
void close_device (int fd);
int get_frame(int fd, frame_t *fm);
int put_frame(int fd);

#endif
