/*
 *  drivers/video/backlight/sep0611_bl.c
 *
 *  SEP0611 Backlight Driver
 *
 *  Copyright (c) 2010-2011 Zhang Min Southeast University
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/fb.h>
#include <linux/backlight.h>

#define		DEFAULT_BACKLIGHT_BRIGHTNESS	255 
#define 	MAX_BACKLIGHT_BRIGHTNESS	255
#define 	REGISTER_REVERSE		255
#define 	BACKLIGHT_BASE			0xB2025020
///* GPIO bits for the back light and lcdc_enable */
//#define		GPIO_LCDCENABLE_BIT	(1 << 6)
//#define		GPIO_BACKLIGHT_BIT	(1 << 4)

static void __iomem *backlight_base;

///*enable:1; disable:0*/
//void set_backlight(int enable)
//{
//	if (enable == 1) {
//		/*lcdc enable:PA6 LOW*/
//		writel(readl(GPIO_BASE_V + 0x14) | GPIO_LCDCENABLE_BIT, GPIO_BASE_V + 0x14); /*sel*/
//		writel(readl(GPIO_BASE_V + 0x10) | GPIO_LCDCENABLE_BIT, GPIO_BASE_V + 0x10); /*dir*/
//		writel(readl(GPIO_BASE_V + 0x18) & (~GPIO_LCDCENABLE_BIT), GPIO_BASE_V + 0x18); /*data*/
//
//		/*backlight enable:PF4 HIGH*/
//		writel(readl(GPIO_BASE_V + 0xB4) | GPIO_BACKLIGHT_BIT, GPIO_BASE_V + 0xB4); /*sel*/
//		writel(readl(GPIO_BASE_V + 0xB0) | GPIO_BACKLIGHT_BIT, GPIO_BASE_V + 0xB0); /*dir*/
//		writel(readl(GPIO_BASE_V + 0xB8) | GPIO_BACKLIGHT_BIT, GPIO_BASE_V + 0xB8); /*data*/
//	}
//
// 	if (enable == 0) {
//		writel(readl(GPIO_BASE_V + 0x18) | GPIO_LCDCENABLE_BIT, GPIO_BASE_V + 0x18); /*data*/
//		writel(readl(GPIO_BASE_V + 0xB8) & (~GPIO_BACKLIGHT_BIT), GPIO_BASE_V + 0xB8); /*data*/
//	}
//}

static int sep0611_backlight_update_status(struct backlight_device *bd)
{
	void __iomem *base = backlight_base;
	unsigned int brightness = bd->props.brightness;
	unsigned int value = readl(base) & (~0xff);

	if (bd->props.power != FB_BLANK_UNBLANK) {
		brightness = 0;
printk("DEBUG in sep0611_backlight_update_status: FB_BLANK_UNBLANK!\n");
	}	
	if (bd->props.state & BL_CORE_FBBLANK) {
		brightness = 0;
printk("DEBUG in sep0611_backlight_update_status: BL_CORE_FBBLANK!\n");
}
//	if (bd->props.state & BL_CORE_SUSPENDED)
//		brightness = 0;
//	if (bd->props.state & GENERICBL_BATTLOW)
//		brightness &= bl_machinfo->limit_mask;

	writel(value | (REGISTER_REVERSE - brightness), base);

	return 0;
}

static int sep0611_backlight_get_brightness(struct backlight_device *bd)
{
	void __iomem *base = backlight_base;
	unsigned int brightness; 

	brightness = readl(base);
	return REGISTER_REVERSE - (brightness & 0xff);
}

static struct backlight_ops sep0611_backlight_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.get_brightness = sep0611_backlight_get_brightness,
	.update_status  = sep0611_backlight_update_status,
};

static int sep0611_backlight_probe(struct platform_device *pdev)
{
	struct backlight_device *bd;
	int ret;

	backlight_base = ioremap(BACKLIGHT_BASE, 4);
	if (backlight_base == NULL) {
		dev_err(&pdev->dev, "failed to io remap backlight address!\n");
		ret = -EINVAL;
		return ret;
	}

	bd = backlight_device_register (dev_name(&pdev->dev),
		&pdev->dev, NULL, &sep0611_backlight_ops);
	if (IS_ERR (bd)) {
		dev_err(&pdev->dev, "backlight: failed to register device!\n");
		ret = PTR_ERR (bd);
		goto err_map;
	}

	bd->props.power = FB_BLANK_UNBLANK;
	bd->props.brightness = DEFAULT_BACKLIGHT_BRIGHTNESS;
	bd->props.max_brightness = MAX_BACKLIGHT_BRIGHTNESS;

	backlight_update_status(bd);

	platform_set_drvdata(pdev, bd);

	printk(KERN_INFO "sep0611 backlight driver initialized!\n");
	return 0;
err_map:
	iounmap(backlight_base);
	return ret;
}

static int sep0611_backlight_remove(struct platform_device *pdev)
{
	struct backlight_device *bd = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);

	bd->props.power = 0;
	bd->props.brightness = 0;

	backlight_update_status(bd);

	backlight_device_unregister(bd);

	iounmap(backlight_base);

	printk("sep0611 backlight driver unloaded!\n");
	return 0;
}

#ifdef CONFIG_PM
static int sep0611_backlight_suspend(struct platform_device *pdv, pm_message_t state)
{
	return 0;
}

static int sep0611_backlight_resume(struct platform_device *pdev)
{
	return 0;
}
#else
#define sep0611_backlight_suspend	NULL
#define sep0611_backlight_resume	NULL
#endif

static struct platform_driver sep0611_backlight_driver = {
	.probe		= sep0611_backlight_probe,
	.remove		= sep0611_backlight_remove,
	.suspend	= sep0611_backlight_suspend,
	.resume		= sep0611_backlight_resume,
	.driver		= {
		.name	= "sep0611-backlight",
		.owner	= THIS_MODULE,
	},
};

static int __init sep0611_backlight_init(void)
{
	return platform_driver_register(&sep0611_backlight_driver);
}

static void __exit sep0611_backlight_exit(void)
{
	platform_driver_unregister(&sep0611_backlight_driver);
}

module_init(sep0611_backlight_init);
module_exit(sep0611_backlight_exit);

MODULE_AUTHOR("Zhang Min <zmdesperado@gmail.com>");
MODULE_DESCRIPTION("SEP0611 Backlight Driver");
MODULE_LICENSE("GPL");

