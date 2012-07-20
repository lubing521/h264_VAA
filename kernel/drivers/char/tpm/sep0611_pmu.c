/* linux/drivers/char/tpm/sep0611_pmu.c
 *
 * Copyright (c) 2009-2011 SEUIC
 *
 * Southeast University ASIC SoC support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Modifications:
 *  05-08-2010	Xiaoyi inital version
 *  06-07-2011	Zcj update version
 *  08-05-2011	cgm update version
 */
#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>

#include <asm/uaccess.h>

#include <mach/hardware.h>

static int __init sep0611_pmu_init(void)
{
    *(volatile unsigned long*)(PMU_PWR_GT_CFG_V) |= (0x1<<2);

	return 0;
}

static void __exit sep0611_pmu_exit(void)
{
	
    *(volatile unsigned long*)(PMU_PWR_GT_CFG_V) &= ~(0x1<<2);
}

module_init(sep0611_pmu_init);
module_exit(sep0611_pmu_exit);

MODULE_AUTHOR("xiaoyi");                         
MODULE_DESCRIPTION("pmu_set");         
MODULE_LICENSE("GPL"); 
