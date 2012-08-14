/*
 * CH37X HCD (SPI to USB Host Controller Driver) SPI part
 *
 * Copyright (C) 2012 SEUIC
 *
 * Author : cgm <chenguangming@wxseuic.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * cgm 2012.4.11 create the file to support CH37x USB controller, v1.0
 * cgm 2012.6.27 update the version to use SPI FIFO, v1.1
 */

#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/usb.h>

#include <asm/io.h>
#include <mach/hardware.h>
#include <mach/dma.h>
#include <mach/regs-gpio.h>
#include <board/board.h>

#include "../../core/hcd.h"
#include "ch37x.h"
#include "ch374_hw.h"

extern struct ch37x *ch37x;

typedef struct {
	struct resource	*ioarea;
	void __iomem	*regs;

	uint8_t *tx;
	uint8_t *rx;
	dma_addr_t tx_addr;
	dma_addr_t rx_addr;

	struct completion done;
	spinlock_t lock;
} CH37X_SPI;

static CH37X_SPI gCh37xSpi;
static inline void ch37x_spi_Disable_dma_txrx(void);
static inline void ch37x_spi_Enable_dma_txrx(dma_addr_t tx_addr, uint32_t tx_size, dma_addr_t rx_addr, uint32_t rx_size);
static u8 bait[CH37X_MAX_ENDPOINT_SIZE];
static dma_addr_t bait_phys;

inline uint8_t ch37x_spi_exchange(uint8_t d)
{  	
	writel(d, SPI_DR);
	while (!(readl(SPI_SR) & (0x1 << 3)));
	return readl(SPI_DR);
}

#define	ch37x_spi_out_byte(d) 	ch37x_spi_exchange(d)
#define	ch37x_spi_in_byte()		ch37x_spi_exchange(0xFF)

inline void ch37x_spi_start(uint8_t addr, uint8_t cmd)
{
	SEP0611_INT_DISABLE(SEP0611_CH37X_INTSRC);
	writel(0x1, SPI_SER);
	ch37x_spi_out_byte(addr); 
	ch37x_spi_out_byte(cmd);
}

inline void ch37x_spi_stop(void)
{
	writel(0x0, SPI_SER);
	SEP0611_INT_ENABLE(SEP0611_CH37X_INTSRC);
}

uint8_t ch37x_readb(uint8_t addr)
{
	uint8_t tmp;

	SEP0611_INT_DISABLE(SEP0611_CH37X_INTSRC);
	writel(addr, SPI_DR);
	writel(CMD_CH37X_SPI_READ, SPI_DR);
	writel(0xff, SPI_DR);
	writel(0x1, SPI_SER);
	while(readl(SPI_RXFLR) != 3);

	tmp = readl(SPI_DR);
	tmp = readl(SPI_DR);
	tmp = readl(SPI_DR);

	writel(0x0, SPI_SER);
	SEP0611_INT_ENABLE(SEP0611_CH37X_INTSRC);

	return tmp;
}

void ch37x_writeb(uint8_t addr, uint8_t data)
{
	uint8_t tmp;
	volatile int i;

	SEP0611_INT_DISABLE(SEP0611_CH37X_INTSRC);
	writel(addr, SPI_DR);
	writel(CMD_CH37X_SPI_WRITE, SPI_DR);
	writel(data, SPI_DR);
	writel(0x1, SPI_SER);
	while(readl(SPI_RXFLR) != 3);

	tmp = readl(SPI_DR);
	tmp = readl(SPI_DR);
	tmp = readl(SPI_DR);

	writel(0x0, SPI_SER);
	SEP0611_INT_ENABLE(SEP0611_CH37X_INTSRC);
}

void ch37x_read_fifo_cpu(uint8_t *buf, int len)
{
	int i;
	u8 *p = buf;
	int l = len;

	ch37x_spi_start(RAM_HOST_RECV, CMD_CH37X_SPI_READ);
	for (i = 0; i < l; i++)
		*p++ = ch37x_spi_in_byte();
	ch37x_spi_stop();
}

void ch37x_read_fifo_dma(uint8_t *buf, int len)
{
	int i;
	u8 *p = buf;
	int l = len;

	ch37x_spi_start(RAM_HOST_RECV, CMD_CH37X_SPI_READ);
	for (i = 0; i < l; i++)
		*p++ = ch37x_spi_in_byte();
	ch37x_spi_stop();
}

void ch37x_write_fifo_cpu(uint8_t *buf, int len)
{
#if 0 //not use SPI FIFO
	int i;
	u8 *p = buf;
	int l = len;
	ch37x_spi_start(RAM_HOST_TRAN, CMD_CH37X_SPI_WRITE);
	for (i = 0; i < l; i++)
		ch37x_spi_out_byte(*p++);
	ch37x_spi_stop();
#else
	uint8_t *pout = buf;
	uint8_t tmp;
	uint8_t *out_end = buf + len;
	volatile int i;

	SEP0611_INT_DISABLE(SEP0611_CH37X_INTSRC);
	writel(RAM_HOST_TRAN, SPI_DR);
	writel(CMD_CH37X_SPI_WRITE, SPI_DR);
	writel(0x1, SPI_SER);
	while(pout < out_end){
		if(readl(SPI_SR) & (1 << 1)){
			writel(*pout++, SPI_DR);
			for(i=0;i<8;i++);
		}
	}
	while(readl(SPI_SR) & (1 << 3))
		tmp = readl(SPI_DR);
	writel(0x0, SPI_SER);
	SEP0611_INT_ENABLE(SEP0611_CH37X_INTSRC);
#endif
}

void ch37x_write_fifo_dma(uint8_t *buf, int len)
{
	int size_4 = len & ~0x03;

	ch37x_spi_start(RAM_HOST_TRAN, CMD_CH37X_SPI_WRITE);
	if (size_4) {
		memcpy(gCh37xSpi.tx, buf, size_4);
		ch37x_spi_Enable_dma_txrx(gCh37xSpi.tx_addr, size_4, bait_phys, size_4);
	}
}

static inline void ch37x_spi_Tx_by_dma(dma_addr_t bus_addr, uint32_t size)
{		
	/* program SARx and DARx*/
	writel(bus_addr, DMAC2_SAR_CH(DMA_SSI2_TX_CH));
	writel(SSI2_DR, DMAC2_DAR_CH(DMA_SSI2_TX_CH));

	/* program CTLx and CFGx */
	writel(DMAC_DONE(0) | DMAC_BLK_TRAN_SZ(size >> 2), DMAC2_CTL_CH(DMA_SSI2_TX_CH) + 4);
#define DMA_SSI2_CTL_L_WR	\
	     ( DMAC_SRC_LLP_EN(0)     | DMAC_DST_LLP_EN(0)         | DMAC_SRC_MASTER_SEL(0)	\
   	  	 | DMAC_DST_MASTER_SEL(1) | DMAC_TRAN_TYPE_FLOW_CTL(1) | DMAC_DST_SCAT_EN(0)	\
   	  	 | DMAC_SRC_GATH_EN(0)    | DMAC_SRC_BST_SZ(0)         | DMAC_DST_BST_SZ(1)		\
   	  	 | DMAC_SRC_INCR(0)       | DMAC_DST_INCR(2)           | DMAC_SRC_TRAN_WIDTH(2)	\
         | DMAC_DST_TRAN_WIDTH(0) | DMAC_INT_EN(0) )
	writel(DMA_SSI2_CTL_L_WR, DMAC2_CTL_CH(DMA_SSI2_TX_CH));

#define DMA_SSI2_CFG_H_WR	\
		 ( DMAC_DST_PER(3)         | DMAC_SRC_PER(2)  | DMAC_SRC_STAT_UPD_EN(0)	\
   	  	 | DMAC_DST_STAT_UPD_EN(0) | DMAC_PROT_CTL(1) | DMAC_FIFO_MODE(1) 		\
   	  	 | DMAC_FLOW_CTL_MODE(0) )
	writel(DMA_SSI2_CFG_H_WR, DMAC2_CFG_CH(DMA_SSI2_TX_CH) + 4);
	
#define DMA_SSI2_CFG_L_WR	\
		 ( DMAC_AUTO_RELOAD_DST(0) | DMAC_AUTO_RELOAD_SRC(0) | DMAC_MAX_AMBA_BST_LEN(0)	\
   	  	 | DMAC_SRC_HS_POL(0)      | DMAC_DST_HS_POL     (0) | DMAC_BUS_LOCK(0)			\
   	  	 | DMAC_CH_LOCK(0)         | DMAC_BUS_LOCK_LEVEL (0) | DMAC_CH_LOCK_LEVEL(0)	\
   	  	 | DMAC_HW_SW_SEL_SRC(1)   | DMAC_HW_SW_SEL_DST  (0) | DMAC_FIFO_EMPTY(1)		\
		 | DMAC_CH_SUSP(0)         | DMAC_CH_PRIOR(0) )
	writel(DMA_SSI2_CFG_L_WR, DMAC2_CFG_CH(DMA_SSI2_TX_CH));

    /* enable channelx */
	writel(DMAC_CH_WRITE_EN(1 << DMA_SSI2_TX_CH) | DMAC_CH_EN(1 << DMA_SSI2_TX_CH), DMAC2_CHEN_V);
}

static inline void ch37x_spi_Rx_by_dma(dma_addr_t bus_addr, uint32_t size)
{	
	/* program SARx and DARx */
	writel(SSI2_DR, DMAC2_SAR_CH(DMA_SSI2_RX_CH));
	writel(bus_addr, DMAC2_DAR_CH(DMA_SSI2_RX_CH));

	/* program CTLx and CFGx */
	writel(DMAC_DONE(0) | DMAC_BLK_TRAN_SZ(size), DMAC2_CTL_CH(DMA_SSI2_RX_CH) + 4);
#define DMA_SSI2_CTL_L_RD	\
	     ( DMAC_SRC_LLP_EN(0)     | DMAC_DST_LLP_EN(0)         | DMAC_SRC_MASTER_SEL(1)	\
   	  	 | DMAC_DST_MASTER_SEL(0) | DMAC_TRAN_TYPE_FLOW_CTL(2) | DMAC_DST_SCAT_EN(0)	\
   	  	 | DMAC_SRC_GATH_EN(0)    | DMAC_SRC_BST_SZ(1)         | DMAC_DST_BST_SZ(0)		\
   	  	 | DMAC_SRC_INCR(2)       | DMAC_DST_INCR(0)           | DMAC_SRC_TRAN_WIDTH(0)	\
         | DMAC_DST_TRAN_WIDTH(2) | DMAC_INT_EN(1) )
	writel(DMA_SSI2_CTL_L_RD, DMAC2_CTL_CH(DMA_SSI2_RX_CH));

#define DMA_SSI2_CFG_H_RD	\
		 ( DMAC_DST_PER(3)        | DMAC_SRC_PER(2) | DMAC_SRC_STAT_UPD_EN(0)	\
   	  	 | DMAC_DST_STAT_UPD_EN(0)| DMAC_PROT_CTL(1)| DMAC_FIFO_MODE(1)        \
   	  	 | DMAC_FLOW_CTL_MODE(0) )
	writel(DMA_SSI2_CFG_H_RD, DMAC2_CFG_CH(DMA_SSI2_RX_CH) + 4);
	
#define DMA_SSI2_CFG_L_RD	\
		 ( DMAC_AUTO_RELOAD_DST(0) | DMAC_AUTO_RELOAD_SRC(0) | DMAC_MAX_AMBA_BST_LEN(0)	\
   	  	 | DMAC_SRC_HS_POL(0)      | DMAC_DST_HS_POL(0)      | DMAC_BUS_LOCK(0)	        \
   	  	 | DMAC_CH_LOCK(0)         | DMAC_BUS_LOCK_LEVEL(0)  | DMAC_CH_LOCK_LEVEL(0)	\
   	  	 | DMAC_HW_SW_SEL_SRC(0)   | DMAC_HW_SW_SEL_DST(1)   | DMAC_FIFO_EMPTY(1)	    \
		 | DMAC_CH_SUSP(0)         | DMAC_CH_PRIOR(0) )
	writel(DMA_SSI2_CFG_L_RD, DMAC2_CFG_CH(DMA_SSI2_RX_CH));
	
    /* enable channelx */
	writel(DMAC_CH_WRITE_EN(1 << DMA_SSI2_RX_CH) | DMAC_CH_EN(1 << DMA_SSI2_RX_CH), DMAC2_CHEN_V);
}

static int devid; /* shared interrupts must pass in a real dev-ID */
irqreturn_t ch37x_spi_dma_irq_handler(int irq, void *devid)
{	
	if (readl(DMAC2_STATBLK_V) & (1 << DMA_SSI2_RX_CH)) {
		writel(1 << DMA_SSI2_RX_CH, DMAC2_CLRBLK_V);
		writel(DMAC_INT_WE(1 << (DMA_SSI2_RX_CH + 8)) | DMAC_INT_MASK(0 << DMA_SSI2_RX_CH), DMAC2_MASKBLK_V);

		struct ch37x_td *td = ch37x->current_td;		
		ch37x_spi_Disable_dma_txrx();
		
		//if(td->use_dma){
		if(1){
			ch37x_writeb(REG_USB_ADDR, td->address);
			ch37x_writeb(REG_USB_LENGTH, td->length);
			ch37x_writeb(REG_USB_H_PID, M_MK_HOST_PID_ENDP(td->pid, td->epnum));
			if( td->state != 3 ) printk("err td state %d-3\n",td->state);
			td->state = 1;
			ch37x_writeb(REG_USB_H_CTRL, td->tog ? ( BIT_HOST_START | BIT_HOST_TRAN_TOG | BIT_HOST_RECV_TOG ) : BIT_HOST_START);
			td->use_dma = 0;
		}
		else
			printk("err.........\n");
	}

	return IRQ_HANDLED;
}

static inline void ch37x_spi_Enable_dma_txrx(dma_addr_t tx_addr, uint32_t tx_size, dma_addr_t rx_addr, uint32_t rx_size)
{	
	ch37x_spi_Tx_by_dma(tx_addr, tx_size);
	ch37x_spi_Rx_by_dma(rx_addr, rx_size);
	writel(DMAC_INT_WE(1 << (DMA_SSI2_RX_CH + 8)) | DMAC_INT_MASK(1 << DMA_SSI2_RX_CH), DMAC2_MASKBLK_V);

	writel(0x4, SPI_DMATDLR);
	writel(0x3, SPI_DMARDLR);
	writel(0x3, SPI_DMACR);
}

static inline void ch37x_spi_Disable_dma_txrx(void)
{	
	writel(0x0, SPI_DMACR);
}

static inline void ch37x_spi_gio_cfg(void)
{
	sep0611_gpio_cfgpin(SEP0611_GPE11, SSI2_CLK);	
	sep0611_gpio_cfgpin(SEP0611_GPE10, SSI2_CS);
	sep0611_gpio_cfgpin(SEP0611_GPE9, SSI2_TXD);
	sep0611_gpio_cfgpin(SEP0611_GPE8, SSI2_RXD);
}

static void ch37x_spi_cfg(void)
{   
	ch37x_spi_gio_cfg();

	writel(0x0, SPI_SPIENR); 	
	writel(0x7, SPI_CONTROL0);  
	writel(0x0, SPI_SER);
	writel(0x4, SPI_BAUDR);
	writel(0x4, SPI_TXFTLR);
	writel(0x0, SPI_IMR);
	writel(0x0, SPI_DMACR);
	writel(0x1, SPI_SPIENR);
}

static int __init ch37x_spi_probe(struct platform_device *pdev)  
{
	int ret = 0;

	printk("%s\n", __func__);
	
	gCh37xSpi.tx = (uint8_t *)dma_alloc_coherent(NULL, CH37X_MAX_ENDPOINT_SIZE * 2, (dma_addr_t *)&gCh37xSpi.tx_addr, GFP_KERNEL);
	if (gCh37xSpi.tx == NULL) {
		printk("%s: dma_alloc_coherent fail.\n", __func__);
		ret = -ENOMEM;
		goto err_dma_alloc_coherent;
	}
	gCh37xSpi.rx = gCh37xSpi.tx + CH37X_MAX_ENDPOINT_SIZE;
	gCh37xSpi.rx_addr = gCh37xSpi.tx_addr + CH37X_MAX_ENDPOINT_SIZE;

	memset(bait, 0xff, CH37X_MAX_ENDPOINT_SIZE);
	bait_phys = virt_to_phys(bait);

	ret = request_irq(INTSRC_DMAC2, ch37x_spi_dma_irq_handler, IRQF_SHARED, "dmac2_4_ch37x_spi", &devid);
	if (ret) {
		goto err_request_irq;
	}

	init_completion(&gCh37xSpi.done);
	spin_lock_init(&gCh37xSpi.lock);
	
	ch37x_spi_cfg();
	
	return 0;

err_request_irq:
	dma_free_coherent(NULL, CH37X_MAX_ENDPOINT_SIZE * 2, gCh37xSpi.tx, gCh37xSpi.tx_addr);
err_dma_alloc_coherent:
	return ret;	
}

static int __exit ch37x_spi_remove(struct platform_device *dev)
{
	free_irq(INTSRC_DMAC2, &devid);
	dma_free_coherent(NULL, CH37X_MAX_ENDPOINT_SIZE * 2, gCh37xSpi.tx, gCh37xSpi.tx_addr);
 	iounmap(gCh37xSpi.regs);
	release_resource(gCh37xSpi.ioarea);
	kfree(gCh37xSpi.ioarea);
	
	return 0;
}

#ifdef CONFIG_PM
static int ch37x_spi_suspend(struct platform_device *pdev, pm_message_t msg)
{
	return 0;
}

static int ch37x_spi_resume(struct platform_device *pdev)
{
	ch37x_spi_cfg();
	
	return 0;
}
#else
#define ch37x_spi_suspend NULL
#define ch37x_spi_resume  NULL
#endif

struct platform_driver ch37x_spi_driver = {
	.remove		= __exit_p(ch37x_spi_remove),
	.probe		= ch37x_spi_probe,
	.suspend	= ch37x_spi_suspend,
	.resume		= ch37x_spi_resume,
	.driver		= {
		.name	= "ch37x-spi",
		.owner	= THIS_MODULE,
	},
};
EXPORT_SYMBOL(ch37x_spi_driver);

MODULE_ALIAS("platform:ch37x-spi");
MODULE_DESCRIPTION("CH37X SPI Driver");
MODULE_AUTHOR("cgm, <chenguangming@wxseuic.com>");
MODULE_LICENSE("GPL");
