/*
 * linux/drivers/gps/sep_gps.c
 *
 * Author:  chenguangming@wxseuic.coom
 * Created: Nov 30, 2007
 * Description: On Chip GPS driver for SEP
 *
 * Copyright (C) 2010-2011 SEP
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see the file COPYING, or write
 * to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
 
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/moduleparam.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>

#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include <board/board.h>

#include "gps_lib.h"

#define GPS_DEV_MAJOR		100
#define GPS_REGS_EN			0

struct sep_gps_dev{
	struct cdev cdev;
	struct class *gps_class;
	dev_t dev_num;
	void __iomem *regs;
	void __iomem *esram;
	unsigned int ddr2;
};

static struct sep_gps_dev *gps_dev = NULL;
static struct task_struct *gps_monitor = NULL;

static __inline void gps_power_init(void)
{
#ifdef SEP0611_GPS_EN
	gps_dbg("%s\n", __func__);
	sep0611_gpio_cfgpin(SEP0611_GPS_EN, SEP0611_GPIO_IO);
	sep0611_gpio_dirpin(SEP0611_GPS_EN, SEP0611_GPIO_OUT);
	sep0611_gpio_setpin(SEP0611_GPS_EN, GPIO_LOW);
#endif
}

static __inline void gps_power_on(void)
{
#ifdef SEP0611_GPS_EN
	gps_dbg("%s\n", __func__);
	sep0611_gpio_setpin(SEP0611_GPS_EN, GPIO_HIGH);
#endif
}

static __inline void gps_power_off(void)
{
#ifdef SEP0611_GPS_EN
	gps_dbg("%s\n", __func__);
	sep0611_gpio_setpin(SEP0611_GPS_EN, GPIO_LOW);
#endif
}

static __inline void stop_gps(void)
{
	gps_dbg("%s\n", __func__);
	
	writel(0, (gps_dev->regs + GPS_REGS_EN));
}

static __inline void start_gps(void)
{
	gps_dbg("%s\n", __func__);
	
	writel(0, (gps_dev->regs + GPS_REGS_EN));	/* disable it first to reset ARM7 */
	
	msleep(1);
	
	writel(1, (gps_dev->regs + GPS_REGS_EN));

	gps_dbg("PMU_CLK_GT_CFG1 = 0x%08lx\n", readl(PMU_CLK_GT_CFG1_V));
	gps_dbg("PMU_CLK_GT_CFG2 = 0x%08lx\n", readl(PMU_CLK_GT_CFG2_V));
	gps_dbg("PMU_PWR_GT_CFG  = 0x%08lx\n", readl(PMU_PWR_GT_CFG_V));
	gps_dbg("PMU_PIX_CLK_CFG = 0x%08lx\n", readl(PMU_PIX_CLK_CFG_V));
	gps_dbg("SYS_CTRL = 0x%08lx\n", readl(SYS_CTRL_V));
	gps_dbg("GPS_REGS_EN = 0x%08lx\n", readl((gps_dev->regs + GPS_REGS_EN)));
}

static void reset_gps(void)
{
	start_gps();
}

static int sep_gps_open(struct inode *inode, struct file *filp)
{
	gps_dbg("%s\n", __func__);

	gps_power_on();

	/* load esram.bin in ESRAM */
	gps_load_firmware(gps_dev->esram, gps_dev->ddr2);
	
	gps_load_cfg_data(); /* not care the results */
	
	if(gps_hardware_init() != GPS_SUCCESS){
		return -1;
	}
	gps_dbg("hardware init finish\n");

	gps_last_counter = 0;
	gps_monitor = kthread_create(gps_monitor_thread, reset_gps, "gps_monitor");
    if(IS_ERR(gps_monitor)){
        printk("Unable to create gps_monitor_thread!\n");
        gps_monitor = NULL;
    }
    else{
        wake_up_process(gps_monitor);
    }
	
	gps_running = 0;
	
	return 0;
}

static int sep_gps_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	gps_dbg("%s\n", __func__);
	
    switch(cmd){
    case GPS_CMD_ON:
    	start_gps();
    	gps_running = 1;
  		gps_dbg("GPS ON\n");
    	break;
    	
    case GPS_CMD_OFF:
    	stop_gps();
    	gps_running = 0;
    	gps_dbg("GPS OFF\n");
    	break;
    	
    default:
    	break;
    }
    
	return 0;
}

static int sep_gps_release(struct inode *inode, struct file *filp)
{
	gps_dbg("%s\n", __func__);
	
	if(gps_running){
		stop_gps();
		gps_running = 0;
	}
	
	if(gps_monitor){
        kthread_stop(gps_monitor);
        gps_monitor = NULL;
        gps_dbg("killed gps_monitor!\n");
    }
    
    if(gps_hardware_exit() != GPS_SUCCESS){
		printk("BUG:GPS maybe not stop!\n");
	}
	
	gps_store_cfg_data();
	gps_erase_firmware(gps_dev->esram, gps_dev->ddr2);
	gps_power_off();
	
	return 0;
}

static struct file_operations sep_gps_ops = {
    .owner 	= THIS_MODULE,  
	.open	= sep_gps_open,
	.ioctl	= sep_gps_ioctl,
	.release= sep_gps_release,
};

/*********************************************************************************/
/************************gps platform driver part*********************************/
/*********************************************************************************/
static int sep_gps_probe(struct platform_device *pdev)
{
	struct resource *regs_res;
	struct resource *esram_res;
	int ret = 0;
	
	printk("SEP onchip GPS driver v1.1\n");
	
	gps_dev = kmalloc(sizeof(struct sep_gps_dev), GFP_KERNEL);
	if(gps_dev == NULL){
		printk(KERN_ERR "Failed to allocate GPS device memory.\n");
		ret = -ENOMEM;
		goto err_out;
	}
	memset(gps_dev, 0, sizeof(struct sep_gps_dev));
	
	/* get the GPS resources */
	regs_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	esram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	
	if((regs_res == NULL) || (esram_res == NULL)){
		printk(KERN_ERR "Failed to get GPS MEM resource.\n");
		ret = -ENODEV;
		goto release_mem;
	}
	
	if(!request_mem_region(regs_res->start, resource_size(regs_res), pdev->name)){
		printk(KERN_ERR "Failed to request gps regs mem region.\n");
		ret = -EBUSY;
		goto release_mem;
	}
	
	if(!request_mem_region(esram_res->start, resource_size(esram_res), pdev->name)){
		printk(KERN_ERR "Failed to request gps esram mem region.\n");
		ret = -EBUSY;
		goto release_regs_region;
	}

	gps_dev->ddr2 = GPS_DDR2_ADDR;
	gps_dev->regs = ioremap(regs_res->start, resource_size(regs_res));
	gps_dev->esram = ioremap(esram_res->start, resource_size(esram_res));
	if((gps_dev->regs == NULL) || (gps_dev->esram == NULL)){
		ret = -ENOMEM;
		printk("Failed to ioremap GPS spaces.\n");
		goto release_io_map;	/* maybe have one success */
	}
	
	gps_dev->dev_num = MKDEV(GPS_DEV_MAJOR, 0);
	if(register_chrdev_region(gps_dev->dev_num, 1, "GPS") < 0){
		printk(KERN_ERR "Failed to register GPS chrdev region.\n");
		ret = -EBUSY;
		goto release_io_map;
	}
	
	cdev_init(&(gps_dev->cdev), &sep_gps_ops);
	gps_dev->cdev.owner = THIS_MODULE;
	if(cdev_add(&(gps_dev->cdev), gps_dev->dev_num, 1)){
		printk(KERN_ERR "Failed to add cdev.\n");
		ret = -ENODEV;
		goto release_chrdev_region;
	}
	
	/* create "/dev/gps" ourself, and add it in class "SEP-GPS" */
	gps_dev->gps_class = class_create(THIS_MODULE, "SEP-GPS");
    device_create(gps_dev->gps_class, NULL, gps_dev->dev_num, NULL, "gps");
	
	gps_power_init();

	return 0;

	/* do with error */	
release_chrdev_region:
	unregister_chrdev_region(gps_dev->dev_num, 1);
		
release_io_map:
	if(gps_dev->regs)
		iounmap(gps_dev->regs);
	
	if(gps_dev->esram)
		iounmap(gps_dev->esram);
		
	if(gps_dev->ddr2)
		iounmap(gps_dev->esram);

	release_mem_region(esram_res->start, resource_size(esram_res));
	
release_regs_region:
	release_mem_region(regs_res->start, resource_size(regs_res));
	
release_mem:
	kfree(gps_dev);	

err_out:
	return ret;
}

static int sep_gps_remove(struct platform_device *pdev)
{
	struct resource *regs_res;
	struct resource *esram_res;
	
	if(!gps_dev)
		return 0;
		
    device_destroy(gps_dev->gps_class, gps_dev->dev_num);
    class_destroy(gps_dev->gps_class);

    cdev_del(&(gps_dev->cdev));
    unregister_chrdev_region(gps_dev->dev_num, 1);
	
	if(gps_dev->regs)
		iounmap(gps_dev->regs);
	
	if(gps_dev->esram)
		iounmap(gps_dev->esram);

	regs_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	esram_res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
		
	release_mem_region(regs_res->start, resource_size(regs_res));
	release_mem_region(esram_res->start, resource_size(esram_res));
    
	gps_dbg("%s\n", __func__);
	return 0;
}

static int sep_gps_suspend(struct platform_device *pdev, pm_message_t state)
{
	gps_dbg("%s\n", __func__);
#if 0	
	if(gps_running)
		stop_gps();
	
	if(gps_monitor){
        kthread_stop(gps_monitor);
        gps_monitor = NULL;
        gps_dbg("killed gps_monitor!\n");
    }
    
    if(gps_hardware_exit() != GPS_SUCCESS){
		printk("BUG:GPS maybe not stop!\n");
	}
	
	gps_store_cfg_data();
	gps_erase_firmware(gps_dev->esram, gps_dev->ddr2);
	gps_power_off();
#endif
	
	return 0;
}

static int sep_gps_resume(struct platform_device *pdev)
{
	gps_dbg("%s\n", __func__);
#if 0
	gps_power_init();
	gps_power_on();

	/* load esram.bin in ESRAM */
	gps_load_firmware(gps_dev->esram, gps_dev->ddr2);
	
	gps_load_cfg_data(); /* not care the results */
	
	if(gps_hardware_init() != GPS_SUCCESS){
		return -1;
	}
	gps_dbg("hardware init finish\n");

	gps_last_counter = 0;
	gps_monitor = kthread_create(gps_monitor_thread, reset_gps, "gps_monitor");
    if(IS_ERR(gps_monitor)){
        printk("Unable to create gps_monitor_thread!\n");
        gps_monitor = NULL;
    }
    else{
        wake_up_process(gps_monitor);
    }

	if(gps_running)
		start_gps();
#endif

	return 0;
}


static struct platform_driver sep_gps_driver = {
	.probe	= sep_gps_probe,
	.remove	= sep_gps_remove,
	.suspend= sep_gps_suspend,
	.resume	= sep_gps_resume,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "sep-gps", 
	},
};

static int __init sep_gps_init(void)
{
	return platform_driver_register(&sep_gps_driver);
}

static void __exit sep_gps_exit(void)
{
	platform_driver_unregister(&sep_gps_driver);
}

module_init(sep_gps_init);
module_exit(sep_gps_exit);

/* Module information */
MODULE_AUTHOR("chenguangming@wxseuic.com");
MODULE_DESCRIPTION("driver of GPS on SEP");
MODULE_LICENSE("GPL");
