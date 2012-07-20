/* linux/drivers/mmc/host/sep0611_sdio.c
 *
 * Copyright (c) 2009-2011 SEUIC
 *  fjj <fanjianjun@wxseuic.com>
 *
 * Southeast University ASIC SoC support
 *
 * SDIO Driver for SEP0611 Board
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Modifications:
 *	xx-xx-xxxx 	zwm		initial version
 *  09-08-2011 	fjj    	second version
 */ 

#include <linux/module.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/timer.h>
#include <linux/dma-mapping.h>
#include <linux/dw_dmac.h>

#include <asm/sizes.h>
#include <asm/scatterlist.h>
#include <asm/dma.h>
#include <mach/dma.h>
#include <mach/hardware.h>
#include <board/board.h>

#include <linux/mmc/card.h>
#include <linux/mmc/host.h>
#include <linux/mmc/mmc.h>
#include <linux/mmc/sd.h>

#include <mach/regs-sdio.h>

//#define SDIO_DEBUG
#ifdef  SDIO_DEBUG
#define sdio_debug(fmt...)	printk(fmt)
#else
#define sdio_debug(fmt...)
#endif

struct sepsdio_host {
	struct mmc_host	*mmc;

	struct mmc_request *mrq;
	struct mmc_command *cmd;
	struct mmc_data	*data;

	enum dma_data_direction dir;
	int seg_num;
	int cur_seg;

	spinlock_t lock;

	unsigned int hclk;

	struct completion cmd_complete_request;	
	struct completion data_complete_request;

	struct timer_list timer;
	int card_status;
};

#ifdef CONFIG_ATHEROS_AR6003
struct mmc_host *wifi_mmc_host = NULL;
EXPORT_SYMBOL(wifi_mmc_host);
#endif 

static inline void clear_any_pending_int(struct sepsdio_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	writel(0x1ffff, SDIO2_RINTSTS_V);
	spin_unlock_irqrestore(&host->lock, flags); 
}

static inline void disable_any_int(struct sepsdio_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	writel(0, SDIO2_INTMASK_V);
	spin_unlock_irqrestore(&host->lock, flags); 
}

static inline void enable_command_done_int(struct sepsdio_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	writel(SDIO_INT_CD(1), SDIO2_INTMASK_V);
	spin_unlock_irqrestore(&host->lock, flags); 
}

static inline void enable_data_transfer_over_int(struct sepsdio_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	writel(SDIO_INT_DTO(1), SDIO2_INTMASK_V);
	spin_unlock_irqrestore(&host->lock, flags); 
}

static inline void dma_write(dma_addr_t bus_addr, int blk_size)
{
	sdio_debug("[%s], bus_addr: 0x%x, blk_size: 0x%x\n", __func__, bus_addr, blk_size);	

	/* program SARx */
	writel(bus_addr, DMAC1_SAR_CH(DMA_SDIO2_CH));

	/* program DARx */
	writel(SDIO2_BASE + 0x100, DMAC1_DAR_CH(DMA_SDIO2_CH));

	/* program CTLx */	
	writel(DMA_CH_FOR_SDIO2_CTL_L_WR, DMAC1_CTL_CH(DMA_SDIO2_CH));
	writel(DMAC_DONE(0) | DMAC_BLK_TRAN_SZ(blk_size >> 2), DMAC1_CTL_CH(DMA_SDIO2_CH) + 4);
	
	/* program CFGx */	
	writel(DMA_CH_FOR_SDIO2_CFG_L_WR, DMAC1_CFG_CH(DMA_SDIO2_CH));
	writel(DMA_CH_FOR_SDIO2_CFG_H_WR, DMAC1_CFG_CH(DMA_SDIO2_CH) + 4);

	/* enable dma channel */
	writel(DMAC_CH_WRITE_EN(1 << DMA_SDIO2_CH) | DMAC_CH_EN(1 << DMA_SDIO2_CH), DMAC1_CHEN_V);
}

static inline void dma_read(dma_addr_t bus_addr, int blk_size)
{
	sdio_debug("[%s], bus_addr: 0x%x, blk_size: 0x%x\n", __func__, bus_addr, blk_size);	

	/* program SARx */
	writel(SDIO2_BASE + 0x100, DMAC1_SAR_CH(DMA_SDIO2_CH));

	/* program DARx */
	writel(bus_addr, DMAC1_DAR_CH(DMA_SDIO2_CH));

	/* program CTLx */	
	writel(DMA_CH_FOR_SDIO2_CTL_L_RD, DMAC1_CTL_CH(DMA_SDIO2_CH));
	writel(DMAC_DONE(0) | DMAC_BLK_TRAN_SZ(blk_size >> 2), DMAC1_CTL_CH(DMA_SDIO2_CH) + 4);

	/* program CFGx */
	writel(DMA_CH_FOR_SDIO2_CFG_L_RD, DMAC1_CFG_CH(DMA_SDIO2_CH));
	writel(DMA_CH_FOR_SDIO2_CFG_H_RD, DMAC1_CFG_CH(DMA_SDIO2_CH) + 4);

	/* enable dma channel */
	writel(DMAC_CH_WRITE_EN(1 << DMA_SDIO2_CH) | DMAC_CH_EN(1 << DMA_SDIO2_CH), DMAC1_CHEN_V);
}

static inline void enable_blk_transfer_complete_int(struct sepsdio_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	writel(DMAC_INT_WE(1 << (DMA_SDIO2_CH + 8)) | DMAC_INT_MASK(1 << DMA_SDIO2_CH), DMAC1_MASKBLK_V);	
	spin_unlock_irqrestore(&host->lock, flags);
}

static inline void disable_blk_transfer_complete_int(struct sepsdio_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	writel(DMAC_INT_WE(1 << (DMA_SDIO2_CH + 8)) | DMAC_INT_MASK(0 << DMA_SDIO2_CH), DMAC1_MASKBLK_V);
	spin_unlock_irqrestore(&host->lock, flags);	
}

static inline void clear_blk_transfer_complete_int(struct sepsdio_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	writel(1 << DMA_SDIO2_CH, DMAC1_CLRBLK_V);
	spin_unlock_irqrestore(&host->lock, flags);
}

irqreturn_t dma_chan_for_sdio2_irq_handler(int irq, void *devid)
{
	if (readl(DMAC1_STATBLK_V) & (1 << DMA_SDIO2_CH)) {
		struct sepsdio_host *host = devid;
		dma_addr_t bus_addr;
		int seg_len;

		clear_blk_transfer_complete_int(host);

		host->cur_seg++;

		if (host->cur_seg < host->seg_num) {
			bus_addr = sg_dma_address(&host->data->sg[host->cur_seg]);
			seg_len = sg_dma_len(&host->data->sg[host->cur_seg]);

			sdio_debug("[%s], cur_seg=%d, bus_addr=0x%08x, seg_len=0x%x\n", __func__, host->cur_seg, bus_addr, seg_len);

			if (host->dir == DMA_TO_DEVICE) /* write data to sd */
				dma_write(bus_addr, seg_len);
			else /* read data from sd */
				dma_read(bus_addr, seg_len);
		} else 
			sdio_debug("[%s], up to the last segment\n", __func__);
	}

	return IRQ_HANDLED;
}

static inline void sepsdio_dma_transfer(struct sepsdio_host *host)
{	
	dma_addr_t bus_addr;
	int seg_len;

	host->seg_num = dma_map_sg(mmc_dev(host->mmc), host->data->sg, host->data->sg_len, host->dir);

	sdio_debug("[%s], seg_num = %d\n", __func__, host->seg_num);	

	host->cur_seg = 0;

	bus_addr = sg_dma_address(&host->data->sg[host->cur_seg]);
	seg_len = sg_dma_len(&host->data->sg[host->cur_seg]);

	sdio_debug("[%s], cur_seg=%d, bus_addr=0x%08x, seg_len=0x%x\n", __func__, host->cur_seg, bus_addr, seg_len);

	clear_blk_transfer_complete_int(host);

	if (host->seg_num > 1)
		enable_blk_transfer_complete_int(host);
	else
		disable_blk_transfer_complete_int(host);

	if (host->dir == DMA_TO_DEVICE) /* write data to sd */
		dma_write(bus_addr, seg_len);
	else /* read data from sd */
		dma_read(bus_addr, seg_len);
}

static inline void sepsdio_prepare_data(struct sepsdio_host *host, struct mmc_data *data)
{	
	host->data = data;

	/* block size register */
	writel(data->blksz, SDIO2_BLKSIZ_V);
	/* bytes count register */
	writel(data->blksz * data->blocks, SDIO2_BYTCNT_V);

	sdio_debug("blksz=%d, blocks=%d\n", data->blksz, data->blocks);

	if (host->data->flags & MMC_DATA_WRITE)
		host->dir = DMA_TO_DEVICE;
	else
		host->dir = DMA_FROM_DEVICE;
	
	sepsdio_dma_transfer(host);	
}

static void sepsdio_start_cmd(struct sepsdio_host *host, struct mmc_command *cmd)
{
	unsigned int cmd_reg = 0;

	sdio_debug("[%s], cmd:%d\n", __func__, cmd->opcode);

	host->cmd = cmd;
			
	if (cmd->data)
		sepsdio_prepare_data(host, cmd->data);

	/* command index */
	cmd_reg |= cmd->opcode;

	/* expect response */
	if (cmd->flags & MMC_RSP_PRESENT)
		cmd_reg |= SDIO_CMD_RESP_EXPE(1);
	
	/* response length */
	if (cmd->flags & MMC_RSP_136)
		cmd_reg |= SDIO_CMD_LONG_RESP(1);	

	/* check response crc */
	if (cmd->flags & MMC_RSP_CRC)
		cmd_reg |= SDIO_CMD_CHK_RESP_CRC(1);

	if (cmd->data) {
		/* expect data */
		cmd_reg |= SDIO_CMD_HAVE_DAT_TRAN(1);

		/* write to card */
		if (cmd->data->flags & MMC_DATA_WRITE)
			cmd_reg |= SDIO_CMD_WRITE(1);
	}

	/* wait previous data transfer completion */
	if ((cmd->opcode != MMC_STOP_TRANSMISSION) && (cmd->opcode != MMC_SEND_STATUS))
		cmd_reg |= SDIO_CMD_WAIT_DAT(1);

	/* send initialization */
	if (cmd->opcode == MMC_GO_IDLE_STATE)
		cmd_reg |= SDIO_CMD_SEND_INIT(1);

	/* start command */
	cmd_reg |= SDIO_CMD_START;

	/* write command argument */
	writel(cmd->arg, SDIO2_CMDARG_V);
	/* write command, at this moment, this command starts */
	writel(cmd_reg, SDIO2_CMD_V);
}

static inline void reset_hardware(struct sepsdio_host *host)
{
	unsigned long flags;

	spin_lock_irqsave(&host->lock, flags);
	/* disable dma channel */
	writel(DMAC_CH_WRITE_EN(1 << DMA_SDIO2_CH) | DMAC_CH_EN(0 << DMA_SDIO2_CH), DMAC1_CHEN_V);
	while (readl(DMAC1_CHEN_V) & DMAC_CH_EN(1 << DMA_SDIO2_CH));
	/* reset fifo, dma */
	writel(readl(SDIO2_CTRL_V) | SDIO_DMA_RST(1) | SDIO_FIFO_RST(1), SDIO2_CTRL_V); 
	while (readl(SDIO2_CTRL_V) & SDIO_DMA_RST(1) & SDIO_FIFO_RST(1));
	spin_unlock_irqrestore(&host->lock, flags); 
}

static void sepsdio_request(struct mmc_host *mmc, struct mmc_request *mrq)
{
	struct sepsdio_host *host = mmc_priv(mmc);		
	
	host->mrq = mrq;

	sdio_debug("[%s], into\n", __func__);

	reset_hardware(host);

	clear_any_pending_int(host);

	sepsdio_start_cmd(host, mrq->cmd);

	init_completion(&host->cmd_complete_request);
	enable_command_done_int(host);
	wait_for_completion(&host->cmd_complete_request);

	if (!mrq->cmd->error && mrq->data) {
		init_completion(&host->data_complete_request);		
		enable_data_transfer_over_int(host);
		wait_for_completion(&host->data_complete_request);

		if (!mrq->data->error && mrq->stop) {
			init_completion(&host->cmd_complete_request);
			enable_command_done_int(host);
			wait_for_completion(&host->cmd_complete_request);
		}
	}

	disable_any_int(host);

	host->mrq = NULL;
	host->cmd = NULL;
	host->data = NULL;

	mmc_request_done(mmc, mrq);

	sdio_debug("[%s], exit\n", __func__);
}

static inline void calculate_clock_divider(struct sepsdio_host *host, struct mmc_ios *ios, unsigned char *f_div, unsigned char *s_div)
{
#ifdef SDIO_DEBUG
	unsigned char ahb_div;
	unsigned char clk_div0;
	unsigned int cclk_out;
#endif	

	if (ios->clock == host->mmc->f_min) { /* 400KHz */
		*f_div = 8;
		*s_div = (host->hclk / 16 + ios->clock - 1) / ios->clock;
	} else {	/* ios->clock == host->mmc->f_max, 50MHz */
		*f_div = (host->hclk / 2 + ios->clock - 1) / ios->clock * 2;
		*s_div = 0;
	}

#ifdef SDIO_DEBUG
	ahb_div = *f_div;

	if (*s_div == 0)
		clk_div0 = 1;
	else
		clk_div0 = *s_div * 2;	

	cclk_out = host->hclk / ahb_div / clk_div0;
	
	printk("[%s], hclk=%u, ahb_divider=%u, clk_divider0=%u, cclk_out=%d\n", 
		__func__, host->hclk, *f_div, *s_div, cclk_out);	
#endif
}

static inline void update_clock_register(void)
{
	writel( SDIO_CMD_START            | SDIO_CMD_UPDATE_CLK_REG_ONLY(1) | SDIO_CMD_CARD_NUM (0) \
          | SDIO_CMD_SEND_INIT    (0) | SDIO_CMD_STP_ABT            (0) | SDIO_CMD_WAIT_DAT (1) \
          | SDIO_CMD_SET_STOP     (0) | SDIO_CMD_STR_TRAN           (0) | SDIO_CMD_WRITE    (0) \
          | SDIO_CMD_HAVE_DAT_TRAN(0) | SDIO_CMD_CHK_RESP_CRC       (0) | SDIO_CMD_LONG_RESP(0) \
          | SDIO_CMD_RESP_EXPE    (0) | CMD0, SDIO2_CMD_V);
     
    /* wait for the CIU to take the command by polling for 0 on the start_cmd bit. */
	while ((readl(SDIO2_CMD_V) & 0x80000000) == 0x80000000);
}

static void sepsdio_set_ios(struct mmc_host *mmc, struct mmc_ios *ios)
{
	struct sepsdio_host *host = mmc_priv(mmc);
	/* f_div >> ahbclkdiv, s_div >> clk_divider0 */
	unsigned char f_div, s_div;

	sdio_debug("[%s], into\n", __func__);

	if (ios->clock) { 
		calculate_clock_divider(host, ios, &f_div, &s_div);
	
		/* stop clock */
		writel(SDIO_CCLK_LW_PWR(0) | SDIO_CCLK_EN(0), SDIO2_CLKENA_V);
		update_clock_register();

		writel(f_div, SDIO2_AHBCLKDIV_V);
		writel(s_div, SDIO2_CLKDIV_V);
		update_clock_register();

		/* re-enable clock */
		writel(SDIO_CCLK_LW_PWR(0) | SDIO_CCLK_EN(1), SDIO2_CLKENA_V);
		update_clock_register();
	}

	switch (ios->bus_width) {
		case MMC_BUS_WIDTH_1: {
			writel(0x0, SDIO2_CTYPE_V);
			sdio_debug("[%s], 1 bit mode\n", __func__);	
			break;
		}

		case MMC_BUS_WIDTH_4: {
			writel(0x1, SDIO2_CTYPE_V);
			sdio_debug("[%s], 4 bit mode\n", __func__);
			break;
		}

		default : {	/* MMC_BUS_WIDTH_8 */
			printk("sep sd controller doesn't support this bus width\n");
			break;
		}
	} 

	sdio_debug("[%s], exit\n", __func__);
}

static struct mmc_host_ops sepsdio_ops = {
	.request = sepsdio_request,
	.set_ios = sepsdio_set_ios,
};

static inline void sepsdio_command_done(struct sepsdio_host *host, unsigned int rintsts_reg)
{
	struct mmc_command *cmd = host->cmd;

	sdio_debug("[%s]\n", __func__);

	BUG_ON(rintsts_reg == 0);

	if (!cmd) {
		printk("[%s]: Got command done interrupt 0x%08x even "
				"though no command operation was in progress.\n",
				mmc_hostname(host->mmc), rintsts_reg);
		return;
	}

	if (cmd->flags & MMC_RSP_PRESENT) {
		if (cmd->flags & MMC_RSP_136) {
			cmd->resp[0] = readl(SDIO2_RESP3_V);	
			cmd->resp[1] = readl(SDIO2_RESP2_V);
			cmd->resp[2] = readl(SDIO2_RESP1_V);
			cmd->resp[3] = readl(SDIO2_RESP0_V);

			//sdio_debug("[%s], cmd:%d, arg:0x%08x, resp[0]=0x%08x, resp[1]=0x%08x, resp[2]=0x%08x, resp[3]=0x%08x\n",
			//		__func__, cmd->opcode, cmd->arg, cmd->resp[0], cmd->resp[1], cmd->resp[2], cmd->resp[3]);;
		} else {
			cmd->resp[0] = readl(SDIO2_RESP0_V);

			//sdio_debug("[%s], cmd:%d, arg:0x%08x, resp[0]=0x%08x\n",
			//		 __func__, cmd->opcode, cmd->arg, cmd->resp[0]);
		}
	}

	cmd->error = 0;

	if (rintsts_reg & SDIO_INT_RTO(1)) {                
     	cmd->error = -ETIMEDOUT; /* response timeout */
	} else if (rintsts_reg & (SDIO_INT_RE(1) | SDIO_INT_RCRC(1))) {
		cmd->error = -EILSEQ; /* response error or response crc error */
	}

	complete(&host->cmd_complete_request);		
}

static inline void sepsdio_data_transfer_over(struct sepsdio_host *host, unsigned int rintsts_reg)
{
	struct mmc_data *data = host->data;

	BUG_ON(rintsts_reg == 0);

	sdio_debug("[%s]\n", __func__);

	if (!data) {
		printk("[%s]: Got data transfer over interrupt 0x%08x even "
				"though no data operation was in progress.\n",
				mmc_hostname(host->mmc), rintsts_reg);
		return;
	}

	data->error = 0;

	if (rintsts_reg & SDIO_INT_DRTO(1)) {           
		data->error = -ETIMEDOUT;	/* data read timeout */	
	} else if (rintsts_reg & (SDIO_INT_DCRC(1) | SDIO_INT_SBE(1) | SDIO_INT_EBE(1))) {
		data->error = -EILSEQ;	/* data crc error or start bit error or ebe */
	}

	if (!data->error) {
		data->bytes_xfered = data->blocks * data->blksz;
	} else {	
		data->bytes_xfered = 0;
	}	

	complete(&host->data_complete_request);	

	if (data->stop)
		sepsdio_start_cmd(host, data->stop);
}

irqreturn_t sdio2_irq_handler(int irq, void *devid)
{
	struct sepsdio_host *host = devid;
	unsigned int mintsts_reg;
	unsigned int rintsts_reg;

	/* read masked interrupt status register */
	mintsts_reg = readl(SDIO2_MINTSTS_V);
	/* read raw interrupt status register */
	rintsts_reg = readl(SDIO2_RINTSTS_V);

	/* command done */
	if (mintsts_reg & SDIO_INT_CD(1)) {
		writel(SDIO_INT_RTO(1) | SDIO_INT_RCRC(1) | SDIO_INT_CD(1) | SDIO_INT_RE(1), SDIO2_RINTSTS_V);	
		sepsdio_command_done(host, rintsts_reg);
	}

	/* data transfer over */
	if (mintsts_reg & SDIO_INT_DTO(1)) {
		writel(SDIO_INT_EBE(1) | SDIO_INT_SBE(1) | SDIO_INT_DRTO(1) | SDIO_INT_DCRC(1) | SDIO_INT_DTO(1), SDIO2_RINTSTS_V);
		sepsdio_data_transfer_over(host, rintsts_reg);	
	}	

	return IRQ_HANDLED;
}

static void sdio2_init_gpio(void)
{
#ifdef SEP0611_WIFI_EN
	sep0611_gpio_cfgpin(SEP0611_WIFI_EN, SEP0611_GPIO_IO);
	sep0611_gpio_dirpin(SEP0611_WIFI_EN, SEP0611_GPIO_OUT);
	sep0611_gpio_setpin(SEP0611_WIFI_EN, GPIO_HIGH);
#endif

#ifdef SEP0611_WIFI_NRST
	sep0611_gpio_cfgpin(SEP0611_WIFI_NRST, SEP0611_GPIO_IO);
	sep0611_gpio_dirpin(SEP0611_WIFI_NRST, SEP0611_GPIO_OUT);
	sep0611_gpio_setpin(SEP0611_WIFI_NRST, GPIO_LOW);
#endif

#ifdef SEP0611_WIFI_RST
	sep0611_gpio_cfgpin(SEP0611_WIFI_RST, SEP0611_GPIO_IO);
	sep0611_gpio_dirpin(SEP0611_WIFI_RST, SEP0611_GPIO_OUT);
	sep0611_gpio_setpin(SEP0611_WIFI_RST, GPIO_LOW);
#endif

	mdelay(200);

	sep0611_gpio_cfgpin(SEP0611_GPA0, SD2_WP);
	sep0611_gpio_cfgpin(SEP0611_GPA1, SD2_DAT0);
	sep0611_gpio_cfgpin(SEP0611_GPA2, SD2_DAT1);
	sep0611_gpio_cfgpin(SEP0611_GPA3, SD2_DAT2);
	sep0611_gpio_cfgpin(SEP0611_GPA4, SD2_DAT3);
	sep0611_gpio_cfgpin(SEP0611_GPA5, SD2_CMD);
	sep0611_gpio_cfgpin(SEP0611_GPA6, SD2_CLK);
}

static void sdio2_conrtoller_init(struct sepsdio_host *host)
{
	sdio2_init_gpio();

	clear_any_pending_int(host);

	writel(0, SDIO2_INTMASK_V);

  	/* enable dma transfer, enable global inttrupt, reset controller, fifo, dma */ 
	writel( SDIO_DAT3_DETECT_EN(0) | SDIO_BIG_ENDIAN_EN(0) | SDIO_ABT_RD_DAT(0) | SDIO_SEND_IRQ_RESP(0) \
		  | SDIO_READ_WAIT     (0) | SDIO_DMA_EN       (1) | SDIO_INT_EN    (1) | SDIO_DMA_RST      (1) \
		  | SDIO_FIFO_RST      (1) | SDIO_CTRL_RST     (1),  SDIO2_CTRL_V); 
	while (readl(SDIO2_CTRL_V) & (SDIO_DMA_RST(1) | SDIO_FIFO_RST(1) | SDIO_CTRL_RST(1))); 

	writel(MSIZE8 | RX_WMARK(7) | TX_WMARK(8), SDIO2_FIFOTH_V);
}

static int sdio_get_card_status(void)
{
#ifdef SEP0611_WIFI_RST 
	return sep0611_gpio_getpin(SEP0611_WIFI_RST);
#endif

#ifdef SEP0611_WIFI_NRST
	return sep0611_gpio_getpin(SEP0611_WIFI_NRST);
#endif
}

static void sepsdio_card_detect(unsigned long data)
{
	struct sepsdio_host *host =(struct sepsdio_host *)data;
	int card_status = sdio_get_card_status();

	if (host->card_status != card_status) {
		host->card_status = card_status;
		mmc_detect_change(host->mmc, msecs_to_jiffies(500));
	}

	mod_timer(&host->timer, jiffies + msecs_to_jiffies(HZ/2));
}

static int sepsdio_probe(struct platform_device *pdev)
{
 	struct mmc_host *mmc = NULL;
	struct sepsdio_host *host = NULL;
	struct clk *sdio_clk = NULL;
	int ret;

	mmc = mmc_alloc_host(sizeof(struct sepsdio_host), &pdev->dev);
	if (!mmc)  {
		printk("[%s], mmc alloc host fail\n", __func__);
		return -ENOMEM;
	}

	host = mmc_priv(mmc);
	host->mmc = mmc;

#ifdef CONFIG_ATHEROS_AR6003
	wifi_mmc_host = mmc;
#endif 
	
	platform_set_drvdata(pdev, mmc);

	mmc->ops = &sepsdio_ops;
	mmc->f_min = 400000; /* 400KHz */
	mmc->f_max = 50000000; /* 50MHz */
	mmc->ocr_avail = MMC_VDD_32_33 | MMC_VDD_33_34;
	mmc->caps = MMC_CAP_4_BIT_DATA | MMC_CAP_SD_HIGHSPEED;
	
	mmc->max_hw_segs = 128;
	mmc->max_phys_segs = 128;
	mmc->max_seg_size = 512 * 30;	/* limited by dma block_ts field: 4095 * 4 ~= 512 * 31 */
	mmc->max_blk_size = 512;
	mmc->max_blk_count = 128 * 30;
	mmc->max_req_size = mmc->max_blk_size * mmc->max_blk_count;

	ret = request_irq(INTSRC_DMAC1, dma_chan_for_sdio2_irq_handler, IRQF_SHARED, "dmac1_4_sdio2", host);
	if (ret) {
		printk("[%s], request INTSRC_DMAC1 irq fail\n", __func__);
		goto out0;
	}
	
	ret = request_irq(INTSRC_SDIO2, sdio2_irq_handler, IRQF_DISABLED, "sdio2", host);
	if (ret) {
		printk("[%s], request INTSRC_SDIO2 irq fail\n", __func__);
		goto out1;
	}

	sdio_clk = clk_get(NULL, "sdio-2");
	if (sdio_clk == NULL) {
		printk("fail to get sdio2 clk, assume 320MiB");
		host->hclk = 320000000;
	} else
		host->hclk = clk_get_rate(sdio_clk);

	/* inital sdio2 controler */
	sdio2_conrtoller_init(host);

	mmc_add_host(mmc);	

	host->card_status = sdio_get_card_status();
	init_timer(&host->timer);
	host->timer.function = sepsdio_card_detect;
	host->timer.data = (unsigned long)host;
	host->timer.expires = jiffies + msecs_to_jiffies(HZ/2);
	add_timer(&host->timer);
	return 0;

 out1:
	free_irq(INTSRC_DMAC1, host);
 out0:
	mmc_free_host(mmc);
	return ret;
} 

static int sepsdio_remove(struct platform_device *pdev)
{	
	struct mmc_host *mmc = platform_get_drvdata(pdev);

	if (mmc) {
		struct sepsdio_host *host = mmc_priv(mmc);
		
		platform_set_drvdata(pdev, NULL);

		mmc_remove_host(mmc);
		mmc_free_host(mmc);
		
		free_irq(INTSRC_SDIO2, host);
		free_irq(INTSRC_DMAC1, host);
	}

	return 0;
}

#ifdef CONFIG_PM
static int sepsdio_suspend(struct platform_device *dev, pm_message_t state)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);
	int ret = 0;
	
	printk("%s\n", __func__);  

	if (mmc)
		ret = mmc_suspend_host(mmc, state);

	return ret;
}

static int sepsdio_resume(struct platform_device *dev)
{
	struct mmc_host *mmc = platform_get_drvdata(dev);	
	int ret = 0;
	
	printk("%s\n", __func__);

	if (mmc)
		ret = mmc_resume_host(mmc);

	return ret;
}
#else
#define sepsdio_suspend	NULL
#define sepsdio_resume	NULL
#endif

static struct platform_driver sepsdio_driver = {
	.probe		= sepsdio_probe,
	.remove		= sepsdio_remove,
	.suspend	= sepsdio_suspend,
	.resume		= sepsdio_resume,
	.driver		= {
		.name	= "sep0611_sdio",
		.owner	= THIS_MODULE,
	},
};

static int __init sepsdio_init(void)
{
	printk("SEP0611 SDIO Driver v1.2\n");

	return platform_driver_register(&sepsdio_driver);
}

static void __exit sepsdio_exit(void)
{
	platform_driver_unregister(&sepsdio_driver);
}

module_init(sepsdio_init);
module_exit(sepsdio_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("fjj <fanjianjun@wxseuic.com>");
MODULE_DESCRIPTION("SEP0611 SDIO Driver");
