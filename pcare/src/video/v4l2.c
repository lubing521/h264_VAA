/*
 * v4l2.c
 *  V4L2 video capture. 
 *		flow mothods: open -> init -> start -> ... -> stop -> uninit -> close
 *  chenguangming@wxseuic.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>              /* low-level i/o */
#include <unistd.h>
#include <errno.h>
#include <malloc.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <asm/types.h>          /* for videodev2.h */
#include <linux/videodev2.h>

#include <types.h>
#include <camera.h>
#include "v4l2.h"

#define CLEAR(x) memset (&(x), 0, sizeof (x))

struct buffer {
    void *	start;
    size_t	length;
};

struct buffer *         buffers		= NULL;
static unsigned int     n_buffers	= 0;
static struct v4l2_buffer v4l_buf;

static void errno_exit (const char *s)
{
    fprintf (stderr, "%s error %d, %s\n",s, errno, strerror (errno));
    exit (EXIT_FAILURE);
}

/*
 * store the raw image data in file
 */
void store_raw(unsigned char *buf, int len)
{
	FILE *file;
	char name[16];

	static int nn=0;
	sprintf(name, "/nfs/imgs/%d.yuv", nn++);
	file = fopen(name, "wb");
	if(file == NULL){
		printf("fail create file %s\n", name);
		return;
	}

	fwrite(buf, len, 1, file);
	fclose(file);
}

/*
 * read a frame of image from video device
 */
int get_frame(int fd, frame_t *fm)
{
    if (-1 == ioctl (fd, VIDIOC_DQBUF, &v4l_buf))
		return 0;
    
    fm->data = buffers[v4l_buf.index].start;
    fm->length = v4l_buf.bytesused;

    return 1;
}

/*
 * enqueue the frame again
 */
int put_frame(int fd)
{
	return ioctl(fd, VIDIOC_QBUF, &v4l_buf);
}

/*
 * stop video device capturing
 */
void stop_capturing (int fd)
{
    enum v4l2_buf_type type;

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

   	if (-1 == ioctl (fd, VIDIOC_STREAMOFF, &type))
		errno_exit ("VIDIOC_STREAMOFF");
}

/*
 * start video device capturing
 */
void start_capturing (int fd)
{
    unsigned int i;
    enum v4l2_buf_type type;
    
    printf("start_capturing...\n");

    for (i = 0; i < n_buffers; ++i) {
        struct v4l2_buffer buf;

        CLEAR (buf);

        buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory	= V4L2_MEMORY_MMAP;
        buf.index	= i;

        if (-1 == ioctl (fd, VIDIOC_QBUF, &buf))
  			errno_exit ("VIDIOC_QBUF");
    }
    
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

    if (-1 == ioctl (fd, VIDIOC_STREAMON, &type))
		errno_exit ("VIDIOC_STREAMON");
}

/*
 * uninit the video device
 */
void uninit_device (void)
{
    unsigned int i;

	for (i = 0; i < n_buffers; ++i)
   		if (-1 == munmap (buffers[i].start, buffers[i].length))
  			errno_exit ("munmap");
  			
    free (buffers);
}

/*
 * init the video device
 */
int init_device (int fd, int width, int height, int fps)
{
    struct v4l2_requestbuffers req;
    struct v4l2_capability cap;
    struct v4l2_cropcap cropcap;
    struct v4l2_crop crop;
    struct v4l2_format fmt;
    unsigned int min;
	int ret;
	struct v4l2_fmtdesc fmtdesc;
    
    if (-1 == ioctl (fd, VIDIOC_QUERYCAP, &cap)) {
        fprintf (stderr, "VIDIOC_QUERYCAP fail\n");
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
        fprintf (stderr, "VIDIOC_QUERYCAP fail\n");
        goto err;
    }

    if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
   		fprintf (stderr, "device does not support streaming i/o\n");
  		goto err;
    }

    /* Print capability infomations */
    printf("Capability Informations:\n");
    printf(" driver: %s\n", cap.driver);
    printf(" card: %s\n", cap.card);
    printf(" bus_info: %s\n", cap.bus_info);
    printf(" version: %08X\n", cap.version);
    
    /* Select video input, video standard and tune here. */
    CLEAR (cropcap);
    cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 == ioctl (fd, VIDIOC_CROPCAP, &cropcap)) {
        crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        crop.c = cropcap.defrect; /* reset to default */

        if (-1 == ioctl (fd, VIDIOC_S_CROP, &crop)) {
            switch (errno) {
            case EINVAL:
               	/* Cropping not supported. */
               	break;
            default:
                /* Errors ignored. */
                break;
            }
        }
    }
    
    /* enum video formats. */
    CLEAR(fmtdesc);
	fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    printf("Enum format:\n");
	while ((ret = ioctl(fd, VIDIOC_ENUM_FMT, &fmtdesc)) == 0)
	{
	    fmtdesc.index++;
		printf(" <%d> pixelformat = \"%c%c%c%c\", description = %s\n",fmtdesc.index,
			fmtdesc.pixelformat & 0xFF, 
			(fmtdesc.pixelformat >> 8) & 0xFF, 
			(fmtdesc.pixelformat >> 16) & 0xFF,
			(fmtdesc.pixelformat >> 24) & 0xFF, 
			fmtdesc.description);
	}

    /* set video formats. */
    CLEAR (fmt);
	char * p = (char *)(&fmt.fmt.pix.pixelformat);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (ioctl (fd, VIDIOC_G_FMT, &fmt) < 0) {        
 		/* Errors ignored. */
		printf("get fmt fail\n");
    }

	fmt.fmt.pix.width       = width; 
	fmt.fmt.pix.height      = height;

	init_mem_repo();
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
	fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED;
 
	if (-1 == ioctl (fd, VIDIOC_S_FMT, &fmt)){
		printf("set format fail\n");
   		return -1;
   	}
   	
    if (ioctl (fd, VIDIOC_G_FMT, &fmt) < 0) {        
 		/* Errors ignored. */
		printf("get fmt fail\n");
    }
    
	printf("fmt.type = %d\n", fmt.type);
	printf("fmt.width = %d\n", fmt.fmt.pix.width);
	printf("fmt.height = %d\n", fmt.fmt.pix.height);
	printf("fmt.format = %c%c%c%c\n", p[0], p[1], p[2], p[3]);
	printf("fmt.field = %d\n", fmt.fmt.pix.field);
	printf("fps = %d\n", fps);

    /* Buggy driver paranoia. */
    min = fmt.fmt.pix.width * 2;
    if (fmt.fmt.pix.bytesperline < min)
    	fmt.fmt.pix.bytesperline = min;
    
    min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
    if (fmt.fmt.pix.sizeimage < min)
      	fmt.fmt.pix.sizeimage = min;

    struct v4l2_streamparm* setfps;  
    setfps=(struct v4l2_streamparm *) calloc(1, sizeof(struct v4l2_streamparm));
    memset(setfps, 0, sizeof(struct v4l2_streamparm));
    setfps->type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    setfps->parm.capture.timeperframe.numerator = 1;
    setfps->parm.capture.timeperframe.denominator = fps;
    if(ioctl(fd, VIDIOC_S_PARM, setfps) < 0){
		printf("set fps fail\n");
   		return -1;
    }
    
    CLEAR (v4l_buf);
    v4l_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    v4l_buf.memory = V4L2_MEMORY_MMAP;

    CLEAR (req);
    req.count	= 4;
    req.type 	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory	= V4L2_MEMORY_MMAP;

    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &req)) {
        fprintf (stderr, "VIDIOC_QUERYCAP fail\n");
        goto err;
    }

    if (req.count < 2) {
        fprintf (stderr, "Insufficient buffer memory\n");
        return -1;
    }
    
    buffers = calloc (req.count, sizeof (*buffers));
    if (!buffers) {
        fprintf (stderr, "Out of memory\n");
        return -1;
    }

    for (n_buffers = 0; n_buffers < req.count; ++n_buffers) {
        struct v4l2_buffer buf;   

        CLEAR (buf);

        buf.type	= V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory	= V4L2_MEMORY_MMAP;
        buf.index	= n_buffers;

        if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buf)){
		    fprintf (stderr, "VIDIOC_QUERYCAP fail\n");
		    goto err;
		}

        buffers[n_buffers].length = buf.length;
        buffers[n_buffers].start = mmap (NULL /* start anywhere */,
				buf.length,
				PROT_READ | PROT_WRITE /* required */,
				MAP_SHARED /* recommended */,
				fd, buf.m.offset);

        if (MAP_FAILED == buffers[n_buffers].start){
		    fprintf (stderr, "mmap fail\n");
		    goto err;
		}
    }
    return 0;

err:
	return -1;
}

/*
 * close the video device
 */
void close_device (int fd)
{
    if (-1 == close (fd))
    	errno_exit ("close");
}

/*
 * open the video device
 */
int open_device (char *dev_name)
{
    struct stat st; 
    int fd = -1;

    if (-1 == stat (dev_name, &st)) {
		fprintf (stderr, "Cannot identify '%s': %d, %s\n",
                     dev_name, errno, strerror (errno));
       	goto err;
    }

    if (!S_ISCHR (st.st_mode)) {
        fprintf (stderr, "%s is no device\n", dev_name);
       	goto err;
    }

    fd = open (dev_name, O_RDWR /* required */ | O_NONBLOCK, 0);
    if (-1 == fd) {
        fprintf (stderr, "Cannot open '%s': %d, %s\n",
                 dev_name, errno, strerror (errno));
       	goto err;
    }

err:
	return fd;
}
