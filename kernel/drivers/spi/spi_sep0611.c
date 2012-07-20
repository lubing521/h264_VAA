/* linux/drivers/spi/spi_sep0611.c
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


#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/platform_device.h>


#include <linux/spi/spi.h>
#include <linux/spi/spi_bitbang.h>

#include <asm/io.h>
#include <asm/dma.h>
#include <mach/hardware.h>

#include <mach/regs-spi.h>
#include <mach/regs-gpio.h>
#include <mach/spi.h>


struct sep0611_spi {
	/* bitbang has to be first */
	struct spi_bitbang	 bitbang;
	struct completion	 done;
	void __iomem		*regs;
	int			 irq;
	int			 len;
	int			 count;

	void			(*set_cs)(struct sep0611_spi_info *spi,
					  int cs, int pol);

	/* data buffers */
	const unsigned char	*tx;
	unsigned char		*rx;

	struct clk		*clk;
	struct resource		*ioarea;
	struct spi_master	*master;
	struct spi_device	*curdev;
	struct device		*dev;
	struct sep0611_spi_info *pdata;
};

#define MODE_DEFAULT (SEP0611_IMR_NOMASK) 


static inline struct sep0611_spi *to_hw(struct spi_device *sdev)
{
	return spi_master_get_devdata(sdev->master);
}



static void sep0611_spi_gpiocs(struct sep0611_spi_info *spi, int cs, int pol)
{

		if(pol)
			*(volatile unsigned long *)spi->pin_cs |= (1 << 3);
		else
			*(volatile unsigned long *)spi->pin_cs &= ~(1 << 3);
		
	#if 0
	if(pol==1)
		spi->pin_cs|=(1<<pol);
		else if(pol==0)
 		spi->pin_cs &= ~(0x1<<3);
	#endif
}

static void sep0611_spi_chipsel(struct spi_device *spi, int value)
{
	struct sep0611_spi *hw = to_hw(spi);
	unsigned int cspol = spi->mode & SPI_CS_HIGH ? 1 : 0;
	unsigned int control0;

	switch (value) {
	case BITBANG_CS_INACTIVE:

		hw->set_cs(hw->pdata, spi->chip_select, cspol^1);
		
 		break;
	case BITBANG_CS_ACTIVE:
		control0 = readb(hw->regs + SEP0611_CONTROL0);
		if (spi->mode & SPI_CPHA)
			control0 |= SEP0611_CONTROL0_CPHA_FMTB;
		else
			control0 &= ~SEP0611_CONTROL0_CPHA_FMTB;
		if (spi->mode & SPI_CPOL)
			control0 |= SEP0611_CONTROL0_SCPOL_HIGH;
		else
			control0 &= ~SEP0611_CONTROL0_SCPOL_HIGH;

		/* write new configration */
		writeb(control0, hw->regs + SEP0611_CONTROL0);
	
     /*disable and enable the sck */
     writeb(0x0, hw->regs + SEP0611_SSIENR);
     writeb(0x1, hw->regs + SEP0611_SSIENR);

	   hw->set_cs(hw->pdata, spi->chip_select, cspol);

		break;
	}
}

static int sep0611_spi_setupxfer(struct spi_device *spi, struct spi_transfer *t)
{
	struct sep0611_spi *hw = to_hw(spi);
	unsigned int bpw;
	unsigned int hz;
	unsigned int div;

	bpw = (spi->bits_per_word) ? spi->bits_per_word : t->bits_per_word;
	hz  = t ? t->speed_hz : spi->max_speed_hz;
	if (bpw != 8) 
	{
		dev_err(&spi->dev, "invalid bits-per-word (%d)\n", bpw);
		return -EINVAL;
	}

	//div = clk_get_rate(hw->clk) / hz;                                    /*we do not have the clock modules*/
		div=50000000/hz;

	div &= 0xfffe;

	if (div > 65534)
		div = 65534;
	writel(0x300, hw->regs + SEP0611_BAUDR);                           
	spin_lock(&hw->bitbang.lock);
	if (!hw->bitbang.busy) {
		hw->bitbang.chipselect(spi, BITBANG_CS_INACTIVE);
		/* need to ndelay for 0.5 clocktick ? */
	}
	spin_unlock(&hw->bitbang.lock);
	return 0;
}

/* the spi->mode bits understood by this driver: */
#define MODEBITS (SPI_CPOL | SPI_CPHA | SPI_CS_HIGH)             

static int sep0611_spi_setup(struct spi_device *spi)
{
	int ret;
 
	if (!spi->bits_per_word)
		spi->bits_per_word = 8;

	if (spi->mode & ~MODEBITS) {                                     
		dev_dbg(&spi->dev, "setup: unsupported mode bits %x\n",
			spi->mode & ~MODEBITS);                                   
		return -EINVAL;
	}

	ret = sep0611_spi_setupxfer(spi, NULL);
	if (ret < 0) {
		dev_err(&spi->dev, "setupxfer returned %d\n", ret);
		return ret;
	}

	dev_dbg(&spi->dev, "%s: mode %d, %u bpw, %d hz\n",
		__func__, spi->mode, spi->bits_per_word,
		spi->max_speed_hz);

	return 0;
}


static inline unsigned int hw_txbyte(struct sep0611_spi *hw, int count)
{
	return hw->tx ? hw->tx[count] : 0;
}

static int sep0611_spi_txrx(struct spi_device *spi, struct spi_transfer *t)
{
	struct sep0611_spi *hw = to_hw(spi);

	hw->tx = t->tx_buf;
	hw->rx = t->rx_buf;
	hw->len = t->len;
	hw->count = 0;
	//printk("sep0611_spi_txrx\n");
	init_completion(&hw->done);

	writeb(0x01, hw->regs + SEP0611_IMR);

	wait_for_completion(&hw->done);

	return hw->count;
}

static irqreturn_t sep0611_spi_irq(int irq, void *dev)
{
	struct sep0611_spi *hw = dev;
	unsigned int spsta = readb(hw->regs + SEP0611_SR);
	unsigned int count = hw->count;


	if (spsta & SEP0611_SR_DCOL) {
		dev_dbg(hw->dev, "data-collision\n");
		goto irq_done;
		}

	if (spsta & SEP0611_SR_BUSY) {
		while (!(*(volatile unsigned long*)(hw->regs + SEP0611_SR)& 0x4));
		while (*(volatile unsigned long*)(hw->regs + SEP0611_SR) & 0x1);
		goto irq_done;		
		}


		/* mask the IMR*/
		writeb(0x0, hw->regs + SEP0611_IMR); 

		//printk("sent data is 0x%x\n",hw_txbyte(hw, count));
		writeb(hw_txbyte(hw, count), hw->regs + SEP0611_DR);

		while (!(*(volatile unsigned long*)(hw->regs + SEP0611_SR)& 0x4));
		while (*(volatile unsigned long*)(hw->regs + SEP0611_SR) & 0x1);
	hw->count++;
	if (hw->rx)
		{

		hw->rx[count] =readb(hw->regs + SEP0611_DR);	
		//while ((*(volatile unsigned long*)(hw->regs + SEP0611_SR) & 0x8));
		while (*(volatile unsigned long*)(hw->regs + SEP0611_SR) & 0x1);
		//printk("receive data is 0x%x\n",hw->rx[count]);
		}
#if 0		
	else	
		{
		 udelay(2);
		 writeb(0x00, hw->regs + SEP0611_DR);
		//while (!(*(volatile unsigned long*)(hw->regs + SEP0611_SR)& 0x4));
		while (*(volatile unsigned long*)(hw->regs + SEP0611_SR) & 0x1);
		 
		//readb(hw->regs + SEP0611_DR);	
		printk("else receive data is 0x%x\n",readb(hw->regs + SEP0611_DR));
		//while ((*(volatile unsigned long*)(hw->regs + SEP0611_SR) & 0x8));
		while (*(volatile unsigned long*)(hw->regs + SEP0611_SR) & 0x1);

		}	
#endif
		count++;
	if(count < hw->len)
		{
		/* unmask the IMR*/
		writeb(0x1, hw->regs + SEP0611_IMR); 		
		}
		else
		complete(&hw->done);
	
 irq_done:

	return IRQ_HANDLED;
}


static void sep0611_spi_initialsetup(struct sep0611_spi *hw)
{
 	/* for the moment, permanently enable the clock */;
	//clk_enable(hw->clk);                             /*we do not have the clock modules*/                           
    hw->regs = SSI1_BASE_V;
    writeb(0x0, hw->regs + SEP0611_SSIENR);
//#if defined(CONFIG_SPI_SPIDEV)    
    //writel(0x847, hw->regs + SEP0611_CONTROL0);
//#else
	//writel(0x047, hw->regs + SEP0611_CONTROL0);
    writel(0x07, hw->regs + SEP0611_CONTROL0);
//#endif    
    //writeb(0x01, hw->regs + SEP0611_CONTROL1);
    writeb(0x00, hw->regs + SEP0611_CONTROL1);
  	writel(0x23f, hw->regs + SEP0611_BAUDR);
    writeb(0x1, hw->regs + SEP0611_TXFTLR);
    writeb(0x0, hw->regs + SEP0611_RXFTLR);
    writeb(0x0, hw->regs + SEP0611_DMACR);
    writeb(0x0, hw->regs + SEP0611_IMR);
    writeb(0x1, hw->regs + SEP0611_SER);
    writeb(0x1, hw->regs + SEP0611_SSIENR);
	SEP0611_INT_ENABLE(INTSRC_SSI1);
  //	*(volatile unsigned long *)GPIO_PORTD_SEL_V |= 0x1<<3;   	//portD3设置为通用（普通io） 
  	//*(volatile unsigned long *)GPIO_PORTD_DIR_V  &= ~(0x1<<3);	// portD3(SSI_CS1) 输出

}
#if 1

int read_xpt2046(void)
{   
	int i =3;
   	int j = 3;
   	unsigned short val = 0;
   	unsigned short data =0;
   	unsigned short raw_data[3] = {0,0,0};
   	int s1 = 0;
   	int s2 = 0;
   	while(i--)
   	{
		j = 10;
		writel(0xa4,SSI1_BASE_V+0x60);
		do
		{
			s2 = readl(SSI1_BASE_V+0x28);
		}while(!(s2&0x8)&&j--);
		
		raw_data[0] = readl(SSI1_BASE_V+0x60);
		
		writel(0x0,SSI1_BASE_V+0x60);
		do
		{
			s2 = readl(SSI1_BASE_V+0x28);
		}while(!(s2&0x8)&&j--);
		raw_data[1] = readl(SSI1_BASE_V+0x60);
		
		writel(0x0,SSI1_BASE_V+0x60);
		do
		{
			s2 = readl(SSI1_BASE_V+0x28);
		}while(!(s2&0x8)&&j--);
		raw_data[2] = readl(SSI1_BASE_V+0x60);
		s2 = readl(SSI1_BASE_V+0x28);
		data = (( raw_data[1] << 4 ) | ( raw_data[2] >> 4 ));
    }

//    printk("data = %d\n",data);

    return data;
}
SYSMBOL_EXPORT(read_xpt2046);

#endif
static int __init sep0611_spi_probe(struct platform_device *pdev)  
{
	struct sep0611_spi_info *pdata;
	struct sep0611_spi *hw;
	struct spi_master *master;
	struct resource *res;
	struct clk *clk = NULL;
	int err = 0;

	printk("%s\n", __func__);

	master = spi_alloc_master(&pdev->dev, sizeof(struct sep0611_spi));
	if (master == NULL) {
		dev_err(&pdev->dev, "No memory for spi_master\n");
		err = -ENOMEM;
		goto err_nomem;
	}

	hw = spi_master_get_devdata(master);
	memset(hw, 0, sizeof(struct sep0611_spi));
	hw->master = spi_master_get(master);
	hw->pdata = pdata = pdev->dev.platform_data;
	hw->dev = &pdev->dev;

	if (pdata == NULL) {
		dev_err(&pdev->dev, "No platform data supplied\n");
		err = -ENOENT;
		goto err_no_pdata;
	}
	platform_set_drvdata(pdev, hw);
	init_completion(&hw->done);

	/* setup the master state. */

	master->num_chipselect = hw->pdata->num_cs;
	master->bus_num = pdata->bus_num;

	/* setup the state for the bitbang driver */

	hw->bitbang.master         = hw->master;
	hw->bitbang.setup_transfer = sep0611_spi_setupxfer;
	hw->bitbang.chipselect     = sep0611_spi_chipsel;
	hw->bitbang.txrx_bufs      = sep0611_spi_txrx;
	hw->bitbang.master->setup  = sep0611_spi_setup;

	/* find and map our resources */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);                  
	if (res == NULL) {
		dev_err(&pdev->dev, "Cannot get IORESOURCE_MEM\n");
		err = -ENOENT;
		goto err_no_iores;
	}
	hw->ioarea = request_mem_region(res->start, (res->end - res->start)+1,
					pdev->name);

	if (hw->ioarea == NULL) {
		dev_err(&pdev->dev, "Cannot reserve region\n");
		err = -ENXIO;
		goto err_no_iores;
	}
 
	hw->regs = ioremap(res->start, (res->end - res->start)+1);
	if (hw->regs == NULL) {
		dev_err(&pdev->dev, "Cannot map IO\n");
		err = -ENXIO;
		goto err_no_iomap;
	}

	hw->irq = platform_get_irq(pdev, 0);
	if (hw->irq < 0) {
		dev_err(&pdev->dev, "No IRQ specified\n");
		err = -ENOENT;
		goto err_no_irq;
	}
	
	switch(pdev->id){
	case 1:
		sep0611_gpio_cfgpin(SEP0611_GPE15, SSI1_CLK);
		sep0611_gpio_cfgpin(SEP0611_GPE14, SSI1_CS);
		sep0611_gpio_setpin(SEP0611_GPE14,GPIO_LOW);
		sep0611_gpio_cfgpin(SEP0611_GPE13, SSI1_TXD);
		sep0611_gpio_cfgpin(SEP0611_GPE12, SSI1_RXD);
		clk = clk_get(NULL, "spi-1");
		break;
		
	case 2:
		sep0611_gpio_cfgpin(SEP0611_GPE11, SSI2_CLK);
		sep0611_gpio_cfgpin(SEP0611_GPE10, SSI2_CS);
		sep0611_gpio_cfgpin(SEP0611_GPE9, SSI2_TXD);
		sep0611_gpio_cfgpin(SEP0611_GPE8, SSI2_RXD);
		clk = clk_get(NULL, "spi-2");
		break;
	
	case 3:
		sep0611_gpio_cfgpin(SEP0611_GPA10, SSI3_CLK);
		sep0611_gpio_cfgpin(SEP0611_GPA9, SSI3_CS);
		sep0611_gpio_cfgpin(SEP0611_GPA8, SSI3_TXD);
		sep0611_gpio_cfgpin(SEP0611_GPA7, SSI3_RXD);
		clk = clk_get(NULL, "spi-3");
		break;
	
	default:
		break;
	}

	printk("we have set hw->irq is %d\n",hw->irq);
	
	err = request_irq(hw->irq, sep0611_spi_irq, 0, pdev->name, hw);
	if (err) {
		dev_err(&pdev->dev, "Cannot claim IRQ\n");
		goto err_no_irq;
	}
	
	if(clk == NULL){
		printk("ERR:not found spi%d clk\n", pdev->id);
		return -ENODEV;
	}
	clk_enable(clk);
	hw->clk = clk;
 	
	sep0611_spi_initialsetup(hw);

	/* register our spi controller */ 
	err = spi_bitbang_start(&hw->bitbang);
	if (err) {
		dev_err(&pdev->dev, "Failed to register SPI master\n");
		goto err_register;
	}
	printk("%s:register SPI master\n",__FUNCTION__);
	return 0;

 err_register:
	clk_disable(hw->clk);					/*we do not have the clock modules*/

 err_no_clk:
	free_irq(hw->irq, hw);

 err_no_irq:
	iounmap(hw->regs);

 err_no_iomap:
	release_resource(hw->ioarea);
	kfree(hw->ioarea);

 err_no_iores:
 err_no_pdata:
	spi_master_put(hw->master);;

 err_nomem:
	return err;
}

static int __exit sep0611_spi_remove(struct platform_device *dev)
{
	struct sep0611_spi *hw = platform_get_drvdata(dev);

	platform_set_drvdata(dev, NULL);

	spi_unregister_master(hw->master);

	clk_disable(hw->clk);                 /*we do not have the clock modules*/
	clk_put(hw->clk);                    /*we do not have the clock modules*/

	free_irq(hw->irq, hw);
	iounmap(hw->regs);

	release_resource(hw->ioarea);
	kfree(hw->ioarea);

	spi_master_put(hw->master);
	return 0;
}


#ifdef CONFIG_PM

static int sep0611_spi_suspend(struct platform_device *pdev, pm_message_t msg)
{
	struct sep0611_spi *hw = platform_get_drvdata(pdev);
	clk_disable(hw->clk);                 /*we do not have the clock modules*/
	return 0;
}

static int sep0611_spi_resume(struct platform_device *pdev)
{
	struct sep0611_spi *hw = platform_get_drvdata(pdev);
	struct clk *clk;
	
	switch(pdev->id){
	case 1:
		sep0611_gpio_cfgpin(SEP0611_GPE15, SSI1_CLK);
		sep0611_gpio_cfgpin(SEP0611_GPE14, SSI1_CS);
		sep0611_gpio_cfgpin(SEP0611_GPE13, SSI1_TXD);
		sep0611_gpio_cfgpin(SEP0611_GPE12, SSI1_RXD);
		clk = clk_get(NULL, "spi-1");
		break;
		
	case 2:
		sep0611_gpio_cfgpin(SEP0611_GPE11, SSI2_CLK);
		sep0611_gpio_cfgpin(SEP0611_GPE10, SSI2_CS);
		sep0611_gpio_cfgpin(SEP0611_GPE9, SSI2_TXD);
		sep0611_gpio_cfgpin(SEP0611_GPE8, SSI2_RXD);
		clk = clk_get(NULL, "spi-2");
		break;
	
	case 3:
		sep0611_gpio_cfgpin(SEP0611_GPA10, SSI3_CLK);
		sep0611_gpio_cfgpin(SEP0611_GPA9, SSI3_CS);
		sep0611_gpio_cfgpin(SEP0611_GPA8, SSI3_TXD);
		sep0611_gpio_cfgpin(SEP0611_GPA7, SSI3_RXD);
		clk = clk_get(NULL, "spi-3");
		break;
	
	default:
		break;
	}

	clk_enable(clk);

	sep0611_spi_initialsetup(hw);
	return 0;
}

#else
#define sep0611_spi_suspend NULL
#define sep0611_spi_resume  NULL
#endif

MODULE_ALIAS("platform:sep0611-spi");
static struct platform_driver sep0611_spi_driver = {
	.remove		= __exit_p(sep0611_spi_remove),
	.suspend	= sep0611_spi_suspend,
	.resume		= sep0611_spi_resume,
	.driver		= {
	.name	= "sep0611-spi",
	.owner	= THIS_MODULE,
	},
};

static int __init sep0611_spi_init(void)
{
	return platform_driver_probe(&sep0611_spi_driver, sep0611_spi_probe);
}

static void __exit sep0611_spi_exit(void)
{
        platform_driver_unregister(&sep0611_spi_driver);
}

module_init(sep0611_spi_init);
module_exit(sep0611_spi_exit);

MODULE_DESCRIPTION("sep0611 SPI Driver");
MODULE_AUTHOR("liyu, <allenseu@gmai.com>");
MODULE_LICENSE("GPL");
