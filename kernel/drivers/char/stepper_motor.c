/*
 *kernel/driver/char/stepper_motor.c
 *init this version 
 *1-7-2012 zjw zhangjunwei166@163.com
 *
 *add sm2 contorl 
 *7-19-2012 xjl xuejilong@foxmail.com
 *
 */

#include <linux/module.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/signal.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/irqs.h>
#include <board/board.h>



/* QIHUAN STEPPER MOTOR DEBUD SWITCH */
//#define _QIHUAN_STEPPER_DEBUGE

#ifdef _QIHUAN_STEPPER_DEBUGE
#define stepper_dbg(fmt, args...) printk(fmt, ##args)
#else
#define stepper_dbg(fmt, args...)
#endif
/* SM1_MASK (PF7~PF10) SM2_MASK (PF11~PF14)*/
#define SM1_MASK  0xFFFFF87F
#define SM1_SHIFT 7
#define SM2_MASK  0xFFFF87FF
#define SM2_SHIFT 11
#define STEPPERMOTOR_MAJOR 248
#define SM1_CONFIG 1
#define SM1_POWER 3
#define SM2_CONFIG 4
#define SM2_POWER 5
#define SM1_UP_IN_PLACE 6
#define SM1_DOWN_IN_PLACE 8
#define SM2_LEFT_IN_PLACE 9
#define SM2_RIGHT_IN_PLACE 10
#define LOW  0
#define HIGH  1

typedef unsigned long    U32;     /* unsigned 32-bit integer */

typedef volatile U32 *   RP;

#define    write_reg(reg, data) \
		*(RP)(reg) = data
#define    read_reg(reg) \
		*(RP)(reg)

static void __iomem *base;
char up_flag = 0;
char down_flag = 0;
char left_flag = 0;
char right_flag = 0;
#define STEP_DET_DELAY msecs_to_jiffies(20)

struct sep_steppermotor_dev{
		struct cdev cdev;
		struct class *steppermotor_class;
		dev_t dev_num;

		struct timer_list step_timer;
		struct work_struct step_wq;

		struct fasync_struct *async_queue;
};

//struct fasync_struct *async_queue_bk;
static struct sep_steppermotor_dev *steppermotor_dev = NULL;

static int stepper_fasync(int fd, struct file *filp, int mode)
{
		struct sep_steppermotor_dev *dev = filp->private_data;
		return fasync_helper(fd, filp, mode, &dev->async_queue);
}

static int sep_steppermotor_open(struct inode *inode, struct file *file)
{
		//int data;
		stepper_dbg("we are in sep_steppermotor_open before config\n");
		/********for steppermotor up&down  left&right*********/
		sep0611_gpio_cfgpin(SEP0611_PT_IN1,SEP0611_GPIO_IO);
		sep0611_gpio_cfgpin(SEP0611_PT_IN2,SEP0611_GPIO_IO);
		sep0611_gpio_cfgpin(SEP0611_PT_IN3,SEP0611_GPIO_IO);
		sep0611_gpio_cfgpin(SEP0611_PT_IN4,SEP0611_GPIO_IO);
		sep0611_gpio_cfgpin(SEP0611_PT_IN5,SEP0611_GPIO_IO);
		sep0611_gpio_cfgpin(SEP0611_PT_IN6,SEP0611_GPIO_IO);
		sep0611_gpio_cfgpin(SEP0611_PT_IN7,SEP0611_GPIO_IO);
		sep0611_gpio_cfgpin(SEP0611_PT_IN8,SEP0611_GPIO_IO);
		/****************config as output*******************/
		sep0611_gpio_dirpin(SEP0611_PT_IN1,SEP0611_GPIO_OUT);
		sep0611_gpio_dirpin(SEP0611_PT_IN2,SEP0611_GPIO_OUT);
		sep0611_gpio_dirpin(SEP0611_PT_IN3,SEP0611_GPIO_OUT);
		sep0611_gpio_dirpin(SEP0611_PT_IN4,SEP0611_GPIO_OUT);
		sep0611_gpio_dirpin(SEP0611_PT_IN5,SEP0611_GPIO_OUT);
		sep0611_gpio_dirpin(SEP0611_PT_IN6,SEP0611_GPIO_OUT);
		sep0611_gpio_dirpin(SEP0611_PT_IN7,SEP0611_GPIO_OUT);
		sep0611_gpio_dirpin(SEP0611_PT_IN8,SEP0611_GPIO_OUT);
		/************when init we set all low**************/	
		sep0611_gpio_setpin(SEP0611_PT_IN1,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN2,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN3,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN4,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN5,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN6,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN7,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN8,LOW);
		return 0;
}

static int sep_steppermotor_release(struct inode *inode, struct file *file)
{
		stepper_dbg("in sep_steppermotor_release\n");
		sep0611_gpio_setpin(SEP0611_PT_IN1,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN2,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN3,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN4,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN5,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN6,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN7,LOW);
		sep0611_gpio_setpin(SEP0611_PT_IN8,LOW);
		return 0;
}

static int sep_steppermotor_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg)
{
		int data;
		u32 DATA,i;
		stepper_dbg("cmd:%d;%s\n",cmd,__func__);
		switch (cmd)
		{
				case SM1_CONFIG:
						{
								if(get_user(data,(int __user *)arg))
										return -EFAULT;
								stepper_dbg("1data is :%d\n",data);
								DATA = (u32)data;
								i = read_reg(GPIO_PORTF_DATA_V);
								i &= SM1_MASK;
								DATA = i | (DATA << SM1_SHIFT);
								write_reg(GPIO_PORTF_DATA_V,DATA);
								break;
						}
				case SM1_POWER:
						{
								//printk("sm1_stop,set all port in\n");
								up_flag = 0;
								down_flag = 0;
								msleep(20);
								sep0611_gpio_setpin(SEP0611_PT_IN1,LOW);
								sep0611_gpio_setpin(SEP0611_PT_IN2,LOW);
								sep0611_gpio_setpin(SEP0611_PT_IN3,LOW);
								sep0611_gpio_setpin(SEP0611_PT_IN4,LOW);
								break;
						}
				case SM2_CONFIG:
						{
								if(get_user(data,(int __user *)arg))
										return -EFAULT;
								stepper_dbg("2data is :%d\n",data);
								DATA = (u32)data;	
								i = read_reg(GPIO_PORTF_DATA_V);
								i &= SM2_MASK;
								DATA = i | (DATA << SM2_SHIFT);
								write_reg(GPIO_PORTF_DATA_V,DATA);
								break;
						}
				case SM2_POWER:
						{
								left_flag = 0;
								right_flag = 0;
								msleep(20);
								sep0611_gpio_setpin(SEP0611_PT_IN5,LOW);
								sep0611_gpio_setpin(SEP0611_PT_IN6,LOW);
								sep0611_gpio_setpin(SEP0611_PT_IN7,LOW);
								sep0611_gpio_setpin(SEP0611_PT_IN8,LOW);
								break;
						}
				case SM1_UP_IN_PLACE:
						{
								stepper_dbg("\nup_flag:%d\n",up_flag);
								if(put_user(up_flag,(int __user *)arg))
										return -EFAULT;
								break;
						}
				case SM1_DOWN_IN_PLACE:
						{
								stepper_dbg("\ndown_flag:%d\n",down_flag);
								if(put_user(down_flag,(int __user *)arg))
										return -EFAULT;
								break;
						}
				case SM2_LEFT_IN_PLACE:
						{
								stepper_dbg("\nleft_flag:%d\n",left_flag);
								if(put_user(left_flag,(int __user *)arg))
										return -EFAULT;
								break;
						}
				case SM2_RIGHT_IN_PLACE:
						{
								stepper_dbg("\nright_flag:%d\n",right_flag);
								if(put_user(right_flag,(int __user *)arg))
										return -EFAULT;
								break;
						}
				default:
						return -EINVAL;
		}
		return 0;

}

static struct file_operations sep_steppermotor_fops={
		.owner = THIS_MODULE,
		.open = sep_steppermotor_open,
		.release = sep_steppermotor_release,
		.ioctl = sep_steppermotor_ioctl,
		//	.fasync = stepper_fasync,
};

# if 0
static void step_wq_func(struct work_struct *work)
{
		printk("%s\n",__func__);
		stepper_dbg("\nup_flag:%d;down_flag:%d\n",up_flag,down_flag);
		struct sep_steppermotor_dev* steppermotor_dev_0 = container_of(work, struct sep_steppermotor_dev, step_wq);
		up_flag = 1;
		down_flag = 0;
		//	if (steppermotor_dev_1->async_queue)
		//	{
		//		printk("**********************\n");
		//		kill_fasync(&steppermotor_dev_1->async_queue, SIGIO, POLL_IN);
		//	}
		SEP0611_INT_ENABLE(SEP0611_TOY_STEP_UP_DET_INTSRC);
}

static void step_timer_func(unsigned long arg)
{
		printk("FUNC:%s\n",__func__);
		struct sep_steppermotor_dev *steppermotor_dev_1 = (struct sep_steppermotor_dev *)arg;
		if (schedule_work(&(steppermotor_dev_1->step_wq)) == 0)
				SEP0611_INT_ENABLE(SEP0611_TOY_STEP_UP_DET_INTSRC);
		else
				SEP0611_INT_DISABLE(SEP0611_TOY_STEP_UP_DET_INTSRC);
}
#endif

static irqreturn_t stepper_up_irq(int irq, void *handle)
{
		printk("FUNC:%s\n",__func__);
		unsigned long  ret;
		SEP0611_INT_DISABLE(SEP0611_TOY_STEP_UP_DET_INTSRC);
		sep0611_gpio_clrirq(SEP0611_TOY_STEP_UP_DET);
		up_flag = 1;
		down_flag = 0;
		SEP0611_INT_ENABLE(SEP0611_TOY_STEP_UP_DET_INTSRC);
		return IRQ_HANDLED;
}

static irqreturn_t stepper_down_irq(int irq)
{
		printk("FUNC:%s\n",__func__);
		unsigned long ret;
		SEP0611_INT_DISABLE(SEP0611_TOY_STEP_DOWN_DET_INTSRC);
		sep0611_gpio_clrirq(SEP0611_TOY_STEP_DOWN_DET);	
		up_flag = 0;
		down_flag = 1;
		SEP0611_INT_ENABLE(SEP0611_TOY_STEP_DOWN_DET_INTSRC);
		return IRQ_HANDLED;
}
static irqreturn_t stepper_left_irq(int irq, void *handle)
{
		printk("FUNC:%s\n",__func__);
		unsigned long  ret;
		SEP0611_INT_DISABLE(SEP0611_TOY_STEP_LEFT_DET_INTSRC);
		sep0611_gpio_clrirq(SEP0611_TOY_STEP_LEFT_DET);
		left_flag = 1;
		right_flag = 0;
		SEP0611_INT_ENABLE(SEP0611_TOY_STEP_LEFT_DET_INTSRC);
		return IRQ_HANDLED;
}

static irqreturn_t stepper_right_irq(int irq)
{
		printk("FUNC:%s\n",__func__);
		unsigned long ret;
		SEP0611_INT_DISABLE(SEP0611_TOY_STEP_RIGHT_DET_INTSRC);
		sep0611_gpio_clrirq(SEP0611_TOY_STEP_RIGHT_DET);	
		left_flag = 0;
		right_flag = 1;
		SEP0611_INT_ENABLE(SEP0611_TOY_STEP_RIGHT_DET_INTSRC);
		return IRQ_HANDLED;
}
static irqreturn_t dart1_irq(int irq)
{
		//printk("FUNC:%s\n",__func__);
		unsigned long  ret;
/*        
		SEP0611_INT_DISABLE(SEP0611_TOY_DART1_DET_INTSRC);
		sep0611_gpio_clrirq(SEP0611_TOY_DART1_DET);
		sep0611_gpio_cfgpin(SEP0611_GPF10,SEP0611_GPIO_IO);
		sep0611_gpio_dirpin(SEP0611_GPF10,SEP0611_GPIO_OUT);
		sep0611_gpio_setpin(SEP0611_GPF10,LOW);
		SEP0611_INT_ENABLE(SEP0611_TOY_DART1_DET_INTSRC);
*/        
		return IRQ_HANDLED;
}
static int __init sep_steppermotor_init(void)
{
		int ret,rc;

		steppermotor_dev = kmalloc(sizeof(struct sep_steppermotor_dev), GFP_KERNEL);
		if(steppermotor_dev == NULL){
				printk(KERN_ERR "Failed to allocate stepper motor  device memory.\n");
				ret = -ENOMEM;
		}
		memset(steppermotor_dev,0,sizeof(struct sep_steppermotor_dev));
		steppermotor_dev->dev_num = MKDEV(STEPPERMOTOR_MAJOR,0);

		steppermotor_dev->async_queue = NULL;
		ret = register_chrdev(STEPPERMOTOR_MAJOR,"STEPPERMOTOR",&sep_steppermotor_fops);
		if (ret<0)
		{
				printk("register  steppermotor failed\n");
				return ret;
		}

		/* create /dev/steppermotor */
		steppermotor_dev->steppermotor_class = class_create(THIS_MODULE, "SEP-STEPPERMOTOR");
		device_create(steppermotor_dev->steppermotor_class, NULL, steppermotor_dev->dev_num, NULL, "steppermotor");

		/****here we will register step interrupt********/
		/*************stepper mtor up******************/
		SEP0611_INT_DISABLE(SEP0611_TOY_STEP_UP_DET_INTSRC);
		sep0611_gpio_setirq(SEP0611_TOY_STEP_UP_DET, DOWN_TRIG);
		mdelay(2);

		SEP0611_INT_ENABLE(SEP0611_TOY_STEP_UP_DET_INTSRC);

		rc = request_irq(SEP0611_TOY_STEP_UP_DET_INTSRC, stepper_up_irq,
						IRQF_TRIGGER_RISING, "stepper_up", NULL);
		if(rc)
		{
				printk("stepper up request irq failed!!!\n");
		}
		/*************stepper mtor down******************/
		SEP0611_INT_DISABLE(SEP0611_TOY_STEP_DOWN_DET_INTSRC);
		sep0611_gpio_setirq(SEP0611_TOY_STEP_DOWN_DET, DOWN_TRIG);
		mdelay(2);

		SEP0611_INT_ENABLE(SEP0611_TOY_STEP_DOWN_DET_INTSRC);

		rc = request_irq(SEP0611_TOY_STEP_DOWN_DET_INTSRC, stepper_down_irq,
						IRQF_TRIGGER_RISING, "stepper_up", NULL);
		if(rc)
		{
				printk("stepper down request irq failed!!!\n");
		}
		/*************stepper mtor left******************/
		SEP0611_INT_DISABLE(SEP0611_TOY_STEP_LEFT_DET_INTSRC);
		sep0611_gpio_setirq(SEP0611_TOY_STEP_LEFT_DET, DOWN_TRIG);
		mdelay(2);

		SEP0611_INT_ENABLE(SEP0611_TOY_STEP_LEFT_DET_INTSRC);

		rc = request_irq(SEP0611_TOY_STEP_LEFT_DET_INTSRC, stepper_left_irq,
						IRQF_TRIGGER_RISING, "stepper_left", NULL);
		if(rc)
		{
				printk("stepper left request irq failed!!!\n");
		}
		/*************stepper mtor right******************/
		SEP0611_INT_DISABLE(SEP0611_TOY_STEP_RIGHT_DET_INTSRC);
		sep0611_gpio_setirq(SEP0611_TOY_STEP_RIGHT_DET, DOWN_TRIG);
		mdelay(2);

		SEP0611_INT_ENABLE(SEP0611_TOY_STEP_RIGHT_DET_INTSRC);

		rc = request_irq(SEP0611_TOY_STEP_RIGHT_DET_INTSRC, stepper_right_irq,
						IRQF_TRIGGER_RISING, "stepper_right", NULL);
		if(rc)
		{
				printk("stepper right request irq failed!!!\n");
		}
		base = ioremap(GPIO_BASE,SZ_4K);
		if (base == NULL)
		{
				printk("base is null\n");
		}
		printk("stepper motor has registed\n");
		return ret;
}

static void __exit sep_steppermotor_exit(void)
{
		unregister_chrdev(STEPPERMOTOR_MAJOR,"SEP_STEPPERMOTOR");
		printk("unregister sep_steppermotor");
}

module_init(sep_steppermotor_init);
module_exit(sep_steppermotor_exit);


MODULE_AUTHOR("zjw zhangjunwei166@163.com ");
MODULE_DESCRIPTION("sep0611 steppermotor driver");
MODULE_LICENSE("GPL");
