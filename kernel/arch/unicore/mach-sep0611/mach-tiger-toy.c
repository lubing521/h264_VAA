/* linux/arch/unicore/mach-sep0611/mach_sep0611.c
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
 *  09-04-2010	Changelog initial version
 *  05-31-2011	cgm Updated for release version
 *
 */

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/spi/spi.h>
#include <linux/spi/ad7879.h>
#include <linux/i2c.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/mtd.h>

#include <asm/io.h>
#include <asm/setup.h>
#include <asm/mach/arch.h>

#include <mach/hardware.h>
#include <mach/irqs.h>
#include <mach/devices.h>
#include <mach/nand.h>
#include <mach/leds.h>

#include <linux/gpio_keys.h>
#include <linux/input.h>
#include <board/board.h>
#include "../../../drivers/power/xpt2046_ts.h"

#define MACH_TYPE_DDB0611 2556

extern struct sys_timer sep0611_timer;
extern void sep0611_map_io(void);
extern void sep0611_init_irq(void);

static struct platform_device sep0611_device_battery = {
	.name   = "sep0611-battery",
	.id = -1,
};

/*****************SEP0611 GPIO keyboard support**********************************************/
static struct gpio_keys_button spdw_buttons[] = {
	[0] = {
		.gpio		= SEP0611_VOLUME_SET_INT,
		.code		= 139,
		.desc		= "volume",
		.active_low	= 1,
		.wakeup		= 0,
	},
};

static struct gpio_keys_platform_data spdw_button_data = {
	.buttons	= spdw_buttons,
	.nbuttons	= ARRAY_SIZE(spdw_buttons),
};

static struct platform_device sep0611_device_button = {
	.name	= "gpio-keys",
	.id		= -1,
	.dev	= {
		.platform_data	= &spdw_button_data,
	},
};
static struct sep0611_led_platdata sep0611_led1_pdata = {
	.name		= "LED_Statue",
	.gpio		= SEP0611_GPF19,
	.flags		= SEP0611_LEDF_ACTHIGH,
	.def_trigger	= "heartbeat",
};

static struct platform_device sep0611_device_led1 = {
	.name 	= "sep0611_led",
	.id		= 1,
	.dev	= {
		.platform_data	= &sep0611_led1_pdata,
	},
};

static struct xpt2046_platform_data xpt2046_info = {
       .model          = 2046,
       .keep_vref_on   = 1,
};

static struct spi_board_info board_spi1_devices[] = {
       {
               .modalias       = "xpt2046_ts",//
               .chip_select    = 0,
               .max_speed_hz   = 300000,// 125 * 1000 * 26,/* (max sample rate @ 3V) * (cmd + data + overhead) */
               .bus_num        = 0,
               .platform_data = &xpt2046_info,
       },
#if defined (CONFIG_SPI_SPIDEV)
       {
               .modalias       = "spidev",
               .chip_select    = 0,
               .bus_num        = 0,
	},
#endif
};

static struct mtd_partition __initdata sep0611_mtd_parts[] = {
        [0] = {
                .name   = "u-boot",
                .offset = 0,
                .size   = MTD_SIZE_1KiB * 896,
                .mask_flags = MTD_WRITEABLE,
        },

        [1] = {
                .name   = "env",
                .offset = MTDPART_OFS_NXTBLK,
                .size   = MTD_SIZE_1KiB * 128,
//                .mask_flags = MTD_WRITEABLE,
        },

        [2] = {
                .name   = "kernel",
                .offset = MTDPART_OFS_NXTBLK,
                .size   = MTD_SIZE_1MiB * 8,
//                .mask_flags = MTD_WRITEABLE,
        },

        [3] = {
                .name   = "root",
                .offset = MTDPART_OFS_NXTBLK,
                .size   = MTDPART_SIZ_FULL,
        },
};


static struct i2c_board_info __initdata sep0611_i2c1_board_info[]  = {
	[0] = {
		I2C_BOARD_INFO("CS3700", 0x1A),
	},
	[1] = {
		I2C_BOARD_INFO("sht20", 0x40),
	},
};

static struct resource sep0611_ch37x_resource[] = {
    [0] = {
        .start = SEP0611_CH37X_INTSRC,
        .end   = SEP0611_CH37X_INTSRC,
        .flags = IORESOURCE_IRQ | IORESOURCE_IRQ_LOWEDGE,
    },
};

static struct platform_device sep0611_device_ch37x = {
    .name   = "ch37x_hcd",
    .id     = -1,
    .num_resources  = ARRAY_SIZE(sep0611_ch37x_resource),
    .resource       = sep0611_ch37x_resource,
};

static struct platform_device *devices[] __initdata = 
{
	&sep0611_device_i2c1,
	&sep0611_device_rtc,
	&sep0611_device_nand,
	&sep0611_device_serial,
	&sep0611_device_mmc,
	&sep0611_device_sdio,
	&sep0611_device_spi1,
	&ch37x_spi_device,
	&sep0611_device_ch37x,
	&sep0611_device_battery,
    &sep0611_device_button,
	&sep0611_device_led1,
#ifdef CONFIG_ANDROID_PMEM
	&android_pmem_device,
	//&android_pmem_adsp_device,
	//&android_pmem_vpu_device,
#endif
	&sep0611_device_usb,
#ifdef CONFIG_USB_ANDROID
	&usb_mass_storage_device,
	&android_usb_device,
#endif
};

void __init sep0611_init(void)
{	
	SEP0611_INT_ENABLE(INTSRC_UART0);
	SEP0611_INT_ENABLE(INTSRC_UART1);
	SEP0611_INT_ENABLE(INTSRC_UART2);
	SEP0611_INT_ENABLE(INTSRC_UART3);

	platform_add_devices(devices, ARRAY_SIZE(devices));

 	spi_register_board_info(board_spi1_devices, ARRAY_SIZE(board_spi1_devices));	
	/* board infomation for I2C '1' */
	i2c_register_board_info(1, sep0611_i2c1_board_info, ARRAY_SIZE(sep0611_i2c1_board_info));

	sep0611_partitions.table = sep0611_mtd_parts;
	sep0611_partitions.num = ARRAY_SIZE(sep0611_mtd_parts);	
}

MACHINE_START(DDB0611, "SEP0611 board")
	.phys_io		= 0xB1000000,
	.io_pg_offst	= (SEP_IO_BASE_V >> 18) & 0xFFFC,
	.boot_params	= 0x40000100,
	.map_io			= sep0611_map_io,
	.init_irq		= sep0611_init_irq,
	.init_machine	= sep0611_init,
	.timer			= &sep0611_timer,
MACHINE_END

