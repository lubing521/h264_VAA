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
#define UP_MASK  0x00000040
#define DOWN_MASK  0x00000080
#define RIGHT_MASK  0x00000200
#define LEFT_MASK  0x0000100
#define SM1_SHIFT 7
#define SM2_MASK  0xFFFF87FF
#define SM2_SHIFT 11
#define STEPPERMOTOR_MAJOR 248
#define SM1_CONFIG_UP 1
#define SM1_CONFIG_DOWN 21
#define SM1_POWER 3
#define SM2_CONFIG_LEFT 4
#define SM2_CONFIG_RIGHT 24
#define SM2_POWER 5
#define LOW  0
#define HIGH  1
#define STEPPER_DELAY 1

typedef unsigned long    U32;     /* unsigned 32-bit integer */

typedef volatile U32 *   RP;

#define    write_reg(reg, data) \
    *(RP)(reg) = data
#define    read_reg(reg) \
    *(RP)(reg)

static void __iomem *base;
U32 sm_phase[]={0x7,0x3,0xB,0x9,0xD,0xC,0xE,0x6}; 
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
    int data,i;
    u32 tmp_reg,reg_gpio_i;
    stepper_dbg("cmd:%d;%s\n",cmd,__func__);
    switch (cmd)
    {
    case SM1_CONFIG_UP:
        {
            reg_gpio_i = read_reg(GPIO_PORTI_DATA_V);
            if((reg_gpio_i&UP_MASK) == 0)
            {
                return -EFAULT;
            }
            for (i=7;i>=0;i--)
            {
                tmp_reg = read_reg(GPIO_PORTF_DATA_V);
                tmp_reg &= SM1_MASK;
                tmp_reg = tmp_reg | (sm_phase[i] << SM1_SHIFT);
                write_reg(GPIO_PORTF_DATA_V,tmp_reg);
                msleep(STEPPER_DELAY);
            }
            break;
        }
    case SM1_CONFIG_DOWN:
        {
            reg_gpio_i = read_reg(GPIO_PORTI_DATA_V);
            if((reg_gpio_i&DOWN_MASK) == 0)
            {
                return -EFAULT;
            }
            for (i=0;i<8;i++)
            {
                tmp_reg = read_reg(GPIO_PORTF_DATA_V);
                tmp_reg &= SM1_MASK;
                tmp_reg = tmp_reg | (sm_phase[i] << SM1_SHIFT);
                write_reg(GPIO_PORTF_DATA_V,tmp_reg);
                msleep(STEPPER_DELAY);
            }
            break;
        }
    case SM1_POWER:
        {
            //printk("sm1_stop,set all port in\n");
            sep0611_gpio_setpin(SEP0611_PT_IN1,LOW);
            sep0611_gpio_setpin(SEP0611_PT_IN2,LOW);
            sep0611_gpio_setpin(SEP0611_PT_IN3,LOW);
            sep0611_gpio_setpin(SEP0611_PT_IN4,LOW);
            break;
        }
    case SM2_CONFIG_LEFT:
        {
            reg_gpio_i = read_reg(GPIO_PORTI_DATA_V);
            if((reg_gpio_i&LEFT_MASK) == 0)
            {
                return -EFAULT;
            }
            for (i=7;i>=0;i--)
            {
                tmp_reg = read_reg(GPIO_PORTF_DATA_V);
                tmp_reg &= SM2_MASK;
                tmp_reg = tmp_reg | (sm_phase[i] << SM2_SHIFT);
                write_reg(GPIO_PORTF_DATA_V,tmp_reg);
                msleep(STEPPER_DELAY);
            }
            break;
        }
    case SM2_CONFIG_RIGHT:
        {
            reg_gpio_i = read_reg(GPIO_PORTI_DATA_V);
            if((reg_gpio_i&RIGHT_MASK) == 0)
            {
                return -EFAULT;
            }
            for (i=0;i<8;i++)
            {
                tmp_reg = read_reg(GPIO_PORTF_DATA_V);
                tmp_reg &= SM2_MASK;
                tmp_reg = tmp_reg | (sm_phase[i] << SM2_SHIFT);
                write_reg(GPIO_PORTF_DATA_V,tmp_reg);
                msleep(STEPPER_DELAY);
            }
            break;
        }
    case SM2_POWER:
        {
            sep0611_gpio_setpin(SEP0611_PT_IN5,LOW);
            sep0611_gpio_setpin(SEP0611_PT_IN6,LOW);
            sep0611_gpio_setpin(SEP0611_PT_IN7,LOW);
            sep0611_gpio_setpin(SEP0611_PT_IN8,LOW);
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
    sep0611_gpio_cfgpin(SEP0611_TOY_STEP_UP_DET,SEP0611_GPIO_IO);
    sep0611_gpio_cfgpin(SEP0611_TOY_STEP_DOWN_DET,SEP0611_GPIO_IO);
    sep0611_gpio_cfgpin(SEP0611_TOY_STEP_LEFT_DET,SEP0611_GPIO_IO);
    sep0611_gpio_cfgpin(SEP0611_TOY_STEP_RIGHT_DET,SEP0611_GPIO_IO);
    /****************config as output*******************/
    sep0611_gpio_dirpin(SEP0611_TOY_STEP_UP_DET,SEP0611_GPIO_IN);
    sep0611_gpio_dirpin(SEP0611_TOY_STEP_DOWN_DET,SEP0611_GPIO_IN);
    sep0611_gpio_dirpin(SEP0611_TOY_STEP_LEFT_DET,SEP0611_GPIO_IN);
    sep0611_gpio_dirpin(SEP0611_TOY_STEP_RIGHT_DET,SEP0611_GPIO_IN);
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
