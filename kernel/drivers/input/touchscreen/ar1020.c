/* linux/drivers/input/touchscreen/sep0611_ts.c
 *  
 *
 *  Copyright (C) 2010 SEU ASIC.
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
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/delay.h>

#include <asm/io.h>

#include <mach/hardware.h>
#include <mach/regs-spi.h>
#include <mach/irqs.h>

#include <board/board.h>
			
/* For ts->dev.id.version */
#define SEP_TSVERSION	0x0101

#define SPI_BASE 	SSI2_BASE
#define MAX_X  		800
#define MIN_X  		0
#define MAX_Y  		480
#define MIN_Y  		0
#define PEN_DOWN 	1
#define PEN_UP   	0
#define POT_NUM	 	5
#define RX_MAX  	3863
#define RY_MAX  	3898
#define NO_DATA 	0x4d
#define AR1020_DOWN	0x81
#define AR1020_UP  	0x80
#define PEN_DOWN_DEALY  20
#define PEN_DOWN_CONTINUE_DEALY  15

/* SEP0611 TS DEBUD SWITCH */
//#define _SEP0611_TS_DEBUGE

#ifdef _SEP0611_TS_DEBUGE
	#define ts_dbg(fmt, args...) printk(fmt, ##args)
#else
	#define ts_dbg(fmt, args...)
#endif

/* 触摸屏结构体 */
struct sep_ts{
	struct input_dev *dev;
	unsigned short zp;
	unsigned short xp;
	unsigned short yp;
	unsigned short down_flag;
	int zp_last;
	int xp_last;
	int yp_last;
	struct timer_list ts_timer;
	struct work_struct ts_wq;
	struct mutex mutex;
};

static struct sep_ts *ts;
static void __iomem  *base;

static unsigned short SendCommand(unsigned short ADCommand)
{	
	int data = 0;	

	writel(ADCommand, base + SSI_DR);

	udelay(200); /* wait for SPI data in */
	
	data = readl(base + SSI_DR);
	
	return data;  
}

#if 0

/*
 * ar1020_write_reg - write ar1020 reg
 * @addr: ar1020的寄存器便移地址
 * @data: 写入的数据
 */
static void ar1020_write_reg(unsigned short addr, unsigned short data)
{
	SendCommand(0x55);
	SendCommand(0x05);
	SendCommand(0x21);
	SendCommand(0x00);
	SendCommand(addr);
	SendCommand(0x01);
	SendCommand(data);
}

/*
 * ar1020_read_reg - read ar1020 reg
 * @addr: ar1020的寄存器便移地址
 */
static void ar1020_read_reg(unsigned short addr)
{
	SendCommand(0x55);
	SendCommand(0x04);
	SendCommand(0x20);
	SendCommand(0x00);
	SendCommand(addr);
	SendCommand(0x01);
}

#endif

/*
 * sep0611_ts_setup - 触摸屏初始化
 * 主要完成中断口的配置和spi寄存器的配置
 */
static void sep0611_ts_setup(void)
{
	disable_irq(SEP0611_TOUCH_INTSRC);
   	sep0611_gpio_setirq(SEP0611_TOUCH_INT, HIGH_TRIG);

	sep0611_gpio_cfgpin(SEP0611_GPE8, SSI2_RXD);
	sep0611_gpio_cfgpin(SEP0611_GPE9, SSI2_TXD);
	sep0611_gpio_cfgpin(SEP0611_GPE10, SSI2_CS);
	sep0611_gpio_cfgpin(SEP0611_GPE11, SSI2_CLK);

	/* spi寄存器的配置  */
	writel(0x0, base + SSI_ENR);
	writel(0x47, base + SSI_CONTROL0);
	writel(0x00, base + SSI_CONTROL1);
	writel(0x300, base + SSI_BAUDR);
	writel(0x1, base + SSI_TXFTLR);
	writel(0x0, base + SSI_RXFTLR);
	writel(0x0, base + SSI_DMACR);
	writel(0x0, base + SSI_IMR);
	writel(0x1, base + SSI_SER);
	writel(0x1, base + SSI_ENR);
	   	
	mdelay(2);	
      
	sep0611_gpio_clrirq(SEP0611_TOUCH_INT); /* 清除中断  */	
	enable_irq(SEP0611_TOUCH_INTSRC);
} 

/*
 * tsevent - 触摸事件产生后的处理函数
 * 主要完成坐标的转换和事件的报告
 */
static void tsevent(struct sep_ts *ts_data, struct input_dev *dev)
{
	unsigned short data[POT_NUM] = {0};
	
	writel(0x00, base + SSI_ENR);
	udelay(1);
	writel(0x01, base + SSI_ENR);
	
	data[0] = SendCommand(0x00);
	data[1] = SendCommand(0x00);
	data[2] = SendCommand(0x00);
	data[3] = SendCommand(0x00);
	data[4] = SendCommand(0x00);

	ts_data->zp = data[0];
	ts_data->xp = (((data[2]<<7)+data[1]))*MAX_X/RX_MAX;
	ts_data->yp = (((data[4]<<7)+data[3]))*MAX_Y/RY_MAX;

	if(data[0]==0 && data[1]==0 && data[2]==0 && data[3]==0 && data[4]==0){
		printk("ERROR:slave doesn't reponse\n");
		goto err_out;
	}
	else if (data[0] == NO_DATA){	
		ts_dbg("the invalue number \n");
		goto err_out;
	}
	
	ts_dbg("x:%d, y:%d, p:%x\n", ts_data->xp, ts_data->yp, ts_data->zp);
			
	if(ts_data->zp == AR1020_DOWN){		/* pen down */
		input_report_abs(ts_data->dev, ABS_X, ts_data->xp);
		input_report_abs(ts_data->dev, ABS_Y, ts_data->yp);
		input_report_key(ts_data->dev, BTN_TOUCH, PEN_DOWN);
		input_report_abs(ts_data->dev, ABS_PRESSURE, PEN_DOWN);
		input_sync(ts_data->dev);
		
		ts_data->zp_last = ts_data->zp;
		ts_data->xp_last = ts_data->xp;
		ts_data->yp_last = ts_data->yp;
		ts_data->zp = 0;
		ts_data->xp = 0;
		ts_data->yp = 0;
		ts_data->down_flag = 1;

		ts_data->ts_timer.expires = jiffies + msecs_to_jiffies(PEN_DOWN_CONTINUE_DEALY);
		add_timer(&ts->ts_timer);
	}
	else if(ts_data->zp == AR1020_UP){		/* pen up */
		input_report_abs(ts_data->dev, ABS_X, ts_data->xp);
  		input_report_abs(ts_data->dev, ABS_Y, ts_data->yp);
   		input_report_key(ts_data->dev, BTN_TOUCH, PEN_UP);
   		input_report_abs(ts_data->dev, ABS_PRESSURE, PEN_UP);
   		input_sync(ts_data->dev);
   		
   		ts_data->down_flag = 0;
   		
		SEP0611_INT_ENABLE(SEP0611_TOUCH_INTSRC);
	}
	else{
		goto err_out;
	}
	
	return;
	
err_out:
	if (ts_data->down_flag){
	   ts_data->xp = ts_data->xp_last;
	   ts_data->yp = ts_data->yp_last;
	   input_report_abs(ts_data->dev, ABS_X, ts_data->xp);
  	   input_report_abs(ts_data->dev, ABS_Y, ts_data->yp);
   	   input_report_key(ts_data->dev, BTN_TOUCH, PEN_UP);
   	   input_report_abs(ts_data->dev, ABS_PRESSURE, PEN_UP);
       input_sync(ts_data->dev);
       ts_data->down_flag = 0;
	}
	
	SEP0611_INT_ENABLE(SEP0611_TOUCH_INTSRC);
}

static void ts_wq_func(struct work_struct *work)
{
	struct sep_ts* ts_data = container_of(work, struct sep_ts, ts_wq);
	struct input_dev *dev = ts_data->dev;
	
	ts_dbg("%s\n", __func__);
	
	preempt_disable();
	tsevent(ts_data,dev);
	preempt_enable();
}

/*  
 *  定时器中断处理函数
 *  执行tsevent()函数
 */
static void ts_timer_handler(unsigned long arg)
{	
	struct sep_ts *ts_data = (struct sep_ts *)arg;
	
	ts_dbg("%s\n", __func__);
	
	if(schedule_work(&(ts_data->ts_wq)) == 0){
		SEP0611_INT_ENABLE(SEP0611_TOUCH_INTSRC);
	}
	else{
		SEP0611_INT_DISABLE(SEP0611_TOUCH_INTSRC);
	}
}

/*  
 *  触摸中断处理函数
 *  设定定时时间，添加定时器
 */
static irqreturn_t sep0611_ts_irqhandler(int irq, void *param)
{ 
	struct sep_ts *ts_data = (struct sep_ts *)param;
	
	ts_dbg("%s\n", __func__);
	
	SEP0611_INT_DISABLE(SEP0611_TOUCH_INTSRC);
	sep0611_gpio_clrirq(SEP0611_TOUCH_INT);/* 清除中断  */
	
	ts_data->ts_timer.expires = jiffies + msecs_to_jiffies(PEN_DOWN_DEALY);
	add_timer(&ts->ts_timer);
	
	return IRQ_HANDLED;
}

/*  
 *  驱动注册函数
 */
static int __init sep0611_ts_probe(struct platform_device *pdev)
{
	struct input_dev *input_dev;
	int err;
	
	ts = kzalloc(sizeof(struct sep_ts), GFP_KERNEL);	/* 申请内存 */
	base = ioremap(SPI_BASE, SZ_4K);
	if (base == NULL) {
		printk("base is null\n");
	}

	input_dev = input_allocate_device();	/* 创建一个input设备  */                                

	if (!input_dev) {
		err = -ENOMEM;
		goto fail;
	}

	ts->dev = input_dev;

	ts->dev->evbit[0] = ts->dev->evbit[0] = BIT_MASK(EV_SYN) | BIT_MASK(EV_KEY) | BIT_MASK(EV_ABS);
	ts->dev->keybit[BIT_WORD(BTN_TOUCH)] = BIT_MASK(BTN_TOUCH);

	input_set_abs_params(ts->dev, ABS_X, MIN_X, MAX_X, 0, 0);	/* 设定ABS_X, ABS_Y, ABS_PRESSURE的绝对坐标值  */ 
	input_set_abs_params(ts->dev, ABS_Y, MIN_Y, MAX_Y, 0, 0);
	input_set_abs_params(ts->dev, ABS_PRESSURE, PEN_UP, PEN_DOWN, 0, 0);

	ts->dev->name = "SEP0611 Touchscreen";
	ts->dev->id.bustype = BUS_RS232;
	ts->dev->id.vendor 	= 0xDEAD;
	ts->dev->id.product = 0xBEEF;
	ts->dev->id.version = SEP_TSVERSION;
	
	INIT_WORK(&(ts->ts_wq), ts_wq_func);
	mutex_init(&(ts->mutex));	
	init_timer(&ts->ts_timer);
	ts->ts_timer.data = (unsigned long)ts;
	ts->ts_timer.function = ts_timer_handler;

	sep0611_ts_setup();

	if(request_irq(SEP0611_TOUCH_INTSRC, sep0611_ts_irqhandler, IRQF_ONESHOT, "sep0611_ts", ts)){	/* 申请i/o中断 */ 
		printk("failed to request TouchScreen IRQ %d!\n", SEP0611_TOUCH_INTSRC);
		kfree(ts);
   
   		return -1;			
	}
	/* All went ok, so register to the input system */
	err = input_register_device(ts->dev);
	if(err) {
		free_irq(SEP0611_TOUCH_INTSRC, ts->dev);
		return -EIO;
	}

	return 0;

fail:	
	input_free_device(input_dev);
	kfree(ts);
	return err;	
}

static int sep0611_ts_remove(struct platform_device *dev)
{
	disable_irq(SEP0611_TOUCH_INTSRC);
	
	free_irq(SEP0611_TOUCH_INTSRC, ts->dev);
	input_unregister_device(ts->dev);
	
	return 0;
}


#ifdef CONFIG_PM
static int sep0611_ts_suspend(struct platform_device *dev, pm_message_t state)
{
	writel(0x00, base + SSI_ENR);
	disable_irq(SEP0611_TOUCH_INTSRC);
	
	return 0;
}

static int sep0611_ts_resume(struct platform_device *pdev)
{
	del_timer(&ts->ts_timer);
	setup_timer(&ts->ts_timer, ts_timer_handler, 0);
	
	sep0611_ts_setup();
	
	return 0;
}
#else

#define sep0611_ts_suspend NULL
#define sep0611_ts_resume NULL

#endif

static struct platform_driver sep0611_ts_driver = {
	.probe   = sep0611_ts_probe,
	.remove  = sep0611_ts_remove,
 	.suspend = sep0611_ts_suspend,
 	.resume  = sep0611_ts_resume,
 	.driver		= {
		.owner	= THIS_MODULE,
		.name	= "sep0611_ts",
	},
};

static char banner[] __initdata = KERN_INFO "SEP0611 Touchscreen driver v1.0, (c) 2010 SEU ASIC\n";

static int __init sep0611_ts_init(void)
{
	printk(banner);
	return platform_driver_register(&sep0611_ts_driver);
}

static void __exit sep0611_ts_exit(void)
{
	platform_driver_unregister(&sep0611_ts_driver);
}

module_init(sep0611_ts_init);
module_exit(sep0611_ts_exit);

MODULE_AUTHOR("zjw zhangjunwei166@163.com ");
MODULE_DESCRIPTION("sep0611 touchscreen driver");
MODULE_LICENSE("GPL");

