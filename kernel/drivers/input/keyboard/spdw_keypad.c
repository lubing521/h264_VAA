/* linux/drivers/input/keyboard/spdw_keypad.c
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
 *  07-08-2011	cgm initial version
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pm.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/gpio_keys.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/delay.h>

#include <asm/irq.h>
#include <asm/io.h>

#include <mach/hardware.h>
#include <mach/gpio.h>
#include <mach/regs-gpio.h>

#include <board/board.h>

#define KEY_DELAY       msecs_to_jiffies(100)
#define KEY_CONTINUE    msecs_to_jiffies(500)
#define KEY_RELEASED    0
#define KEY_PRESSED     1

#define _KEY_DEBUG		0

#if _KEY_DEBUG
	#define key_dbg(fmt, args...) printk(fmt, ##args)
#else
	#define key_dbg(fmt, args...)
#endif

struct gpio_key_data {
	struct gpio_keys_button *button;
	struct input_dev *input;
	struct timer_list timer;
	struct work_struct work;
	unsigned int state;
};

struct gpio_keys_dev {
	struct input_dev *input;
	struct gpio_key_data datas[0];
};

struct gpio_keys_dev *keys_dev;

static bool button_first = true;
static void gpio_check_button(unsigned long data)
{
	struct gpio_key_data *key_data = (struct gpio_key_data *)data;
	struct gpio_keys_button *button = key_data->button;
	unsigned long down_flag;

	/**********************************************
		low_active		gpio		down_flag
		1				1			0
		1				0			1
		0				1			1
		0				0			0
	 **********************************************/
	down_flag = (button->active_low) ^ (sep0611_gpio_getpin(button->gpio));
	if(down_flag){
        if(button_first){
            button_first = false;
        }
        key_data->state = KEY_PRESSED;
		input_report_key(key_data->input, button->code, KEY_PRESSED);
		/*mod_timer(&key_data->timer, jiffies + KEY_CONTINUE);*//* for button */
        key_dbg("%s key(code:%d) down\n", button->desc, button->code);
	}
    else{
        if(button_first){
            key_data->state = KEY_PRESSED;
            input_report_key(key_data->input, button->code, KEY_PRESSED);
            /*input_sync(key_data->input);*/
            button_first = false;
            key_dbg("%s key(code:%d) first down\n", button->desc, button->code);
        }
	    key_data->state = KEY_RELEASED;
		input_report_key(key_data->input, button->code, KEY_RELEASED);
        key_dbg("%s key(code:%d) up\n", button->desc, button->code);
	}
    /*input_sync(key_data->input);*/
}

static irqreturn_t gpio_keys_isr(int irq, void *dev_id)
{
	struct gpio_key_data *key_data = dev_id;
	struct gpio_keys_button *button = key_data->button;
	
    if(sep0611_gpio_getpin(button->gpio))
        sep0611_gpio_setirq(button->gpio, LOW_TRIG);
    else
        sep0611_gpio_setirq(button->gpio, HIGH_TRIG);
	sep0611_gpio_clrirq(button->gpio);
	
	mod_timer(&key_data->timer, jiffies + KEY_DELAY);
	
	return IRQ_HANDLED;
}

static int __devinit gpio_keys_probe(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_button *buttons = pdata->buttons;
	struct gpio_key_data *key_data;
	struct input_dev *input;
	unsigned int type;	
	int wakeup = 0;
	int i, error;
	int irq;

	keys_dev = kzalloc(sizeof(struct gpio_keys_dev) +
		pdata->nbuttons * sizeof(struct gpio_key_data),
		GFP_KERNEL);
	input = input_allocate_device();
	if (!keys_dev || !input) {
		error = -ENOMEM;
		goto fail1;
	}

	platform_set_drvdata(pdev, keys_dev);

	input->name = pdev->name;
	input->phys = "gpio-keys/input0";
	input->dev.parent = &pdev->dev;
	input->id.bustype = BUS_HOST;
	input->id.vendor = 0x0001;
	input->id.product = 0x0001;
	input->id.version = 0x0100;

	keys_dev->input = input;

	for (i = 0; i < pdata->nbuttons; i++) {
		key_data = &keys_dev->datas[i];
		key_data->button = &buttons[i];
		
		type = key_data->button->type;
		if(!type)
		    type = EV_KEY;

		key_data->input = input;
		setup_timer(&key_data->timer,
			gpio_check_button, (unsigned long)key_data);

		irq = gpio_to_irq(key_data->button->gpio);
		error = request_irq(irq, gpio_keys_isr, IRQF_DISABLED,
		    ((key_data->button->desc) ? (key_data->button->desc) : "gpio_key"), key_data);
		if (error) {
			pr_err("gpio-keys: Unable to claim irq %d; error %d\n",
				irq, error);
			goto fail2;
		}
		
		/*if(key_data->button->active_low)*/
			/*sep0611_gpio_setirq(key_data->button->gpio, DOWN_TRIG);*/
		/*else*/
			/*sep0611_gpio_setirq(key_data->button->gpio, UP_TRIG);*/
        if(sep0611_gpio_getpin(key_data->button->gpio))
            sep0611_gpio_setirq(key_data->button->gpio, LOW_TRIG);
        else
            sep0611_gpio_setirq(key_data->button->gpio, HIGH_TRIG);

		if (key_data->button->wakeup)
			wakeup = 1;

		input_set_capability(input, type, key_data->button->code);
		
		key_data->state = KEY_RELEASED;

		printk("register key %s(code:%d)\n", key_data->button->desc, key_data->button->code); 
	}

	error = input_register_device(input);
	if (error) {
		pr_err("gpio-keys: Unable to register input device, "
			"error: %d\n", error);
		goto fail2;
	}

	device_init_wakeup(&pdev->dev, wakeup);

	return 0;

fail2:
	while (--i >= 0) {
	    struct gpio_keys_button *button = &buttons[i];

		free_irq(gpio_to_irq(button->gpio), &keys_dev->datas[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&keys_dev->datas[i].timer);
	}
	platform_set_drvdata(pdev, NULL);

fail1:
	input_free_device(input);
	kfree(keys_dev);

	return error;
}

static int __devexit gpio_keys_remove(struct platform_device *pdev)
{
	struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_dev *keys_dev = platform_get_drvdata(pdev);
	struct input_dev *input = keys_dev->input;
	struct gpio_keys_button *button;
	int i, irq;

	device_init_wakeup(&pdev->dev, 0);

	for (i = 0; i < pdata->nbuttons; i++) {
	    button = &pdata->buttons[i];
		irq = gpio_to_irq(button->gpio);
		free_irq(irq, &keys_dev->datas[i]);
		if (pdata->buttons[i].debounce_interval)
			del_timer_sync(&keys_dev->datas[i].timer);
	}

	input_unregister_device(input);

	return 0;
}


#ifdef CONFIG_PM
static int gpio_keys_suspend(struct platform_device *pdev, pm_message_t state)
{
    struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_button *button;
	int i, irq;

	for (i = 0; i < pdata->nbuttons; i++) {
	    button = &pdata->buttons[i];
		irq = gpio_to_irq(button->gpio);
		disable_irq(irq);
	}

	return 0;
}

static int gpio_keys_resume(struct platform_device *pdev)
{   
    struct gpio_keys_platform_data *pdata = pdev->dev.platform_data;
	struct gpio_keys_button *button;
	int i, irq;
	
	for (i = 0; i < pdata->nbuttons; i++) {
	    button = &pdata->buttons[i];
		irq = gpio_to_irq(button->gpio);
		enable_irq(irq);
		
		sep0611_gpio_clrirq(button->gpio);
		
		if(button->active_low)
			sep0611_gpio_setirq(button->gpio, DOWN_TRIG);
		else
			sep0611_gpio_setirq(button->gpio, UP_TRIG);
	}
	return 0;
}
#else
#define gpio_keys_suspend	NULL
#define gpio_keys_resume	NULL
#endif

static struct platform_driver gpio_keys_device_driver = {
	.probe		= gpio_keys_probe,
	.remove		= __devexit_p(gpio_keys_remove),
	.suspend	= gpio_keys_suspend,
	.resume		= gpio_keys_resume,
	.driver		= {
		.name	= "gpio-keys",
		.owner	= THIS_MODULE,
	},
};

static int __init gpio_keys_init(void)
{
	printk("SEP0611 Keyboard driver v1.1\n");
	return platform_driver_register(&gpio_keys_device_driver);
}

static void __exit gpio_keys_exit(void)
{
	platform_driver_unregister(&gpio_keys_device_driver);
}

module_init(gpio_keys_init);
module_exit(gpio_keys_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("cgm<chenguangming@wxseuic.com>");
MODULE_DESCRIPTION("Keyboard driver for SEP0611 Spreadwin board");
MODULE_ALIAS("platform:gpio-keys");
