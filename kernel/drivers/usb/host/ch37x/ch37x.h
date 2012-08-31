/*
 * CH37X HCD (SPI to USB Host Controller Driver)
 *
 * Copyright (C) 2012 SEUIC
 *
 * Author : cgm <chenguangming@wxseuic.com> 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifndef __CH37X_H__
#define __CH37X_H__

#include "ch37x_spi.h"

#define _DEV_CH370

#define LSMODE		(0)
#define FSMODE		(1)
#define HSMODE		(2)

#define CH37X_EP0_INVAL		0x0
#define CH37X_EP0_SETUP		0x1
#define CH37X_EP0_DATA		0x2
#define CH37X_EP0_STATUS	0x3

#define CH37X_MAX_NAK_CTRL		512
#define CH37X_MAX_NAK_OUT		4//512
#define CH37X_MAX_NAK_IN		10000
#define CH37X_MAX_ROOT_HUB		1
#define CH37X_MAX_DEVICE		1
#define CH37X_MAX_EPNUM			8
#define CH37X_RH_POLL_TIME		10

#define CH37X_MAX_PIPE_NUM		4
#define CTRL_PIPE				0
#define INT_PIPE				1
#define OUT_PIPE				2
#define IN_PIPE					3

struct ch37x_td {
	struct urb *urb;
	struct list_head queue;
	
	int iso_cnt;
	
	u16 maxpacket;
	u16 nak_times;

	u8 epnum;
	u8 pid;
	u8 ctrl_step;
	u8 address;		/* CH37X's USB address */
	u8 status;
	u8 length;
	u8 use_dma;
	u8 hold;

	u8 state;
	
	unsigned zero_packet:1;
	unsigned short_packet:1;
	unsigned set_address:1;
	unsigned again:1;
	unsigned tog:1;
};

struct ch37x_endpoint {
	struct list_head pipe_queue;
	unsigned int tog;
};

struct ch37x_device {
	u16	hub_port;
	u16	root_port;
	u8 speed;

	unsigned short ep_in_toggle;
	unsigned short ep_out_toggle;

	enum usb_device_state state;

	struct usb_device *udev;
	struct list_head device_list;
	int usb_address;
};

struct ch37x_root_hub {
	u32 port;
	struct ch37x_device	*dev;
};

struct ch37x {
	spinlock_t lock;
	struct ch37x_platdata	*pdata;
	struct ch37x_device		device0;
	struct ch37x_root_hub	root_hub[CH37X_MAX_ROOT_HUB];

	struct ch37x_td			*current_td;
	struct ch37x_endpoint ep0;
	struct ch37x_endpoint ep_in[CH37X_MAX_EPNUM];
	struct ch37x_endpoint ep_out[CH37X_MAX_EPNUM];
	unsigned int epnum_x2;
	atomic_t td_count;

	struct timer_list rh_timer;
	struct timer_list int_timer;
	struct timer_list hold_timer;
	struct work_struct work;
	struct tasklet_struct task;

	u8 speed;
	u8 status;
	u8 interval;
	u8 int_ticked;

	unsigned int max_root_hub;

	struct list_head child_device;
	unsigned long child_connect_map[4];

	unsigned bus_suspended:1;
	unsigned irq_sense_low:1;
};

static inline struct ch37x *hcd_to_ch37x(struct usb_hcd *hcd)
{
	return (struct ch37x *)(hcd->hcd_priv);
}

static inline struct usb_hcd *ch37x_to_hcd(struct ch37x *ch37x)
{
	return container_of((void *)ch37x, struct usb_hcd, hcd_priv);
}

static inline int ch37x_queue_empty(struct ch37x *ch37x)
{
	return atomic_read(&ch37x->td_count) == 0;
}

static inline struct urb *ch37x_get_urb(struct ch37x *ch37x)
{
	struct ch37x_td *td;
	td = ch37x->current_td;
	return (td ? td->urb : NULL);
}


static inline void ch37x_mdfy(u8 val, u8 pat, unsigned long offset)
{
	u8 tmp;
	tmp = ch37x_readb(offset);
	tmp = tmp & (~pat);
	tmp = tmp | val;
	ch37x_writeb(tmp, offset);
}

#define ch37x_bclr(ch37x, val, offset)	\
			ch37x_mdfy(ch37x, 0, val, offset)
#define ch37x_bset(ch37x, val, offset)	\
			ch37x_mdfy(ch37x, val, 0, offset)

#define ch37x_enable_irq_resume()	\
	ch37x_enable_irq(BIT_IE_USB_RESUME)
#define ch37x_disable_irq_suspend()	\
	ch37x_disable_irq(BIT_IE_USB_SUSPEND)
#define ch37x_enable_irq_detect()	\
	ch37x_enable_irq(BIT_IE_DEV_DETECT)
#define ch37x_disable_irq_detect()	\
	ch37x_disable_irq(BIT_IE_DEV_DETECT)
#define ch37x_enable_irq_transfer()	\
	ch37x_enable_irq(BIT_IE_TRANSFER)
#define ch37x_disable_irq_transfer()	\
	ch37x_disable_irq(BIT_IE_TRANSFER)
#define ch37x_enable_irq_all()	\
	ch37x_enable_irq(0x0F)
#define ch37x_disable_irq_all()	\
	ch37x_disable_irq(0x0F)

#endif	/* __CH37X_H__ */

