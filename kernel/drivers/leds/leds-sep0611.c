/* drivers/leds/leds-sep0611.c
 *
 * (c) 2012 WXSEUIC
 *	chen.mint <chenguangming@wxseuic.com>
 *
 * SEP0611 - LEDs GPIO driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/gpio.h>

#include <mach/hardware.h>
#include <mach/regs-gpio.h>
#include <mach/leds.h>

/* our context */

struct sep0611_gpio_led {
	struct led_classdev		 cdev;
	struct sep0611_led_platdata	*pdata;
};

static inline struct sep0611_gpio_led *pdev_to_gpio(struct platform_device *dev)
{
	return platform_get_drvdata(dev);
}

static inline struct sep0611_gpio_led *to_gpio(struct led_classdev *led_cdev)
{
	return container_of(led_cdev, struct sep0611_gpio_led, cdev);
}

static void sep0611_led_set(struct led_classdev *led_cdev,
			    enum led_brightness value)
{
	struct sep0611_gpio_led *led = to_gpio(led_cdev);
	struct sep0611_led_platdata *pd = led->pdata;

	/* there will be a short delay between setting the output and
	 * going from output to input when using tristate. */

	sep0611_gpio_setpin(pd->gpio, (value ? 1 : 0) ^ (pd->flags & SEP0611_LEDF_ACTLOW));
//	printk("val = %d\n", (value ? 1 : 0) ^ (pd->flags & SEP0611_LEDF_ACTLOW));
//	printk("pd->gpio=%d, %d, %d\n", pd->gpio, SEP0611_GPE2, SEP0611_GPE3);

#if 0
	if(value)
		printk("led on\n");
	else
		printk("led off\n");
#endif
}

static int sep0611_led_remove(struct platform_device *dev)
{
	struct sep0611_gpio_led *led = pdev_to_gpio(dev);

	led_classdev_unregister(&led->cdev);
	kfree(led);

	return 0;
}

static int sep0611_led_probe(struct platform_device *dev)
{
	struct sep0611_led_platdata *pdata = dev->dev.platform_data;
	struct sep0611_gpio_led *led;
	int ret;
	
	led = kzalloc(sizeof(struct sep0611_gpio_led), GFP_KERNEL);
	if (led == NULL) {
		dev_err(&dev->dev, "No memory for device\n");
		return -ENOMEM;
	}

	platform_set_drvdata(dev, led);

	led->cdev.brightness_set = sep0611_led_set;
	led->cdev.default_trigger = pdata->def_trigger;
	led->cdev.name = pdata->name;
	led->cdev.flags |= LED_CORE_SUSPENDRESUME;

	led->pdata = pdata;

	/* no point in having a pull-up if we are always driving */

	sep0611_gpio_cfgpin(pdata->gpio, SEP0611_GPIO_IO);
	sep0611_gpio_dirpin(pdata->gpio, SEP0611_GPIO_OUT);

	if (pdata->flags & SEP0611_LEDF_ACTLOW) {
		sep0611_gpio_setpin(pdata->gpio, 1);
	} else {
		sep0611_gpio_setpin(pdata->gpio, 0);
	}

	/* register our new led device */

	ret = led_classdev_register(&dev->dev, &led->cdev);
	if (ret < 0) {
		dev_err(&dev->dev, "led_classdev_register failed\n");
		kfree(led);
		return ret;
	}

	return 0;
}

static struct platform_driver sep0611_led_driver = {
	.probe		= sep0611_led_probe,
	.remove		= sep0611_led_remove,
	.driver		= {
		.name		= "sep0611_led",
		.owner		= THIS_MODULE,
	},
};

static int __init sep0611_led_init(void)
{
	printk("SEP0611 LED driver v1.0.\n");
	return platform_driver_register(&sep0611_led_driver);
}

static void __exit sep0611_led_exit(void)
{
	platform_driver_unregister(&sep0611_led_driver);
}

module_init(sep0611_led_init);
module_exit(sep0611_led_exit);

MODULE_AUTHOR("chen.mint <chenguangming@wxseuic.com>");
MODULE_DESCRIPTION("SEP0611 LED driver");
MODULE_LICENSE("GPL");
MODULE_ALIAS("platform:sep0611_led");
