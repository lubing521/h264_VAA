/*
 * mma7660 i2c-bus g-sensor device driver
 *
 * Written by: lichun <lichun08@seuic.com>
 *
 * Copyright (C) 2011 SEUIC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <asm/uaccess.h>
#include <linux/fs.h>

#ifdef CONFIG_I2C
#include <linux/i2c.h>
#endif

#ifdef CONFIG_GSENSOR_MMA7660_USE_INTERRUPT
#include <mach/hardware.h>
#include <mach/irqs.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <board/board.h>
#endif

//#define _MMA7660_DEBUG

#ifdef _MMA7660_DEBUG
	#define ad_dbg(f, a...) printk(f, ##a)
#else
	#define ad_dbg(f, a...)
#endif

#define SENSOR_DEV_NAME		"gsensor"
#define SENSOR_DEV_MAJOR		249
#define SENSOR_DEV_MINOR		1

#define SENSOR_DEV_MAJOR_NUM 	234
#define SENSOR_DEV_MINOR_NUM 	1

#define IOCTL_SENSOR_SET_DELAY_ACCEL   	_IO(SENSOR_DEV_MAJOR_NUM, 100)
#define IOCTL_SENSOR_GET_DATA_ACCEL		_IO(SENSOR_DEV_MAJOR_NUM, 101)
#define IOCTL_SENSOR_GET_STATE_ACCEL   	_IO(SENSOR_DEV_MAJOR_NUM, 102)
#define IOCTL_SENSOR_SET_STATE_ACCEL		_IO(SENSOR_DEV_MAJOR_NUM, 103)
#define IOCTL_SENSOR_SET_CALIB_ACCEL	_IO(SENSOR_DEV_MAJOR_NUM, 104)

#define GSENSOR_TIMER_DELAY 	200

enum {
      MMA7660_REG_XOUT = 0x00,
      MMA7660_REG_YOUT,
      MMA7660_REG_ZOUT,
      MMA7660_REG_TILT,
      MMA7660_REG_SRST,
      MMA7660_REG_SPCNT,
      MMA7660_REG_INTSU,
      MMA7660_REG_MODE,
      MMA7660_REG_SR,
      MMA7660_REG_PDET,
      MMA7660_REG_PD,
};

typedef struct {
    int x;
    int y;
    int z;
    int delay_time;
} gsensor_accel_t;

static gsensor_accel_t  gsensor_data;
static int gsensor_ready;

struct mma7660 {
	struct 		i2c_client	*client;
	struct 		work_struct work;
	struct 		timer_list timer;
};

static void mma7660_write(struct i2c_client *client,
			unsigned char reg, unsigned char val)
{
	 i2c_smbus_write_byte_data(client, reg, val);
}

static unsigned char mma7660_read(struct i2c_client *client,
		unsigned char reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static void mma7660_showconfig(struct i2c_client *client)
{
	int i;
	for(i=MMA7660_REG_XOUT; i<MMA7660_REG_PD;i++)
	{
		ad_dbg("        mma7660:[reg %2d] 0x%2x\n", i, mma7660_read(client, i));
	}
}

static void mma7660_get_xyz(struct i2c_client *client)
{
	int x, y, z;
	
	x = (int)mma7660_read(client, MMA7660_REG_XOUT) & 0x3f;
	y = (int)mma7660_read(client,MMA7660_REG_YOUT) & 0x3f;
	z = (int)mma7660_read(client,MMA7660_REG_ZOUT) & 0x3f;

	if (x >= 32)
		x = x- 64;

	if (y >= 32)
		y = y -64;

	if (z >= 32)
		z = z -64;
	#ifdef CONFIG_GSENSOR_MMA7660_BOTTOM
	gsensor_data.x = x;
	gsensor_data.y = -y;
	gsensor_data.z = -z;
	#endif
	#ifdef CONFIG_GSENSOR_MMA7660_TOP
	gsensor_data.x = -y;
        gsensor_data.y = x;
        gsensor_data.z = -z;
	#endif
	ad_dbg("        mma7660: x:%d, y:%d, z:%d\n", x, y, z);
}


static void mma7660_work(struct work_struct *work)
{
	struct mma7660 *gs = container_of(work, struct mma7660, work);

	mma7660_get_xyz(gs->client);

#ifndef CONFIG_GSENSOR_MMA7660_USE_INTERRUPT	
	mod_timer(&gs->timer, jiffies + GSENSOR_TIMER_DELAY);
#endif
}

static void mma7660_timer(unsigned long handle)
{
	struct mma7660 *gs = (void *)handle;

	if (!work_pending(&gs->work))
		schedule_work(&gs->work);
}

#ifdef CONFIG_GSENSOR_MMA7660_USE_INTERRUPT
static irqreturn_t mma7660_irq(int irq, void *handle)
{
	struct mma7660 *gs = handle;

	/* The repeated conversion sequencer controlled by TMR kicked off too fast.
	 * We ignore the last and process the sample sequence currently in the queue.
	 * It can't be older than 9.4ms
	 */
	SEP0611_INT_DISABLE(SEP0611_GSENSOR_INTSRC);
	
	sep0611_gpio_clrirq(SEP0611_GSENSOR_INT);

	if (!work_pending(&gs->work))
		schedule_work(&gs->work);
	
	SEP0611_INT_ENABLE(SEP0611_GSENSOR_INTSRC); 

	return IRQ_HANDLED;
}
#endif


/*
 * Called when a mma7660 device is matched with this driver
 */
static int mma7660_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int rc;
	struct mma7660 *gs;
	gs = kzalloc(sizeof(struct mma7660), GFP_KERNEL);
	if (!gs)
		return -ENOMEM;

	gsensor_ready = 0;

	i2c_set_clientdata(client, gs);
	gs->client = client;

	if (!i2c_check_functionality(client->adapter,
				     I2C_FUNC_SMBUS_I2C_BLOCK)) {
		dev_err(&client->dev, "i2c bus does not support the mma7660\n");
		rc = -ENODEV;
		goto exit;
	}

	mma7660_write(client, MMA7660_REG_MODE, 0x0);
	mma7660_write(client, MMA7660_REG_INTSU, 0x10);
	mma7660_write(client, MMA7660_REG_SR, 0x02);
	mma7660_write(client, MMA7660_REG_MODE, 0xC1);
	mma7660_showconfig(client);

	INIT_WORK(&gs->work, mma7660_work);

#ifdef CONFIG_GSENSOR_MMA7660_USE_INTERRUPT
	SEP0611_INT_DISABLE(SEP0611_GSENSOR_INTSRC);
	
	sep0611_gpio_setirq(SEP0611_GSENSOR_INT, UP_TRIG);
	mdelay(2);
	sep0611_gpio_clrirq(SEP0611_GSENSOR_INT);

	SEP0611_INT_ENABLE(SEP0611_GSENSOR_INTSRC);
	
	rc = request_irq(SEP0611_GSENSOR_INTSRC, mma7660_irq,
			  IRQF_SHARED, "mma7660", gs);
	if(rc)
	{
		dev_err(&client->dev, "mma7660 request irq failed!\n");
		goto exit;
	}
#else
	setup_timer(&gs->timer, mma7660_timer, (unsigned long) gs);
	mod_timer(&gs->timer, jiffies + GSENSOR_TIMER_DELAY);
#endif

	gsensor_ready = 1;

	printk( "mma7660 g-sensor driver inited.\n");
	
	return 0;

 exit:
	return rc;
}

static int mma7660_remove(struct i2c_client *client)
{
	struct  mma7660 *gs = dev_get_drvdata(&client->dev);

	i2c_set_clientdata(client, NULL);
	kfree(gs);
	
	return 0;
}

#ifdef CONFIG_PM
static int mma7660_i2c_suspend(struct device *dev)
{
	printk("%s", __FUNCTION__);
	SEP0611_INT_DISABLE(SEP0611_GSENSOR_INTSRC);

	return 0;
}

static int mma7660_i2c_resume(struct device *dev)
{
	printk("%s", __FUNCTION__);
	SEP0611_INT_ENABLE(SEP0611_GSENSOR_INTSRC);

	return 0;
}

static SIMPLE_DEV_PM_OPS(mma7660_i2c_pm, mma7660_i2c_suspend, mma7660_i2c_resume);
#endif


static const struct i2c_device_id mma7660_id[] = {
	{ "mma7660", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, mma7660_id);

static struct i2c_driver mma7660_driver = {
	.driver = {
		.name = "mma7660",
#ifdef CONFIG_PM
		.pm	= &mma7660_i2c_pm,
#endif
	},
	.probe = mma7660_probe,
	.remove = mma7660_remove,
	.id_table = mma7660_id,
};

static ssize_t mma7660_sensor_write(struct file *file, const char __user *user, size_t size, loff_t *o)
{
	ad_dbg("%s\n", __func__);
    	return 0;
 }

static ssize_t mma7660_sensor_read(struct file *file, char __user *user, size_t size, loff_t *o)
{
	ad_dbg("%s\n", __func__);
	if(copy_to_user((gsensor_accel_t*)user, (const void *)&gsensor_data, 
				sizeof(gsensor_accel_t))!=0)
		printk("mma7660_sensor_read failed!\n");
				
	return 0;
}

static int mma7660_sensor_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, void *arg)
{
	switch (cmd)
	{
		case IOCTL_SENSOR_GET_DATA_ACCEL:
			if(copy_to_user((gsensor_accel_t*)arg, (const void *)&gsensor_data, 
				sizeof(gsensor_accel_t))!=0)
				ad_dbg("IOCTL_SENSOR_GET_DATA_ACCEL error\n");
			break;

		case IOCTL_SENSOR_SET_DELAY_ACCEL:
			break;

		case IOCTL_SENSOR_GET_STATE_ACCEL:
			break;

		case IOCTL_SENSOR_SET_STATE_ACCEL:
			break;

		default:
			ad_dbg("sensor: unrecognized ioctl (0x%x)\n", cmd); 
			return -EINVAL;
			break;
	}

	return 0;
}

static int mma7660_sensor_release(struct inode *inode, struct file *filp)
{ 
	return 0;
}

static int mma7660_sensor_open(struct inode *inode, struct file *filp)
{
	if(gsensor_ready)
		return 0;
	else
		return -1;
}

struct file_operations gsensor_fops =
{
	.owner  	= THIS_MODULE,
	.open     	= mma7660_sensor_open,
	.release  	= mma7660_sensor_release,
	.ioctl 	= mma7660_sensor_ioctl,
	.read 	= mma7660_sensor_read,
	.write 	= mma7660_sensor_write,	
};

static int __init mma7660_init(void)
{
	static struct class *sensor_class;
	int ret;

	ret = register_chrdev(SENSOR_DEV_MAJOR, SENSOR_DEV_NAME, &gsensor_fops);
	if (ret < 0)
		return ret;
	
	sensor_class = class_create(THIS_MODULE, SENSOR_DEV_NAME);
	device_create(sensor_class,NULL,MKDEV(SENSOR_DEV_MAJOR,SENSOR_DEV_MINOR),NULL,SENSOR_DEV_NAME);

	return i2c_add_driver(&mma7660_driver);
}

static void __exit mma7660_exit(void)
{
	i2c_del_driver(&mma7660_driver);
	unregister_chrdev(SENSOR_DEV_MAJOR, SENSOR_DEV_NAME);
}

MODULE_AUTHOR("lichun <lichun08@seuic.com>");
MODULE_DESCRIPTION("MMA7660 i2c-bus g-sensor driver");
MODULE_LICENSE("GPL");

module_init(mma7660_init);
module_exit(mma7660_exit);
