/* linux/drivers/char/tpm/sep0611_enable_irq60.c
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
#include <mach/irqs.h>

static int __init sep0611_enable_irq60_init(void)
{	
 	SEP0611_INT_ENABLE(INTSRC_GPU);

    return 0;
}

static void __exit sep0611_disable_irq60_exit(void)
{	
	SEP0611_INT_DISABLE(INTSRC_GPU);
}

module_init(sep0611_enable_irq60_init);
module_exit(sep0611_disable_irq60_exit);

MODULE_AUTHOR("xiaoyi");                         
MODULE_DESCRIPTION("enable_irq60");         
MODULE_LICENSE("GPL"); 
