/********************************************************
 *
 *   Author : joans@joans-computer
 *   Date   : 2011-08-13
 *   File   : main.c
 *
 *
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <error.h>
#include <string.h>
#include <fcntl.h>
#include <linux/videodev2.h>

int fd;
const char *input_dev = "/dev/video0";
const char *qctrl_name = NULL;
int qctrl_value = 0;
int print_ctrl = 0;

struct v4l2_capability cap;
struct v4l2_queryctrl qctrl;

static void print_qctrl(struct v4l2_queryctrl *qctrl)
{
	struct v4l2_control ctrl;
	
	ctrl.id = qctrl->id;
	if (ioctl(fd, VIDIOC_G_CTRL, &ctrl) < 0) {
		perror("get ctrl failed");
		ctrl.value = -999;
	}

	if(print_ctrl){
	 	printf("%-14s : id=%08x, type=%d, minimum=%d, maximum=%d\n"
        	"\t\t value = %d, step=%d, default_value=%d\n",
        	qctrl->name, qctrl->id, qctrl->type, qctrl->minimum, qctrl->maximum,
        	ctrl.value, qctrl->step, qctrl->default_value);
	}
}

static void print_menu(struct v4l2_querymenu *menu)
{
	if(print_ctrl)
		printf("\t %d : %s\n", menu->index, menu->name);
}

static int set_qctrl(struct v4l2_queryctrl *qctrl)
{
	struct v4l2_control ctrl;
	
	printf("set %s = %d\n", qctrl_name, qctrl_value);
	
	ctrl.id = qctrl->id;
	ctrl.value = qctrl_value;
	return ioctl(fd, VIDIOC_S_CTRL, &ctrl);
}

static void deal_qctrl(struct v4l2_queryctrl *qctrl)
{
  print_qctrl(qctrl);
  if (qctrl_name && !strcmp(qctrl_name, qctrl->name))
    set_qctrl(qctrl);
}

static void qctrl_get(int id)
{
    qctrl.id = id;
    if (ioctl(fd, VIDIOC_QUERYCTRL, &qctrl) == 0) {
    	deal_qctrl(&qctrl);
      	if (qctrl.type == V4L2_CTRL_TYPE_MENU) {
        	int idx;
        	struct v4l2_querymenu menu;
        	for (idx = qctrl.minimum; idx <= qctrl.maximum; idx++) {
         		menu.id = qctrl.id;
         		menu.index = idx;
         		if (ioctl(fd, VIDIOC_QUERYMENU, &menu)==0) {
            		print_menu(&menu);
          		}
        	}
      	}
    }
}

int main(int argc, char **argv)
{
	int ret, i;
	
	if (argc == 3) {
		qctrl_name = argv[1];
		qctrl_value = atoi(argv[2]);
	}
	else
		print_ctrl = 1;
	
	fd = open(input_dev, O_RDWR);
	if (fd < 0) {
		perror("open video failed");
		return -1;
	}
	
	for (i = V4L2_CID_BASE; i < V4L2_CID_LASTP1; i++) {
		qctrl_get(i);
	}
	
	for (i = V4L2_CID_PRIVATE_BASE; i < V4L2_CID_PRIVATE_BASE+25; i++) {
		qctrl_get(i);
	}
	
	close(fd);
	
	return 0;
}
