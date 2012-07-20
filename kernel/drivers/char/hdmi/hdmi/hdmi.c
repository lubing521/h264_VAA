/* path:     \kernel\drivers\char\hdmi\hdmi\hdmi.c
 * author: wanglei
 * version 1.0.1 2011-6-8 */
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>

#include <mach/hardware.h>
#include <mach/hdmi.h>
#include <mach/gpio.h>
#include <board/board.h>

#include "../hdmilib/hdmitx.h"
#include "../hdmilib/HDMI_TX.h"
#include "../hdmilib/cat6613_sys.h"

#define HDMI_DEBUG 0
#if HDMI_DEBUG
#define dprintk(args...)    printk(args)
#else
#define dprintk(args...)
#endif

#define VERSION "1.0.1" /* Driver version number */
#define HDMI_MINOR 240 /* Major 10, Minor 240, /dev/hdmi */
#define ENABLE 1
#define DISABLE 0
extern struct i2c_client* gHdmiClient;
//------------------------------------------------------------
//=========================================================================
// VPG data definition
// VPG: Video Pattern Generation, implement in vpg.v
//=========================================================================



//VPG_MODE gVpgMode = MODE_1280x720; //MODE_1920x1080;
//COLOR_TYPE gVpgColor = COLOR_RGB444;   // video pattern generator - output color (defined ind vpg.v)
extern BYTE bOutputColorMode;
extern enum ColorDepth gColorDepth;
extern int bHDMIMode;
extern int bAudioEnable;
extern BYTE gAudioSampleFreq;
extern BYTE gAudioChannelNum;
extern BYTE gAudioWordLen;
//------------------------------------------------------------

#define HDMI_PWR_INIT()         sep0611_gpio_cfgpin(SEP0611_HDMI_EN,SEP0611_GPIO_IO);\
								sep0611_gpio_dirpin(SEP0611_HDMI_EN,0);\
								sep0611_gpio_cfgpin(SEP0611_USB5V_EN,SEP0611_GPIO_IO);\
								sep0611_gpio_dirpin(SEP0611_USB5V_EN,0);\
								sep0611_gpio_cfgpin(SEP0611_HDMI_RST,SEP0611_GPIO_IO);\
								sep0611_gpio_dirpin(SEP0611_HDMI_RST,0);


#define HDMI_PWR_ON()       	sep0611_gpio_setpin(SEP0611_HDMI_EN,ENABLE);\
								sep0611_gpio_setpin(SEP0611_USB5V_EN,ENABLE);


#define HDMI_PWR_OFF()       	sep0611_gpio_setpin(SEP0611_HDMI_EN,DISABLE);\
								sep0611_gpio_setpin(SEP0611_USB5V_EN,ENABLE);

#define HDMI_HARD_RESET()   	sep0611_gpio_setpin(SEP0611_HDMI_RST,0);  \
								mdelay(200);  \
								sep0611_gpio_setpin(SEP0611_HDMI_RST,1);  \
								mdelay(200);

static int /*__init*/ hdmi_init(void);
static void /*__init*/ hdmi_exit(void);

//file_operations
static int hdmi_open(struct inode *inode, struct file *file);
static int hdmi_release(struct inode *inode, struct file *file);
static ssize_t hdmi_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos);
static ssize_t hdmi_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos);
static int hdmi_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg);

//ioctrl
void hdmi_start(void);
void hdmi_stop(void);

void sep_hdmi_power_on(void);
void sep_hdmi_power_off(void);

//=========================================================================
// TX video formation control
//=========================================================================

/**
 * Enable HDMI output.
 */
void hdmi_start(void)
{	
	dprintk("hdmi_start\n");
	HDMITX_EnableVideoOutput();
}

void hdmi_stop(void)
{
	HDMITX_DisableVideoOutput();
}

int hdmi_set_color_space(HDMI_OutputColorMode space)
{
	bOutputColorMode = space;
	return 0;
}

int hdmi_set_color_depth(enum ColorDepth depth)
{
	gColorDepth = depth;
	return 0;
}

int hdmi_set_hdmimode(int mode)
{
	bHDMIMode = mode;
	return 0;
}

int hdmi_set_avmute(int mute)
{
	bAudioEnable = mute;
	return 0;
}

int hdmi_set_audio_channel(enum ChannelNum channel)
{
	gAudioChannelNum = channel;
	return 0;
}

int hdmi_set_audio_sample_freq(enum SamplingFreq freq)
{
	gAudioSampleFreq = freq;
	return 0;
}	     

static int hdmi_open(struct inode *inode, struct file *file)
{
	sep_hdmi_power_on();
    return 0;
}

static int hdmi_release(struct inode *inode, struct file *file)
{
	dprintk("hdmi release");
	sep_hdmi_power_off();
    return 0;
}

ssize_t hdmi_read(struct file *file, char __user *buffer, size_t count, loff_t *ppos)
{
    return 0;
}

ssize_t hdmi_write(struct file *file, const char __user *buffer, size_t count, loff_t *ppos)
{
    return 0;
}

int hdmi_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
	unsigned int data;
	switch (cmd)
    {
		case HDMI_IOC_START_HDMI:
		{
			dprintk("hdmi_ioctl:HDMI_IOC_START_HDMI\n");
			hdmi_start();
			break;
		}
		case HDMI_IOC_STOP_HDMI:
		{
			dprintk("hdmi_ioctl:HDMI_IOC_STOP_HDMI\n");
			hdmi_stop();
			break;
		}
		case HDMI_IOC_SET_COLORSPACE:
		{
			dprintk("hdmi_ioctl:HDMI_IOC_SET_COLORSPACE\n");
			if(get_user(data, (int __user *) arg))
				return -EFAULT;
				
			hdmi_set_color_space(data);
			
			break;
		}
		case HDMI_IOC_SET_COLORDEPTH:
		{
			dprintk("hdmi_ioctl:HDMI_IOC_SET_COLORDEPTH\n");
			if(get_user(data,(int __user *) arg))
				return -EFAULT;
				
			hdmi_set_color_depth(data);
			 
			break;
		}
		case HDMI_IOC_SET_HDMIMODE:
		{
			dprintk("hdmi_ioctl:HDMI_IOC_SET_HDMIMODE\n");
			if(get_user(data,(int __user *)arg))
				return -EFAULT;
			
			hdmi_set_hdmimode(data);
			
			break;
		}
		case HDMI_IOC_SET_VIDEOFORMAT_INFO:
		{
			dprintk("hdmi_ioctl:HDMI_IOC_SET_VIDEOFORMAT_INFO\n");
			
			if( get_user(data,(int __user *)arg))
			{
				dprintk("error to get resolution: resolution=%d\n",data);
				return -EFAULT;
			}
			dprintk("success get resolution: resolution=%d\n",data);
			HDMITX_ChangeVideoTiming(data);
			break;
		}
		case HDMI_IOC_SET_AVMUTE:
		{
			dprintk("hdmi_ioctl:HDMI_IOC_SET_AVMUTE\n");
			if(get_user(data,(int __user *)arg))
				return -EFAULT;
			
			hdmi_set_avmute(data);
			
			break;
		}
		case HDMI_IOC_GET_AUDIOPACKETTYPE:
		{
			break;
		}
		case HDMI_IOC_SET_AUDIOSAMPLEFREQ:
		{
			// get arg
			if (get_user(data, (int __user *) arg))
				return -EFAULT;
			
			hdmi_set_audio_sample_freq(data);
			
			break;
		}
		case HDMI_IOC_GET_AUDIOSAMPLEFREQ:
		{
			break;
		}
		case HDMI_IOC_SET_AUDIOCHANNEL:
		{
            // get arg
            if (get_user(data, (int __user *) arg))
                return -EFAULT;

	        hdmi_set_audio_channel(data);
	        
			break;
		}
		case HDMI_IOC_GET_AUDIOCHANNEL:
		{
			break;
		}
        default:
            return -EINVAL;
    }
	return 0;
}


static const struct file_operations hdmi_fops =
{
    .owner      = THIS_MODULE,
    .open       = hdmi_open,
    .release    = hdmi_release,
    .read       = hdmi_read,
    .write      = hdmi_write,
    .ioctl      = hdmi_ioctl,
};

static struct miscdevice hdmi_misc_device =
{
    HDMI_MINOR,
    "hdmi",  //"HDMI",
    &hdmi_fops,
};

void sep_hdmi_power_on(void)
{
	dprintk("sep_hdmi_power_on....\n");
	HDMI_PWR_ON();
	mdelay(200);
	
	HDMI_HARD_RESET();
	
	mdelay(500);
	
	//init cat6613
	if(!HDMITX_Init())
	{
		dprintk("init cat6613 failed!!\n");
	}
}

void sep_hdmi_power_off(void)
{
	dprintk("sep_hdmi_power_off....\n");
	//power down gpio
	HDMI_PWR_OFF();
}

static int hdmi_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    dprintk("hdmi_probe......\n");
    
    //if(gHdmiClient)
    	//printk("client has existed....\n");
    //else
    gHdmiClient = client;
    
    if (misc_register(&hdmi_misc_device))
    {
        dprintk(KERN_WARNING "HDMI: Couldn't register device 10, %d.\n", HDMI_MINOR);
        return -EBUSY;
    }
#ifdef HDCP_SUPPORT
	
#endif
	//init gpio
	HDMI_PWR_INIT();
	dprintk("init gpio....\n");
	  
	//power down gpio
	HDMI_PWR_OFF();
	dprintk("disable gpio....\n");

    return 0;
}

static int hdmi_remove(struct i2c_client *client)
{
	dprintk("hdmi remove....\n");
	misc_deregister(&hdmi_misc_device);
    return 0;
}

static const struct i2c_device_id hdmi_id[] = {
	{ "hdmi", 0 },
	{},
};

static struct i2c_driver sep_hdmi = {
	.probe	= hdmi_probe,
	.remove	= hdmi_remove,
	.id_table = hdmi_id,
	.driver	= {
		.name	= "hdmi",
		.owner	= THIS_MODULE,
	},
};

static __init int hdmi_init(void)
{
	dprintk("hdmi module added....\n");
	return i2c_add_driver(&sep_hdmi);
}

static __exit void hdmi_exit(void)
{
	dprintk("hdmi module deleted....\n");
	i2c_del_driver(&sep_hdmi);
}

module_init(hdmi_init);
module_exit(hdmi_exit);

MODULE_AUTHOR("SEP hdmi");
MODULE_LICENSE("GPL");

