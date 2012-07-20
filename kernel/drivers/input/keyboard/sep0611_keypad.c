#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input-polldev.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <linux/platform_device.h>

#define DRV_NAME             "sep0611-keypad"

#define BUTTON_DELAY   msecs_to_jiffies(200) 

#define KEY_RELEASED    0
#define KEY_PRESSED     1

struct sep0611_button
{
    int             s_scancode;
    int             e_scancode;
    int             vkcode;
};

static struct sep0611_button sep0611_demoboard_buttons[] = {
	{83 , 103 , KEY_DOWN},  //sw3
	{337, 357, KEY_LEFT},//sw4
	{147 , 167, KEY_RIGHT},//sw5
	{571, 591, KEY_UP}, //sw6
	{20 , 40 , KEY_1}, //sw7
	{430, 450, KEY_ENTER},//sw8
};

struct sep0611_private
{
    	struct input_dev *input_dev;
    	struct input_polled_dev *poll_dev;
	struct platform_device *pdev;
    	int key_pressed;
   	 int old_key;
    	short status;
	int suspended;
};

struct sep0611_private        *sep0611_private;

#ifdef CONFIG_AD799X
extern int sep0611_read_ad7997(int channel);
#endif

int sep0611_keypad_readadc(int channel)
{
	#ifdef CONFIG_AD799X
		return sep0611_read_ad7997(channel);
	#else
		return -1;
	#endif
}

int sep0611_keypad_getkeycodebyscancode(unsigned int adcdata)
{
	int i;
	int key = -1;
	
	if(adcdata > 1024)
		key = -1;
	else
	{	
		for (i = 0; i < sizeof(sep0611_demoboard_buttons)/sizeof(struct sep0611_button); i++)
		{
			if ((adcdata >= sep0611_demoboard_buttons[i].s_scancode) && (adcdata <= sep0611_demoboard_buttons[i].e_scancode))
				key = sep0611_demoboard_buttons[i].vkcode;
		}
	}
	
	return key;
}

static void  sep0611_key_poll_callback(struct input_polled_dev *dev)
{
    int key = -1;
    int key_temp = -1;
    unsigned int value[2];

    if(sep0611_private->suspended == 1)
    	return;
	
    value[0] = (unsigned int)sep0611_keypad_readadc(1);
    key =  sep0611_keypad_getkeycodebyscancode(value[0]);

    //printk("key=%d/ status=%d/ old_key=%d\n", key, sep0611_private->status, sep0611_private->old_key);

    if(key >= 0){
        if(sep0611_private->old_key == key){
            sep0611_private->key_pressed = key;
            input_report_key(sep0611_private->poll_dev->input, sep0611_private->key_pressed, KEY_PRESSED);
            sep0611_private->status = KEY_PRESSED;
        } else {
        	if(sep0611_private->old_key == -1)
        	{
        	    value[1] =(unsigned int)sep0611_keypad_readadc(1);
                key_temp = sep0611_keypad_getkeycodebyscancode(value[1]);

                if(abs(value[1] - value[0]) < 10)
                {
                    sep0611_private->key_pressed = key;
                    input_report_key(sep0611_private->poll_dev->input, sep0611_private->key_pressed, KEY_PRESSED);
                    sep0611_private->status = KEY_PRESSED;
                 }else
                 {
                    input_report_key(sep0611_private->poll_dev->input, sep0611_private->key_pressed, KEY_RELEASED);
                    sep0611_private->key_pressed =  -1;
                    sep0611_private->status = KEY_RELEASED;
                 }
        	}else
        	{
        	    input_report_key(sep0611_private->poll_dev->input, sep0611_private->key_pressed, KEY_RELEASED);
        	    sep0611_private->status = KEY_RELEASED;
        	}
        }
    }else{ 
        if (sep0611_private->key_pressed >= 0)
        {
            input_report_key(sep0611_private->poll_dev->input, sep0611_private->key_pressed, KEY_RELEASED);
            sep0611_private->key_pressed =  -1;
            sep0611_private->status = KEY_RELEASED;
        }
    }

    input_sync(sep0611_private->poll_dev->input);
    sep0611_private->old_key = key;
}

static int __devinit sep0611_key_probe(struct platform_device *pdev)
{
    struct input_polled_dev *poll_dev; 
    struct input_dev *input_dev;
	
    int error;
    int  i;

    printk("sep0611_key_probe enter.\n");
	
    sep0611_private = kzalloc(sizeof(struct sep0611_private), GFP_KERNEL);
    poll_dev = input_allocate_polled_device();
    
    if (!sep0611_private || !poll_dev) {
        error = -ENOMEM;
        goto fail;
    }
        
    platform_set_drvdata(pdev, sep0611_private);
    
    poll_dev->private = sep0611_private;
    poll_dev->poll = sep0611_key_poll_callback;
    poll_dev->poll_interval = BUTTON_DELAY;

    input_dev = poll_dev->input;
    input_dev->evbit[0] = BIT(EV_KEY);
    input_dev->name = "SEP0611 demoboard keypad(use AD7997)";
    input_dev->phys = "SEP0611-keypad";
    input_dev->id.version = 0x1;

    for (i = 0; i < ARRAY_SIZE(sep0611_demoboard_buttons); i++)
        set_bit(sep0611_demoboard_buttons[i].vkcode & KEY_MAX, input_dev->keybit);
	
    sep0611_private->poll_dev    = poll_dev;
    sep0611_private->key_pressed = -1;
    sep0611_private->input_dev   = input_dev;
    sep0611_private->pdev        = pdev;
    sep0611_private->suspended = 0;

    input_register_polled_device(sep0611_private->poll_dev);

    printk("sep0611_key_probe done.\n");
   
    return 0;
    
fail:        
    kfree(sep0611_private);
    input_free_polled_device(poll_dev);
    return 0;
}

static int __devexit sep0611_key_remove(struct platform_device *pdev)
{
    input_unregister_polled_device(sep0611_private->poll_dev);
    kfree(sep0611_private);
    return 0;
}

#ifdef CONFIG_PM
static int sep0611_key_suspend(struct platform_device *pdev, pm_message_t state)
{
	printk("%s\n", __func__);
	sep0611_private->suspended = 1;
	
	return 0;
}

static int sep0611_key_resume(struct platform_device *pdev)
{
	printk("%s\n", __func__);
	sep0611_private->suspended = 0;
	
	return 0;
}

#else
#define sep0611_key_suspend NULL
#define sep0611_key_resume  NULL
#endif



static struct platform_driver sep0611_key_driver = {
       .driver         = {
	       .name   = "sep0611-keypad",
	       .owner  = THIS_MODULE,
       },
       .probe		= sep0611_key_probe,
       .remove	= sep0611_key_remove,
       .suspend	= sep0611_key_suspend,
       .resume	= sep0611_key_resume,
};

int __init sep0611_key_init(void)
{
	int res;
	
    	printk(KERN_INFO "SEP0611 ADC Keypad Driver(Use AD7997)\n");

	res =  platform_driver_register(&sep0611_key_driver);
	if(res)
	{
		printk("fail : platrom driver %s (%d) \n", sep0611_key_driver.driver.name, res);
		return res;
	}

	return 0;
}

void __exit sep0611_key_exit(void)
{
	platform_driver_unregister(&sep0611_key_driver);
}

module_init(sep0611_key_init);
module_exit(sep0611_key_exit);


MODULE_AUTHOR("lichun08@seuic.com");
MODULE_DESCRIPTION("SEP0611 ADC keypad driver");
MODULE_LICENSE("GPL");

