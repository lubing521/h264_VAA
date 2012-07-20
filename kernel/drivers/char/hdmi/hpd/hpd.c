#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>

#define HPD_DEBUG 		1

#if HPD_DEBUG
#define DPRINTK(args...)    printk("hpd-gpio:" args)
#else
#define DPRINTK(args...)
#endif

static int hpd_probe(struct platform_device *pdev)
{
/*	unsigned int reg;
	struct tcc_hpd_platform_data *hpd_dev;
	hpd_dev = pdev->dev.platform_data;

    if (!machine_is_hdmidp())
        return -ENODEV;

    printk(KERN_INFO "HPD Driver ver. %s (built %s %s)\n", VERSION, __DATE__, __TIME__);

	hdmi_hpd_clk = clk_get(0, "hdmi");
	
	clk_enable(hdmi_hpd_clk);
	hpd_port = hpd_dev->hpd_port;

	gpio_request(hpd_port, "hpd");
	gpio_direction_input(hpd_port);

    if (misc_register(&hpd_misc_device))    {
        printk(KERN_WARNING "HPD: Couldn't register device 10, %d.\n", HPD_MINOR);
        return -EBUSY;
    }
    spin_lock_init(&hpd_struct.lock);
	
     disable HPD interrupts 
    reg = readb(HDMI_SS_INTC_CON);
    reg &= ~(1<<HDMI_IRQ_HPD_PLUG);
    reg &= ~(1<<HDMI_IRQ_HPD_UNPLUG);
    writeb(reg, HDMI_SS_INTC_CON);
    atomic_set(&hpd_struct.state, -1);

	clk_disable(hdmi_hpd_clk);*/

    return 0;
}

static int hpd_remove(struct platform_device *pdev)
{
    DPRINTK(KERN_INFO "%s\n", __FUNCTION__);

   // misc_deregister(&hpd_misc_device);
	return 0;
}

static struct platform_driver cat_hdmi_hpd = {
	.probe	= hpd_probe,
	.remove	= hpd_remove,
	.driver	= {
		.name	= "cat_hdmi_hpd",
		.owner	= THIS_MODULE,
	},
};
static __init int hpd_init(void)
{
	return platform_driver_register(&cat_hdmi_hpd);
}

static __exit void hpd_exit(void)
{
	platform_driver_unregister(&cat_hdmi_hpd);
}


module_init(hpd_init);
module_exit(hpd_exit);
MODULE_AUTHOR("SEP CAT6613 hpd driver");
MODULE_LICENSE("GPL");


