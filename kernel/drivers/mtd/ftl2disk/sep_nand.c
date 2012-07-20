/* linux/drivers/mtd/nand/sep0611.c
 *
 * Copyright (c) 2009-2011 SEUIC
 *  fjj <fanjianjun@wxseuic.com>
 *
 * Southeast University ASIC SoC support
 *
 * NAND Driver for SEP0611 Board
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Modifications:
 *  07-27-2011 fjj initial version
 */

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>

#include <asm/sizes.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <mach/nand.h>
#include <mach/dma.h>
#include <mach/hardware.h>

#include "ftl_global.h"
#include "ftl_def.h"
#include "flash_const.h"

#define DEBUG_TMP(fmt...)	LOG_TMP(fmt)
#define DEBUG_OUT(fmt...)	\
	do {					\
		LOG_TMP(fmt); 		\
		printk(fmt);		\
	} while(0)

#define USER_DATA_LEN		(SECOTR_PART_SIZE * FTL_PARTS_PER_PP)
#define SYS_DATA_LEN		(FTL_SPARE_SIZE_PER_PART * FTL_PARTS_PER_PP)

/* only for normal/two-plane, interleave two-plane can't use */
#define	USE_RB_INTERRUPT


static inline void dma_write(dma_addr_t bus_addr, uint32_t size)
{
	DEBUG_TMP("[dma_write], bus_addr: 0x%x, size: %d\n", bus_addr, size);	

	/* program SARx and DARx*/
	writel(bus_addr, DMAC1_SAR_CH(DMA_NAND_RDWR_CH));
	writel(NAND_BASE + 0x100, DMAC1_DAR_CH(DMA_NAND_RDWR_CH));

	/* program CTLx and CFGx */
	writel(DMAC_DONE(0) | DMAC_BLK_TRAN_SZ(size >> 2), DMAC1_CTL_CH(DMA_NAND_RDWR_CH) + 4);
#define VALUE_DMAC1_CTLX_L_WR	\
	     ( DMAC_SRC_LLP_EN(0)     | DMAC_DST_LLP_EN(0)         | DMAC_SRC_MASTER_SEL(0)	\
   	  	 | DMAC_DST_MASTER_SEL(1) | DMAC_TRAN_TYPE_FLOW_CTL(1) | DMAC_DST_SCAT_EN(0)	\
   	  	 | DMAC_SRC_GATH_EN(0)    | DMAC_SRC_BST_SZ(1)         | DMAC_DST_BST_SZ(1)		\
   	  	 | DMAC_SRC_INCR(0)       | DMAC_DST_INCR(2)           | DMAC_SRC_TRAN_WIDTH(2)	\
         | DMAC_DST_TRAN_WIDTH(2) | DMAC_INT_EN(0) )
	writel(VALUE_DMAC1_CTLX_L_WR, DMAC1_CTL_CH(DMA_NAND_RDWR_CH));

#define VALUE_DMAC1_CFGX_H_WR	\
		 ( DMAC_DST_PER(DMAC_HS_NAND_RD) | DMAC_SRC_PER(DMAC_HS_NAND_WR) | DMAC_SRC_STAT_UPD_EN(0)	\
   	  	 | DMAC_DST_STAT_UPD_EN(0)       | DMAC_PROT_CTL(1)              | DMAC_FIFO_MODE(0) 		\
   	  	 | DMAC_FLOW_CTL_MODE(0) )
	writel(VALUE_DMAC1_CFGX_H_WR, DMAC1_CFG_CH(DMA_NAND_RDWR_CH) + 4);
	
#define VALUE_DMAC1_CFGX_L_WR	\
		 ( DMAC_AUTO_RELOAD_DST(0) | DMAC_AUTO_RELOAD_SRC(0) | DMAC_MAX_AMBA_BST_LEN(0)	\
   	  	 | DMAC_SRC_HS_POL(0)      | DMAC_DST_HS_POL     (0) | DMAC_BUS_LOCK(0)			\
   	  	 | DMAC_CH_LOCK(0)         | DMAC_BUS_LOCK_LEVEL (0) | DMAC_CH_LOCK_LEVEL(0)	\
   	  	 | DMAC_HW_SW_SEL_SRC(1)   | DMAC_HW_SW_SEL_DST  (0) | DMAC_FIFO_EMPTY(1)		\
		 | DMAC_CH_SUSP(0)         | DMAC_CH_PRIOR(0) )
	writel(VALUE_DMAC1_CFGX_L_WR, DMAC1_CFG_CH(DMA_NAND_RDWR_CH));

    /* enable channelx */
	writel(DMAC_CH_WRITE_EN(1 << DMA_NAND_RDWR_CH) | DMAC_CH_EN(1 << DMA_NAND_RDWR_CH), DMAC1_CHEN_V);
	
	/* start write */
	writel(NFC_TRAN_LEN(size >> 2) | NFC_TRAN_DIR(1) | NFC_TRAN_START(1), NAND_DMA_V);
}

static inline void dma_read(dma_addr_t bus_addr, uint32_t size)
{
	DEBUG_TMP("[dma_read], bus_addr: 0x%x, size: %d\n", bus_addr, size);

	/* program SARx and DARx */
	writel(NAND_BASE + 0x100, DMAC1_SAR_CH(DMA_NAND_RDWR_CH));
	writel(bus_addr, DMAC1_DAR_CH(DMA_NAND_RDWR_CH));

	/* program CTLx and CFGx */
	writel(DMAC_DONE(0) | DMAC_BLK_TRAN_SZ(size >> 2), DMAC1_CTL_CH(DMA_NAND_RDWR_CH) + 4);
#define VALUE_DMAC1_CTLX_L_RD	\
	     ( DMAC_SRC_LLP_EN(0)     | DMAC_DST_LLP_EN(0)         | DMAC_SRC_MASTER_SEL(1)	\
   	  	 | DMAC_DST_MASTER_SEL(0) | DMAC_TRAN_TYPE_FLOW_CTL(2) | DMAC_DST_SCAT_EN(0)	\
   	  	 | DMAC_SRC_GATH_EN(0)    | DMAC_SRC_BST_SZ(1)         | DMAC_DST_BST_SZ(1)		\
   	  	 | DMAC_SRC_INCR(2)       | DMAC_DST_INCR(0)           | DMAC_SRC_TRAN_WIDTH(2)	\
         | DMAC_DST_TRAN_WIDTH(2) | DMAC_INT_EN(0) )
	writel(VALUE_DMAC1_CTLX_L_RD, DMAC1_CTL_CH(DMA_NAND_RDWR_CH));

#define VALUE_DMAC1_CFGX_H_RD	\
		 ( DMAC_DST_PER(DMAC_HS_NAND_RD) | DMAC_SRC_PER(DMAC_HS_NAND_WR) | DMAC_SRC_STAT_UPD_EN(0)	\
   	  	 | DMAC_DST_STAT_UPD_EN(0)       | DMAC_PROT_CTL(1)              | DMAC_FIFO_MODE(0)        \
   	  	 | DMAC_FLOW_CTL_MODE(0) )
	writel(VALUE_DMAC1_CFGX_H_RD, DMAC1_CFG_CH(DMA_NAND_RDWR_CH) + 4);
	
#define VALUE_DMAC1_CFGX_L_RD	\
		 ( DMAC_AUTO_RELOAD_DST(0) | DMAC_AUTO_RELOAD_SRC(0) | DMAC_MAX_AMBA_BST_LEN(0)	\
   	  	 | DMAC_SRC_HS_POL(0)      | DMAC_DST_HS_POL(0)      | DMAC_BUS_LOCK(0)	        \
   	  	 | DMAC_CH_LOCK(0)         | DMAC_BUS_LOCK_LEVEL(0)  | DMAC_CH_LOCK_LEVEL(0)	\
   	  	 | DMAC_HW_SW_SEL_SRC(0)   | DMAC_HW_SW_SEL_DST(1)   | DMAC_FIFO_EMPTY(1)	    \
		 | DMAC_CH_SUSP(0)         | DMAC_CH_PRIOR(0) )
	writel(VALUE_DMAC1_CFGX_L_RD, DMAC1_CFG_CH(DMA_NAND_RDWR_CH));
	
    /* enable channelx */
	writel(DMAC_CH_WRITE_EN(1 << DMA_NAND_RDWR_CH) | DMAC_CH_EN(1 << DMA_NAND_RDWR_CH), DMAC1_CHEN_V);
	
	/* start read */
	writel(NFC_TRAN_LEN(size >> 2) | NFC_TRAN_DIR(0) | NFC_TRAN_START(1), NAND_DMA_V);
}

void memcpy_by_dma(unsigned int nDestAddr, unsigned int nSourceAddr, uint16_t dsize)
{
	DEBUG_TMP("[memcpy_by_dma], nDestAddr: 0x%x, nSourceAddr, 0x%x, size: %d\n", nDestAddr, nSourceAddr, dsize);	

	/* program SARx and DARx */
	writel(nSourceAddr, DMAC1_SAR_CH(DMA_NAND_RDWR_CH));
	writel(nDestAddr, DMAC1_DAR_CH(DMA_NAND_RDWR_CH));

	/* program CTLx and CFGx */
	writel(DMAC_DONE(0) | DMAC_BLK_TRAN_SZ(dsize >> 2), DMAC1_CTL_CH(DMA_NAND_RDWR_CH) + 4);
#define VALUE_DMAC1_CTLX_L	\
	     ( DMAC_SRC_LLP_EN(0)     | DMAC_DST_LLP_EN(0)         | DMAC_SRC_MASTER_SEL(0)	\
   	  	 | DMAC_DST_MASTER_SEL(0) | DMAC_TRAN_TYPE_FLOW_CTL(0) | DMAC_DST_SCAT_EN(0)	\
   	  	 | DMAC_SRC_GATH_EN(0)    | DMAC_SRC_BST_SZ(2)         | DMAC_DST_BST_SZ(2)		\
   	  	 | DMAC_SRC_INCR(0)       | DMAC_DST_INCR(0)           | DMAC_SRC_TRAN_WIDTH(2)	\
         | DMAC_DST_TRAN_WIDTH(2) | DMAC_INT_EN(0) )
	writel(VALUE_DMAC1_CTLX_L, DMAC1_CTL_CH(DMA_NAND_RDWR_CH));

#define VALUE_DMAC1_CFGX_H	\
		 ( DMAC_DST_PER(0)         | DMAC_SRC_PER(0)  | DMAC_SRC_STAT_UPD_EN(0)	\
   	  	 | DMAC_DST_STAT_UPD_EN(0) | DMAC_PROT_CTL(1) | DMAC_FIFO_MODE(0)        \
   	  	 | DMAC_FLOW_CTL_MODE(0) )
	writel(VALUE_DMAC1_CFGX_H, DMAC1_CFG_CH(DMA_NAND_RDWR_CH) + 4);
	
#define VALUE_DMAC1_CFGX_L	\
		 ( DMAC_AUTO_RELOAD_DST(0) | DMAC_AUTO_RELOAD_SRC(0) | DMAC_MAX_AMBA_BST_LEN(0)	\
   	  	 | DMAC_SRC_HS_POL(0)      | DMAC_DST_HS_POL(0)      | DMAC_BUS_LOCK(0)	        \
   	  	 | DMAC_CH_LOCK(0)         | DMAC_BUS_LOCK_LEVEL(0)  | DMAC_CH_LOCK_LEVEL(0)	\
   	  	 | DMAC_HW_SW_SEL_SRC(0)   | DMAC_HW_SW_SEL_DST(1)   | DMAC_FIFO_EMPTY(1)	    \
		 | DMAC_CH_SUSP(0)         | DMAC_CH_PRIOR(0) )
	writel(VALUE_DMAC1_CFGX_L, DMAC1_CFG_CH(DMA_NAND_RDWR_CH));
	
    /* enable channelx */
	writel(DMAC_CH_WRITE_EN(1 << DMA_NAND_RDWR_CH) | DMAC_CH_EN(1 << DMA_NAND_RDWR_CH), DMAC1_CHEN_V);
}

static inline void write_4bytes_dummy_data_per_step(void)
{
	uint8_t i;

	/* software control CE, pull up */
	writel(readl(NAND_CTRL_V) | NFC_CE_CARE(1) | NFC_CE_REG(1), NAND_CTRL_V);
	if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
		for (i = 0; i < 4; i++)
			writeb(0xff, NAND_SDATA_V);
	} else {
		for (i = 0; i < 2; i++)
			writew(0xffff, NAND_SDATA_V);	
	}
	/* hardware control CE */
	writel(readl(NAND_CTRL_V) & (~NFC_CE_CARE(1)), NAND_CTRL_V);
}

static inline void read_4bytes_dummy_data_per_step(void)
{
	uint8_t i;

	/* software control CE, pull up */
	writel(readl(NAND_CTRL_V) | NFC_CE_CARE(1) | NFC_CE_REG(1), NAND_CTRL_V);
	if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
		for (i = 0; i < 4; i++)
			readb(NAND_SDATA_V);
	} else {
		for (i = 0; i < 2; i++)
			readw(NAND_SDATA_V);	
	}
	/* hardware control CE */
	writel(readl(NAND_CTRL_V) & (~NFC_CE_CARE(1)), NAND_CTRL_V);
}

static inline void write_ecc_code_per_step(void)
{	
	uint32_t ecc_code;
	uint8_t i;

	switch (gNfcInfo.ecc_mode) {  
		case ECC_MODE_16: {
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
				for (i = 0; i < 7; i++)	{
					ecc_code = readl(NAND_ECC_PARITY0_V + (i << 2));
					writeb(ecc_code, NAND_SDATA_V);
					writeb(ecc_code >> 8, NAND_SDATA_V);
					writeb(ecc_code >> 16, NAND_SDATA_V);
					writeb(ecc_code >> 24, NAND_SDATA_V);
				}
			} else {
				for (i = 0; i < 7; i++)	{
					ecc_code = readl(NAND_ECC_PARITY0_V + (i << 2));
					writew(ecc_code, NAND_SDATA_V);
					writew(ecc_code >> 16, NAND_SDATA_V);
				}
			}	
			break;
		}
					
		case ECC_MODE_24: {
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS)	{
				for (i = 0; i < 10; i++) {
					ecc_code = readl(NAND_ECC_PARITY0_V + (i << 2));
					writeb(ecc_code, NAND_SDATA_V);
					writeb(ecc_code >> 8, NAND_SDATA_V);
					writeb(ecc_code >> 16, NAND_SDATA_V);
					writeb(ecc_code >> 24, NAND_SDATA_V);
				}
	
				ecc_code = readl(NAND_ECC_PARITY0_V + (i << 2));
				writeb(ecc_code, NAND_SDATA_V);
				writeb(ecc_code >> 8, NAND_SDATA_V);	
			} else {
				for (i = 0; i < 10; i++) {
					ecc_code = readl(NAND_ECC_PARITY0_V + (i << 2));
					writew(ecc_code, NAND_SDATA_V);
					writew(ecc_code >> 16, NAND_SDATA_V);
				}
				
				ecc_code = readl(NAND_ECC_PARITY0_V + (i << 2));
				writew(ecc_code, NAND_SDATA_V);
			}	
			break;
		}
			
		default: {
			DEBUG_OUT("sep ftl doesn't supported this ecc mode.\n");
			BUG();
		}	
	}
}

static inline void read_ecc_code_per_step(void)
{
	uint8_t i;

	switch (gNfcInfo.ecc_mode) {
		case ECC_MODE_16: {
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) 
				for (i = 0; i < 28; i++) 
					readb(NAND_SDATA_V);	
			else 
				for (i = 0; i < 14; i++) 
					readw(NAND_SDATA_V);	
			break;
		}

		case ECC_MODE_24:{ 
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) 
				for (i = 0; i < 42; i++) 
					readb(NAND_SDATA_V);	
			else
				for (i = 0; i < 21; i++) 
					readw(NAND_SDATA_V);	
			break;
		}
		
		default: {
			DEBUG_OUT("sep ftl doesn't supported this ecc mode.\n");
			BUG();
		}
	}				
}

static inline uint8_t ecc_correct_by_step(uint8_t * databuf)
{
	uint8_t i;
	uint16_t *buf = (uint16_t *)databuf;
	uint32_t error_sum, error_reg, error_word, error_bit;

	if (readl(NAND_STATUS_V) & (1 << 10)) {			
		DEBUG_OUT("[ftl], The number of error bits exceeds correction range!\n");
		return FLASH_TOO_MANY_ERROR;
	} else {
  		error_sum = (readl(NAND_STATUS_V) >> 12) & 0x1f;
		if (error_sum) {						
			for (i = 0; i < error_sum; i++) {
				error_reg = readl(NAND_ERR_ADDR0_V + (i << 2));
				error_word = error_reg & 0x3ff; 
				error_bit = (error_reg >> 10) & 0xffff;
				if (error_word < 512)				
					buf[error_word] ^= error_bit;  					
			}		
		}
	}
	
	return 0;
}

static inline void SEP_SelectCe_f(uint8_t ceNo)
{	
	DEBUG_TMP("[SEP_SelectCe_f], ceNo: %d\n", ceNo);	
	
	if (unlikely(gNfcInfo.cs != ceNo)) {
		uint32_t old, new;
		
		/* record the slected ceNo */
		gNfcInfo.cs = ceNo;
	
		old = readl(NAND_CFG_V);
		new = (old & (~(0x3 << 25))) | (gNfcInfo.cs << 25);
		writel(new, NAND_CFG_V);
	}
}

static inline void FTL_Low_Driver_Into(void)
{
	mutex_lock(&gNfcInfo.mutex_sepnfc);

#ifndef USE_RB_INTERRUPT
	writel( NFC_CE_CARE(0)         | NFC_CE_REG(0)     | NFC_EN_TRANS_DONE_INT(0)	\
		  | NFC_EN_DEC_DONE_INT(0) | NFC_EN_RnB_INT(0) | NFC_RnB_TRAN_MODE(1), NAND_CTRL_V);
#endif
}

static inline void FTL_Low_Driver_Out(void)
{
#ifndef USE_RB_INTERRUPT
	writel(0xffffffff, NAND_STATUS_V);
	writel( NFC_CE_CARE(0)         | NFC_CE_REG(0)     | NFC_EN_TRANS_DONE_INT(0)	\
		  | NFC_EN_DEC_DONE_INT(0) | NFC_EN_RnB_INT(1) | NFC_RnB_TRAN_MODE(1), NAND_CTRL_V);
#endif

	mutex_unlock(&gNfcInfo.mutex_sepnfc);	
}

static inline void SEP_WriteCmd_f(uint8_t cmd)
{
	DEBUG_TMP("[SEP_WriteCmd_f], cmd: 0x%x\n", cmd);									

	writeb(cmd, NAND_CMD_V);
}

static inline void SEP_WriteAddr_f(int32_t colAddr, int32_t rowAddr)
{
	uint8_t i;

	DEBUG_TMP("[SEP_WriteAddr_f], colAddr: %d, rowAddr: %d(0x%x)\n", colAddr, rowAddr, rowAddr);									

	/* write column address */
	if (colAddr != -1) {
		if (gNfcInfo.bus_type != SINGLE_CHAN_8BITS)
			colAddr >>= 1;
		for (i = 0; i < gNfcInfo.nand->ColCycle; i++) {
			writeb(colAddr, NAND_ADDR_V);
			colAddr >>= 8;
		}
	}
	
	/* write row address */
	if (rowAddr != -1) {
		rowAddr += RESERVED_PAGES_NUM;	
		for (i = 0; i < gNfcInfo.nand->RowCycle; i++) {
			writeb(rowAddr, NAND_ADDR_V);
			rowAddr >>= 8;
		}
	}
}

static inline void SEP_Random_Data_Input(int32_t offset)
{	
	DEBUG_TMP("[SEP_Random_Data_Input], offset: %d\n", offset);

	SEP_WriteCmd_f(0x85);
	SEP_WriteAddr_f(offset, -1); 
}

static inline void SEP_Random_Data_output(int32_t offset)
{	
	DEBUG_TMP("[SEP_Random_Data_output], offset: %d\n", offset);

	SEP_WriteCmd_f(0x05);
	SEP_WriteAddr_f(offset, -1);	
	SEP_WriteCmd_f(0xE0); 
}

static inline uint8_t SEP_WaitforReady_f(uint8_t statusCMD)
{	
	uint8_t ret = 0;
#ifndef USE_RB_INTERRUPT
	uint16_t ready_bit = 0x40;
#endif
	uint16_t fail_bit;
	enum FtlState state;

	DEBUG_TMP("[SEP_WaitforReady_f], statusCMD: 0x%x\n", statusCMD);	

	switch (statusCMD) {
		case 0xF2:
			/* chip 1 state */
			state = gNfcInfo.interleave_state[1];
			fail_bit = 0x04;
			break;
	
		case 0xF1:
			/* chip 0 state */
			state = gNfcInfo.interleave_state[0];
			fail_bit = 0x02;
			break;
			
		default: /* 0x70 */ 
			state = gNfcInfo.state;
			fail_bit = 0x01;
			break;
	}

	if (state == FTL_READY)
		return ret;

	if (gNfcInfo.bus_type == DOUBLE_CHANS_16BITS) {
#ifndef USE_RB_INTERRUPT
		ready_bit |= ready_bit << 8;
#endif
		fail_bit |= fail_bit << 8;
	}

#ifndef USE_RB_INTERRUPT
	/* issue read status command */
	SEP_WriteCmd_f(statusCMD);
	ndelay(100);
	/* temporary, it is a dead-loop */
	while (!(readl(NAND_SDATA_V) & ready_bit));
#else
	if (wait_for_completion_timeout(&gNfcInfo.nand_ready_request, 400) == 0) {
		printk("Ftl timeout!\n");
		return FLASH_RB_TIMEOUT;
	}
#endif
	
	switch (state) {			
		case FTL_ERASING:
			if (readl(NAND_SDATA_V) & fail_bit) {
				DEBUG_OUT("FLASH_ERASE_FAIL\n");
				//ret = FLASH_ERASE_FAIL;
			}
			break;
			
		case FTL_WRITING:
			if (readl(NAND_SDATA_V) & fail_bit) {
				DEBUG_OUT("FLASH_PROGRAM_FAIL\n");
				//ret = FLASH_PROGRAM_FAIL;
			}
			break;
					
		default: /* FTL_READING/FTL_DUMMY_WRITING/... */
			break;
	}

	return ret;
}

static inline void SEP_ResetChip(uint8_t statusCMD)
{
	DEBUG_TMP("[SEP_ResetChip], statusCMD: 0x%x\n", statusCMD);

	/* issue reset command */
	SEP_WriteCmd_f(0xFF);
	/* wait for chip ready */
	SEP_WaitforReady_f(statusCMD);
}

dma_addr_t nand_dma_handle;
#define DATABUF_VA_2_PA(x) ((unsigned long)(x) - (unsigned long)gFTLSysInfo.userDataBufPtr + nand_dma_handle)
static void SEP_Write_Page_f(uint8_t eccStartInPP, uint8_t eccEndInPP, uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{
	uint8_t step;
	
	DEBUG_TMP("[SEP_Write_Page_f], eccStartInPP: %d, eccEndInPP: %d, dataBufPtr: 0x%x, spareBufPtr: 0x%x, flag: %d\n",
							eccStartInPP, eccEndInPP, (uint32_t)dataBufPtr, (uint32_t)spareBufPtr, flag);									
									
	if ((flag & 0x1) == 0x1) { /* write user data */	
		SEP_Random_Data_Input(eccStartInPP * gNfcInfo.eccUnitDatalen);			
	
		for (step = eccStartInPP; step <= eccEndInPP; step++) {		
			/* open ecc module */
			writel(readl(NAND_CFG_V) & (~(1 << 29)), NAND_CFG_V);
			/* init ecc module */				
			writel(0x1, NAND_INIT_V);
			
			/* write 1024 bytes user data */
			dma_write(DATABUF_VA_2_PA(dataBufPtr), 1024);
			
			/* wait for dma transfer completion */
			while (!(readl(NAND_STATUS_V) & (1 << 9)));
			writel(readl(NAND_STATUS_V) | (1 << 9), NAND_STATUS_V);

			/* write 4 bytes dummy data */
			write_4bytes_dummy_data_per_step();	
 
			/* close ecc module */
			writel(readl(NAND_CFG_V) | (1 << 29), NAND_CFG_V);
			/* write ecc code */			
			write_ecc_code_per_step();
			
			/* pointer to next 1024 bytes user data start address */
			dataBufPtr += 1024;				
		}		
	}

	if ((flag & 0x2) == 0x2) { /* write spare data */
		uint8_t i;
		
		SEP_Random_Data_Input(gNfcInfo.ftlSysDataStartPos + (eccStartInPP << 2));
		
		for (step = eccStartInPP; step <= eccEndInPP; step++) {			
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
				for (i = 0; i < 4; i++)
					writeb(spareBufPtr[i], NAND_SDATA_V);
				spareBufPtr += 4;
			} else {
				uint16_t *spareBuf = (uint16_t *)spareBufPtr;
			
				for (i = 0; i < 2; i++)
					writew(spareBuf[i], NAND_SDATA_V);
				spareBuf += 2;
			}
		}
	}
}

static uint8_t SEP_Read_Page_f(uint8_t eccStartInPP, uint8_t eccEndInPP, uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{
	uint8_t ret = 0;
	uint8_t step;
	
	DEBUG_TMP("[SEP_Read_Page_f], eccStartInPP: %d, eccEndInPP: %d, dataBufPtr: 0x%x, spareBufPtr: 0x%x, flag: %d\n",
							eccStartInPP, eccEndInPP, (uint32_t)dataBufPtr, (uint32_t)spareBufPtr, flag);
								
	if ((flag & 0x1) == 0x1) { /* read user data */
		SEP_Random_Data_output(eccStartInPP * gNfcInfo.eccUnitDatalen);	
	
		for (step = eccStartInPP; step <= eccEndInPP; step++) {
			/* open ecc module */
			writel(readl(NAND_CFG_V) & (~(1 << 29)), NAND_CFG_V);
			/* init ecc module */				
			writel(0x1, NAND_INIT_V);
		
			/* read 1024 bytes user data */				
			dma_read(DATABUF_VA_2_PA(dataBufPtr), 1024);			
			
			/* wait for dma transfer completion */
			while (!(readl(NAND_STATUS_V) & (1 << 9)));
			writel(readl(NAND_STATUS_V) | (1 << 9), NAND_STATUS_V);
			
			/* read 4 bytes dummy data */
			read_4bytes_dummy_data_per_step();			

			/* read ecc code */
			read_ecc_code_per_step();
			
			/* wait for ecc decoder completion */
			while (!(readl(NAND_STATUS_V) & (1 << 8)));
			writel(readl(NAND_STATUS_V) | (1 << 8), NAND_STATUS_V);
			/* close ecc module */
			writel(readl(NAND_CFG_V) | (1 << 29), NAND_CFG_V);	
			
			/* correct data */
			ret |= ecc_correct_by_step(dataBufPtr);
			
			/* pointer next 1024 bytes boundary */	
			dataBufPtr += 1024;	
		}
	}

	if ((flag & 0x2) == 0x2) { /* read spare data */
		uint8_t i;
		
		SEP_Random_Data_output(gNfcInfo.ftlSysDataStartPos + (eccStartInPP << 2));	
					
		for (step = eccStartInPP; step <= eccEndInPP; step++) {	
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
				for (i = 0; i < 4; i++)
					spareBufPtr[i] = readb(NAND_SDATA_V);
				spareBufPtr += 4;
			} else {
				uint16_t *spareBuf = (uint16_t *)spareBufPtr;
			
				for (i = 0; i < 2; i++)
					spareBuf[i] = readw(NAND_SDATA_V);
				spareBuf += 2;
			}
		}
	}
	
	return ret;	
}

static void SEP_Write_for_eccAlign(uint8_t bufNo, uint8_t eccStartInPP)
{	
	DEBUG_TMP("[SEP_Write_for_eccAlign], bufNo: %d, eccStartInPP: %d\n",
					bufNo, eccStartInPP);
					
	SEP_Random_Data_Input(eccStartInPP * gNfcInfo.eccUnitDatalen);

	/* open ecc module */
	writel(readl(NAND_CFG_V) & (~(1 << 29)), NAND_CFG_V);
	/* init ecc module */				
	writel(0x1, NAND_INIT_V);
		
	if (bufNo)
		gNfcInfo.eccAlign_busAddr = gNfcInfo.eccAlign_busAddr_1;
	else  
		gNfcInfo.eccAlign_busAddr = gNfcInfo.eccAlign_busAddr_0;
		
	/* write 1024 bytes user data */
	dma_write(gNfcInfo.eccAlign_busAddr, 1024);
				
	/* wait for dma transfer completion */
	while (!(readl(NAND_STATUS_V) & (1 << 9)));
	writel(readl(NAND_STATUS_V) | (1 << 9), NAND_STATUS_V);

	/* write 4 bytes dummy data */
	write_4bytes_dummy_data_per_step();	
 
	/* close ecc module */
	writel(readl(NAND_CFG_V) | (1 << 29), NAND_CFG_V);
	/* write ecc code */			
	write_ecc_code_per_step();									
}

static uint8_t SEP_Read_for_eccAlign(uint8_t bufNo, uint8_t eccStartInPP, uint8_t *spareBufPtr, uint8_t flag)
{	
	uint8_t ret = 0;
	
	DEBUG_TMP("[SEP_Read_for_eccAlign], bufNo: %d, eccStartInPP: %d, spareBufPtr: 0x%x, flag: %d\n",
					bufNo, eccStartInPP, (uint32_t)spareBufPtr, flag);
						
	if ((flag & 0x1) == 0x1) { /* read user data */	
		SEP_Random_Data_output(eccStartInPP * gNfcInfo.eccUnitDatalen);	
		
		/* open ecc module */
		writel(readl(NAND_CFG_V) & (~(1 << 29)), NAND_CFG_V);
		/* init ecc module */				
		writel(0x1, NAND_INIT_V);
	
		if (bufNo) {	
			gNfcInfo.eccAlign_virAddr = gNfcInfo.eccAlign_virAddr_1;
			gNfcInfo.eccAlign_busAddr = gNfcInfo.eccAlign_busAddr_1;
		} else  {
			gNfcInfo.eccAlign_virAddr = gNfcInfo.eccAlign_virAddr_0;
			gNfcInfo.eccAlign_busAddr = gNfcInfo.eccAlign_busAddr_0;
		}	
	
		/* read 1024 bytes user data */	
		dma_read(gNfcInfo.eccAlign_busAddr, 1024);
	
		/* wait for dma transfer completion */
		while (!(readl(NAND_STATUS_V) & (1 << 9)));
		writel(readl(NAND_STATUS_V) | (1 << 9), NAND_STATUS_V);
		
		/* read 4 bytes dummy data */
		read_4bytes_dummy_data_per_step();			

		/* read ecc code */
		read_ecc_code_per_step();
			
		/* wait for ecc decoder completion */
		while (!(readl(NAND_STATUS_V) & (1 << 8)));
		writel(readl(NAND_STATUS_V) | (1 << 8), NAND_STATUS_V);
		/* close ecc module */
		writel(readl(NAND_CFG_V) | (1 << 29), NAND_CFG_V);	

		/* correct data */
		ret |= ecc_correct_by_step(gNfcInfo.eccAlign_virAddr);
	}
	
	if ((flag & 0x2) == 0x2) { /* read spare data */	
		if (bufNo) 
			SEP_Random_Data_output(gNfcInfo.ftlSysDataStartPos + (eccStartInPP << 2));
		else	
			SEP_Random_Data_output(gNfcInfo.ftlSysDataStartPos + (eccStartInPP << 2) + 2);
			
		if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
			spareBufPtr[0] = readb(NAND_SDATA_V);
			spareBufPtr[1] = readb(NAND_SDATA_V);
		} else {
			uint16_t *spareBuf = (uint16_t *)spareBufPtr;
			
			spareBuf[0] = readw(NAND_SDATA_V);
		}		
	}
	
	return ret;						
}

int SEP_Nand_Init_f(void)
{	
	gNfcInfo.eccAlign_virAddr_0 = (uint8_t *)dma_alloc_coherent(NULL, 2048, (dma_addr_t *)&gNfcInfo.eccAlign_busAddr_0, GFP_KERNEL);
	if (gNfcInfo.eccAlign_virAddr_0 == NULL) {
		DEBUG_OUT("alloc eccAlign fail.\n");	
		return -ENOMEM;	
	}
	gNfcInfo.eccAlign_virAddr_1 = gNfcInfo.eccAlign_virAddr_0 + 1024;
	gNfcInfo.eccAlign_busAddr_1 = gNfcInfo.eccAlign_busAddr_0 + 1024;
	
	DEBUG_TMP("\n[SEP_Nand_Init_f], eccAlign_virAddr_0: 0x%x, eccAlign_busAddr_0: 0x%x, eccAlign_virAddr_1: 0x%x, eccAlign_busAddr_1: 0x%x\n", 
					gNfcInfo.eccAlign_virAddr_0, gNfcInfo.eccAlign_busAddr_0, gNfcInfo.eccAlign_virAddr_1, gNfcInfo.eccAlign_busAddr_1);

	return 0;
}

void SEP_Nand_Release_f(void)
{
	dma_free_coherent(NULL, 2048, gNfcInfo.eccAlign_virAddr_0, gNfcInfo.eccAlign_busAddr_0);
}

/**************************************************************************************
Description: Init flash driver runtime env
Input:
		void.
Return:
		0--successful.
		others--Init fail.
**************************************************************************************/
__INLINE__ UINT8 FLASH_Init_f(void)
{
	return 0;
}

/**************************************************************************************
Description: Erase a PB.
Input:
	ceNo, the CE number.
	rowAddr, 
			[0]: the row address.
Return:
		0--successful.
		others--Erase failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Erase_f(uint8_t ceNo, uint32_t *rowAddr)
{
	uint8_t ret;
	
	DEBUG_TMP("\n[FLASH_Erase_f], ceNo: %d, rowAddr[0]: 0x%x\n", 
					ceNo, rowAddr[0]);
	
	FTL_Low_Driver_Into();
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	/* issue block erase setup command */
	SEP_WriteCmd_f(0x60);
	/* issue row address */
	SEP_WriteAddr_f(-1, rowAddr[0]);
	/* issue block erase confirm command */
	SEP_WriteCmd_f(0xD0);	
	/* record flash state */
	gNfcInfo.state = FTL_ERASING;
	
	/* wait for tBERS */
	ret = SEP_WaitforReady_f(0x70);
	if (ret)
		SEP_ResetChip(0x70);

	FTL_Low_Driver_Out();
	return ret;
}

/**************************************************************************************
Description: Erase two PBs of a VPB in two chips with different CE#.
Input:
		ceNo, the CE number, the lower CE# of the two CE.
		rowAddr, 
				[0]: the row address.
Return:
		0--successful.
		others--Erase failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Erase_Interleaving_f(uint8_t ceNo, uint32_t *rowAddr)
{
	return 0;
}

/**************************************************************************************
Description: Erase PBs of a VPB through two-plane block erase.
Input:
		ceNo, the CE number.
		rowAddr,
				[0]: the row address of plane 0(first plane the two-plane).
			    [1]: the row address of plane 1(second plane of the two-plane).
Return:
		0--successful.
		others--Erase failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Erase_TwoPlane_f(uint8_t ceNo, uint32_t *rowAddr)
{
	uint8_t ret;
	
	DEBUG_TMP("\n[FLASH_Erase_TwoPlane_f], ceNo: %d, rowAddr[0]: 0x%x, rowAddr[1]: 0x%x\n",
			 	ceNo, rowAddr[0], rowAddr[1]);

	FTL_Low_Driver_Into();
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	/* issue block erase setup command */
	SEP_WriteCmd_f(0x60);	
	/* issue row addess of plane 0 */
	SEP_WriteAddr_f(-1, rowAddr[0]);
	/* issue block erase setup command */
	SEP_WriteCmd_f(0x60);
	/* issue row addess of plane 1 */
	SEP_WriteAddr_f(-1, rowAddr[1]);
	/* issue block erase confirm command */
	SEP_WriteCmd_f(0xD0);	
	/* record flash state */
	gNfcInfo.state = FTL_ERASING;
	
	/* wait for tBERS */
	ret = SEP_WaitforReady_f(0x70);
	if (ret)
		SEP_ResetChip(0x70);

	FTL_Low_Driver_Out();
	return ret;
}

/**************************************************************************************
Description: Erase PBs of a VPB through interleaving two-plane block erase.
Input:
		ceNo, the CE number.
		rowAddr,
				[0]: the row address of plane 0 in chip 0(first plane of first chip of the interleaving two-plane).
				[1]: the row address of plane 1 in chip 0(second plane of first chip of the interleaving two-plane).
				[2]: the row address of plane 0 in chip 1(first plane of second chip of the interleaving two-plane).
				[3]: the row address of plane 1 in chip 1(second plane of second chip of the interleaving two-plane).
Return:
		0--successful.
		others--Erase failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Erase_InterleavingTwoPlane_f(uint8_t ceNo, uint32_t *rowAddr)
{
	uint8_t ret;
	uint8_t i;

	DEBUG_TMP("\n[FLASH_Erase_InterleavingTwoPlane_f], ceNo: %d, rowAddr[0]: 0x%x, rowAddr[1]: 0x%x, rowAddr[2]: 0x%x, rowAddr[3]: 0x%x\n",
				ceNo, rowAddr[0], rowAddr[1], rowAddr[2], rowAddr[3]);
	
	FTL_Low_Driver_Into();
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	gNfcInfo.failOp = NO_OPERATE_FAIL;	

	for (i = 0; i < 2; i++) {
		/* wait for chip i be ready */
		ret = SEP_WaitforReady_f(0xF1 + i);	/* 0xF1 or 0xF2 */
		if (ret) {
			DEBUG_OUT("[FLASH_Erase_InterleavingTwoPlane_f], chip %d, previous operation fails or is overtime: 0x%x.\n", i, ret);
			gNfcInfo.failOp = PREVIOUS;
			for (i = 0; i < 2; i++) {
				SEP_ResetChip(0xF1 + i); 
				gNfcInfo.interleave_state[i] = FTL_READY;
			}
			goto exit_FLASH_Erase_InterleavingTwoPlane_f;
		}
	
		/* issue block erase setup command */
		SEP_WriteCmd_f(0x60);	
		/* issue row address of plane 0 in chip i */
		SEP_WriteAddr_f(-1, rowAddr[(i << 1) + 0]);
		/* issue block erase setup command */
		SEP_WriteCmd_f(0x60);
		/* issue row address of plane 1 in chip i */
		SEP_WriteAddr_f(-1, rowAddr[(i << 1) + 1]);
		/* issue block erase confirm command */
		SEP_WriteCmd_f(0xD0);
		/* record chip i state */
		gNfcInfo.interleave_state[i] = FTL_ERASING;
	}

 exit_FLASH_Erase_InterleavingTwoPlane_f:
	FTL_Low_Driver_Out();	
	return ret;
}

/**************************************************************************************
Description: Write data to a PP through normal page program.
Input:
		ceNo, the CE number.
		rowAddr,
		        [0]: the row address.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
Return:
		0--successful.
		others--program failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Write_f(uint8_t ceNo, uint32_t *rowAddr, uint8_t *dataBufPtr, uint8_t *spareBufPtr)
{
	uint8_t ret;

	DEBUG_TMP("\n[FLASH_Write_f], ceNo: %d, rowAddr[0]: 0x%x, dataBufPtr: 0x%x, spareBufPtr: 0x%x\n",
				ceNo, rowAddr[0], (uint32_t)dataBufPtr, (uint32_t)spareBufPtr);
	
	FTL_Low_Driver_Into();
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	/* issue serial data input command */
	SEP_WriteCmd_f(0x80);	
	/* issue address */
	SEP_WriteAddr_f(0, rowAddr[0]);
	/* involk write a physical page function */
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP -1) >> 1, dataBufPtr, spareBufPtr, 0x3);
	/* issue program confirm command */
	SEP_WriteCmd_f(0x10);	
	/* record flash state */
	gNfcInfo.state = FTL_WRITING;
	
	/* wait for tPROG */
	ret = SEP_WaitforReady_f(0x70);
	if (ret)
		SEP_ResetChip(0x70);

	FTL_Low_Driver_Out();
	return ret;
}

/**************************************************************************************
Description: Write data to PPs of a VPB in two chips with different CE#.
Input:
		ceNo, the CE number, the lower CE# of the two CE.
		rowAddr, 
				[0]: the row address.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
Return:
		0--successful.
		others--Program failure or timeout.
**************************************************************************************/
__INLINE__  uint8_t FLASH_Write_Interleaving_f(uint8_t ceNo, uint32_t *rowAddr, uint8_t *dataBufPtr, uint8_t *spareBufPtr)
{
	return 0;
}

/**************************************************************************************
Description: Write data to PPs of a VPB through two-plane page program.
Input:
		ceNo, the CE number.
		rowAddr,
				[0]: the row address of plane 0(first plane of the two-plane).
				[1]: the row address of plane 1(second plane of the two-plane).
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
Return:
		0--successful.
		others--program failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Write_TwoPlane_f(uint8_t ceNo, uint32_t *rowAddr, uint8_t *dataBufPtr, uint8_t *spareBufPtr)
{
	uint8_t ret;

	DEBUG_TMP("\n[FLASH_Write_TwoPlane_f], ceNo: %d, rowAddr[0]: 0x%x, rowAddr[1]: 0x%x, dataBufPtr: 0x%x, spareBufPtr: 0x%x\n",
				ceNo, rowAddr[0], rowAddr[1], (uint32_t)dataBufPtr, (uint32_t)spareBufPtr);
	
	FTL_Low_Driver_Into();			
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	/* issue serial data input command */
	SEP_WriteCmd_f(0x80);
	/* issue address of plane 0 */
	SEP_WriteAddr_f(0, rowAddr[0]);
	/* involk write a physical page function */
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP -1) >> 1, dataBufPtr, spareBufPtr, 0x3);
	/* issue dummy program command for plane 0 */
	SEP_WriteCmd_f(0x11);
	/* record flash state */
	gNfcInfo.state = FTL_DUMMY_WRITING;

	/* wait for tDBSY */	
	ret = SEP_WaitforReady_f(0x70);
	if (ret) { 
		SEP_ResetChip(0x70);
		gNfcInfo.state = FTL_READY;
		goto exit_FLASH_Write_TwoPlane_f;
	}
		
	/* points to next physical page's data area and spare area */
	dataBufPtr += USER_DATA_LEN;
	spareBufPtr += SYS_DATA_LEN;

	/* issue another plane data input command */
	SEP_WriteCmd_f(0x81);
	/* issue address of plane 1 */
	SEP_WriteAddr_f(0, rowAddr[1]);
	/* involk write a physical page function */
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP -1) >> 1, dataBufPtr, spareBufPtr, 0x3);
	/* issue program confirm command */
	SEP_WriteCmd_f(0x10);	
	/* record flash state */
	gNfcInfo.state = FTL_WRITING;
	
	/* wait for tPROG */
	ret = SEP_WaitforReady_f(0x70);
	if (ret)
		SEP_ResetChip(0x70);

 exit_FLASH_Write_TwoPlane_f:
	FTL_Low_Driver_Out();
	return ret;
}

/**************************************************************************************
Description: Write data of a VPB through interleaving two-plane page program.
Input:
		ceNo, the CE number.
		rowAddr,
				[0]: the row address of plane 0 in chip 0(first plane of first chip of the interleaving two-plane).
				[1]: the row address of plane 1 in chip 0(second plane of first chip of the interleaving two-plane).
				[2]: the row address of plane 0 in chip 1(first plane of second chip of the interleaving two-plane).
				[3]: the row address of plane 1 in chip 1(second plane of second chip of the interleaving two-plane).
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
Return:
		0--successful.
		others--Program failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Write_InterleavingTwoPlane_f(uint8_t ceNo, uint32_t *rowAddr, uint8_t *dataBufPtr, uint8_t *spareBufPtr)
{
	uint8_t ret;
	uint8_t i;

	DEBUG_TMP("\n[FLASH_Write_InterleavingTwoPlane_f], ceNo: %d, rowAddr[0]: 0x%x, rowAddr[1]: 0x%x, rowAddr[2]: 0x%x, rowAddr[3]: 0x%x,  dataBufPtr: 0x%x, spareBufPtr: 0x%x\n",
				ceNo, rowAddr[0], rowAddr[1], rowAddr[2], rowAddr[3], (uint32_t)dataBufPtr, (uint32_t)spareBufPtr);

	FTL_Low_Driver_Into();
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	gNfcInfo.failOp = NO_OPERATE_FAIL;

	for (i = 0; i < 2; i++) {
		/* wait for chip i be ready */
		ret = SEP_WaitforReady_f(0xF1 + i);	/* 0xF1 or 0xF2 */
		if (ret) {
			DEBUG_OUT("[FLASH_Write_InterleavingTwoPlane_f], chip %d, previous operation fails or is overtime: 0x%x.\n", i, ret);
			gNfcInfo.failOp = PREVIOUS;
			for (i = 0; i < 2; i++) {
				SEP_ResetChip(0xF1 + i); 
				gNfcInfo.interleave_state[i] = FTL_READY;
			}
			goto exit_FLASH_Write_InterleavingTwoPlane_f;
		}
		
		/* issue serial data input command */
		SEP_WriteCmd_f(0x80);
		/* issue address of plane 0 chip i */
		SEP_WriteAddr_f(0, rowAddr[(i << 1) + 0]);
		/* involk write a physical page function */
		SEP_Write_Page_f(0, (FTL_PARTS_PER_PP -1) >> 1, dataBufPtr, spareBufPtr, 0x3);
		/* issue dummy program command for plane 0 chip i */
		SEP_WriteCmd_f(0x11);
		/* record chip i state */
		gNfcInfo.interleave_state[i] = FTL_DUMMY_WRITING;
		
		/* wait for tDBSY */
		ret = SEP_WaitforReady_f(0xF1 + i); /* 0xF1 or 0xF2 */
		if (ret) {
			DEBUG_OUT("[FLASH_Write_InterleavingTwoPlane_f], chip %d, current operation waits for tDBSY overtime.\n", i);
			if (i == 0)
				ret = SEP_WaitforReady_f(0xF2); /* we must wait for chip 1 previous operation completion */
			if (ret)
				gNfcInfo.failOp = PRE_CUR;
			else
				gNfcInfo.failOp = CURRENT;
			for (i = 0; i < 2; i++) {
				SEP_ResetChip(0xF1 + i); 
				gNfcInfo.interleave_state[i] = FTL_READY;
			}
			goto exit_FLASH_Write_InterleavingTwoPlane_f;
		}
		
		/* points to next physical page's data area and spare area */
		dataBufPtr += USER_DATA_LEN;
		spareBufPtr += SYS_DATA_LEN;

		/* issue another plane data input command */
		SEP_WriteCmd_f(0x81);
		/* issue address of plane 1 chip i */
		SEP_WriteAddr_f(0, rowAddr[(i << 1) + 1]);
		/* involk write a physical page function */
		SEP_Write_Page_f(0, (FTL_PARTS_PER_PP -1) >> 1, dataBufPtr, spareBufPtr, 0x3);
		/* issue program confirm command */
		SEP_WriteCmd_f(0x10);
		/* record flash state */
		gNfcInfo.interleave_state[i] = FTL_WRITING;

		/* points to next physical page's data area and spare area */		
		dataBufPtr += USER_DATA_LEN;
		spareBufPtr += SYS_DATA_LEN;
	}

 exit_FLASH_Write_InterleavingTwoPlane_f:
	FTL_Low_Driver_Out();	
	return ret;
}

/**************************************************************************************
Description: Read data from a PP through normal page read.
Input:
		ceNo, the CE number.
		rowAddr,
				[0]: the row address of plane 0 in chip 0(first plane of first chip of the interleaving two-plane).
				[1]: the row address of plane 1 in chip 0(second plane of first chip of the interleaving two-plane).
				[2]: the row address of plane 0 in chip 1(first plane of second chip of the interleaving two-plane).
				[3]: the row address of plane 1 in chip 1(second plane of second chip of the interleaving two-plane).
		startOffsetInLPPN, start sector's offset in the LPN.
		endOffsetInLPPN, end sector's offset in the LPN.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
		flag,
			 (flag & 0x1) = 1, read user data.
			 (flag & 0x2) = 2, read spare data.
Return:
		0--successful.
		others--Read failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Read_f(uint8_t ceNo, uint32_t *rRowAddr, uint8_t startOffsetInLPPN, uint8_t endOffsetInLPPN, 
									uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{
	uint8_t ret;
	uint8_t ppOffsetS;	
	char secStartInPP;
	char secEndInPP;
	uint8_t *dataBuf = NULL;
	uint8_t *spareBuf = NULL;
	
	DEBUG_TMP("\n[FLASH_Read_f], ceNo:%d, rRowAddr[0]:0x%x, rRowAddr[1]:0x%x, rRowAddr[2]:0x%x, rRowAddr[3]:0x%x, startOffsetInLPPN:%d, endOffsetInLPPN:%d,  dataBufPtr: 0x%x, spareBufPtr: 0x%x, flag:%d\n",
			ceNo, rRowAddr[0], rRowAddr[1], rRowAddr[2], rRowAddr[3], startOffsetInLPPN, endOffsetInLPPN, (uint32_t)dataBufPtr, (uint32_t)spareBufPtr, flag);
 
	ppOffsetS = startOffsetInLPPN / FTL_PARTS_PER_PP;

	secStartInPP = startOffsetInLPPN % FTL_PARTS_PER_PP;
	secEndInPP = endOffsetInLPPN % FTL_PARTS_PER_PP;
	
	FTL_Low_Driver_Into();				
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);
	
	/* issue read setup command */ 
	SEP_WriteCmd_f(0x00);
	/* issue address */
	SEP_WriteAddr_f(0, rRowAddr[ppOffsetS]);
	/* issue read confirm command */
	SEP_WriteCmd_f(0x30);
	
	/* record flash state */
	gNfcInfo.state = FTL_READING;
	/* wait for tR */
	ret = SEP_WaitforReady_f(0x70);
	if (ret) {
		SEP_ResetChip(0x70);
		goto exit_FLASH_Read_f;
	}	
		
	/* issue read data command after read status (must) */
	SEP_WriteCmd_f(0x00);
	
	/*
	 * If startOffsetInLPPN is a odd, 
	 * we need to read additionally the sector before it, 
	 * let them to combine a ECC unit.
	 */		
	if (secStartInPP & 0x1) { /* odd */			
		if ((flag & 0x2) == 0x2)
			spareBuf = spareBufPtr + (ppOffsetS * SYS_DATA_LEN) + (secStartInPP * FTL_SPARE_SIZE_PER_PART);
			
		ret |= SEP_Read_for_eccAlign(0, secStartInPP >> 1, spareBuf, flag);
		
		if ((flag & 0x1) == 0x1) {
			dataBuf = dataBufPtr + (ppOffsetS * USER_DATA_LEN) + (secStartInPP * SECOTR_PART_SIZE);	
			memcpy(dataBuf, gNfcInfo.eccAlign_virAddr_0 + 512, 512);	
		}
		
		secStartInPP += 1;	
	}
	
	/*
	 * If endOffsetInLPPN is a even, 
	 * we need to read additionally the sector after it, 
	 * let them to combine a ECC unit.
	 */	
	if (!(secEndInPP & 0x1)) { /* even */
		if ((flag & 0x2) == 0x2)
			spareBuf = spareBufPtr + (ppOffsetS * SYS_DATA_LEN) + (secEndInPP * FTL_SPARE_SIZE_PER_PART);
		
		ret |= SEP_Read_for_eccAlign(1, secEndInPP >> 1, spareBuf, flag);
		
		if ((flag & 0x1) == 0x1) {
			dataBuf = dataBufPtr + (ppOffsetS * USER_DATA_LEN) + (secEndInPP * SECOTR_PART_SIZE);
			memcpy(dataBuf, gNfcInfo.eccAlign_virAddr_1, 512);	
		}
		
		secEndInPP -= 1;			
	}
	
	/* read remains data */
	if (secStartInPP < secEndInPP) {
		if ((flag & 0x1) == 0x1)
			dataBuf = dataBufPtr + (ppOffsetS * USER_DATA_LEN) + (secStartInPP * SECOTR_PART_SIZE);
		if ((flag & 0x2) == 0x2)
			spareBuf = spareBufPtr + (ppOffsetS * SYS_DATA_LEN) + (secStartInPP * FTL_SPARE_SIZE_PER_PART);
	
		/* involk read a physical page function */
		ret |= SEP_Read_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, spareBuf, flag);
	}

 exit_FLASH_Read_f:
	FTL_Low_Driver_Out();		
	return ret;
}

/**************************************************************************************
Description: Read data from PPs of a VPP through interleaving read.
Input:
		ceNo, the CE number, the lower CE# of the two CE.
		rowAddr,
				[0]: the row address.
		startOffsetInLPPN, start sector's offset in the LPN.
		endOffsetInLPPN, end sector's offset in the LPN.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
		flag,
			 (flag & 0x1) = 1, read user data.
			 (flag & 0x2) = 2, read spare data.
Return:
		0--successful.
		others--Read failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Read_Interleaving_f(uint8_t ceNo, uint32_t *rRowAddr, uint8_t startOffsetInLPPN, uint8_t endOffsetInLPPN,
												uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{
	return 0;
}

/**************************************************************************************
Description: Read data from PPs of a VPP through two-plane page read.
Input:
		ceNo, the CE number.
		rowAddr,
				[0]: the row address of plane 0 in chip 0(first plane of first chip of the interleaving two-plane).
				[1]: the row address of plane 1 in chip 0(second plane of first chip of the interleaving two-plane).
				[2]: the row address of plane 0 in chip 1(first plane of second chip of the interleaving two-plane).
				[3]: the row address of plane 1 in chip 1(second plane of second chip of the interleaving two-plane).
		startOffsetInLPPN, start sector's offset in the LPN.
		endOffsetInLPPN, end sector's offset in the LPN.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
		flag,
			 (flag & 0x1) = 1, read user data.
			 (flag & 0x2) = 2, read spare data.
Return:
		0--successful.
		others--Read failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Read_TwoPlane_f(uint8_t ceNo, uint32_t *rRowAddr, uint8_t startOffsetInLPPN, uint8_t endOffsetInLPPN,
											uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{	
	uint8_t ret;
	uint8_t ppOffsetS;
	uint8_t ppOffsetE;	
	
	DEBUG_TMP("\n[FLASH_Read_TwoPlane_f], ceNo: %d, rRowAddr[0]: 0x%x, rRowAddr[1]: 0x%x, rRowAddr[2]: 0x%x, rRowAddr[3]: 0x%x, startOffsetInLPPN: %d, endOffsetInLPPN: %d, dataBufPtr: 0x%x, spareBufPtr: 0x%x, flag: %d\n",
			ceNo, rRowAddr[0], rRowAddr[1], rRowAddr[2], rRowAddr[3], startOffsetInLPPN, endOffsetInLPPN, (uint32_t)dataBufPtr, (uint32_t)spareBufPtr, flag);
		
	ppOffsetS = startOffsetInLPPN / FTL_PARTS_PER_PP;
	ppOffsetE = endOffsetInLPPN / FTL_PARTS_PER_PP;
		
	if (ppOffsetS == ppOffsetE) { /* in the same physical page, use normal read */
		return FLASH_Read_f(ceNo, rRowAddr, startOffsetInLPPN, endOffsetInLPPN, dataBufPtr, spareBufPtr, flag);
		
	} else { /* in different physical page, use two-plane read */
		char secStartInPP;
		char secEndInPP;
		uint8_t *dataBuf = NULL;
		uint8_t *spareBuf = NULL;
		
		FTL_Low_Driver_Into();
		/* select the specified CE */
		SEP_SelectCe_f(ceNo);
		
		/* issue 1st command of two-plane read */
		SEP_WriteCmd_f(0x60);
		/* issue row address of plane 0 */
		SEP_WriteAddr_f(-1, rRowAddr[ppOffsetS]);
		/* issue 2nd command of two-plane read */
		SEP_WriteCmd_f(0x60);
		/* issue row address of plane 1 */
		SEP_WriteAddr_f(-1, rRowAddr[ppOffsetE]);
		/* issue 3rd command of two-plane read */
		SEP_WriteCmd_f(0x30);
		
		/* record flash state */
		gNfcInfo.state = FTL_READING;
		/* wait for tR */
		ret = SEP_WaitforReady_f(0x70);
		if (ret) {
			SEP_ResetChip(0x70);
			goto exit_FLASH_Read_TwoPlane_f;
		}
		
		/* issue read command */
		SEP_WriteCmd_f(0x00);
		/* issue address of plane 0 */
		SEP_WriteAddr_f(0, rRowAddr[ppOffsetS]);
		
		secStartInPP = startOffsetInLPPN % FTL_PARTS_PER_PP;
		secEndInPP = FTL_PARTS_PER_PP - 1;
		
		/*
	 	 * If startOffsetInLPPN is a odd, 
	 	 * we need to read additionally the sector before it, 
	 	 * let them to combine a ECC unit.
	 	 */		
		if (secStartInPP & 0x1) { /* odd */			
			if ((flag & 0x2) == 0x2)
				spareBuf = spareBufPtr + (ppOffsetS * SYS_DATA_LEN) + (secStartInPP * FTL_SPARE_SIZE_PER_PART);
			
			ret |= SEP_Read_for_eccAlign(0, secStartInPP >> 1, spareBuf, flag);
		
			if ((flag & 0x1) == 0x1) {
				dataBuf = dataBufPtr + (ppOffsetS * USER_DATA_LEN) + (secStartInPP * SECOTR_PART_SIZE);	
				memcpy(dataBuf, gNfcInfo.eccAlign_virAddr_0 + 512, 512);	
			}
		
			secStartInPP += 1;	
		}
		
		/* read remains data */
		if (secStartInPP < secEndInPP) {
			if ((flag & 0x1) == 0x1)
				dataBuf = dataBufPtr + (ppOffsetS * USER_DATA_LEN) + (secStartInPP * SECOTR_PART_SIZE);
			if ((flag & 0x2) == 0x2)
				spareBuf = spareBufPtr + (ppOffsetS * SYS_DATA_LEN) + (secStartInPP * FTL_SPARE_SIZE_PER_PART);
	
			/* involk read a physical page function */
			ret |= SEP_Read_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, spareBuf, flag);
		}
		
		/* issue read command */		
		SEP_WriteCmd_f(0x00);
		/* issue address of plane 1 */
		SEP_WriteAddr_f(0, rRowAddr[ppOffsetE]);
		
		secStartInPP = 0;
		secEndInPP = endOffsetInLPPN  % FTL_PARTS_PER_PP;
				
		/*
	 	* If endOffsetInLPPN is a even, 
	 	* we need to read additionally the sector after it, 
	 	* let them to combine a ECC unit.
	 	*/	
		if (!(secEndInPP & 0x1)) { /* even */
			if ((flag & 0x2) == 0x2)
				spareBuf = spareBufPtr + (ppOffsetE * SYS_DATA_LEN) + (secEndInPP * FTL_SPARE_SIZE_PER_PART);
		
			ret |= SEP_Read_for_eccAlign(1, secEndInPP >> 1, spareBuf, flag);
		
			if ((flag & 0x1) == 0x1) {
				dataBuf = dataBufPtr + (ppOffsetE * USER_DATA_LEN) + (secEndInPP * SECOTR_PART_SIZE);
				memcpy(dataBuf, gNfcInfo.eccAlign_virAddr_1, 512);	
			}
	
			secEndInPP -= 1;			
		}
	
		/* read remains data */
		if (secStartInPP < secEndInPP) {
			if ((flag & 0x1) == 0x1)
				dataBuf = dataBufPtr + (ppOffsetE * USER_DATA_LEN) + (secStartInPP * SECOTR_PART_SIZE);
			if ((flag & 0x2) == 0x2)
				spareBuf = spareBufPtr + (ppOffsetE * SYS_DATA_LEN) + (secStartInPP * FTL_SPARE_SIZE_PER_PART);
	
			/* involk read a physical page function */
			ret |= SEP_Read_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, spareBuf, flag);
		}

 exit_FLASH_Read_TwoPlane_f:
		FTL_Low_Driver_Out();
		return ret;	
	}
}

/**************************************************************************************
Description: Read data from PPs of a VPP through interleaving two-plane page read.
Input:
		ceNo, the CE number.
		rowAddr,
				[0]: the row address of plane 0 in chip 0(first plane of first chip of the interleaving two-plane).
				[1]: the row address of plane 1 in chip 0(second plane of first chip of the interleaving two-plane).
				[2]: the row address of plane 0 in chip 1(first plane of second chip of the interleaving two-plane).
				[3]: the row address of plane 1 in chip 1(second plane of second chip of the interleaving two-plane).
		startOffsetInLPPN, start sector's offset in the LPN.
		endOffsetInLPPN, end sector's offset in the LPN.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
		flag,
			 (flag & 0x1) = 1, read user data.
			 (flag & 0x2) = 2, read spare data.
Return:
		0--successful.
		others--Read failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_Read_InterleavingTwoPlane_f(uint8_t ceNo, uint32_t *rRowAddr, uint8_t startOffsetInLPPN, uint8_t endOffsetInLPPN,
														uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{	
	uint8_t ret;
	uint8_t i;	
	uint8_t ppOffsetS;
	uint8_t ppOffsetE;
	
	DEBUG_TMP("\n[FLASH_Read_InterleavingTwoPlane_f], ceNo: %d, rRowAddr[0]: 0x%x, rRowAddr[1]: 0x%x, rRowAddr[2]: 0x%x, rRowAddr[3]: 0x%x, startOffsetInLPPN: %d, endOffsetInLPPN: %d, dataBufPtr: 0x%x, spareBufPtr: 0x%x, flag: %d\n",
			ceNo, rRowAddr[0], rRowAddr[1], rRowAddr[2], rRowAddr[3], startOffsetInLPPN, endOffsetInLPPN, (uint32_t)dataBufPtr, (uint32_t)spareBufPtr, flag);
		
	ppOffsetS = startOffsetInLPPN / FTL_PARTS_PER_PP;
	ppOffsetE = endOffsetInLPPN / FTL_PARTS_PER_PP;

	gNfcInfo.failOp = NO_OPERATE_FAIL;

	mutex_lock(&gNfcInfo.mutex_sepnfc);

	writel( NFC_CE_CARE(0)         | NFC_CE_REG(0)     | NFC_EN_TRANS_DONE_INT(0)	\
		  | NFC_EN_DEC_DONE_INT(0) | NFC_EN_RnB_INT(0) | NFC_RnB_TRAN_MODE(1), NAND_CTRL_V);

	for (i = 0; i < 2; i++) {		
		ret = SEP_WaitforReady_f(0xF1 + i);	/* 0xF1 or 0xF2 */
		if (ret) {
			DEBUG_OUT("[FLASH_Read_InterleavingTwoPlane_f], chip %d, previous operation fails or is overtime: 0x%x.\n", i, ret);
			gNfcInfo.failOp = PREVIOUS;
			for (i = 0; i < 2; i++) {
				SEP_ResetChip(0xF1 + i); 
				gNfcInfo.interleave_state[i] = FTL_READY;
			}
			mutex_unlock(&gNfcInfo.mutex_sepnfc);
			goto exit_FLASH_Read_InterleavingTwoPlane_f;
		}
	}
	mutex_unlock(&gNfcInfo.mutex_sepnfc);
		
	if (ppOffsetS == ppOffsetE) { /* in the same physical page, use normal read */
		ret = FLASH_Read_f(ceNo, rRowAddr, startOffsetInLPPN, endOffsetInLPPN, dataBufPtr, spareBufPtr, flag);
		
	} else if (ppOffsetE == 1 || ppOffsetS == 2){ /* in the same chip, use two-plane read */
		ret = FLASH_Read_TwoPlane_f(ceNo, rRowAddr, startOffsetInLPPN, endOffsetInLPPN, dataBufPtr, spareBufPtr, flag);
		
	} else { /* in different chip, (ppOffsetS <= 1 &&  ppOffsetE >= 2) */
		#define FTL_PARTS_TWO_PP  (FTL_PARTS_PER_PP * 2)	
	
		/* read pages in chip 0 */
		ret = FLASH_Read_TwoPlane_f(ceNo, rRowAddr, startOffsetInLPPN, FTL_PARTS_TWO_PP -1, dataBufPtr, spareBufPtr, flag);
		if (ret == FLASH_RB_TIMEOUT) 
			goto exit_FLASH_Read_InterleavingTwoPlane_f;
			
		/* read pages in chip 1 */
		ret |= FLASH_Read_TwoPlane_f(ceNo, rRowAddr, FTL_PARTS_TWO_PP, endOffsetInLPPN, dataBufPtr, spareBufPtr, flag);
	}

 exit_FLASH_Read_InterleavingTwoPlane_f:
	for (i = 0; i < 2; i++) 
		gNfcInfo.interleave_state[i] = FTL_READY;

	if (ret == FLASH_RB_TIMEOUT)
		gNfcInfo.failOp = CURRENT;

	return ret;
}

/**************************************************************************************
Description: Copyback data to new PP through normal page copy-back program, spare data SHALL be updated.
Input:
		ceNo, the CE number.
		copyBackPtr, points to a structure holds neccessary address and offset information for copyback.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
		flag,
			 0--copy part sectors back from previouse PP.
			 1--copy whole PP.
Output:
		0--successful.
		others--Copyback failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_HardCopyBack_f(uint8_t ceNo, FTL_COPYBACK_PTR copyBackPtr, uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{
	uint8_t ret;
	char secStartInPP = 0;
	char secEndInPP = 0;
	uint8_t *dataBuf = NULL;

	DEBUG_TMP("\n[FLASH_HardCopyBack_f], ceNo: %d, startOffsetInLPPN: %d, endOffsetInLPPN: %d, dataBufPtr: 0x%x, spareBufPtr: 0x%x, flag: %d\n",
				ceNo, copyBackPtr->startOffsetInLPPN, copyBackPtr->endOffsetInLPPN, (uint32_t)dataBufPtr, (uint32_t)spareBufPtr, flag);
				
	if (flag == 0) { /* There are part data in RAM */
		secStartInPP = copyBackPtr->startOffsetInLPPN;
		secEndInPP = copyBackPtr->endOffsetInLPPN;
	}
	
	FTL_Low_Driver_Into();			
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	/* issue the 1st command of read for copyback */
	SEP_WriteCmd_f(0x00);
	/* issue source addess */
	SEP_WriteAddr_f(0, copyBackPtr->rRowAddr[0]);
	/* issue the 2nd command of read for copyback */
	SEP_WriteCmd_f(0x35);
	/* record flash state */
	gNfcInfo.state = FTL_READING;

	/* wait for tR */	
	ret = SEP_WaitforReady_f(0x70);
	if (ret) {
		SEP_ResetChip(0x70);
		goto exit_FLASH_HardCopyBack_f;
	}
	
	if (flag == 0) { /* There are part data in RAM */
		/* issue read data command after read status (must) */
		SEP_WriteCmd_f(0x00);	
				
		/*
		 * If copyBackPtr->startOffsetInLPPN is a odd, 
		 * we need to read additionally the sector before it, 
		 * let them to combine a ECC unit.
		 */
		if (secStartInPP & 0x1) { 			
			SEP_Read_for_eccAlign(0, secStartInPP >> 1, NULL, 0x1);
			dataBuf = dataBufPtr + (secStartInPP << 9);
			memcpy(gNfcInfo.eccAlign_virAddr_0 + 512, dataBuf, 512);				
		}
		
		/*
		 * If copyBackPtr->endOffsetInLPPN is a even, 
		 * we need to read additionally the sector after it, 
		 * let them to combine a ECC unit.
		 */		
		if (!(secEndInPP & 0x1)) { 			
			SEP_Read_for_eccAlign(1, secEndInPP >> 1, NULL, 0x1);
			dataBuf = dataBufPtr + (secEndInPP << 9);
			memcpy(gNfcInfo.eccAlign_virAddr_1, dataBuf, 512);				
		}
	}
		
	/* issue copyback command */	
	SEP_WriteCmd_f(0x85);
	/* issue destination address */
	SEP_WriteAddr_f(0, copyBackPtr->wRowAddr[0]);
		
	if (flag == 0) { /* There are part data in RAM */
		if (secStartInPP & 0x1) { /* odd */
			SEP_Write_for_eccAlign(0, secStartInPP >> 1);
			secStartInPP += 1;
		} 			
		
		if (!(secEndInPP & 0x1)) { /* even */
			SEP_Write_for_eccAlign(1, secEndInPP >> 1);
			secEndInPP -= 1;
		} 
		
		/* write remains data */
		if (secStartInPP < secEndInPP) {
			dataBuf = dataBufPtr + (secStartInPP << 9);
			SEP_Write_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, NULL, 0x1);
		}					
	}
	
	/* update spare area data */
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP -1) >> 1, NULL, spareBufPtr, 0x2);
	
	/* issue the program confirm command */
	SEP_WriteCmd_f(0x10);		
	/* record flash state */
	gNfcInfo.state = FTL_WRITING;
	
	/* wait for tPROG */
	ret = SEP_WaitforReady_f(0x70);
	if (ret)
		SEP_ResetChip(0x70);

 exit_FLASH_HardCopyBack_f:
	FTL_Low_Driver_Out();	
	return ret;
} 

/**************************************************************************************
Description: Copyback data to new PP through page copy-back program, spare data SHALL be updated.
Input:
		ceNo, the CE number, the lower CE# of the two CE.
		copyBackPtr, points to a structure holds neccessary address and offset information for copyback.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
		flag,
			 0--copy part sectors back from previouse PP.
			 1--copy whole PP.
Output:
		0--successful.
		others--Copyback failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_HardCopyBack_Interleaving_f(uint8_t ceNo, FTL_COPYBACK_PTR copyBackPtr, uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{
	return 0;
}	

/**************************************************************************************
Description: CopyBack data to new VPP through two-plane copyback, spare data SHALL be updated.
Input:
		ceNo, the CE number.
		copyBackPtr, points to a structure holds neccessary address and offset information for copyback.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
		flag,
			 0--copy part sectors back from previouse VPP.
			 1--copy whole VPP.
Output:
		0--successful.
		others--copy back failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_HardCopyBack_TwoPlane_f(uint8_t ceNo, FTL_COPYBACK_PTR copyBackPtr, uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{
	uint8_t ret = 0;
	uint8_t ppOffsetS = 0;
	uint8_t ppOffsetE = 0;
	char secS4eccAlign = 0;
	char secE4eccAlign = 0;
	char secStartInPP = 0;
	char secEndInPP = 0;
	uint8_t *dataBuf = NULL;	
	
	DEBUG_TMP("\n[FLASH_HardCopyBack_TwoPlane_f], ceNo: %d, startOffsetInLPPN: %d, endOffsetInLPPN: %d, dataBufPtr: 0x%x, spareBufPtr: 0x%x, flag: %d\n",
				ceNo, copyBackPtr->startOffsetInLPPN, copyBackPtr->endOffsetInLPPN, (uint32_t)dataBufPtr, (uint32_t)spareBufPtr, flag);
				
	if (flag == 0) { /* There are part data in RAM */
		ppOffsetS = copyBackPtr->startOffsetInLPPN / FTL_PARTS_PER_PP;
		ppOffsetE = copyBackPtr->endOffsetInLPPN / FTL_PARTS_PER_PP;
		secS4eccAlign = copyBackPtr->startOffsetInLPPN % FTL_PARTS_PER_PP;
		secE4eccAlign = copyBackPtr->endOffsetInLPPN % FTL_PARTS_PER_PP;		
	}
	
	FTL_Low_Driver_Into();			
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);
	
	/* issue 1st command of two-plane read */
	SEP_WriteCmd_f(0x60);
	/* issue row address of plane 0 */
	SEP_WriteAddr_f(-1, copyBackPtr->rRowAddr[0]);
	/* issue 2nd command of two-plane read */
	SEP_WriteCmd_f(0x60);
	/* issue row address of plane 1 */
	SEP_WriteAddr_f(-1, copyBackPtr->rRowAddr[1]);
	/* issue 3rd command of two-plane read for copyback */
	SEP_WriteCmd_f(0x35);
	/* record flash state */
	gNfcInfo.state = FTL_READING;

	/* wait for tR */
	ret = SEP_WaitforReady_f(0x70);
	if (ret) {
		SEP_ResetChip(0x70);
		goto exit_FLASH_HardCopyBack_TwoPlane_f;
	}
		
	if (flag == 0) { /* There are part data in RAM */
		/*
		 * If copyBackPtr->startOffsetInLPPN is a odd, 
		 * we need to read additionally the sector before it, 
		 * let them to combine a ECC unit.
		 */
		if (secS4eccAlign & 0x1) {
			/* issue read command, address */
			SEP_WriteCmd_f(0x00);
			SEP_WriteAddr_f(0, copyBackPtr->rRowAddr[ppOffsetS]);
								
			SEP_Read_for_eccAlign(0, secS4eccAlign >> 1, NULL, 0x1);
			dataBuf = dataBufPtr + ppOffsetS * USER_DATA_LEN + (secS4eccAlign << 9);	
			memcpy(gNfcInfo.eccAlign_virAddr_0 + 512, dataBuf, 512);				
		}
		
		/*
		 * If copyBackPtr->endOffsetInLPPN is a even, 
		 * we need to read additionally the sector after it, 
		 * let them to combine a ECC unit.
		 */		
		if (!(secE4eccAlign & 0x1)) {
			if ((ppOffsetS != ppOffsetE) || (!(secS4eccAlign & 0x1))) {
				/* issue read command, address */							
				SEP_WriteCmd_f(0x00);
				SEP_WriteAddr_f(0, copyBackPtr->rRowAddr[ppOffsetE]);	
			}
						
			SEP_Read_for_eccAlign(1, secE4eccAlign >> 1, NULL, 0x1);
			dataBuf = dataBufPtr + ppOffsetE * USER_DATA_LEN + (secE4eccAlign << 9);	
			memcpy(gNfcInfo.eccAlign_virAddr_1, dataBuf, 512);				
		}
	}
	
	/* issue copyback command */
	SEP_WriteCmd_f(0x85);
	/* issue destination address of plane 0 */
	SEP_WriteAddr_f(0, copyBackPtr->wRowAddr[0]);
	
	if (flag == 0) { /* There are part data in RAM */
		if (ppOffsetS == 0) {
			if (secS4eccAlign & 0x1) { /* odd */
				SEP_Write_for_eccAlign(0, secS4eccAlign >> 1);
				secStartInPP = secS4eccAlign + 1;
			} else
				secStartInPP = secS4eccAlign; 
				
			if (ppOffsetE == 0) {
				if (!(secE4eccAlign & 0x1)) { /* even */
					SEP_Write_for_eccAlign(1, secE4eccAlign >> 1);
					secEndInPP = secE4eccAlign - 1;
				} else
					secEndInPP = secE4eccAlign;					
			} else 
				secEndInPP = FTL_PARTS_PER_PP - 1;
				
			/* write remains data */
			if (secStartInPP < secEndInPP) {
				dataBuf = dataBufPtr + (secStartInPP << 9);
				SEP_Write_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, NULL, 0x1);
			}	
		} 	
	}
	
	/* update spare area data of plane 0 */
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP -1) >> 1, NULL, spareBufPtr, 0x2);
	
	/* issue dummy program command for plane 0 */
	SEP_WriteCmd_f(0x11);				
	/* record flash state */
	gNfcInfo.state = FTL_DUMMY_WRITING;
	
	/* wait for tDBSY */
	ret = SEP_WaitforReady_f(0x70);
	if (ret) {
		SEP_ResetChip(0x70);
		goto exit_FLASH_HardCopyBack_TwoPlane_f;
	}
		
	/* issue copyback command */
	SEP_WriteCmd_f(0x81);
	/* issue destination address of plane 1 */
	SEP_WriteAddr_f(0, copyBackPtr->wRowAddr[1]);
	
	if (flag == 0) { /* There are part data in RAM */
		if (ppOffsetE == 1) {
			if (ppOffsetS == 1) {
				if (secS4eccAlign & 0x1) { /* odd */
					SEP_Write_for_eccAlign(0, secS4eccAlign >> 1);
					secStartInPP = secS4eccAlign + 1;
				} else
					secStartInPP = secS4eccAlign;
			} else 
				secStartInPP = 0;
				
			if (!(secE4eccAlign & 0x1)) { /* even */
				SEP_Write_for_eccAlign(1, secE4eccAlign >> 1);
				secEndInPP = secE4eccAlign -1;
			} else
				secEndInPP = secE4eccAlign;
		
			/* write remains data */
			if (secStartInPP < secEndInPP) {
				dataBuf = dataBufPtr + USER_DATA_LEN + (secStartInPP << 9);
				SEP_Write_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, NULL, 0x1);
			}
		}		
	}
	
	/* update spare area data of plane 1 */
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP - 1) >> 1, NULL, spareBufPtr + SYS_DATA_LEN, 0x2);
	
	/* issue the program confirm command */
	SEP_WriteCmd_f(0x10);				
	/* record flash state */
	gNfcInfo.state = FTL_WRITING;
	
	/* wait for tPROG */
	ret = SEP_WaitforReady_f(0x70);
	if (ret)
		SEP_ResetChip(0x70);

 exit_FLASH_HardCopyBack_TwoPlane_f:
	FTL_Low_Driver_Out();		
	return ret;				
}

/**************************************************************************************
Description: CopyBack data to new VPP through interleaving two-plane copyback, spare data SHALL be updated.
Input:
		ceNo, the CE number.
		copyBackPtr, points to a structure holds neccessary address and offset information for copyback.
		dataBufPtr, points to the data buffer of upper layer.
		spareBufPtr, points to the buffer of FTL system data.
		flag,
			 0--copy part sectors back from previouse VPP.
			 1--copy whole VPP.
Output:
		0--successful.
		others--Copyback failure or timeout.
**************************************************************************************/
__INLINE__ uint8_t FLASH_HardCopyBack_InterleavingTwoPlane_f(uint8_t ceNo, FTL_COPYBACK_PTR copyBackPtr, uint8_t *dataBufPtr, uint8_t *spareBufPtr, uint8_t flag)
{
	uint8_t ret = 0;
	uint8_t i;
	uint8_t ppOffsetS = 0;
	uint8_t ppOffsetE = 0;
	char secS4eccAlign = 0;
	char secE4eccAlign = 0;
	char secStartInPP = 0;
	char secEndInPP = 0;
	uint8_t *dataBuf = NULL;
	
	DEBUG_TMP("\n[FLASH_HardCopyBack_InterleavingTwoPlane_f], ceNo: %d, startOffsetInLPPN: %d, endOffsetInLPPN: %d, dataBufPtr: 0x%x, spareBufPtr: 0x%x, flag: %d\n",
				ceNo, copyBackPtr->startOffsetInLPPN, copyBackPtr->endOffsetInLPPN, (uint32_t)dataBufPtr, (uint32_t)spareBufPtr, flag);
				
	if (flag == 0) { /* There are part data in RAM */
		ppOffsetS = copyBackPtr->startOffsetInLPPN / FTL_PARTS_PER_PP;
		ppOffsetE = copyBackPtr->endOffsetInLPPN / FTL_PARTS_PER_PP;		
		secS4eccAlign = copyBackPtr->startOffsetInLPPN % FTL_PARTS_PER_PP;
		secE4eccAlign = copyBackPtr->endOffsetInLPPN % FTL_PARTS_PER_PP;	
	}
	
	FTL_Low_Driver_Into();	
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	gNfcInfo.failOp = NO_OPERATE_FAIL;
	
	/*=======================================================chip 0================================================================*/
	/* wait for chip 0 be ready */
	ret = SEP_WaitforReady_f(0xF1);
	if (ret) {
		DEBUG_OUT("[FLASH_HardCopyBack_InterleavingTwoPlane_f], chip 0, previous operation fails or is overtime: 0x%x.\n", ret);
		gNfcInfo.failOp = PREVIOUS;
		for (i = 0; i < 2; i++) {
			SEP_ResetChip(0xF1 + i);
			gNfcInfo.interleave_state[i] = FTL_READY;
		}
		goto exit_FLASH_HardCopyBack_InterleavingTwoPlane_f;
	}

	/* issue 1st command of two-plane read */
	SEP_WriteCmd_f(0x60);
	/* issue row address of plane 0 in chip 0 */
	SEP_WriteAddr_f(-1, copyBackPtr->rRowAddr[0]);
	/* issue 2nd command of two-plane read */
	SEP_WriteCmd_f(0x60);
	/* issue row address of plane 1 in chip 0 */
	SEP_WriteAddr_f(-1, copyBackPtr->rRowAddr[1]);
	/* issue 3rd command of two-plane read for copyback */
	SEP_WriteCmd_f(0x35);
	/* record chip 0 state */
	gNfcInfo.interleave_state[0] = FTL_READING;

	/* wait for tR */
	ret = SEP_WaitforReady_f(0xF1);
	if (ret) {
		DEBUG_OUT("[FLASH_HardCopyBack_InterleavingTwoPlane_f], chip 0, waits for tR is overtime.\n");
		ret = SEP_WaitforReady_f(0xF2);
		if (ret)
			gNfcInfo.failOp = PRE_CUR;
		else
			gNfcInfo.failOp = CURRENT;
		for (i = 0; i < 2; i++) {
			SEP_ResetChip(0xF1 + i);
			gNfcInfo.interleave_state[i] = FTL_READY;
		}
		goto exit_FLASH_HardCopyBack_InterleavingTwoPlane_f;
	}
		
	if (flag == 0) { /* There are part data in RAM */
		if (ppOffsetS <= 1) {
			/*
			 * If copyBackPtr->startOffsetInLPPN is a odd, 
			 * we need to read additionally the sector before it, 
			 * let them to combine a ECC unit.
			 */
			if (secS4eccAlign & 0x1) {
				/* issue read command, address */
				SEP_WriteCmd_f(0x00);
				SEP_WriteAddr_f(0, copyBackPtr->rRowAddr[ppOffsetS]);
							
				SEP_Read_for_eccAlign(0, secS4eccAlign >> 1, NULL, 0x1);
				dataBuf = dataBufPtr + ppOffsetS * USER_DATA_LEN + (secS4eccAlign << 9);			
				memcpy(gNfcInfo.eccAlign_virAddr_0+ 512, dataBuf, 512);				
			}

			if (ppOffsetE <= 1) {
				/*
				 * If copyBackPtr->endOffsetInLPPN is a even, 
			 	 * we need to read additionally the sector after it, 
				 * let them to combine a ECC unit.
		 		 */		
				if (!(secE4eccAlign & 0x1)) {
					if ((ppOffsetS != ppOffsetE) || (!(secS4eccAlign & 0x1)))	{
						/* issue read command, address */
						SEP_WriteCmd_f(0x00);
						SEP_WriteAddr_f(0, copyBackPtr->rRowAddr[ppOffsetE]);	
					}
						
					SEP_Read_for_eccAlign(1, secE4eccAlign >> 1, NULL, 0x1);
					dataBuf = dataBufPtr + ppOffsetE * USER_DATA_LEN + (secE4eccAlign << 9);		
					memcpy(gNfcInfo.eccAlign_virAddr_1, dataBuf, 512);				
				}
			}
		}
	}

	/* issue copyback command */
	SEP_WriteCmd_f(0x85);
	/* issue destination address of plane 0 in chip 0 */
	SEP_WriteAddr_f(0, copyBackPtr->wRowAddr[0]);
	
	if (flag == 0) { /* There are part data in RAM */
		if (ppOffsetS == 0) {
			if (secS4eccAlign & 0x1) { /* If copyBackPtr->startOffsetInLPPN is a odd */
				SEP_Write_for_eccAlign(0, secS4eccAlign >> 1);
				secStartInPP = secS4eccAlign + 1;
			} else
				secStartInPP = secS4eccAlign;
				
			if (ppOffsetE == 0) {
				if (!(secE4eccAlign & 0x1)) { /* If copyBackPtr->endOffsetInLPPN is a even */
					SEP_Write_for_eccAlign(1, secE4eccAlign >> 1);
					secEndInPP = secE4eccAlign - 1;
				} else
					secEndInPP = secE4eccAlign;
			} else 
				secEndInPP = FTL_PARTS_PER_PP - 1;
				
			/* write remains data */
			if (secStartInPP < secEndInPP) {
				dataBuf = dataBufPtr + (secStartInPP << 9);
				SEP_Write_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, NULL, 0x1);	
			}
		} 	
	}
	
	/* update spare area data of plane 0 in chip 1 */
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP - 1) >> 1, NULL, spareBufPtr, 0x2);
	
	/* issue dummy program command for plane 0  chip 1 */
	SEP_WriteCmd_f(0x11);				
	/* record chip 0 state */
	gNfcInfo.interleave_state[0] = FTL_DUMMY_WRITING;	

	/* wait for tDBSY */
	ret = SEP_WaitforReady_f(0xF1);
	if (ret) {
		DEBUG_OUT("[FLASH_HardCopyBack_InterleavingTwoPlane_f], chip 0 waits for tDBSY is overtime.\n");
		ret = SEP_WaitforReady_f(0xF2);
		if (ret)
			gNfcInfo.failOp = PRE_CUR;
		else
			gNfcInfo.failOp = CURRENT;
		for (i = 0; i < 2; i++) {
			SEP_ResetChip(0xF1 + i);
			gNfcInfo.interleave_state[i] = FTL_READY;
		}
		goto exit_FLASH_HardCopyBack_InterleavingTwoPlane_f;
	}
		
	/* issue copyback command */
	SEP_WriteCmd_f(0x81);
	/* issue destination address of plane 1 in chip 0 */
	SEP_WriteAddr_f(0, copyBackPtr->wRowAddr[1]);
	
	if (flag == 0) { /* There are part data in RAM */
		if (ppOffsetS <= 1 && ppOffsetE >= 1) {
			if (ppOffsetS == 1) {
				if (secS4eccAlign & 0x1) { /* odd */
					SEP_Write_for_eccAlign(0, secS4eccAlign >> 1);
					secStartInPP = secS4eccAlign + 1;
				} else
					secStartInPP = secS4eccAlign;
			} else 
				secStartInPP = 0;
			
			if (ppOffsetE == 1) {
				if (!(secE4eccAlign & 0x1)) { /* even */
					SEP_Write_for_eccAlign(1, secE4eccAlign >> 1);
					secEndInPP = secE4eccAlign - 1;
				} else
					secEndInPP = secE4eccAlign;
			} else
				secEndInPP = FTL_PARTS_PER_PP - 1;
		
			/* write remains data */
			if (secStartInPP < secEndInPP) {
				dataBuf = dataBufPtr + USER_DATA_LEN + (secStartInPP << 9);
				SEP_Write_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, NULL, 0x1);
			}
		}				
	}
	
	/* update spare area data of plane 1 in chip 0*/
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP - 1) >> 1, NULL, spareBufPtr + SYS_DATA_LEN, 0x2);
	
	/* issue the program confirm command */
	SEP_WriteCmd_f(0x10);
	/* record chip 0 state */
	gNfcInfo.interleave_state[0] = FTL_WRITING;

	/*=======================================================chip 1================================================================*/
	/* wait for chip 1 be ready */
	ret = SEP_WaitforReady_f(0xF2);
	if (ret) {
		DEBUG_OUT("[FLASH_HardCopyBack_InterleavingTwoPlane_f], chip 1, previous operation fail or is overtime: 0x%x.\n", ret);
		gNfcInfo.failOp = PREVIOUS;
		for (i = 0; i < 2; i++) {
			SEP_ResetChip(0xF1 + i);
			gNfcInfo.interleave_state[i] = FTL_READY;
		}
		goto exit_FLASH_HardCopyBack_InterleavingTwoPlane_f;
	}

	/* issue 1st command of two-plane read */
	SEP_WriteCmd_f(0x60);
	/* issue row address of plane 0 in chip 1 */
	SEP_WriteAddr_f(-1, copyBackPtr->rRowAddr[2]);
	/* issue 2nd command of two-plane read */
	SEP_WriteCmd_f(0x60);
	/* issue row address of plane 1 in chip 1 */
	SEP_WriteAddr_f(-1, copyBackPtr->rRowAddr[3]);
	/* issue 3rd command of two-plane read for copyback */
	SEP_WriteCmd_f(0x35);
	/* record chip 1 state */
	gNfcInfo.interleave_state[1] = FTL_READING;

	/* wait for tR */
	ret |= SEP_WaitforReady_f(0xF2);
	if (ret) {
		DEBUG_OUT("[FLASH_HardCopyBack_InterleavingTwoPlane_f], chip 1 waits for tR overtime.\n");
		gNfcInfo.failOp = CURRENT;
		for (i = 0; i < 2; i++) {
			SEP_ResetChip(0xF1 + i);
			gNfcInfo.interleave_state[i] = FTL_READY;
		}
		goto exit_FLASH_HardCopyBack_InterleavingTwoPlane_f;
	}
		
	if (flag == 0) { /* There are part data in RAM */
		if (ppOffsetE >= 2) {
			/*
			 * If copyBackPtr->startOffsetInLPPN is a odd, 
			 * we need to read additionally the sector before it, 
			 * let them to combine a ECC unit.
			 */
			if (ppOffsetS >= 2) {
				if (secS4eccAlign & 0x1) {
					/* issue read command, address */
					SEP_WriteCmd_f(0x00);
					SEP_WriteAddr_f(0, copyBackPtr->rRowAddr[ppOffsetS]);
								
					SEP_Read_for_eccAlign(0, secS4eccAlign >> 1, NULL, 0x1);
					dataBuf = dataBufPtr + ppOffsetS * USER_DATA_LEN + (secS4eccAlign << 9);			
					memcpy(gNfcInfo.eccAlign_virAddr_0 + 512, dataBuf, 512);				
				}
			}
		
			/*
			 * If copyBackPtr->endOffsetInLPPN is a even, 
		 	 * we need to read additionally the sector after it, 
			 * let them to combine a ECC unit.
		 	 */		
			if (!(secE4eccAlign & 0x1)) {
				if ((ppOffsetS != ppOffsetE) || (!(secS4eccAlign & 0x1)))	{
					/* issue read command, address */
					SEP_WriteCmd_f(0x00);
					SEP_WriteAddr_f(0, copyBackPtr->rRowAddr[ppOffsetE]);	
				}
						
				SEP_Read_for_eccAlign(1, secE4eccAlign >> 1, NULL, 0x1);
				dataBuf = dataBufPtr + ppOffsetE * USER_DATA_LEN + (secE4eccAlign << 9);			
				memcpy(gNfcInfo.eccAlign_virAddr_1, dataBuf, 512);				
			}
		}
	}
	/* issue copyback command */
	SEP_WriteCmd_f(0x85);
	/* issue destination address of plane 0 in chip 1 */
	SEP_WriteAddr_f(0, copyBackPtr->wRowAddr[2]);
	
	if (flag == 0) { /* There are part data in RAM */
		if (ppOffsetS <= 2 && ppOffsetE >= 2) {
			if (ppOffsetS == 2) {
				if (secS4eccAlign & 0x1) { /* odd */
					SEP_Write_for_eccAlign(0, secS4eccAlign >> 1);
					secStartInPP = secS4eccAlign + 1;
				} else
					secStartInPP = secS4eccAlign;
			} else
				secStartInPP = 0;				
				
			if (ppOffsetE == 2) {
				if (!(secE4eccAlign & 0x1)) { /* even */
					SEP_Write_for_eccAlign(1, secE4eccAlign >> 1);
					secEndInPP = secE4eccAlign - 1;
				} else
					secEndInPP = secE4eccAlign;
			} else 
				secEndInPP = FTL_PARTS_PER_PP - 1;
				
			/* write remains data */
			if (secStartInPP < secEndInPP) {
				dataBuf = dataBufPtr + (USER_DATA_LEN << 1) + (secStartInPP << 9);
				SEP_Write_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, NULL, 0x1);
			}	
		} 	
	}
	
	/* update spare area data of plane 0 in chip 1 */
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP -1) >> 1, NULL, spareBufPtr + (SYS_DATA_LEN << 1), 0x2);
	
	/* issue dummy program command for plane 0  chip 1 */
	SEP_WriteCmd_f(0x11);				
	/* record chip 1 state */
	gNfcInfo.interleave_state[1] = FTL_DUMMY_WRITING;
	
	/* wait for tDBSY */
	ret = SEP_WaitforReady_f(0xF2);
	if (ret) {
		DEBUG_OUT("[FLASH_HardCopyBack_InterleavingTwoPlane_f], chip 0 waits for tDBSY overtime.\n");
		gNfcInfo.failOp = CURRENT;
		for (i = 0; i < 2; i++) {
			SEP_ResetChip(0xF1 + i);
			gNfcInfo.interleave_state[i] = FTL_READY;
		}
		goto exit_FLASH_HardCopyBack_InterleavingTwoPlane_f;
	}
		
	/* issue copyback command */
	SEP_WriteCmd_f(0x81);
	/* issue destination address of plane 1 in chip 1*/
	SEP_WriteAddr_f(0, copyBackPtr->wRowAddr[3]);
	
	if (flag == 0) { /* There are part data in RAM */
		if (ppOffsetE == 3) {
			if (ppOffsetS == 3) {
				if (secS4eccAlign & 0x1) { /* odd */
					SEP_Write_for_eccAlign(0, secS4eccAlign >> 1);
					secStartInPP = secS4eccAlign + 1;
				} else
					secStartInPP = secS4eccAlign; 
			} else 
				secStartInPP = 0;
			
			if (!(secE4eccAlign & 0x1)) { /* even */
				SEP_Write_for_eccAlign(1, secE4eccAlign >> 1);
				secEndInPP = secE4eccAlign - 1;
			} else
				secEndInPP = secE4eccAlign;
		
			/* write remains data */
			if (secStartInPP < secEndInPP) {
				dataBuf = dataBufPtr + USER_DATA_LEN * 3 + (secStartInPP << 9);
				SEP_Write_Page_f(secStartInPP >> 1, secEndInPP >> 1, dataBuf, NULL, 0x1);
			}
		}
	}
	
	/* update spare area data of plane 1 in chip 1 */
	SEP_Write_Page_f(0, (FTL_PARTS_PER_PP - 1) >> 1, NULL, spareBufPtr + SYS_DATA_LEN * 3, 0x2);
	
	/* issue the program confirm command */
	SEP_WriteCmd_f(0x10);
	/* record chip 1 state */
	gNfcInfo.interleave_state[1] = FTL_WRITING;

 exit_FLASH_HardCopyBack_InterleavingTwoPlane_f:	
	FTL_Low_Driver_Out();
	return ret;				
}
