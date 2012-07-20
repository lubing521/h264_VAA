/*
 * Backlight driver for sep0611
 *
 * Copyright 2011 seuic.
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/fb.h>
#include <linux/backlight.h>

#include <linux/leds.h>
#include "sep0611_fb.h"

#define DEFAULT_BACKLIGHT_BRIGHTNESS 102

static void sep0611_brightness_set(struct led_classdev *led_cdev, enum led_brightness value)
{
	//sepfb_get_panel()->set_backlight_level(sepfb_get_panel(), value);
	//printk("set brightness...%d\n", value);
	sep0611fb_set_backlight_level(value);
}

static struct led_classdev sep0611_backlight_led = {
	.name		= "lcd-backlight",
	.brightness	= DEFAULT_BACKLIGHT_BRIGHTNESS,
	.brightness_set	= sep0611_brightness_set,
};

static int __devinit sep0611_bl_probe(struct platform_device *pdev)
{
	return led_classdev_register(&pdev->dev, &sep0611_backlight_led);
}

static int __devexit sep0611_bl_remove(struct platform_device *pdev)
{
	led_classdev_unregister(&sep0611_backlight_led);
	return 0;
}

#ifdef CONFIG_PM
static int sep0611_bl_suspend(struct platform_device *pdev,
				 pm_message_t state)
{
	sep0611_enable_backlight(0);
	printk("%s\n", __func__);
	//sepfb_get_panel()->set_backlight_power(sepfb_get_panel(), 0);
	
	return 0;
}

static int sep0611_bl_resume(struct platform_device *pdev)
{
	sep0611_enable_backlight(1);
	printk("%s\n", __func__);
	//sepfb_get_panel()->set_backlight_power(sepfb_get_panel(), 1);
	
	return 0;
}
#else
#define sep0611_bl_suspend	NULL
#define sep0611_bl_resume	NULL
#endif

static struct platform_driver sep0611_bl_driver = {
	.driver		= {
		.name	= "sep0611-backlight",
		.owner	= THIS_MODULE,
	},
	.probe		= sep0611_bl_probe,
	.remove		= __devexit_p(sep0611_bl_remove),
	.suspend		= sep0611_bl_suspend,
	.resume		= sep0611_bl_resume,
};

static int __init sep0611_bl_init(void)
{
	return platform_driver_register(&sep0611_bl_driver);
}
module_init(sep0611_bl_init);

static void __exit sep0611_bl_exit(void)
{
	platform_driver_unregister(&sep0611_bl_driver);
}
module_exit(sep0611_bl_exit);

MODULE_DESCRIPTION("sep0611 Backlight Driver");
MODULE_LICENSE("GPL");

