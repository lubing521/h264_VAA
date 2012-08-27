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
 * cgm 2012.4.11 create the file to support CH37x USB controller, v1.0
 * cgm 2012.6.27 update the version move the ch37x_work into irq handler, v1.1
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/timer.h>
#include <linux/delay.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/usb.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/mm.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <asm/cacheflush.h>

#include <mach/gpio.h>
#include <mach/regs-gpio.h>
#include <board/board.h>

#include "../../core/hcd.h"
#include "ch37x.h"
#include "ch374_hw.h"

#if 0
	#define ch37x_dbg(x...) printk(x)
#else
	#define ch37x_dbg(x...)
#endif

struct ch37x *ch37x;

static inline void start_timer6(void)
{
     writel(0x00, TIMER_T6CR_V);
     writel(0xFFFFFFFF, TIMER_T6LCR_V);
     writel(0x1B, TIMER_T6CR_V);
}

static inline int see_timer6(void)
{
	int tt;
	tt = readl(TIMER_T6CCR_V);
	tt = 0xFFFFFFFF - tt;
	return tt/240;
}

#define DRIVER_VERSION	"v1.1"

static const char hcd_name[] = "ch37x_hcd";

static int packet_write(struct ch37x *ch37x);
static int ch37x_get_frame(struct usb_hcd *hcd);
static void ch37x_work(struct ch37x *_ch37x);

/* this function must be called with interrupt disabled */
static void ch37x_enable_irq(u8 irq)
{
	u8 ie = ch37x_readb(REG_INTER_EN);
	ch37x_writeb(REG_INTER_EN, ie | irq);
}

/* this function must be called with interrupt disabled */
static void ch37x_disable_irq(u8 irq)
{
	u8 ie = ch37x_readb(REG_INTER_EN);
	ch37x_writeb(REG_INTER_EN, ie & ~irq);
}

static inline void ch37x_set_addr(u8 addr)
{
	ch37x_writeb(REG_USB_ADDR, addr);	
}

/* in full or high speed, free usb bus means D+ is keep high, D- is keep low, low speed reverse */
static void ch37x_bus_free(void)
{
	ch37x_writeb(REG_USB_SETUP, BIT_SETP_HOST_MODE);  // USB主机方式
	ch37x_writeb(REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF);  // USB主机方式,允许SOF
#ifdef _DEV_CH374
	ch37x_writeb(REG_HUB_SETUP, 0x00);  // 清BIT_HUB_DISABLE,允许内置的ROOT-HUB
	ch37x_writeb(REG_HUB_CTRL, 0x00);  	// 清除ROOT-HUB信息
#endif
}

static void ch37x_bus_reset(void)
{
	ch37x_set_addr(0);
	ch37x_writeb(REG_USB_H_CTRL, 0);
#ifdef _DEV_CH374
	//对于ch374系列，u型号有内置hub
	ch37x_writeb(REG_HUB_SETUP, BIT_HUB0_RESET);
	mdelay(15);
	ch37x_writeb(REG_HUB_SETUP, 0);  // 结束复位
	mdelay(1);
	ch37x_writeb(REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_USB_SUSPEND);  // 清中断标志
	ch37x_writeb(REG_HUB_SETUP, BIT_HUB0_EN );  // 使能HUB0端口
#endif

#ifdef _DEV_CH370
	//对于ch370系列，是没有内置hub的
	u8 usb_set;
	usb_set = ch37x_readb(REG_USB_SETUP);
	ch37x_writeb(REG_USB_SETUP, M_SET_USB_BUS_RESET(usb_set));
	mdelay(15);
	ch37x_writeb(REG_USB_SETUP, M_SET_USB_BUS_FREE(usb_set)); 
	ch37x_writeb(REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_USB_SUSPEND);  // 清中断标志
#endif
}

void ch37x_set_fullspeed( void )  // 设定全速USB设备运行环境
{
	ch37x_writeb(REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF );  // 全速且发SOF
#ifdef _DEV_CH374
	ch37x_writeb(REG_HUB_SETUP, BIT_HUB0_EN );  // 使能HUB0端口
#endif
}

void ch37x_set_lowspeed( void )  // 设定低速USB设备运行环境
{
	ch37x_writeb(REG_USB_SETUP, BIT_SETP_HOST_MODE | BIT_SETP_AUTO_SOF | BIT_SETP_LOW_SPEED );  // 低速且发SOF
#ifdef _DEV_CH374
	ch37x_writeb(REG_HUB_SETUP, BIT_HUB0_EN | BIT_HUB0_POLAR );  // 使能HUB0端口
#endif
}

static int enable_controller(void)
{
	ch37x_writeb(REG_USB_SETUP, 0);
	ch37x_set_addr(0);
	ch37x_writeb(REG_USB_H_CTRL, 0);
	ch37x_writeb(REG_INTER_FLAG, BIT_IF_USB_PAUSE | BIT_IF_INTER_FLAG);
	ch37x_writeb(REG_INTER_EN, BIT_IE_TRANSFER | BIT_IE_DEV_DETECT);
	ch37x_writeb(REG_SYS_CTRL, BIT_CTRL_OE_POLAR);

	ch37x_bus_free();

	return 0;
}

static void disable_controller(void)
{
	ch37x_writeb(REG_USB_SETUP, 0x82);
}

static void get_port_number(struct ch37x *ch37x,
			    char *devpath, u16 *root_port, u16 *hub_port)
{
	if (root_port) {
		*root_port = (devpath[0] & 0x0F) - 1;
		if (*root_port >= ch37x->max_root_hub)
			printk(KERN_ERR "ch37x: Illegal root port number.\n");
	}
	if (hub_port)
		*hub_port = devpath[2] & 0x0F;
}

static struct ch37x_device *get_urb_to_ch37x_dev(struct ch37x *ch37x, struct urb *urb)
{
	if (usb_pipedevice(urb->pipe) == 0)
		return &ch37x->device0;

	return dev_get_drvdata(&urb->dev->dev);
}

static int make_ch37x_device(struct ch37x *ch37x, struct urb *urb)
{
	struct ch37x_device *dev;
	int usb_address = urb->setup_packet[2];	/* urb->pipe is address 0 */

	ch37x_dbg("%s:%d\n", __func__, usb_address);

	dev = kzalloc(sizeof(struct ch37x_device), GFP_ATOMIC);
	if (dev == NULL)
		return -ENOMEM;

	dev_set_drvdata(&urb->dev->dev, dev);
	dev->udev = urb->dev;
	dev->usb_address = usb_address;
	dev->state = USB_STATE_ADDRESS;
	INIT_LIST_HEAD(&dev->device_list);
	list_add_tail(&dev->device_list, &ch37x->child_device);

	get_port_number(ch37x, urb->dev->devpath,
			&dev->root_port, &dev->hub_port);
	
	ch37x->root_hub[dev->root_port].dev = dev;

	return 0;
}

static void ch37x_urb_done(struct ch37x *ch37x, struct urb *urb, int status)
__releases(ch37x->lock)
__acquires(ch37x->lock)
{
	if (usb_pipein(urb->pipe) && usb_pipetype(urb->pipe) != PIPE_CONTROL) {
		void *ptr;

		for (ptr = urb->transfer_buffer;
		     ptr < urb->transfer_buffer + urb->transfer_buffer_length;
		     ptr += PAGE_SIZE)
			flush_dcache_page(virt_to_page(ptr));
	}

	usb_hcd_unlink_urb_from_ep(ch37x_to_hcd(ch37x), urb);
	spin_unlock(&ch37x->lock);
	usb_hcd_giveback_urb(ch37x_to_hcd(ch37x), urb, status);
	spin_lock(&ch37x->lock);
}

/* this function must be called with interrupt disabled */
static void force_dequeue(struct ch37x *ch37x, u16 address)
{
	struct ch37x_td *td, *next;
	struct urb *urb;
	struct list_head *list;
	int i;
	printk("%s\n", __func__);

	for(i=0; i<=CH37X_MAX_EPNUM*2; i++){
		if(i==0)
			list = &ch37x->ep0.pipe_queue;
		else if(i&1)
			list = &ch37x->ep_in[i].pipe_queue;
		else
			list = &ch37x->ep_out[i].pipe_queue;

		if (list_empty(list))
			continue;
	
		list_for_each_entry_safe(td, next, list, queue) {
			if (!td)
				continue;
			if (td->address != address)
				continue;
	
			urb = td->urb;
	
			if (urb)
				ch37x_urb_done(ch37x, urb, -ENODEV);

			list_del(&td->queue);
			kfree(td);
			atomic_dec(&ch37x->td_count);
	
			break;
		}
	}
}

/* this function must be called with interrupt disabled */
static void ch37x_usb_connect(struct ch37x *ch37x)
{
	struct ch37x_root_hub *rh = &ch37x->root_hub[0];

	if(rh->port & USB_PORT_STAT_CONNECTION)
		return;

	rh->port &= ~(USB_PORT_STAT_LOW_SPEED | USB_PORT_STAT_HIGH_SPEED | USB_PORT_STAT_ENABLE);
	rh->port |= USB_PORT_STAT_CONNECTION | (USB_PORT_STAT_C_CONNECTION << 16);

	if(ch37x->speed == LSMODE)
		rh->port |= USB_PORT_STAT_LOW_SPEED;
}

/* this function must be called with interrupt disabled */
static void ch37x_usb_disconnect(struct ch37x *ch37x)
{
	struct ch37x_root_hub *rh = &ch37x->root_hub[0];
	struct ch37x_device *dev = rh->dev;

	rh->port |= (1 << USB_PORT_FEAT_POWER) | (1 << USB_PORT_FEAT_C_CONNECTION);
	rh->port &= ~(USB_PORT_STAT_LOW_SPEED | USB_PORT_STAT_HIGH_SPEED | USB_PORT_STAT_ENABLE);

	ch37x_disable_irq_transfer();
	force_dequeue(ch37x, dev->usb_address);
}

/* this function must be called with interrupt disabled */
static int is_set_address(unsigned char *setup_packet)
{
	if (((setup_packet[0] & USB_TYPE_MASK) == USB_TYPE_STANDARD) &&
			setup_packet[1] == USB_REQ_SET_ADDRESS)
		return 1;
	else
		return 0;
}

int in_count;

/* schedule a TD in queue to transfer */
static struct ch37x_td *schedule_td(struct ch37x *_ch37x)
{
	struct ch37x_td *td = NULL, *hold_td = NULL;
	struct ch37x *ch37x = _ch37x;
	struct list_head *list;
	int flags, i;
	int epnum_x2, epnum;
	struct ch37x_endpoint *ep_in, *ep_out;
	
	ch37x_dbg("From ep%d ", ch37x->epnum_x2);
	spin_lock_irqsave(&ch37x->lock, flags);
	epnum_x2 = ch37x->epnum_x2;
	ep_in = ch37x->ep_in; 
	ep_out = ch37x->ep_out; 

	for(i=0; i<=CH37X_MAX_EPNUM*2; i++){
		if(++epnum_x2 > CH37X_MAX_EPNUM*2)
			epnum_x2 = 0;
		if(epnum_x2==0){
			ch37x_dbg("->ep%d ", epnum_x2);
			list = &ch37x->ep0.pipe_queue;
		}
		else if(epnum_x2 & 1){
			epnum = (epnum_x2 - 1)>>1;
			ch37x_dbg("->ep%d in", epnum + 1);
			list = &ep_in[epnum].pipe_queue;
		}
		else{
			epnum = (epnum_x2 - 1)>>1;
			ch37x_dbg("->ep%d out", epnum + 1);
			list = &ep_out[epnum].pipe_queue;
		}
		if(!(list_empty(list))){
			ch37x_dbg("...get\n");
			td = list_entry(list->next, struct ch37x_td, queue);

			if(td->hold){
				mod_timer(&ch37x->hold_timer, jiffies + msecs_to_jiffies(4)/2);
				//ch37x->current_td = NULL;
				td = NULL;
				continue;
			}

			break;
		}
	}
	if( td == NULL ) ch37x->current_td = NULL;
	ch37x->epnum_x2 = epnum_x2;
	spin_unlock_irqrestore(&ch37x->lock, flags);

	return td;
}

/* start a transfer */
static int start_transfer(struct ch37x *ch37x, struct ch37x_td *td)
{
	struct urb *urb;
	struct ch37x_td *tmp_td;

	ch37x_dbg("%s\n", __func__);

	tmp_td = schedule_td(ch37x);
	if(td == NULL && tmp_td == NULL){
		return -ENODEV;
	}
	
	if(td && tmp_td==NULL){
		return -ENODEV;
	}

	if(td != tmp_td){
		if(td != NULL){//change td
			td->again = 0;
			if(td->pid != DEF_USB_PID_IN) td->nak_times = 0;
		}
		
		tmp_td->again = 0;
		td = tmp_td;
	}

	if(td->again){
		goto go;
	}
	
	ch37x->current_td = td;

	switch(td->pid){
	case DEF_USB_PID_SETUP:
		urb = td->urb;
		if(is_set_address(urb->setup_packet)){
			td->set_address = 1;
			ch37x_dbg("host set device addr = %d\n", urb->setup_packet[2]);
		}
		ch37x_write_fifo_cpu(urb->setup_packet, 8);
		ch37x_writeb(REG_USB_LENGTH, 8);
		ch37x_writeb(REG_USB_ADDR, td->address);
		break;

	case DEF_USB_PID_OUT:
		if(td->ctrl_step == CH37X_EP0_STATUS){
			ch37x_writeb(REG_USB_LENGTH, 0);
		}
		else{
			if( td->epnum ) td->tog = ch37x->ep_out[td->epnum-1].tog;    
			packet_write(ch37x);
			return 0;
		}
		break;

	case DEF_USB_PID_IN:
		if( td->epnum ) td->tog = ch37x->ep_in[td->epnum-1].tog;    
        break;
	
	default:
		printk("should never be here!\n");
		ch37x->current_td = NULL;
		return -1;
	}

go:
	if(!td->again) ch37x_writeb(REG_USB_H_PID, M_MK_HOST_PID_ENDP(td->pid, td->epnum));
	td->state = 1;
	ch37x_writeb(REG_USB_H_CTRL, td->tog ? ( BIT_HOST_START | BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : BIT_HOST_START);
	
	return 0;
}

static int check_transfer_finish(struct ch37x_td *td, struct urb *urb)
{
	/* control or bulk or interrupt, no iso */
	if ((urb->transfer_buffer_length <= urb->actual_length) ||
	    (td->short_packet) || (td->zero_packet))
		return 1;

	return 0;
}

/* this function must be called with interrupt disabled */
static void finish_request(struct ch37x *ch37x, struct ch37x_td *td,
		struct urb *urb, int status)
{
	ch37x_dbg("%s\n", __func__);

	if (likely(td)) {
		list_del(&td->queue);
		kfree(td);
		atomic_dec(&ch37x->td_count);
		ch37x->current_td = NULL;
	}

	if (likely(urb)) {
		ch37x_urb_done(ch37x, urb, status);
	}

	ch37x_work(ch37x);
}

static void packet_skip(struct ch37x *ch37x)
{
	int rcv_len, size;
	u8 buf[64];

	/* prepare parameters */
	rcv_len = ch37x_readb(REG_USB_LENGTH);
	
	if(rcv_len == 0)
		return;

	if (rcv_len <= 64) {
		size = rcv_len;
		printk("skip %d\n",rcv_len);
	} else {
		size = 64;
		printk("buffer Overflow %d!\n",rcv_len - 64);
	}

	/* read fifo */
	ch37x_read_fifo_cpu(buf, size);
	
}

/* read a packet from ch37x's FIFO to urb buffer */
static void packet_read(struct ch37x *ch37x)
{
	int rcv_len, bufsize, urb_len, size;
	struct ch37x_td *td = ch37x->current_td;
	struct urb *urb;
	int status = 0;
	u8 *buf;

	if (unlikely(!td))
		return;
	urb = td->urb;

	/* prepare parameters */
	rcv_len = ch37x_readb(REG_USB_LENGTH);
	
	ch37x_dbg("%s:%dB\n", __func__, rcv_len);
	if(rcv_len == 0)
		goto out;

	buf = (void *)urb->transfer_buffer + urb->actual_length;
	urb_len = urb->transfer_buffer_length - urb->actual_length;
	bufsize = min(urb_len, (int) td->maxpacket);
	if (rcv_len <= bufsize) {
		size = rcv_len;
	} else {
		size = bufsize;
		status = -EOVERFLOW;
	}

	/* read fifo */
	ch37x_read_fifo_cpu(buf, size);
	
	/* update parameters */
	urb->actual_length += size;

out:
	if (rcv_len == 0)
		td->zero_packet = 1;
	else if (rcv_len < bufsize) 
		td->short_packet = 1;
}

static int out_again = 0;
/* write a packet to ch37x's FIFO from urb buffer */
static int packet_write(struct ch37x *ch37x)
{
	int bufsize, size;
	u8 *buf;
	struct ch37x_td *td = ch37x->current_td;
	struct urb *urb;

	if (unlikely(!td))
		return 0;

	urb = td->urb;

	ch37x_dbg("%s\n", __func__);

	/* prepare parameters */
	bufsize = td->maxpacket;
	buf = (u8 *)(urb->transfer_buffer + urb->actual_length);
	size = min_t(u32, bufsize, urb->transfer_buffer_length - urb->actual_length);

	if(size == 0){
		ch37x_writeb(REG_USB_LENGTH, 0);
		//ch37x_writeb(REG_USB_ADDR, td->address);
		ch37x_writeb(REG_USB_H_PID, M_MK_HOST_PID_ENDP(td->pid, td->epnum));
		td->state = 1;
		ch37x_writeb(REG_USB_H_CTRL, td->tog ? ( BIT_HOST_START | BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : BIT_HOST_START);
		return 0;
	}

	if(!out_again)
	{
		/* write fifo */
		td->state = 3;
		td->use_dma = 1;
		td->length = size;
		urb->actual_length += size;
		ch37x_write_fifo_dma(buf, size);
	}
	else
	{
		ch37x_writeb(REG_USB_LENGTH, size);
		ch37x_writeb(REG_USB_H_PID, M_MK_HOST_PID_ENDP(td->pid, td->epnum));
		td->state = 1;
		urb->actual_length += size;
		ch37x_writeb(REG_USB_H_CTRL, td->tog ? ( BIT_HOST_START | BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : BIT_HOST_START);
	}
	return size;
}

/* check the next phase of control transfer */
static void check_ep0_next_phase(struct ch37x *ch37x, int status)
{
	struct ch37x_td *td = ch37x->current_td;
	struct urb *urb;
	u8 finish = 0;

	ch37x_dbg("%s\n", __func__);

	if (unlikely(!td))
		return;
	urb = td->urb;

	if(status == -EAGAIN && td->nak_times < CH37X_MAX_NAK_CTRL){
		td->nak_times++;
		td->again = 1;
		status = 0;
		goto next;
	}
	else if(status){
		printk("===========err:%d, nak:%d=========status:%x\n", status, td->nak_times, td->status);
		goto next;
	}

	switch (td->ctrl_step) {
	case CH37X_EP0_DATA:
		if (check_transfer_finish(td, urb)){
			td->ctrl_step = CH37X_EP0_STATUS;
			if(usb_pipein(urb->pipe))
				td->pid = DEF_USB_PID_OUT;
			else
				td->pid = DEF_USB_PID_IN;
			td->tog = true;
			ch37x_dbg("ep0 data finish\n");
		}
		else{
			td->tog = !(td->tog);
			ch37x_dbg("ep0 data continue, %d/%d\n", urb->actual_length, urb->transfer_buffer_length);
		}
		
		break;

	case CH37X_EP0_SETUP:
		if (check_transfer_finish(td, urb)){
			td->pid = DEF_USB_PID_IN;
			td->ctrl_step = CH37X_EP0_STATUS;
			ch37x_dbg("ep0 setup ack:no data\n");
		}
		else{
			if (usb_pipeout(urb->pipe))
				td->pid = DEF_USB_PID_OUT;
			else
				td->pid = DEF_USB_PID_IN;
			
			td->ctrl_step = CH37X_EP0_DATA;
			ch37x_dbg("ep0 setup ack:req=%d, act=%d\n", urb->transfer_buffer_length, urb->actual_length);
		}
		td->tog = !(td->tog);
		break;

	case CH37X_EP0_STATUS:
		ch37x_dbg("ep0 status ack\n");
		if(td->set_address)
			make_ch37x_device(ch37x, urb);
		finish = 1;
		break;
	} 
	td->nak_times = 0;
	td->again = 0;

next:
	if (finish || status != 0 || urb->unlinked)
		finish_request(ch37x, td, urb, status);
	else
		start_transfer(ch37x, td);
}

/* check the next phase of non_control transfer */
static void check_next_phase(struct ch37x *ch37x, int status)
{
	struct ch37x_td *td = ch37x->current_td;
	struct ch37x_endpoint *ep;
	struct urb *urb;
	int pipe, max_nak;
	u8 finish = 0;

	ch37x_dbg("%s\n", __func__);

	if (unlikely(!td))
		return;
	urb = td->urb;
	pipe = urb->pipe;

	if(usb_pipein(pipe)){
		max_nak = CH37X_MAX_NAK_IN;
		ep = &ch37x->ep_in[td->epnum - 1];
	}
	else{
		max_nak = CH37X_MAX_NAK_OUT;
		ep = &ch37x->ep_out[td->epnum - 1];
	}

	if(status == -EAGAIN && td->nak_times < max_nak){
		td->nak_times++;
		if(usb_pipein(pipe) && td->nak_times > 4){
			td->hold = 1;
		}
		else{
			td->again = 1;
		}
		status = 0;
		goto next;
	}
	else if(status){
		printk("ep%d--pid=%d--nak=%d--status:%x\n", td->epnum, td->pid, td->nak_times, td->status);
		goto next;
	}

	if (check_transfer_finish(td, urb)){
		//printk("finish(%d):%d\n",td->tog,urb->actual_length);
		ep->tog = !ep->tog;
		finish = 1;
	}
	else{
//longn_qi
		//printk("data(%d):%d\n",td->tog,urb->actual_length);
		ep->tog = !ep->tog;
		td->tog = ep->tog;
		//td->tog = !td->tog;
	}
	
	td->nak_times = 0;
	td->again = 0;

next:
	if (finish || status != 0 || urb->unlinked)
		finish_request(ch37x, td, urb, status);
	else
		start_transfer(ch37x, td);
}

static void ch37x_root_hub_start_polling(struct ch37x *ch37x)
{
	mod_timer(&ch37x->rh_timer,
			jiffies + msecs_to_jiffies(CH37X_RH_POLL_TIME));
}

static void ch37x_root_hub_control(struct ch37x *ch37x, int port)
{
	u8 info;
	struct ch37x_root_hub *rh = &ch37x->root_hub[port];

	ch37x_dbg("%s\n", __func__);

	if (rh->port & (1 << USB_PORT_FEAT_RESET)) {
		info = ch37x_readb(REG_SYS_INFO);
		if(!(info & BIT_INFO_POWER_RST)){
			ch37x_root_hub_start_polling(ch37x);
		} 
		else{
			rh->port &= ~(1 << USB_PORT_FEAT_RESET);
			rh->port |= (1 << USB_PORT_FEAT_ENABLE);
			ch37x_dbg("port reset finish, rh->port=0x%08x\n", rh->port);
		}
	}
}

static void ch37x_timer(unsigned long _ch37x)
__releases(ch37x->lock) __acquires(ch37x->lock)
{
	struct ch37x *ch37x = (struct ch37x *)_ch37x;
	unsigned long flags;
	int port;

	spin_lock_irqsave(&ch37x->lock, flags);

	for (port = 0; port < ch37x->max_root_hub; port++)
		ch37x_root_hub_control(ch37x, port);

	spin_unlock_irqrestore(&ch37x->lock, flags);
}

static void ch37x_int_timer(unsigned long _ch37x)
__releases(ch37x->lock) __acquires(ch37x->lock)
{
	struct ch37x *ch37x = (struct ch37x *)_ch37x;
	unsigned long flags;
	int interval;

	spin_lock_irqsave(&ch37x->lock, flags);

	ch37x->int_ticked = 1;
	interval = ch37x->interval;
	if(interval < 8)
		interval = 8;

	spin_unlock_irqrestore(&ch37x->lock, flags);

	mod_timer(&ch37x->int_timer, jiffies + msecs_to_jiffies(interval));
}

static void ch37x_hold_timer(unsigned long _ch37x)
{
	struct ch37x_td *td = NULL;
	struct ch37x *ch37x = _ch37x;
	struct list_head *list;
	int i;
	struct ch37x_endpoint *ep_in;
	
	ep_in = ch37x->ep_in; 

	for(i=0; i<CH37X_MAX_EPNUM; i++){
		list = &ep_in[i].pipe_queue;

		if(!(list_empty(list))){
			td = list_entry(list->next, struct ch37x_td, queue);
			if(td->hold)
				td->hold = 0;
		}
	}

	if(ch37x->current_td == NULL)
		start_transfer(ch37x, NULL);
}

/* ch37x work entry */
static void ch37x_work(struct ch37x *_ch37x)
{
	struct ch37x *ch37x = _ch37x;
	struct ch37x_td *td = ch37x->current_td;
	struct ch37x_endpoint *ep;
	struct urb *urb;
	int pipe, err = -EPIPE, epnum;
	u8 status, r;
	static int in_cnt=0,out_cnt=0;

	ch37x_dbg("%s\n", __func__);

	if(!td){
		start_transfer(ch37x, NULL);
		return;
	}

	urb = td->urb;
	epnum = td->epnum;
	if(urb->unlinked){
		printk("urb unlinked\n");
		return;
	}

	pipe = urb->pipe;
	status = ch37x_readb(REG_USB_STATUS);
	r = status & BIT_STAT_DEV_RESP;

	switch(td->pid){
		case DEF_USB_PID_SETUP:
		case DEF_USB_PID_OUT:
			if(r == DEF_USB_PID_ACK)
			{	
				err = 0; 
				out_again = 0;
				if(epnum) out_cnt++;
				if(!(status & BIT_STAT_TOG_MATCH)){
					printk("ep %d out %d no match, tog %d\n",epnum, out_cnt,td->tog);
					err = -EAGAIN;
					out_again = 1;
				}
			}
			else if(r == DEF_USB_PID_NAK)
			{
				err = -EAGAIN;
				out_again = 1;
			}
			else
				printk("unkown usb out packet response!\n");

			if( td->state != 1 )
				printk("err td state %d-1!\n", td->state);
			td->state = 2;

			break;

		case DEF_USB_PID_IN:
			if( td->state != 1 )
				printk("err td state %d-1!\n", td->state);
			td->state = 2;
			if(M_IS_HOST_IN_DATA(status)){
				if(!(status & BIT_STAT_TOG_MATCH)){
					ep = &ch37x->ep_in[epnum-1];
					//longn_qi
					//ep->tog = !ep->tog;
					printk("ep %d in %d no match, tog %d\n",epnum, in_cnt,td->tog);
					err = -EAGAIN;
					packet_skip(ch37x);
					//err = 0;
					//packet_read(ch37x);
				}
				else
				{
					err = 0;
					packet_read(ch37x);
					in_cnt++;
				}
			}
			else if(r == DEF_USB_PID_NAK)
				err = -EAGAIN;
			break;

		default:	//impossible
			printk("should never be here. line %d, pid=%d, ep=%d\n", __LINE__, td->pid, td->epnum);
			break;
	}

	td->status = status;

	if(usb_pipecontrol(pipe))
		check_ep0_next_phase(ch37x, err);
	else
		check_next_phase(ch37x, err);
}

/* the IRQ handler of ch37x */
static irqreturn_t ch37x_irq(struct usb_hcd *hcd)
{
	struct ch37x *ch37x = hcd_to_ch37x(hcd);
	u8 status, setup, intr;

	intr = ch37x_readb(REG_INTER_FLAG);
	status = ch37x_readb(REG_USB_STATUS);

	ch37x_writeb(REG_INTER_FLAG, BIT_IF_USB_SUSPEND | BIT_IF_USB_PAUSE | BIT_IF_DEV_DETECT | BIT_IF_TRANSFER);	//clear flag
	sep0611_gpio_clrirq(SEP0611_CH37X_INT);

	if((intr & BIT_IF_DEV_DETECT) && !(status & BIT_STAT_BUS_RESET)){
		if(intr & BIT_IF_DEV_ATTACH){
			printk("dev attch:");

			setup = ch37x_readb(REG_USB_SETUP);
			if(intr & BIT_IF_USB_DX_IN){
				if(setup & BIT_SETP_USB_SPEED){
					//low speed device
					ch37x->speed = LSMODE;
					printk("low speed\n");
				}
				else{
					// full speed device
					ch37x->speed = FSMODE;
					printk("full speed\n");
				}
			}
			else{	//need change speed
				if(setup & BIT_SETP_USB_SPEED){
					//full speed device
					ch37x->speed = FSMODE;
					ch37x_set_fullspeed();
					printk("change to full speed\n");
				}
				else{
					// low speed device
					ch37x->speed = LSMODE;
					ch37x_set_lowspeed();
					printk("change to low speed\n");
				}
			}
			ch37x_usb_connect(ch37x);
		}
		else {
			printk("dev dettch\n");

			ch37x_usb_disconnect(ch37x);
		}
	}
	else if(intr & BIT_IF_TRANSFER){
		ch37x_work(ch37x);
	}

	return IRQ_HANDLED;
}

int ch37x_start(struct usb_hcd *hcd)
{
	hcd->state = HC_STATE_RUNNING;
	return enable_controller();
}

void ch37x_stop(struct usb_hcd *hcd)
{
	disable_controller();
}

/* make a transaction description binding with urb to describe a transaction */
static struct ch37x_td *ch37x_make_td(struct ch37x *ch37x,
					    struct urb *urb,
					    struct usb_host_endpoint *hep)
{
	struct ch37x_td *td;
	struct ch37x_endpoint *ep;
	unsigned int pipe;
	unsigned int epnum;

	td = kzalloc(sizeof(struct ch37x_td), GFP_ATOMIC);
	if (td == NULL)
		return NULL;

	td->urb = urb;
	pipe = urb->pipe;
	td->address = usb_pipedevice(pipe);
	td->maxpacket = usb_maxpacket(urb->dev, pipe,
				      !usb_pipein(pipe));
	td->tog = false;
	td->state = 0;

	epnum = usb_pipeendpoint(pipe);
	if(epnum > CH37X_MAX_EPNUM){
		printk("over the ch37x max epnum %d!!!!\n", CH37X_MAX_EPNUM);
		return NULL;
	}

	td->epnum = epnum;

	if(usb_pipeint(pipe)){
		printk("make int TD, becareful!we don't surpport it well!\n");
	}

	if(usb_pipeisoc(pipe)){
		printk("make iso TD, we don't support it at all!");
		kfree(td);
		return NULL;
	}

	INIT_LIST_HEAD(&td->queue);

	if (usb_pipecontrol(pipe)){
		td->pid = DEF_USB_PID_SETUP;
		td->ctrl_step = CH37X_EP0_SETUP;
		list_add_tail(&td->queue, &ch37x->ep0.pipe_queue);
	}
	else if (usb_pipein(pipe)){
		td->pid = DEF_USB_PID_IN;
		ep = &ch37x->ep_in[epnum - 1];
//longn_qi
		//td->tog = ep->tog;
		//ep->tog = 0;
		list_add_tail(&td->queue, &ch37x->ep_in[epnum].pipe_queue);
	}
	else{
		td->pid = DEF_USB_PID_OUT;
		ep = &ch37x->ep_out[epnum - 1];
//longn_qi
		//td->tog = ep->tog;
		//ep->tog = 0;
		list_add_tail(&td->queue, &ch37x->ep_out[epnum].pipe_queue);
	}

	ch37x_dbg("make TD:addr=%d, pid=%d, ep=%d, tog=%d\n", td->address, td->pid, td->epnum, td->tog);
	atomic_inc(&ch37x->td_count);

	return td;
}

static int ch37x_urb_enqueue(struct usb_hcd *hcd, struct urb *urb, gfp_t mem_flags)
__releases(ch37x->lock) __acquires(ch37x->lock)
{
	struct usb_host_endpoint *hep = urb->ep;
	struct ch37x *ch37x = hcd_to_ch37x(hcd);
	struct ch37x_td *td = NULL;
	int ret;
	unsigned long flags;
	int no_another;

	ch37x_dbg("%s\n", __func__);

	spin_lock_irqsave(&ch37x->lock, flags);
	if (!get_urb_to_ch37x_dev(ch37x, urb)) {
		ret = -ENODEV;
		goto error_not_linked;
	}

	ret = usb_hcd_link_urb_to_ep(hcd, urb);
	if (ret)
		goto error_not_linked;
	
	//is there another TD in any queue
	no_another = ch37x_queue_empty(ch37x);

	td = ch37x_make_td(ch37x, urb, hep);
	if (td == NULL) {
		ret = -ENOMEM;
		goto error;
	}
	
	urb->hcpriv = td;

	//we just start tranfer in this function when no another
	if (no_another){
		ch37x_dbg("no one td in queue\n");
		ch37x->current_td = NULL;
		ch37x_work(ch37x);
	}

error:
	if (ret)
		usb_hcd_unlink_urb_from_ep(hcd, urb);
error_not_linked:
	spin_unlock_irqrestore(&ch37x->lock, flags);
	return ret;
}

static int ch37x_urb_dequeue(struct usb_hcd *hcd, struct urb *urb, int status)
__releases(ch37x->lock) __acquires(ch37x->lock)
{
	struct ch37x *ch37x = hcd_to_ch37x(hcd);
	struct ch37x_td *td;
	unsigned long flags;
	int rc;

	printk("%s\n", __func__);

	spin_lock_irqsave(&ch37x->lock, flags);
	rc = usb_hcd_check_unlink_urb(hcd, urb, status);
	if (rc)
		goto done;

	if (urb->hcpriv) {
		td = urb->hcpriv;
		finish_request(ch37x, td, urb, status);
	}
 done:
	spin_unlock_irqrestore(&ch37x->lock, flags);
	return rc;
}

static int ch37x_get_frame(struct usb_hcd *hcd)
{
	return 0;
}

static void ch37x_endpoint_disable(struct usb_hcd *hcd, struct usb_host_endpoint *hep)
__releases(ch37x->lock) __acquires(ch37x->lock)
{
	struct ch37x *ch37x = hcd_to_ch37x(hcd);
	struct ch37x_pipe *pipe = (struct ch37x_pipe *)hep->hcpriv;
	struct ch37x_td *td;
	struct urb *urb = NULL;
	unsigned long flags;

	if (pipe == NULL)
		return;

	spin_lock_irqsave(&ch37x->lock, flags);
	ch37x_disable_irq_all();
	td = ch37x->current_td; 
	if (td)
		urb = td->urb;
	finish_request(ch37x, td, urb, -ESHUTDOWN);
	kfree(hep->hcpriv);
	hep->hcpriv = NULL;
	spin_unlock_irqrestore(&ch37x->lock, flags);
}

/* this function must be called with interrupt disabled */
static struct ch37x_device *get_ch37x_device(struct ch37x *ch37x, int addr)
{
	struct ch37x_device *dev;
	struct list_head *list = &ch37x->child_device;

	list_for_each_entry(dev, list, device_list) {
		if (!dev)
			continue;
		if (dev->usb_address != addr)
			continue;

		return dev;
	}

	ch37x_dbg(KERN_ERR "ch37x: get_ch37x_device fail.(%d)\n", addr);
	return NULL;
}

static int ch37x_hub_status_data(struct usb_hcd *hcd, char *buf)
{
	struct ch37x *ch37x = hcd_to_ch37x(hcd);
	unsigned long flags;
	int i;

	spin_lock_irqsave(&ch37x->lock, flags);

	*buf = 0;	/* initialize (no change) */

	for (i = 0; i < ch37x->max_root_hub; i++) {
		if (ch37x->root_hub[i].port & 0xffff0000)
			*buf |= 1 << (i + 1);
	}

	spin_unlock_irqrestore(&ch37x->lock, flags);

//	printk("%s:buf=0x%x\n", __func__, *buf);

	return (*buf != 0);
}

static void ch37x_hub_descriptor(struct ch37x *ch37x, struct usb_hub_descriptor *desc)
{
	desc->bDescriptorType = 0x29;
	desc->bHubContrCurrent = 0;
	desc->bNbrPorts = ch37x->max_root_hub;
	desc->bDescLength = 9;
	desc->bPwrOn2PwrGood = 0;
	desc->wHubCharacteristics = cpu_to_le16(0x0011);
	desc->bitmap[0] = ((1 << ch37x->max_root_hub) - 1) << 1;
	desc->bitmap[1] = ~0;
}

static int ch37x_hub_control(struct usb_hcd *hcd, u16 typeReq, u16 wValue,
				u16 wIndex, char *buf, u16 wLength)
{
	struct ch37x *ch37x = hcd_to_ch37x(hcd);
	int ret = 0;
	int port = (wIndex & 0x00FF) - 1;
	struct ch37x_root_hub *rh = &ch37x->root_hub[port];
	unsigned long flags;

	ch37x_dbg("%s, reqtype=0x%04x, code=0x%04x, port=%d.\n", __func__, typeReq, wValue, port);

	spin_lock_irqsave(&ch37x->lock, flags);
	switch (typeReq) {
	case ClearHubFeature:
	case SetHubFeature:
		switch (wValue) {
		case C_HUB_OVER_CURRENT:
		case C_HUB_LOCAL_POWER:
			break;
		default:
			goto error;
		}
		break;
	case ClearPortFeature:
		if (wIndex > ch37x->max_root_hub)
			goto error;
		if (wLength != 0)
			goto error;

		switch (wValue) {
		case USB_PORT_FEAT_ENABLE:
			rh->port &= ~(1 << USB_PORT_FEAT_POWER);
			break;
		case USB_PORT_FEAT_SUSPEND:
			break;
		case USB_PORT_FEAT_POWER:
			ch37x_bus_free();
			break;
		case USB_PORT_FEAT_C_ENABLE:
		case USB_PORT_FEAT_C_SUSPEND:
		case USB_PORT_FEAT_C_CONNECTION:
		case USB_PORT_FEAT_C_OVER_CURRENT:
		case USB_PORT_FEAT_C_RESET:
			break;
		default:
			goto error;
		}
		rh->port &= ~(1 << wValue);
		break;
	case GetHubDescriptor:
		ch37x_hub_descriptor(ch37x, (struct usb_hub_descriptor *)buf);
		break;
	case GetHubStatus:
		*buf = 0x00;
		break;
	case GetPortStatus:
		if (wIndex > ch37x->max_root_hub)
			goto error;

		*(__le32 *)buf = cpu_to_le32(rh->port);
		break;
	case SetPortFeature:
		if (wIndex > ch37x->max_root_hub)
			goto error;
		if (wLength != 0)
			goto error;

		switch (wValue) {
		case USB_PORT_FEAT_SUSPEND:
			break;
		case USB_PORT_FEAT_POWER:
			printk("USB_PORT_FEAT_POWER\n");
			rh->port |= (1 << USB_PORT_FEAT_POWER);
			ch37x_bus_free();
			break;
		case USB_PORT_FEAT_RESET: 

			printk("USB_PORT_FEAT_RESET\n");
			ch37x_bus_reset();

			mod_timer(&ch37x->rh_timer, jiffies + msecs_to_jiffies(10));

			break;
		default:
			goto error;
		}
		rh->port |= 1 << wValue;
		break;
	default:
error:
		ret = -EPIPE;
		break;
	}

	spin_unlock_irqrestore(&ch37x->lock, flags);
	return ret;
}

#if defined(CONFIG_PM)
static int ch37x_bus_suspend(struct usb_hcd *hcd)
{
	struct ch37x *ch37x = hcd_to_ch37x(hcd);
	int port;

	dbg("%s", __func__);

	for (port = 0; port < ch37x->max_root_hub; port++) {
		struct ch37x_root_hub *rh = &ch37x->root_hub[port];

		if (!(rh->port & (1 << USB_PORT_FEAT_ENABLE)))
			continue;

		dbg("suspend port = %d", port);
		rh->port |= 1 << USB_PORT_FEAT_SUSPEND;
	}

	ch37x->bus_suspended = 1;

	return 0;
}

static int ch37x_bus_resume(struct usb_hcd *hcd)
{
	struct ch37x *ch37x = hcd_to_ch37x(hcd);
	int port;

	dbg("%s", __func__);

	for (port = 0; port < ch37x->max_root_hub; port++) {
		struct ch37x_root_hub *rh = &ch37x->root_hub[port];

		if (!(rh->port & (1 << USB_PORT_FEAT_SUSPEND)))
			continue;

		dbg("resume port = %d", port);
		rh->port &= ~(1 << USB_PORT_FEAT_SUSPEND);
		rh->port |= 1 << USB_PORT_FEAT_C_SUSPEND;
	}

	return 0;

}
#else
#define	ch37x_bus_suspend	NULL
#define	ch37x_bus_resume	NULL
#endif

struct hc_driver ch37x_hc_driver = {
	.description 		= hcd_name,
	.hcd_priv_size		= sizeof(struct ch37x),
	.irq 				= ch37x_irq,

	/*
	 * generic hardware linkage
	 */
	.flags				= HCD_USB2,

	.start				= ch37x_start,
	.stop				= ch37x_stop,

	/*
	 * managing i/o requests and associated device resources
	 */
	.urb_enqueue 		= ch37x_urb_enqueue,
	.urb_dequeue		= ch37x_urb_dequeue,
	.endpoint_disable 	= ch37x_endpoint_disable,

	/*
	 * periodic schedule support
	 */
	.get_frame_number	= ch37x_get_frame,

	/*
	 * root hub support
	 */
	.hub_status_data 	= ch37x_hub_status_data,
	.hub_control 		= ch37x_hub_control,
	.bus_suspend 		= ch37x_bus_suspend,
	.bus_resume			= ch37x_bus_resume,
};

#if defined(CONFIG_PM)
static int ch37x_suspend(struct device *dev)
{
	struct ch37x *ch37x = dev_get_drvdata(dev);
	int port;

	dbg("%s", __func__);

	disable_controller();

	for (port = 0; port < ch37x->max_root_hub; port++) {
		struct ch37x_root_hub *rh = &ch37x->root_hub[port];

		rh->port = 0x00000000;
	}

	return 0;
}

static int ch37x_resume(struct device *dev)
{
	struct ch37x *ch37x = dev_get_drvdata(dev);
	struct usb_hcd *hcd = ch37x_to_hcd(ch37x);

	dbg("%s", __func__);

	enable_controller();
	usb_root_hub_lost_power(hcd->self.root_hub);

	return 0;
}

struct dev_pm_ops ch37x_dev_pm_ops = {
	.suspend	= ch37x_suspend,
	.resume		= ch37x_resume,
	.poweroff	= ch37x_suspend,
	.restore	= ch37x_resume,
};

#define CH37X_DEV_PM_OPS	(&ch37x_dev_pm_ops)
#else	/* if defined(CONFIG_PM) */
#define CH37X_DEV_PM_OPS	NULL
#endif

static int __init_or_module ch37x_remove(struct platform_device *pdev)
{
	struct ch37x *ch37x = dev_get_drvdata(&pdev->dev);
	struct usb_hcd *hcd = ch37x_to_hcd(ch37x);

	del_timer_sync(&ch37x->rh_timer);
	usb_remove_hcd(hcd);
	usb_put_hcd(hcd);

	return 0;
}

static int __devinit ch37x_probe(struct platform_device *pdev)
{
	struct resource *res = NULL, *ires;
	int irq = -1;
	void __iomem *reg = NULL;
	struct usb_hcd *hcd = NULL;
	int ret = 0;
	int i;
	unsigned long irq_trigger;

	ires = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!ires) {
		ret = -ENODEV;
		dev_err(&pdev->dev,
			"platform_get_resource IORESOURCE_IRQ error.\n");
		goto clean_up;
	}

	irq = ires->start;
	irq_trigger = ires->flags & IRQF_TRIGGER_MASK;

	/* initialize hcd */
	hcd = usb_create_hcd(&ch37x_hc_driver, &pdev->dev, (char *)hcd_name);
	if (!hcd) {
		ret = -ENOMEM;
		dev_err(&pdev->dev, "Failed to create hcd\n");
		goto clean_up;
	}
	ch37x = hcd_to_ch37x(hcd);
	memset(ch37x, 0, sizeof(struct ch37x));
	dev_set_drvdata(&pdev->dev, ch37x);
	ch37x->pdata = pdev->dev.platform_data;
	ch37x->irq_sense_low = irq_trigger == IRQF_TRIGGER_LOW;
	ch37x->max_root_hub = 1;

	spin_lock_init(&ch37x->lock);
	init_timer(&ch37x->rh_timer);
	ch37x->rh_timer.function = ch37x_timer;
	ch37x->rh_timer.data = (unsigned long)ch37x;

	/* make sure no interrupts are pending */
	enable_controller();

	if(((ch37x_readb(REG_SYS_INFO)) & 0x3) == 0x1)
		printk("Detect ch37x chip successful.\n");
	else
		printk("Fail to read ch37x's ID!!!!!!!!!!!!!!!!!!\n");

	INIT_LIST_HEAD(&ch37x->ep0.pipe_queue);
	ch37x->ep0.tog = false;
	for (i = 0; i < CH37X_MAX_EPNUM; i++) {
		INIT_LIST_HEAD(&ch37x->ep_in[i].pipe_queue);
		INIT_LIST_HEAD(&ch37x->ep_out[i].pipe_queue);
		ch37x->ep_in[i].tog = false;
		ch37x->ep_out[i].tog = false;
	}
	init_timer(&ch37x->int_timer);
	ch37x->int_timer.function = ch37x_int_timer;
	ch37x->int_timer.data = (unsigned long)ch37x;

	init_timer(&ch37x->hold_timer);
	ch37x->hold_timer.function = ch37x_hold_timer;
	ch37x->hold_timer.data = (unsigned long)ch37x;

	hcd->rsrc_start = res->start;

	ret = usb_add_hcd(hcd, irq, irq_trigger);
	if (ret != 0) {
		dev_err(&pdev->dev, "Failed to add hcd\n");
		goto clean_up3;
	}

	//do something to set GPIO irq
	SEP0611_INT_DISABLE(SEP0611_CH37X_INTSRC);
	sep0611_gpio_clrirq(SEP0611_CH37X_INT);
	sep0611_gpio_cfgpin(SEP0611_CH37X_INT, SEP0611_GPIO_IO);
	sep0611_gpio_dirpin(SEP0611_CH37X_INT, SEP0611_GPIO_IN);
	sep0611_gpio_setirq(SEP0611_CH37X_INT, LOW_TRIG);
	SEP0611_INT_ENABLE(SEP0611_CH37X_INTSRC);

	return 0;

clean_up3:
	usb_put_hcd(hcd);

clean_up:
	if (reg)
		iounmap(reg);

	return ret;
}

extern struct platform_driver ch37x_spi_driver; 

static struct platform_driver ch37x_driver = {
	.probe =	ch37x_probe,
	.remove =	ch37x_remove,
	.driver	= {
		.name = (char *) hcd_name,
		.owner	= THIS_MODULE,
	},
};

static int __init ch37x_init(void)
{
	int ret;

	if (usb_disabled())
		return -ENODEV;

	printk(KERN_INFO KBUILD_MODNAME "(SPI to USB Controller) driver %s\n", DRIVER_VERSION);

	ret = platform_driver_register(&ch37x_spi_driver);
	if(ret < 0)
		return -ENODEV;

	return platform_driver_register(&ch37x_driver);
}
module_init(ch37x_init);

static void __exit ch37x_cleanup(void)
{
	platform_driver_unregister(&ch37x_spi_driver);
	platform_driver_unregister(&ch37x_driver);
}

MODULE_DESCRIPTION("CH37X USB Host Controller Driver");
MODULE_LICENSE("GPL");
MODULE_AUTHOR("cgm <chenguangming@wxseuic.com>");
MODULE_ALIAS("platform:ch37x_hcd");
