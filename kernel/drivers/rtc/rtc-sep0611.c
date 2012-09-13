/* drivers/rtc/rtc-sep0611.c
 *
 * Copyright (c) 2010 SOUTHEAST UNIVERSITY
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/module.h>
#include <linux/fs.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/rtc.h>
#include <linux/delay.h>
#include <linux/clk.h>

#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <mach/hardware.h>
#include <mach/regs-rtc.h>

#define CRYSTAL_FREQ 32768

static struct resource *sep0611_rtc_mem;
static void __iomem *sep0611_rtc_base;
static int sep0611_rtc_irq;
static int samp_count;

//add wdt irq by xuejilong
static int sep0611_wdt_irq;
//add wdt irq by xuejilong

static irqreturn_t sep0611_wdt_isr(int irq, void *id)
{
	unsigned int int_stat;
	void __iomem *base = sep0611_rtc_base;
	int_stat = readl(base + RTC_INT_STS);
	writel(int_stat, base + RTC_INT_STS);
	//printk("wdt\n");
	return IRQ_HANDLED;

}


static irqreturn_t sep0611_rtc_isr(int irq, void *id)
{
	unsigned int int_stat;
	struct rtc_device *rdev = id;
	void __iomem *base = sep0611_rtc_base;

	int_stat = readl(base + RTC_INT_STS);

	writel(int_stat, base + RTC_INT_STS);
	if (int_stat & ALARM_FLAG) {
		rtc_update_irq(rdev, 1, RTC_AF | RTC_IRQF);
	}

	if (int_stat & SAMP_FLAG) {
		/*reload the samp_count every time after a samp_int triggers*/
		writel(samp_count << 16, base + RTC_SAMP);

		rtc_update_irq(rdev, 1, RTC_PF | RTC_IRQF);
	}

	if (int_stat & SEC_FLAG) {
		rtc_update_irq(rdev, 1, RTC_UF | RTC_IRQF);
	}

	return IRQ_HANDLED;
}

static int sep0611_rtc_read_time(struct device *dev, struct rtc_time *rtc_tm)
{
	unsigned int have_retried = 0;
	void __iomem *base = sep0611_rtc_base;

retry_read_time:
	rtc_tm->tm_sec  = readl(base + RTC_STA_HMS) & 0x3F;
	rtc_tm->tm_min  = (readl(base + RTC_STA_HMS) >> 6) & 0x3F;
	rtc_tm->tm_hour = (readl(base + RTC_STA_HMS) >> 12) & 0x1F;
	rtc_tm->tm_mday = readl(base + RTC_STA_YMD) & 0x1F;
	rtc_tm->tm_mon  = (readl(base + RTC_STA_YMD) >> 5) & 0x0F;
	rtc_tm->tm_year = (readl(base + RTC_STA_YMD) >> 9) & 0x7FF;


	/* the only way to work out wether the system was mid-update
	 * when we read it is to check the second counter, and if it
	 * is zero, then we re-try the entire read
	 */

	if (rtc_tm->tm_sec == 0 && !have_retried) {
		have_retried = 1;
		goto retry_read_time; /* why use goto */
	}

	pr_debug("read time %04d.%02d.%02d %02d/%02d/%02d\n",
		 rtc_tm->tm_year, rtc_tm->tm_mon, rtc_tm->tm_mday,
		 rtc_tm->tm_hour, rtc_tm->tm_min, rtc_tm->tm_sec);

	rtc_tm->tm_year -= 1900;
	rtc_tm->tm_mon -= 1;

	return 0;
}

static int sep0611_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	void __iomem *base = sep0611_rtc_base;
	unsigned long ymd;
	unsigned long hms; 

	pr_debug("set time %04d.%02d.%02d %02d/%02d/%02d\n",
		 tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);

	if(tm->tm_year + 1900 > 2047)
		return -EINVAL;

	/*set the year,month and day values*/	
	ymd = ((tm->tm_year+1900)<<9) + ((tm->tm_mon+1)<<5) + (tm->tm_mday);
	writel(0xaaaaaaaa, base + RTC_CFG_CHK);   
	udelay(100);
	writel(ymd, base + RTC_STA_YMD);        
	udelay(100);
	writel(0x0, base + RTC_CFG_CHK);
	udelay(100);

	/*set the hour,minute and second values*/
	hms = ((tm->tm_hour)<<12) + ((tm->tm_min)<<6) + (tm->tm_sec);
	writel(0xaaaaaaaa, base + RTC_CFG_CHK);
	udelay(100);
	writel(hms, base + RTC_STA_HMS);
	udelay(100);
	writel(0x0, base + RTC_CFG_CHK);
	udelay(100);

	return 0;
}

static int sep0611_rtc_read_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *alm_tm = &alrm->time;
	void __iomem *base = sep0611_rtc_base;
	unsigned int alm_en;

	alm_tm->tm_min  = readl(base + RTC_ALARM) & 0x3F;
	alm_tm->tm_hour = (readl(base + RTC_ALARM) >> 6) & 0x1F;
	alm_tm->tm_mday = (readl(base + RTC_ALARM) >> 23) & 0x1F;
	alm_tm->tm_mon  = (readl(base + RTC_ALARM) >> 28) & 0x0F;

	alm_en = readb(base + RTC_INT_EN);

	alrm->enabled = (alm_en & ALARM_INT_EN) ? 1 : 0;

	pr_debug("sep0611_rtc_read_alarm:enabled(%d), %02d.%02d %02d/%02d\n",
		 alm_en, alm_tm->tm_mon, alm_tm->tm_mday,
		 alm_tm->tm_hour, alm_tm->tm_min);

	alm_tm->tm_mon -= 1;

	return 0;
}

static int sep0611_rtc_set_alarm(struct device *dev, struct rtc_wkalrm *alrm)
{
	struct rtc_time *tm = &alrm->time;
	void __iomem *base = sep0611_rtc_base;
	unsigned int alrm_en;
	unsigned long alrm_value;

	pr_debug("sep0611_rtc_setalarm: enabled(%d), %02d/%02d %02d.%02d\n",
		alrm->enabled,
		(tm->tm_mon + 1) & 0xff, tm->tm_mday & 0xff,
		tm->tm_hour & 0xff, tm->tm_min & 0xff);

	local_irq_disable();

	alrm_value = tm->tm_min + (tm->tm_hour << 6) + (tm->tm_mday << 23) + ((tm->tm_mon + 1) << 28);
	writel(alrm_value, base + RTC_ALARM);

	alrm_en = readl(base + RTC_INT_EN);
	if (alrm->enabled)
		alrm_en |= ALARM_INT_EN;
	else
		alrm_en &= ~ALARM_INT_EN;
	writel(alrm_en, base + RTC_INT_EN);

	local_irq_enable();

	return 0;
}

static int sep0611_rtc_alarm_irq_enable(struct device *dev, unsigned int enabled)
{
	void __iomem *base = sep0611_rtc_base;
	unsigned int alrm_en;
	
	/*the lastest alarm interrupt set the maskbit,so clear the mask first,
	 *write '1' to the maskbit.2010.8.21. OR this can be done in isr.
	 */
	local_irq_disable();

	alrm_en = readl(base + RTC_INT_EN);

	if(enabled)
		writel(alrm_en | ALARM_INT_EN, base + RTC_INT_EN);
	else
		writel(alrm_en & (~ALARM_INT_EN), base + RTC_INT_EN);

	local_irq_enable();
	
	return 0;
}

static int sep0611_rtc_irq_set_state(struct device *dev, int enabled)
{
	void __iomem *base = sep0611_rtc_base;
	unsigned int samp_en;
	
	/*clear the mask bit*/
	local_irq_disable();

	samp_en = readl(base + RTC_INT_EN);

	if(enabled)
		writel(samp_en | SAMP_INT_EN, base + RTC_INT_EN);
	else
		writel(samp_en & (~SAMP_INT_EN), base + RTC_INT_EN);

	local_irq_enable();
	
	return 0;
}

static int sep0611_rtc_irq_set_freq(struct device *dev, int freq)
{
	void __iomem *base = sep0611_rtc_base;
	unsigned int samp_cnt;
	
	if (!is_power_of_2(freq))
		return -EINVAL;

	samp_cnt = CRYSTAL_FREQ / freq;
	/*we need the samp_count for reload, even when 
	 the sample counts do NOT change*/
	samp_count = samp_cnt;

	writel(samp_cnt << 16, base + RTC_SAMP);

	return 0;
}

static int sep0611_rtc_update_irq_enable(struct device *dev, unsigned int enabled)
{
	void __iomem *base = sep0611_rtc_base;
	unsigned int update_en;

	local_irq_disable();

	update_en = readl(base + RTC_INT_EN);

	if(enabled)
		writel(update_en | SEC_INT_EN, base + RTC_INT_EN);
	else
		writel(update_en & (~SEC_INT_EN), base + RTC_INT_EN);

	local_irq_enable();
	return 0;	
}

static int sep0611_rtc_proc(struct device *dev, struct seq_file *seq)
{
	unsigned int tmp;

	tmp = readl(sep0611_rtc_base + RTC_INT_EN);

	seq_printf(seq, "alarm_IRQ\t: %s\n",
		(tmp & ALARM_INT_EN) ? "yes" : "no");
	seq_printf(seq, "periodic_IRQ\t: %s\n",
		(tmp & SAMP_INT_EN) ? "yes" : "no");
	seq_printf(seq, "update_IRQ\t: %s\n",
		(tmp & SEC_INT_EN) ? "yes" : "no");

	return 0;
}

static int sep0611_rtc_open(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_device *rtc = platform_get_drvdata(pdev);
	void __iomem *base = sep0611_rtc_base;
	int ret;
	
	rtc->irq_task = NULL;
	
	/*enable sample:set bit0 1*/
	writel(1, base + RTC_CTR);
	writel(0x80 << 16, base + RTC_SAMP);

	ret = request_irq(sep0611_rtc_irq, sep0611_rtc_isr, IRQF_DISABLED, dev_name(&rtc->dev), rtc);
	if(ret) {
		pr_debug("%s: RTC interrupt IRQ%d already claimed\n",
			pdev->name, sep0611_rtc_irq);
		return ret;
	}

	SEP0611_INT_ENABLE(sep0611_rtc_irq);

	return 0;
}

static void sep0611_rtc_release(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct rtc_device *rtc = platform_get_drvdata(pdev);
	void __iomem *base = sep0611_rtc_base;

    SEP0611_INT_DISABLE(sep0611_rtc_irq);
    free_irq(sep0611_rtc_irq, rtc);

    //add wdt irq by xuejilong
    SEP0611_INT_DISABLE(sep0611_wdt_irq);
    free_irq(sep0611_wdt_irq,rtc);
    //add wdt irq by xuejilong
    
    /*disable sample:set bit0 0*/
    writel(0, base + RTC_CTR);
}

static struct rtc_class_ops sep0611_rtc_ops = {
	.open		= sep0611_rtc_open,
	.release	= sep0611_rtc_release,
	.read_time	= sep0611_rtc_read_time,
	.set_time	= sep0611_rtc_set_time,
	.read_alarm	= sep0611_rtc_read_alarm,
	.set_alarm	= sep0611_rtc_set_alarm,
	.alarm_irq_enable	= sep0611_rtc_alarm_irq_enable,
	.irq_set_state		= sep0611_rtc_irq_set_state,
	.irq_set_freq		= sep0611_rtc_irq_set_freq,
	.update_irq_enable	= sep0611_rtc_update_irq_enable,
	.proc		= sep0611_rtc_proc,
};

static int __devexit sep0611_rtc_remove(struct platform_device *pdev)
{
	struct rtc_device *rtc = platform_get_drvdata(pdev);

	platform_set_drvdata(pdev, NULL);
	rtc_device_unregister(rtc);

	iounmap(sep0611_rtc_base);
	release_resource(sep0611_rtc_mem);

	return 0;
}

static int __devinit sep0611_rtc_probe(struct platform_device *pdev)
{
	struct rtc_device *rtc;
	struct resource *res;
	int ret = 0;

	/*requset for IRQ*/
	sep0611_rtc_irq = platform_get_irq(pdev,0);
	if (sep0611_rtc_irq <= 0) {
		dev_err(&pdev->dev, "no irq for rtc!\n");
		return -ENOENT;
	}

    //add wdt irq by xuejilong
    sep0611_wdt_irq = platform_get_irq(pdev,1);
	if (sep0611_wdt_irq <= 0) {
		dev_err(&pdev->dev, "no irq for rtc watchdog!\n");
		return -ENOENT;
	}
    //add wdt irq by xuejilong
    
	/*get the memory region*/
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource!\n");
		return -ENOENT;
	}

	sep0611_rtc_mem = request_mem_region(res->start, resource_size(res), pdev->name);
	if(sep0611_rtc_mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region!\n");
		ret = -ENOENT;
		goto err_mem;
	}

	sep0611_rtc_base = ioremap(res->start, resource_size(res));
	if (sep0611_rtc_base == NULL) {
		dev_err(&pdev->dev, "failed to remmap memory!\n");
		ret = -EINVAL;
		goto err_map;
	}

	rtc = rtc_device_register(pdev->name, &pdev->dev, &sep0611_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		dev_err(&pdev->dev, "failed to register rtc\n");
		ret = PTR_ERR(rtc);
		goto err_rtc;
	}

	/*initialize the periodic irq frequency*/
	rtc->irq_freq = 256;
	rtc->max_user_freq =256;

    //add wdt irq by xuejilong
	writel(0x80 << 16 , sep0611_rtc_base + RTC_WD_CNT);
    //printk("RTC_WD_CNT is %lx\n",readl(sep0611_rtc_base + RTC_WD_CNT));
	writel((0x3 << 24) | (0x20 << 8)|(0x1 << 2) | readl(sep0611_rtc_base + RTC_CTR), sep0611_rtc_base + RTC_CTR);
    //printk("RTC_WD_CNT is %lx\n",readl(sep0611_rtc_base + RTC_CTR));
	writel((0x1<<1) |(0x1<<5) | readl(sep0611_rtc_base + RTC_INT_EN), sep0611_rtc_base + RTC_INT_EN);
    //printk("RTC_WD_CNT is %lx\n",readl(sep0611_rtc_base + RTC_INT_EN));
    //printk("RTC_WD_CNT is %lx\n",readl(sep0611_rtc_base + RTC_WD_CNT));

	ret = request_irq(sep0611_wdt_irq, sep0611_wdt_isr, IRQF_DISABLED, dev_name(&rtc->dev), rtc);
	if(ret) {
		pr_debug("%s: RTC interrupt IRQ%d already claimed\n",
			pdev->name, sep0611_wdt_irq);
		return ret;
	}
	SEP0611_INT_ENABLE(sep0611_wdt_irq);
    //add wdt irq by xuejilong

	platform_set_drvdata(pdev, rtc);
	return 0;
	
err_rtc:
	iounmap(sep0611_rtc_base);
err_map:
	release_resource(sep0611_rtc_mem);
err_mem:
	return ret;
}

#ifdef CONFIG_PM

static int sep0611_rtc_suspend(struct platform_device *pdev, pm_message_t state)
{
	/*implement this function when power management works*/
	return 0;
}

static int sep0611_rtc_resume(struct platform_device *pdev)
{
	/*implement this function when power management works*/
	return 0;
}

#else

#define sep0611_rtc_suspend NULL
#define sep0611_rtc_resume  NULL

#endif

static struct platform_driver sep0611_rtc_driver = {
	.probe		= sep0611_rtc_probe,
	.remove		= __devexit_p(sep0611_rtc_remove),
	.suspend	= sep0611_rtc_suspend,
	.resume		= sep0611_rtc_resume,
	.driver		= {
		.name	= "sep0611-rtc",
		.owner	= THIS_MODULE,
	},
};

static int __init sep0611_rtc_init(void)
{
	printk("SEP0611 RTC, (C) SOUTHEAST UNIVERSITY\n");
	return platform_driver_register(&sep0611_rtc_driver);
}

static void __exit sep0611_rtc_exit(void)
{
	platform_driver_unregister(&sep0611_rtc_driver);
}

module_init(sep0611_rtc_init);
module_exit(sep0611_rtc_exit);

MODULE_DESCRIPTION("SEP0611 RTC Driver");
MODULE_AUTHOR("Zhang Min<zmdesperado@gmail.com>");
MODULE_LICENSE("GPL");
