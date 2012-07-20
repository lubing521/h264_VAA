/*
 * linux/drivers/gps/gps_lib.c
 *
 * Author:  chenguangming@wxseuic.coom
 * Created: Nov 30, 2007
 * Description: On Chip GPS driver library for SEP
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
#include <linux/clk.h>

#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/irqs.h>

#include "gps_lib.h"

#define	GPS_ESRAM_BIN		"/etc/gps/esram.bin"
#define	GPS_LOW_MEM_BIN		"/etc/gps/low_mem.bin"
#define	GPS_CFG_FILE		"/etc/gps/gps.cfg"
#define GPS_BIN_MAX_SIZE	(96*1024UL)
#define READ_SIZE			1024
#define GPS_COUNTER_ADDR	0x41070000
#define GPS_STATE_ADDR		0x41070004
#define GPS_CFG_DATA_ADDR	0x41019000
#define GPS_CFG_DATA_SIZE	(320*1024UL)

#define GPS_PORT_SEL		(3<<6)

/* CLKGTCFG1 */
#define GPS_CTL_CLK_GATE	(1<<21)
#define UART1_CLK_GATE		(1<<26)

/* CLKGTCFG2 */
#define GPS_HCLK_GATE		(1<<8)
#define GPS_CORE_CLK_GATE	(1<<9)
#define ESRAM_CLK_GATE		(1<<10)

/* PWRGTCFG */
#define GPS_POWER_GATE		(1<<0)
#define ESRAM_POWER_GATE	(1<<3)

/* PWRSTATE */
#define GPS_PWR_MASK		0x0F
#define GPS_PWR_SHIFT		0
#define GPS_POWER_UP		0
#define GPS_POWER_DOWN		1

/* SYSCTRL */
#define GPS_SYS_CTRL_RAM	(3<<25)
#define GPS_SYS_CTRL_UART	(1<<27)
#define GPS_SYS_CTRL_REMAP	(7<<29)
#define GPS_REMAP_VALUE		(1<<29)

/* store the value of last counter */
unsigned int gps_last_counter = 0;
/* mark the gps running or stoped */
unsigned int gps_running = 0;


/*
 * connect the UART1 to GPS' inner UART
 */
static __inline void gps_uart_connect(void)
{
	writel((readl(SYS_CTRL_V) | GPS_SYS_CTRL_UART), SYS_CTRL_V);
	
	// To active the GPS UART, it is necessary
	writel(0x83, UART1_LCR_V);
	writel(0x03, UART1_LCR_V);

	gps_dbg("%s\n", __func__);
}

/*
 * disconnect the UART1 from GPS' inner UART
 */
static __inline void gps_uart_disconnect(void)
{	
	gps_dbg("%s\n", __func__);
	
	writel((readl(SYS_CTRL_V) & ~GPS_SYS_CTRL_UART), SYS_CTRL_V);
}

/*
 * use the onchip RAM for ESRAM
 */
static __inline void gps_ram_banding(void)
{	
	gps_dbg("%s\n", __func__);
	
	writel((readl(SYS_CTRL_V) | GPS_SYS_CTRL_RAM), SYS_CTRL_V);
}

/*
 * free the onchip RAM from ESRAM
 */
static __inline void gps_ram_disband(void)
{	
	gps_dbg("%s\n", __func__);
	
	writel((readl(SYS_CTRL_V) & ~GPS_SYS_CTRL_RAM), SYS_CTRL_V);
}

/*
 * PORTE[7]:GPS_MAG, PORTE[6]:GPS_SIGN
 */
static __inline void gps_init_gpio(void)
{
	writel(readl(GPIO_PORTE_SEL_V) & ~GPS_PORT_SEL, GPIO_PORTE_SEL_V);
}

/* 
 * open the power gate of GPS
 */
static __inline int gps_all_gate_open(void)
{
	unsigned long gate;
	unsigned long wait = 0;
	
	gate = readl(PMU_CLK_GT_CFG1_V);
	gate |= (GPS_CTL_CLK_GATE | UART1_CLK_GATE);
	writel(gate, PMU_CLK_GT_CFG1_V);

	gate = readl(PMU_CLK_GT_CFG2_V);
	gate |= (GPS_HCLK_GATE | GPS_CORE_CLK_GATE);
	writel(gate, PMU_CLK_GT_CFG2_V);
	
	gate = readl(PMU_PWR_GT_CFG_V);
	gate |= (GPS_POWER_GATE | ESRAM_POWER_GATE);
	writel(gate, PMU_PWR_GT_CFG_V);
	
	while(wait++ < 10){
		gate = readl(PMU_PWR_STATE_V);
		if(((gate & GPS_PWR_MASK) >> GPS_PWR_SHIFT) == GPS_POWER_UP)
			break;
		
		udelay(200);
	}

	if(wait >= 10)
		return GPS_FAILED;
	else
		return GPS_SUCCESS;
}

/* 
 * close the power gate of GPS
 */
static __inline int gps_all_gate_close(void)
{
	unsigned long gate;
	unsigned long wait = 0;
	
	gate = readl(PMU_CLK_GT_CFG1_V);
	gate &= ~(GPS_CTL_CLK_GATE | UART1_CLK_GATE);
	writel(gate, PMU_CLK_GT_CFG1_V);

	gate = readl(PMU_CLK_GT_CFG2_V);
	gate &= ~(GPS_HCLK_GATE | GPS_CORE_CLK_GATE);
	writel(gate, PMU_CLK_GT_CFG2_V);
	
	gate = readl(PMU_PWR_GT_CFG_V);
	gate &= ~GPS_POWER_GATE;
	writel(gate, PMU_PWR_GT_CFG_V);
	
	while(wait++ < 10){
		gate = readl(PMU_PWR_STATE_V);
		if(((gate & GPS_PWR_MASK) >> GPS_PWR_SHIFT) == GPS_POWER_DOWN)
			break;
		
		udelay(200);
	}
	
	if(wait >= 10)
		return GPS_FAILED;
	else
		return GPS_SUCCESS;
}

/* 
 * remap the esram to 0 address
 */
static __inline void esram_remap(void)
{
	unsigned long sysctrl;
	
	sysctrl = readl(SYS_CTRL_V);
	sysctrl &= ~GPS_SYS_CTRL_REMAP;
	sysctrl |= GPS_REMAP_VALUE;
	writel(sysctrl, SYS_CTRL_V);
}

/* 
 * unmap the esram from 0 address
 */
static __inline void esram_unmap(void)
{
	unsigned long sysctrl;
	
	sysctrl = readl(SYS_CTRL_V);
	sysctrl |= GPS_SYS_CTRL_REMAP;
	writel(sysctrl, SYS_CTRL_V);
}

/* 
 * load file(bin or config file) for GPS
 */
static int gps_load_file(const char * file_path, char * addr, unsigned long len)
{
	struct file *fd;
	mm_segment_t fs;
	loff_t pos;
	int count;
	char *p = addr;
	char *end = addr + len;
	
	if(!file_path)
		return -EINVAL;
	
	fd = filp_open(file_path, O_RDONLY, 0644);
	
	if(IS_ERR(fd))
		return -ENODEV;
	
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	do{
		count = vfs_read(fd, p, READ_SIZE, &pos);
		p += count;
		if(count < READ_SIZE)
			break;
	}while(p < end);

	filp_close(fd, NULL);
	set_fs(fs);

	return (p - addr);
}

/*
 * load firmware of GPS to ESRAM and DDR2
 */
int gps_load_firmware(void __iomem *esram, unsigned int ddr2)
{
	volatile unsigned int *p;
	unsigned int *buf;
	int bytes, words;
	int i;
	
	gps_dbg("%s\n", __func__);
	
	buf = kmalloc(GPS_BIN_MAX_SIZE, GFP_KERNEL);
	if(buf == NULL)
		return -ENOMEM;
	
	/* load esram.bin in ESRAM */
	bytes = gps_load_file(GPS_ESRAM_BIN, (char *)buf, GPS_BIN_MAX_SIZE);
	if(bytes < 0){
		printk("Failed to load %s.\n", GPS_ESRAM_BIN);
		return -ENOMEM;
	}
	
	words = bytes/4 + 1;
	p = (volatile unsigned int *)esram;
	for(i=0; i<words; i++){
		*p++ = buf[i];
	}		
	gps_dbg("gps esram bin = %d bytes\n", bytes);
	
	/* load low_mem.bin in DDR2 */
	bytes = gps_load_file(GPS_LOW_MEM_BIN, (char *)buf, GPS_BIN_MAX_SIZE);
	if(bytes < 0){
		printk("Failed to load %s.\n", GPS_LOW_MEM_BIN);
		return -ENOMEM;
	}
	
	words = bytes/4 + 1;
	p = (volatile unsigned int *)ddr2;
	for(i=0; i<words; i++){
		*p++ = buf[i];
	}
	gps_dbg("gps ddr2 bin = %d bytes\n", bytes);
	
	kfree(buf);
	
	return 0;
}

void gps_erase_firmware(void __iomem *esram, unsigned int ddr2)
{
	volatile unsigned int *p;
	int i;
	
	gps_dbg("%s\n", __func__);
	
	p = (volatile unsigned int *)esram;
	for(i=0; i<GPS_BIN_MAX_SIZE/4; i++){
		*p++ = 0;
	}
	
	p = (volatile unsigned int *)ddr2;
	for(i=0; i<GPS_BIN_MAX_SIZE/4; i++){
		*p++ = 0;
	}
}

/*
 * load config data of GPS to DDR2
 */
int gps_load_cfg_data(void)
{
	int bytes;
	volatile unsigned int *p;
	unsigned int *buf;
	int i;
	
	gps_dbg("%s\n", __func__);
	
	buf = kzalloc(GPS_CFG_DATA_SIZE, GFP_KERNEL);
	if(buf == NULL)
		return -ENOMEM;
	
	bytes = gps_load_file(GPS_CFG_FILE, (char *)buf, GPS_CFG_DATA_SIZE);
	if(bytes <= 0){
		/* if not find the cfg file, we still go on initialize the share memorary region */
		printk("not find gps config file, to create it.\n");
	}
	
	p = (volatile unsigned int *)GPS_CFG_DATA_ADDR;
	for(i=0; i<GPS_CFG_DATA_SIZE/4; i++){
		*p++ = buf[i];
	}
	
	kfree(buf);
	
	return 0;
}

/*
 * store config data of GPS
 */
int gps_store_cfg_data(void)
{
	volatile unsigned int *p;
	unsigned int *buf;
	struct file *fd;
	mm_segment_t fs;
	loff_t pos;
	int count;
	int i;
	
	gps_dbg("%s\n", __func__);
	
	buf = kmalloc(GPS_CFG_DATA_SIZE, GFP_KERNEL);
	if(buf == NULL)
		return -ENOMEM;
		
	p = (volatile unsigned int *)GPS_CFG_DATA_ADDR;
	for(i=0; i<GPS_CFG_DATA_SIZE/4; i++){
		buf[i] = *p++;
	}
	
	fd = filp_open(GPS_CFG_FILE, O_RDWR | O_CREAT, 0644);
	
	if(IS_ERR(fd))
		return -ENODEV;
	
	fs = get_fs();
	set_fs(KERNEL_DS);
	pos = 0;

	count = vfs_write(fd, (char *)buf, GPS_CFG_DATA_SIZE, &pos);
	if(count < GPS_CFG_DATA_SIZE){
		printk("store cfg file failed\n");
	}

	filp_close(fd, NULL);
	set_fs(fs);
	
	kfree(buf);
	
	return 0;
}

/*
 * initialize the hardware for GPS
 */
int gps_hardware_init(void)
{
	int ret;

	ret = gps_all_gate_open();
	if(ret == GPS_FAILED)
		return ret;

	msleep(1);
	
	gps_init_gpio();
	gps_uart_connect();
	gps_ram_banding();
	esram_remap();
	
	return ret;
}

/*
 * recover the initialized hardware of GPS
 */
int gps_hardware_exit(void)
{
	gps_uart_disconnect();
	gps_ram_disband();
	esram_unmap();

	return gps_all_gate_close();
}

/*
 * GPS monitor thread
 */
int gps_monitor_thread(void *data)
{
	unsigned int counter;
	gps_callback_t gps_rest = (gps_callback_t)data;
	
	msleep(1000);
	
	while(1)
    {
        set_current_state(TASK_UNINTERRUPTIBLE);
        if(kthread_should_stop()) 
            break;
        schedule_timeout(HZ);
        
        counter = *(volatile unsigned int *)GPS_COUNTER_ADDR;
       	
	 	if(gps_running){
	        if(counter == gps_last_counter){
	        	printk("GPS has stoped itself\n");
	        	if(gps_rest){
	        		gps_rest();
	        	}
	        }
	        else{
	        	gps_last_counter = counter;
	        	//gps_dbg("counter = %d\n", counter);
	        }
	    }
    }
    
    gps_dbg("gps monitor thread exit\n");
    
    return 0;
}

