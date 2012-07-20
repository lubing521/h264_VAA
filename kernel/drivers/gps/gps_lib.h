/*
 * linux/drivers/gps/gps_lib.h
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

#ifndef _GPS_LIB_H
#define _GPS_LIB_H

#define GPS_CMD_ON			0
#define GPS_CMD_OFF			1
#define GPS_SUCCESS			1
#define GPS_FAILED			0

#define GPS_DDR2_ADDR		0x41000000

#define _GPS_DEBUG			0

#if _GPS_DEBUG
	#define gps_dbg(fmt, args...) printk(fmt, ##args)
#else
	#define gps_dbg(fmt, args...)
#endif

typedef void (* gps_callback_t)(void);

extern unsigned int gps_last_counter;
extern unsigned int gps_running;

int gps_hardware_init(void);
int gps_hardware_exit(void);
int gps_monitor_thread(void *data);
int gps_load_firmware(void __iomem *esram, unsigned int ddr2);
void gps_erase_firmware(void __iomem *esram, unsigned int ddr2);
int gps_load_cfg_data(void);
int gps_store_cfg_data(void);

#endif
