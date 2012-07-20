 /* linux/arch/unicore/mach-sep0611/include/board/board-demo.h
 *
 * Copyright (c) 2009-2011 SEUIC
 *  cgm <chenguangming@wxseuic.com>
 *
 * Southeast University ASIC SoC support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Modifications:
 *  05-31-2011	cgm initial version
 *
 */

#ifndef __ASM_BOARD_DEMO_H
#define __ASM_BOARD_DEMO_H

#include <asm/arch/regs-gpio.h>

//#define SEP0611_DDR_PWREN		SEP0611_AO0
//#define SEP0611_DEVICE_IO		SEP0611_AO1
//#define SEP0611_HOLD_POWER		SEP0611_AO2

//#define SEP0611_ETH_CMD			SEP0611_GPA10

//#define SEP0611_VBUS_IN_SEL		SEP0611_GPC7
//#define SEP0611_HID_VAL			SEP0611_GPC6
//#define SEP0611_CHG_EN			SEP0611_GPC14

//#define SEP0611_ETH_RST_		SEP0611_GPD0
//#define SEP0611_USB_DE_SEL		SEP0611_GPD2
//#define SEP0611_VCORE_CTL		SEP0611_GPD3
//#define SEP0611_ETH_EN			SEP0611_GPD4
//#define SEP0611_HDMI_EN			SEP0611_GPD5
//#define SEP0611_HDMI_RST		SEP0611_GPD6
//#define SEP0611_LCD_EN			SEP0611_GPD7
//#define SEP0611_NAND_WP_		SEP0611_GPD8
//#define SEP0611_USB5V_EN		SEP0611_GPD9
//
//#define SEP0611_LCD_MODE		SEP0611_GPE8
//#define SEP0611_LCD_RST			SEP0611_GPE9
//#define SEP0611_WIFI_RST		SEP0611_GPE10
//#define SEP0611_WIFI_EN			SEP0611_GPE11
//#define SEP0611_DC5V_DETC		SEP0611_GPE12
//#define SEP0611_LCD_BL_EN		SEP0611_GPE13
//#define SEP0611_HVBUS_ON		SEP0611_GPE14
//#define SEP0611_USB_EN			SEP0611_GPE15
//#define SEP0611_USB_EN			SEP0611_AO1
//#define SEP0611_AUDIO_EN		SEP0611_GPE16
//#define SEP0611_SPK_CTL			SEP0611_GPE17
//
//#define SEP0611_OTG_VBUS_ON		SEP0611_GPF4
//
//#define SEP0611_HDMI_INT		SEP0611_GPI4
//#define SEP0611_ETH_INT			SEP0611_GPI6
//#define SEP0611_GSENSOR_INT		SEP0611_GPI7
//#define SEP0611_POWER_OK		SEP0611_GPI8
//#define SEP0611_CHG_OK			SEP0611_GPI9
#define SEP0611_PHY_RST			SEP0611_GPF0 //ULPI_nRST
#define SEP0611_USB_OTG_DRV			SEP0611_GPF5 //USB_OTG_DRV
//#define SEP0611_TOUCH_INT		SEP0611_GPI11
//#define SEP0611_ALT_BY			SEP0611_GPI12
//#define SEP0611_CONVST_			SEP0611_GPI13
//#define SEP0611_PWROFF_DET		SEP0611_GPI15
//
//#define SEP0611_HDMI_INTSRC		INTSRC_EXT4
//#define SEP0611_ETH_INTSRC		INTSRC_EXT6
//#define SEP0611_GSENSOR_INTSRC	INTSRC_EXT7
//#define SEP0611_TOUCH_INTSRC	INTSRC_EXT11
//
#endif /* __ASM_BOARD_DEMO_H */

