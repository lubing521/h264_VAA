/* linux/drivers/char/sep0611_char/sep0611_gpio.c
 *
 * Copyright (c) seuic

 * sep0611 gpio driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/spinlock_types.h>
#include <linux/delay.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include <mach/hardware.h>
#include <mach/gpio.h>

struct gpio_dev{
	struct cdev cdev;
};
static struct gpio_dev *sep0611_gpio_dev;

void sep0611_gpio_cfgpin(unsigned int pin, unsigned int num,uint16_t flag)
{
	unsigned int offset = -1;
 	volatile unsigned long *GPIO_SEL, *GPIO_DIR, *GPIO_INTLEL, *GPIO_INTPOL, *GPIO_INTSEL;
	volatile unsigned long *GPIO_INTCLR;
	uint16_t flag_t, f0=0xff, f1=0xff, f2=0xff, f3=0xff, f4=0xff, f5=0xff, f6=0xff;		 		   
	
	offset = (pin - PORT_A )* (0x20)/4 ; 
 	GPIO_SEL = (volatile unsigned long *)GPIO_PORTA_SEL_V + offset ;
 	GPIO_DIR = (volatile unsigned long *)GPIO_PORTA_DIR_V + offset ;
 	GPIO_INTLEL = (volatile unsigned long *)GPIO_PORTA_INTLEL_V + offset ;
 	GPIO_INTPOL = (volatile unsigned long *)GPIO_PORTA_INTPOL_V + offset ;
 	GPIO_INTSEL = (volatile unsigned long *)GPIO_PORTA_INTSEL_V + offset ;
 	GPIO_INTCLR = (volatile unsigned long *)GPIO_PORTA_INTCLR_V + offset ;

	if(flag){
	   	flag_t = flag;
		f0 = flag_t & ((0x01<<0)|(0x01<<1)|(0x01<<2));
		f1 = (flag_t>>3) & 0x01;
		f2 = (flag_t>>4) & 0x01;
		f3 = (flag_t>>5) & 0x01;
		f4 = (flag_t>>6) & 0x01;
		f5 = (flag_t>>7) & 0x01;
		f6 = (flag_t>>8) & 0x01;
	}
	
	if(f0){
		writel(f0<<1, GPIO_DBCLK_V);
	}
	
	if(pin){
		*GPIO_DIR &= ~(1<<num);			
		*GPIO_DIR |= f1<<num; 
		*GPIO_SEL &= ~(1<<num);
		*GPIO_SEL |= f2<<num; 
		*GPIO_INTSEL &= ~(1<<num);
		*GPIO_INTSEL |= f3<<num; 
		*GPIO_INTLEL &= ~(1<<num);
		*GPIO_INTLEL |= f4<<num;
		*GPIO_INTPOL &= ~(1<<num);
		*GPIO_INTPOL |= f5<<num;
		*GPIO_INTCLR &= ~(1<<num);
		*GPIO_INTCLR |= f6<<num; 
	}
	else{
		printk("Please check your port \n");
	}
}
EXPORT_SYMBOL(sep0611_gpio_cfgpin);      

unsigned int sep0611_gpio_getpin(unsigned int pin, unsigned int num)
{	
	unsigned int DATA = -1;
	unsigned int offset = -1;
	volatile unsigned long *GPIO_DATA;
	
	offset = ( pin - PORT_A ) * (0x20)/4 ; 
	GPIO_DATA = (volatile unsigned long*)GPIO_PORTA_DATA_V + offset ;
 	 
	if(pin > PORT_I){
		printk("Please check your pin \n");
		return -1;
	}
	else if (num != -1){
		DATA = (*GPIO_DATA ) & (1 << num);
	}
	else{
		DATA = *GPIO_DATA;
	}
	
	return DATA;
}
EXPORT_SYMBOL(sep0611_gpio_getpin);

void sep0611_gpio_setpin(unsigned int pin, unsigned int to, int num)
{
	unsigned int offset= -1;
	volatile unsigned long *GPIO_DATA;
	offset = (pin - PORT_A ) * (0x20)/4 ; 
	GPIO_DATA = (volatile unsigned long*)GPIO_PORTA_DATA_V + offset ;

	if(pin > PORT_I){
		printk("Please check your ping \n");
	}
	else if (num != -1){			
		*GPIO_DATA &= ~(1 << num);			
		*GPIO_DATA |= (to << num);
	}
	else{			
		*GPIO_DATA = to;
	}	
}
EXPORT_SYMBOL(sep0611_gpio_setpin);

static int sep0611_gpio_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int sep0611_gpio_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int sep0611_gpio_read(struct file *file, char __user *buf, size_t size, loff_t *loff)
{
	static struct gpio_read_config *gpio_config;
	int len, data;
	unsigned int pin, num;
	
	gpio_config = kmalloc(sizeof(struct gpio_read_config), GFP_KERNEL);
	len = copy_from_user(gpio_config, buf, sizeof(struct gpio_read_config));
	pin = gpio_config->pin;
	num = gpio_config->num;
	data= sep0611_gpio_getpin(pin, num);
	
	kfree(gpio_config);
	gpio_config = NULL;
	
	return data;	
}         

static int sep0611_gpio_write(struct file *file, const char __user *buf, size_t size, loff_t *loff)
{
	static struct gpio_write_config *gpio_config;
	int len, num; 
	unsigned int pin, to;
	
	gpio_config = kmalloc(sizeof(struct gpio_write_config), GFP_KERNEL);
	len = copy_from_user(gpio_config, buf, sizeof(struct gpio_write_config));
	pin = gpio_config->pin;
	num = gpio_config->num;
	to  = gpio_config->data;

	sep0611_gpio_setpin(pin, to, num);
	
	kfree(gpio_config);
	gpio_config = NULL;
	
	return len;
}

static int sep0611_gpio_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long param)
{
	static struct gpio_ioctl_config *gpio_config;
	unsigned int pin, num;
	int len;
	uint16_t flag;
		
	gpio_config = kmalloc(sizeof(struct gpio_ioctl_config), GFP_KERNEL);
	len = copy_from_user(gpio_config, (char *)cmd, sizeof(struct gpio_ioctl_config));
	
	pin = gpio_config->pin;
	num = gpio_config->num;
	flag = gpio_config->flag;
	sep0611_gpio_cfgpin(pin, num, flag);

	kfree(gpio_config);
	gpio_config = NULL;
	
	return len;		
}

static struct file_operations sep0611_gpio_fops = {
    .owner   = THIS_MODULE,
    .open    = sep0611_gpio_open,
    .release = sep0611_gpio_release, 
    .write   = sep0611_gpio_write,
    .read    = sep0611_gpio_read,
	.ioctl   = sep0611_gpio_ioctl, 
};

static int __init sep0611_gpio_init(void)
{
	int error;
	int result;
	dev_t devno;
	devno = MKDEV(GPIO_MAJOR,0);
	
	result = register_chrdev_region(devno, 1, DEVICE_NAME);
	
	if(result<0){
		return result;
	}

	sep0611_gpio_dev = kmalloc(sizeof(struct gpio_dev), GFP_KERNEL);
	if (sep0611_gpio_dev == NULL){
		result = -ENOMEM;
		unregister_chrdev_region(devno, 1);
		return result;
	}
	
	memset(sep0611_gpio_dev,0,sizeof(struct gpio_dev));

	cdev_init(&sep0611_gpio_dev->cdev,&sep0611_gpio_fops);
	sep0611_gpio_dev->cdev.owner = THIS_MODULE;

	error = cdev_add(&sep0611_gpio_dev->cdev, devno, 1);
	if(error){
		printk("error adding gpio!\n");
		unregister_chrdev_region(devno,1);
		return error;
	}

	printk("registered!\n");
	return 0;
}



static void __exit sep0611_gpio_exit(void)
{
	cdev_del(&sep0611_gpio_dev->cdev);
	kfree(sep0611_gpio_dev);
	unregister_chrdev_region(MKDEV(GPIO_MAJOR,0),1);
}

module_init(sep0611_gpio_init);
module_exit(sep0611_gpio_exit);

MODULE_AUTHOR("shl");                         
MODULE_DESCRIPTION("sep0611 gpio Driver");         
MODULE_LICENSE("GPL");                              
