/*
 * audio.c
 *  deal with the audio data
 *  chenguangming@wxseuic.com
 *	jgfntu@163.com
 */
#include <stdio.h>
#include <pthread.h>
#include "audio.h"
#include <fcntl.h>

int oss_fd_play=0;
int oss_fd_record=0;
pthread_mutex_t i2c_mutex_lock = PTHREAD_MUTEX_INITIALIZER;
