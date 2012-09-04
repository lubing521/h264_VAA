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
 *  06-08-2011 fjj initial version
 *  07-15-2011 fjj second version
 */
 
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/mtd/partitions.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/nand_ecc.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/clk.h>

#include <asm/sizes.h>
#include <asm/io.h>
#include <asm/irq.h>

#include <mach/nand.h>
#include <mach/dma.h>
#include <mach/hardware.h>

#ifdef CONFIG_CPU_FREQ
#include <linux/cpufreq.h>
#endif

#ifdef CONFIG_PM
#include <linux/pm.h>
#endif

/* default for H27UBG8T2ATR */
SEP_NFC_INFO gNfcInfo = {
	.cs = 0,
	.eccUnitDatalen = 1024 + 42,
	.ftlSysDataStartPos = (1024 + 42) * 8,
	.state = FTL_READY
};

/* mtd partition infomation */
SEP_MTD_PARTS sep0611_partitions;

struct mtd_info *sep_nfc;

EXPORT_SYMBOL(gNfcInfo);
EXPORT_SYMBOL(sep0611_partitions);
EXPORT_SYMBOL(sep_nfc);

/* default for HY27UF081G2M */
static struct mtd_partition __initdata default_mtd_parts[] = {
        [0] = {
                .name   = "u-boot",
                .offset = 0,
                .size   = MTD_SIZE_1MiB * 1,
                .mask_flags = MTD_WRITEABLE,
        },

        [1] = {
                .name   = "kernel",
                .offset = MTDPART_OFS_NXTBLK,
                .size   = MTD_SIZE_1MiB * 3,
                .mask_flags = MTD_WRITEABLE,
        },

        [2] = {
                .name   = "root",
                .offset = MTDPART_OFS_NXTBLK,
                .size   = MTDPART_SIZ_FULL,
        },
};

/* default for H27UBG8T2ATR */
//static struct mtd_partition __initdata default_mtd_parts[] = {
//	[0] = {
//		.name	= "lk",
//		.offset	= 0,
//		.size	= MTD_SIZE_1MiB * 2,
//		.mask_flags = MTD_WRITEABLE,
//	},
//
//	[1] = {
//		.name	= "misc",
//		.offset	= MTDPART_OFS_NXTBLK,
//		.size	= MTD_SIZE_1MiB * 2,
//		.mask_flags = MTD_WRITEABLE,
//	},
//	
//	[2] = {
//		.name	= "recovery",
//		.offset	= MTDPART_OFS_NXTBLK,
//		.size	= MTD_SIZE_1MiB * 10,
//		.mask_flags = MTD_WRITEABLE
//	},
//	
//	[3] = {
//		.name	= "boot",
//		.offset	= MTDPART_OFS_NXTBLK,
//		.size	= MTD_SIZE_1MiB * 10,
//		.mask_flags = MTD_WRITEABLE
//	},
//	
//	[4] = {
//		.name	= "system",
//		.offset	= MTDPART_OFS_NXTBLK,
//		.size	= MTD_SIZE_1MiB * 800,
//	},
//
//	[5] = {
//		.name	= "cache",
//		.offset	= MTDPART_OFS_NXTBLK,
//		.size	= MTD_SIZE_1MiB * 100,
//	},
//	
//	[6] = {
//		.name	= "userdata",
//		.offset	= MTDPART_OFS_NXTBLK,
//		.size	= MTD_SIZE_1MiB * 100 ,
//	},
//
//	[7] = {
//		.name	= "reserved",
//		.offset	= MTDPART_OFS_NXTBLK,
//		.size	= MTDPART_SIZ_FULL,
//	}
//};

#define MAX_SUPPORTED_SAMSUNG_NAND 8
const NAND_FEATURES SAMSUNG_NAND_DevInfo[] = {
 	//*================================================================================================================================================================
    //*[             ID            ][ Name ][                     Size                      ][   Address Cycle  ][           Timing Sequence             ][ Attribute ]
    //*----------------------------------------------------------------------------------------------------------------------------------------------------------------
    //* 1st, 2nd, 3rd, 4th, 5th, 6th, name, ChipSize(MB), BlockSize(KB), PageSize, SpareSize, ColCycle, RowCycle, tCLS, tALS, tWC, tWP, tWH, tRC, tRP, tREH 
    //*================================================================================================================================================================
	// [256M] K9K2G16U0M	
	{{0xEC, 0xCA, 0x00, 0x55, 0x00, 0x00},  "Samsung NAND 256M 3.3V 8-bit",  256, 128, 2048, 64,  2, 3,  25, 25, 45, 25, 15, 50, 25, 15,  A_16BIT},
	// [512MB] K9G4G08U0B
    {{0xEC, 0xDC, 0x14, 0xA5, 0x54, 0x00},  "Samsung NAND 512M 3.3V 8-bit",  512, 256, 2048, 64,  2, 3,  15, 15, 25, 15, 10, 25, 15, 10,  A_08BIT},
    // [1GB] K9G8G08U0A 
    {{0xEC, 0xD3, 0x14, 0xA5, 0x64, 0x00},  "Samsung NAND 1G 3.3V 8-bit",  1024, 256, 2048, 64,  2, 3,  30, 30, 50, 30, 20, 50, 30, 20,  A_08BIT},
    // [2GB] K9GAG08U0M
    {{0xEC, 0xD5, 0x14, 0xB6, 0x74, 0x00},  "Samsung NAND 2G 3.3V 8-bit",  2048, 512, 4096, 128,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10,  A_08BIT},
    // [4GB] K9LBG08U0M	 [8GB] K9HCG08U1M 
	{{0xEC, 0xD7, 0x55, 0xB6, 0x78, 0x00},  "Samsung NAND 4G 3.3V 8-bit",  4096, 512, 4096, 128,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10,  A_08BIT},
  	// [2GB] K9GAG08U0D
    {{0xEC, 0xD5, 0x94, 0x29, 0x34, 0x41},  "Samsung NAND 2G 3.3V 8-bit",  2048, 512, 4096, 218,  2, 3,  15, 15, 30, 15, 10, 30, 15, 10,  A_08BIT},
    // [4GB] K9LBG08U0D  [8GB] K9HCG08U1D  [16G] K9MDG08U5D
    {{0xEC, 0xD7, 0xD5, 0x29, 0x38, 0x41},  "Samsung NAND 4G 3.3V 8-bit",  4096, 512, 4096, 218,  2, 3,  15, 15, 30, 15, 10, 30, 15, 10,  A_08BIT},	
    // [2GB] K9GAG08U0E		
    {{0xEC, 0xD5, 0x84, 0x72, 0x50, 0x42},  "Samsung NAND 2G 3.3V 8-bit",  2048, 1024, 8192, 436,  2, 3,  15, 15, 30, 15, 10, 30, 15, 10,  A_08BIT}
}; 

#define MAX_SUPPORTED_HYNIX_NAND 7
const NAND_FEATURES HYNIX_NAND_DevInfo[] = {
 	//*================================================================================================================================================================
    //*[             ID            ][ Name ][                     Size                      ][   Address Cycle  ][           Timing Sequence             ][ Attribute ]
    //*----------------------------------------------------------------------------------------------------------------------------------------------------------------
    //* 1st, 2nd, 3rd, 4th, 5th, 6th, name, ChipSize(MB), BlockSize(KB), PageSize, SpareSize, ColCycle, RowCycle, tCLS, tALS, tWC, tWP, tWH, tRC, tRP, tREH 
    //*================================================================================================================================================================
   	// [128MB] HY27UF081G2A 
    {{0xAD, 0xF1, 0x80, 0x15, 0xAD, 0xF1},  "Hynix NAND 128M 3.3V 8-bit",  128, 128, 2048, 64,  2, 2,  5, 5, 60, 40, 20, 60, 25, 30, A_08BIT}, // modify by hoursjl
    {{0xAD, 0xF1, 0x80, 0x1D, 0x00, 0x00},  "Hynix NAND 128M 3.3V 8-bit",  128, 128, 2048, 64,  2, 2,  15, 15, 30, 15, 10, 30, 15, 10, A_08BIT},
    // [512MB] HY27UF084G2B
    {{0xAD, 0xDC, 0x10, 0x95, 0x00, 0x00},  "Hynix NAND 512M 3.3V 8-bit",  512, 128, 2048, 64,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10,  A_08BIT},
	//[1GB] HY27UF081G2M
    {{0xAD, 0xF1, 0x00, 0x15, 0x00, 0x00},  "Hynix NAND 1G 3.3V 8-bit",  128, 128, 2048, 64,  2, 2,  5, 5, 60, 40, 20, 20, 25, 30,  A_08BIT},
	//[2GB] HY27UF082G2b
    {{0xAD, 0xDA, 0x10, 0x95, 0x44, 0x00},  "Hynix NAND 2G 3.3V 8-bit",  256, 128, 2048, 64,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10,  A_08BIT},
    // [1GB] HY27UT088G2A
    {{0xAD, 0xD3, 0x14, 0xA5, 0x34, 0x00},  "Hynix NAND 1G 3.3V 8-bit",  1024, 256, 2048, 64,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10,  A_08BIT},
    // [4GB] H27UBG8T2ATR
	{{0xAD, 0xD7, 0x94, 0x9A, 0x74, 0x42},  "Hynix NAND 4G 3.3V 8-bit",  4096 - 2048, 2048, 8192, 448,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10,  A_08BIT}
};

#define MAX_SUPPORTED_MICRON_NAND 1
const NAND_FEATURES MICRON_NAND_DevInfo[] = {
 	//*================================================================================================================================================================
    //*[             ID            ][ Name ][                     Size                      ][   Address Cycle  ][           Timing Sequence             ][ Attribute ]
    //*----------------------------------------------------------------------------------------------------------------------------------------------------------------
    //* 1st, 2nd, 3rd, 4th, 5th, 6th, name, ChipSize(MB), BlockSize(KB), PageSize, SpareSize, ColCycle, RowCycle, tCLS, tALS, tWC, tWP, tWH, tRC, tRP, tREH 
    //*================================================================================================================================================================
  	// [4GB] 29F32G08QAA 
	{{0x2C, 0xD5, 0x94, 0x3E, 0x74, 0x00},  "Micron NAND 4G 3.3V 8-bit",  4096, 512, 4096, 218,  2, 3,  10, 10, 20, 10, 7, 25, 12, 10, A_08BIT}
};

#define MAX_SUPPORTED_TOSHIBA_NAND 3
const NAND_FEATURES TOSHIBA_NAND_DevInfo[] = {
 	//*================================================================================================================================================================
    //*[             ID            ][ Name ][                     Size                      ][   Address Cycle  ][           Timing Sequence             ][ Attribute ]
    //*----------------------------------------------------------------------------------------------------------------------------------------------------------------
    //* 1st, 2nd, 3rd, 4th, 5th, 6th, name, ChipSize(MB), BlockSize(KB), PageSize, SpareSize, ColCycle, RowCycle, tCLS, tALS, tWC, tWP, tWH, tRC, tRP, tREH 
    //*================================================================================================================================================================
  	// [2GB] TC58NVG4D2FTA00
	{{0x98, 0xD5, 0x94, 0x32, 0x76, 0x55},  "Toshiba NAND 2G 3.3V 8-bit",  2048, 1024, 8192, 448,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10, A_08BIT},
	// [4GB] TC58NVG5D2FTA00  [8GB] TH58NVG6D2FTA20
	{{0x98, 0xD7, 0x94, 0x32, 0x76, 0x55},  "Toshiba NAND 4G 3.3V 8-bit",  4096, 1024, 8192, 448,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10, A_08BIT},
    // [128MB] TC58NVG0S3ETA00 
    {{0x98, 0xD1, 0x90, 0x15, 0x76, 0x14},  "Toshiba NAND 128MB 3.3V 8-bit",  128, 128, 2048, 64,  2, 3,  12, 12, 25, 12, 10, 25, 12, 10, A_08BIT} 
};

#define MAX_SUPPORTED_STMICRO_NAND 2
const NAND_FEATURES STMICRO_NAND_DevInfo[] = {
 	//*================================================================================================================================================================
    //*[             ID            ][ Name ][                     Size                      ][   Address Cycle  ][           Timing Sequence             ][ Attribute ]
    //*----------------------------------------------------------------------------------------------------------------------------------------------------------------
    //* 1st, 2nd, 3rd, 4th, 5th, 6th, name, ChipSize(MB), BlockSize(KB), PageSize, SpareSize, ColCycle, RowCycle, tCLS, tALS, tWC, tWP, tWH, tRC, tRP, tREH 
    //*================================================================================================================================================================
   	// [512MB] NAND04GW3B2D
    {{0x20, 0xDC, 0x10, 0x95, 0x54, 0x00},  "Stmicro NAND 512M 3.3V 8-bit", 512, 128, 2048, 64,  2, 3,  15, 15, 25, 15, 10, 25, 15, 10,  A_08BIT},
	// [1GB] NAND08GW3B2CN6
    {{0x20, 0xD3, 0x51, 0x95, 0x58, 0x00},  "Stmicro NAND 1G 3.3V 8-bit", 1024, 128, 2048, 64,  2, 3,  15, 15, 25, 15, 10, 25, 15, 10,  A_08BIT},
};
#define MAX_SUPPORTED_ESMT_NAND 1 
const NAND_FEATURES ESMT_NAND_DevInfo[] = { 
    //*================================================================================================================================================================
    //*[             ID            ][ Name ][                     Size                      ][   Address Cycle  ][           Timing Sequence             ][ Attribute ]
    //*----------------------------------------------------------------------------------------------------------------------------------------------------------------
    //* 1st, 2nd, 3rd, 4th, 5th, 6th, name, ChipSize(MB), BlockSize(KB), PageSize, SpareSize, ColCycle, RowCycle, tCLS, tALS, tWC, tWP, tWH, tRC, tRP, tREH 
    //*================================================================================================================================================================
	// [1GB] F59L1G81A
    {{0x92, 0xf1, 0x80, 0x95, 0x40, 0x00},  "ESMT NAND 128M 3.3V 8-bit", 128, 128, 2048, 64,  2, 2,  12, 12, 25, 12, 10, 25, 12, 10,  A_08BIT},
};

const DEV_INFO DevSupported = {
	/* nand maker code */
	{NAND_MFR_SAMSUNG,
	NAND_MFR_HYNIX,
	NAND_MFR_MICRON,
	NAND_MFR_TOSHIBA,
	NAND_MFR_STMICRO,
    NAND_MFR_ESMT},
	
	/* max supported nand flash of each maker */
	{MAX_SUPPORTED_SAMSUNG_NAND,
	MAX_SUPPORTED_HYNIX_NAND,
	MAX_SUPPORTED_MICRON_NAND,
	MAX_SUPPORTED_TOSHIBA_NAND,
	MAX_SUPPORTED_STMICRO_NAND,
    MAX_SUPPORTED_ESMT_NAND},

	/* pointer of nand flash information */
	{(NAND_FEATURES *)SAMSUNG_NAND_DevInfo,
	(NAND_FEATURES *)HYNIX_NAND_DevInfo,
	(NAND_FEATURES *)MICRON_NAND_DevInfo,
	(NAND_FEATURES *)TOSHIBA_NAND_DevInfo,
	(NAND_FEATURES *)STMICRO_NAND_DevInfo,
	(NAND_FEATURES *)ESMT_NAND_DevInfo}
};

static uint8_t sep_nand_read_byte(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	return readb(chip->IO_ADDR_R);
}

static uint16_t sep_nand_read_word(struct mtd_info *mtd)
{
	struct nand_chip *chip = mtd->priv;
	return readw(chip->IO_ADDR_R);
}

static void sep_nand_select_chip(struct mtd_info *mtd, int chipnr)
{
	uint32_t old, new;
	
	if (chipnr == -1) 
		return;

	if (unlikely(gNfcInfo.cs != chipnr)) {
		/* record the selected chip */
		gNfcInfo.cs = chipnr;
	
		old = readl(NAND_CFG_V);
		new = (old & (~(0x3 << 25))) | (gNfcInfo.cs << 25);
		writel(new, NAND_CFG_V);
	}	
}

static int sep_nand_dev_ready(struct mtd_info *mtd)
{	
	return (readl(NAND_STATUS_V) & (1 << gNfcInfo.cs));
}

static void sep_nand_cmdfunc(struct mtd_info *mtd, unsigned int command, int column, int page_addr)
{
	int i;

	/* Emulate NAND_CMD_READOOB */
	if (command == NAND_CMD_READOOB) {
		column += mtd->writesize;
		command = NAND_CMD_READ0;
	}

	/* write command */
	writeb(command, NAND_CMD_V);

	if (likely(command != NAND_CMD_READID)) {
		/* write column address */
		if (column != -1) {
			if (gNfcInfo.bus_type != SINGLE_CHAN_8BITS)
				column >>= 1;
			for (i = 0; i < gNfcInfo.nand->ColCycle; i++) {
				writeb(column, NAND_ADDR_V);
				column >>= 8;
			}
		}	
		/* write row address */
		if (page_addr != -1) {			
			for (i = 0; i < gNfcInfo.nand->RowCycle; i++) {
				writeb(page_addr, NAND_ADDR_V);
				page_addr >>= 8;
			}
		}
	}

	switch (command) {
		/* Block Erase */
		case NAND_CMD_ERASE1:
		case NAND_CMD_ERASE2:
			return;
	
		/* Page Program */
		case NAND_CMD_SEQIN:
		case NAND_CMD_PAGEPROG:
			return;

		/* Read */
		case NAND_CMD_READ0:
			writeb(NAND_CMD_READSTART, NAND_CMD_V);
			if (wait_for_completion_timeout(&gNfcInfo.nand_ready_request, 10) == 0)
		 		printk("Read timeout!\n");
			return;
	
		/* Reset */
		case NAND_CMD_RESET:
			if (wait_for_completion_timeout(&gNfcInfo.nand_ready_request, 50) == 0)
				printk("Reset timeout!\n");
			return;

		/* Read ID, wait for tWHR */
		case NAND_CMD_READID:
			writeb(0x00, NAND_ADDR_V);
			ndelay(100);
			return;

		/* Read Status, wait for tWHR */
		case NAND_CMD_STATUS:
			ndelay(100);
			return;

		default:
			printk("sep mtd doesn't supported this command.\n");
			BUG();
	}
} 

static int sep_nand_waitfunc(struct mtd_info *mtd, struct nand_chip *this)
{	
	int status;	
	uint16_t timeout;
	uint16_t timeo;

	if (this->state == FL_ERASING)
		timeout = 400;
	else
		timeout = 20;

	timeo = wait_for_completion_timeout(&gNfcInfo.nand_ready_request, timeout);
	if (!timeo)
		printk("%s timeout!\n", this->state == FL_ERASING? "Erase" : "Write");

	this->cmdfunc(mtd, NAND_CMD_STATUS, -1, -1);

	status = readl(NAND_SDATA_V);
	
	if (gNfcInfo.bus_type == DOUBLE_CHANS_16BITS) {  
		if (status & 0x0100)
		 	status |= 0x01;
		status &= ~0xff00;
	}
	
	if (status & 0x01) {
		if (this->state == FL_ERASING)
			printk("erase fail!\n");
		else
			printk("program fail!\n");
	}
		
	return status;	
}

static inline void dma_write(dma_addr_t doubleDma_busAddr, uint32_t size)
{
	/* program SARx and DARx*/
	writel(doubleDma_busAddr, DMAC1_SAR_CH(DMA_NAND_RDWR_CH));
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

static inline void dma_read(dma_addr_t doubleDma_busAddr, uint32_t size)
{
	/* program SARx and DARx */
	writel(NAND_BASE + 0x100, DMAC1_SAR_CH(DMA_NAND_RDWR_CH));
	writel(doubleDma_busAddr, DMAC1_DAR_CH(DMA_NAND_RDWR_CH));

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

static inline void random_data_input(uint16_t offset)
{
	if (gNfcInfo.bus_type != SINGLE_CHAN_8BITS)
		offset >>= 1;
	
	writeb(NAND_CMD_RNDIN, NAND_CMD_V); 
	writeb(offset, NAND_ADDR_V);
	writeb(offset >> 8, NAND_ADDR_V);	 
}

static inline void random_data_output(uint16_t offset)
{
	if (gNfcInfo.bus_type != SINGLE_CHAN_8BITS)
		offset >>= 1;
	
	writeb(NAND_CMD_RNDOUT, NAND_CMD_V);
	writeb(offset, NAND_ADDR_V);
	writeb(offset >> 8, NAND_ADDR_V);
	writeb(NAND_CMD_RNDOUTSTART, NAND_CMD_V);	 
}

static inline void write_fs_info_per_step(struct mtd_info *mtd, int step)
{
	struct nand_chip *this = mtd->priv;
	int offset;
	int i;

	random_data_input(mtd->writesize + (step << 2));

	if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
		offset = step << 2;
		for (i = 0; i < 4; i++)
			writeb(this->oob_poi[offset + i], NAND_SDATA_V);
	} else {
		uint16_t *oobbuf = (uint16_t *)this->oob_poi;

		offset = step << 1;
		for (i = 0; i < 2; i++)
			writew(oobbuf[offset + i], NAND_SDATA_V);
	}
}

static inline void read_fs_info_per_step(struct mtd_info *mtd, int step)
{
	struct nand_chip *this = mtd->priv;
	int offset;
	int i;

	random_data_output(mtd->writesize + (step << 2));

	if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
		offset = step << 2;
		for (i = 0; i < 4; i++)
			this->oob_poi[offset + i] = readb(NAND_SDATA_V);
	} else {
		uint16_t *oobbuf = (uint16_t *)this->oob_poi;

		offset = step << 1;
		for (i = 0; i < 2; i++)
			oobbuf[offset + i] = readw(NAND_SDATA_V);
	}
}

static inline void write_ecc_code_per_step(struct mtd_info *mtd, int step)
{
	struct nand_chip *this = mtd->priv;
	uint32_t *eccpos = this->ecc.layout->eccpos;	
	int i, ecc_code;

	switch (gNfcInfo.ecc_mode) {  
		case ECC_MODE_16: {	
			random_data_input(mtd->writesize + eccpos[0] + step * 28);
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS)	{
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
			random_data_input(mtd->writesize + eccpos[0] + step * 42);
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
			printk("sep mtd doesn't supported this ecc mode.\n");
			BUG();
		}	
	}
}

static inline void read_ecc_code_per_step(struct mtd_info *mtd, int step)
{
	struct nand_chip *this = mtd->priv;
	uint32_t *eccpos = this->ecc.layout->eccpos;	
	int i;

	switch (gNfcInfo.ecc_mode) {
		case ECC_MODE_16: {
			random_data_output(mtd->writesize + eccpos[0] + step * 28);
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
				for (i = 0; i < 28; i++) 
					readb(NAND_SDATA_V);	
			} else {
				for (i = 0; i < 14; i++) 
					readw(NAND_SDATA_V);	
			}
			break;
		}

		case ECC_MODE_24: {
			random_data_output(mtd->writesize + eccpos[0] + step * 42);
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
				for (i = 0; i < 42; i++) 
					readb(NAND_SDATA_V);	
			} else {
				for (i = 0; i < 21; i++) 
					readw(NAND_SDATA_V);	 
			}
			break;
		}
		
		default: {
			printk("sep mtd doesn't supported this ecc mode.\n");
			BUG();
		}			
	}
}

static inline void ecc_correct_by_step(struct mtd_info *mtd, int step)
{
	struct nand_chip *this = mtd->priv;
	uint16_t *oobbuf = (uint16_t *)this->oob_poi;
	uint16_t *databuf = (uint16_t *)gNfcInfo.doubleDma_virAddr;
	uint32_t error_sum, error_reg, error_word, error_bit;
	int i;

	if (!step) {
		gNfcInfo.ecc_failed = 0;
		gNfcInfo.ecc_corrected = 0;
	}

	if (readl(NAND_STATUS_V) & (1 << 10)) {				
		printk("[mtd], The number of error bits exceeds correction range!\n");
		gNfcInfo.ecc_failed = 1;
	} else {
  		error_sum = (readl(NAND_STATUS_V) >> 12) & 0x1f;
		if (error_sum) {					
			for (i = 0; i < error_sum; i++) {
				error_reg = readl(NAND_ERR_ADDR0_V + (i << 2));
				error_word = error_reg & 0x3ff; 
				error_bit = (error_reg >> 10) & 0xffff;
				if (error_word < 512) {					
					databuf[error_word] ^= error_bit;  
					gNfcInfo.ecc_corrected++;
				} else if (512 <= error_word && error_word < 514) {
					oobbuf[(step << 1) + (error_word - 512)] ^= error_bit;
					gNfcInfo.ecc_corrected++;
				}					
			}		
		}
	}
}

static void sep_nand_write_buf(struct mtd_info *mtd, const u_char *buf, int len)
{
	if (len == mtd->writesize) {
		int step;
		
		for (step = 0; step < gNfcInfo.eccSteps; step++) {
			/* open ecc module */
			writel(readl(NAND_CFG_V) & (~(1 << 29)), NAND_CFG_V);
			/* init ecc module */				
			writel(0x1, NAND_INIT_V);

			/* write 1024 bytes data */
			if (step & 0x1) {	/* odd */
				gNfcInfo.doubleDma_virAddr = gNfcInfo.doubleDma_virAddr_1;
				gNfcInfo.doubleDma_busAddr = gNfcInfo.doubleDma_busAddr_1;
			} else { /* even */	
				gNfcInfo.doubleDma_virAddr = gNfcInfo.doubleDma_virAddr_0;  
				gNfcInfo.doubleDma_busAddr = gNfcInfo.doubleDma_busAddr_0;
			}
			
			if (likely(step))
				random_data_input(step << 10);
			else 	/* first step */
				memcpy(gNfcInfo.doubleDma_virAddr, buf, 1024);
			
			dma_write(gNfcInfo.doubleDma_busAddr, 1024);

			if (step != gNfcInfo.eccSteps -1) {  
				if (step & 0x1) /* odd */
					memcpy(gNfcInfo.doubleDma_virAddr_0, buf + 1024, 1024);
				else /* even */	
					memcpy(gNfcInfo.doubleDma_virAddr_1, buf + 1024, 1024);
			}
				
			/* wait for dma transfer completion */
			while (!(readl(NAND_STATUS_V) & (1 << 9)));
			writel(readl(NAND_STATUS_V) | (1 << 9), NAND_STATUS_V);

			/* write 4 bytes file system information */
			write_fs_info_per_step(mtd, step);	
 
			/* close ecc module and write ecc code */
			writel(readl(NAND_CFG_V) | (1 << 29), NAND_CFG_V);	
			/* write ecc code */		
			write_ecc_code_per_step(mtd, step);	

			/* pointer to next 1024 bytes data start address */
			buf += 1024;	
		}
	} else {	/* len == mtd->oobsize */
		int i, size;

		if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
			for (i = 0; i < mtd->oobsize; i++)
				writeb(buf[i],NAND_SDATA_V);
		} else {
			uint16_t *databuf = (uint16_t *)buf;
			size = mtd->oobsize >> 1;

			for (i = 0; i < size; i++)
				writew(databuf[i], NAND_SDATA_V);
		}
	}
}

static void sep_nand_read_buf(struct mtd_info *mtd, u_char *buf, int len)
{
	if (len == mtd->writesize) {
		int step;
		
		for (step = 0; step < gNfcInfo.eccSteps; step++) {
			/* open ecc module */
			writel(readl(NAND_CFG_V) & (~(1 << 29)), NAND_CFG_V);
			/* init ecc module */				
			writel(0x1, NAND_INIT_V);

			/* read 1024 bytes data */
			if (step & 0x1) {	/* odd */
				gNfcInfo.doubleDma_virAddr = gNfcInfo.doubleDma_virAddr_1;
				gNfcInfo.doubleDma_busAddr = gNfcInfo.doubleDma_busAddr_1;
			} else { /* even */	
				gNfcInfo.doubleDma_virAddr = gNfcInfo.doubleDma_virAddr_0;  
				gNfcInfo.doubleDma_busAddr = gNfcInfo.doubleDma_busAddr_0;
			}
			
			if (likely(step))
				random_data_output(step << 10);
		
			dma_read(gNfcInfo.doubleDma_busAddr, 1024);

			/* copy data form dma buffer to destination buf */ 
			if (likely(step)) {
				if (step & 0x1) /* odd */
					memcpy(buf - 1024, gNfcInfo.doubleDma_virAddr_0, 1024);
				else /* even */
					memcpy(buf - 1024, gNfcInfo.doubleDma_virAddr_1, 1024);
			}
			
			/* wait for dma transfer completion */
			while (!(readl(NAND_STATUS_V) & (1 << 9)));
			writel(readl(NAND_STATUS_V) | (1 << 9), NAND_STATUS_V);

			/* read 4 bytes file system information */
			read_fs_info_per_step(mtd, step);			

			/* read ecc code */
			read_ecc_code_per_step(mtd, step);
			
			/* wait for ecc decoder completion */
			while (!(readl(NAND_STATUS_V) & (1 << 8)));
			writel(readl(NAND_STATUS_V) | (1 << 8), NAND_STATUS_V);
			/* close ecc module */
			writel(readl(NAND_CFG_V) | (1 << 29), NAND_CFG_V);	

			/* correct data */
			ecc_correct_by_step(mtd, step);
		
			/* copy the last ecc step data form dma buffer to destination buffer */ 
			if (unlikely(step == gNfcInfo.eccSteps -1))
				memcpy(buf, gNfcInfo.doubleDma_virAddr, 1024);
	
			/* pointer to next 1024 bytes data start address */
			buf += 1024;
		}				
	} else {	/* len == mtd->oobsize */
		int i, size;

		if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
			for (i = 0; i < mtd->oobsize; i++)
				buf[i] = readb(NAND_SDATA_V);
		} else {
			uint16_t *databuf = (uint16_t *)buf;
			size = mtd->oobsize >> 1;

			for (i = 0; i < size; i++)
				databuf[i] = readw(NAND_SDATA_V);
		}
	}	
}

static void sep_nand_write_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip, const uint8_t *buf)
{
	chip->write_buf(mtd, buf, mtd->writesize);
}

static int sep_nand_read_page_hwecc(struct mtd_info *mtd, struct nand_chip *chip, uint8_t *buf, int page)
{	
	chip->read_buf(mtd, buf, mtd->writesize);

	if (gNfcInfo.ecc_failed)
		mtd->ecc_stats.failed++;
	else {
	/* 
	 * why driver force gNfcInfo.ecc_corrected equal zero? because if gNfcInfo.ecc_corrected too large( >= 2), file 
	 * system deals with the block a bad. 
	 */
		gNfcInfo.ecc_corrected = 0;
		mtd->ecc_stats.corrected += gNfcInfo.ecc_corrected;
	}

	return 0;
}

irqreturn_t nfc_irq_handler(int irq, void *dev_id)
{	
	int status;
	
	status = readl(NAND_STATUS_V);
	
	if (status & (1 <<  (4 + gNfcInfo.cs))) {
		writel(status | (1 << (4 + gNfcInfo.cs)), NAND_STATUS_V);	
		complete(&gNfcInfo.nand_ready_request);
	}

	return IRQ_HANDLED;	
}

static void set_nand_ecclayout(void)
{
	int i;
	int base = gNfcInfo.eccSteps << 2;

	switch (gNfcInfo.ecc_mode) {
		case ECC_MODE_16: {
			gNfcInfo.nand_ooblayout.eccbytes = gNfcInfo.eccSteps * 28;
			for (i = 0; i < gNfcInfo.nand_ooblayout.eccbytes; i++)
				gNfcInfo.nand_ooblayout.eccpos[i] = base + i;
				
			gNfcInfo.nand_ooblayout.oobfree[0].offset = 2;
			gNfcInfo.nand_ooblayout.oobfree[0].length = base - 2;
			break;
		}
			
		case ECC_MODE_24: {
			gNfcInfo.nand_ooblayout.eccbytes = gNfcInfo.eccSteps * 42;
			for (i = 0; i < gNfcInfo.nand_ooblayout.eccbytes; i++)
				gNfcInfo.nand_ooblayout.eccpos[i] = base + i;
				
			gNfcInfo.nand_ooblayout.oobfree[0].offset = 2;
			gNfcInfo.nand_ooblayout.oobfree[0].length = base - 2;
			break;
		}
			
		default: {
			printk("sep mtd doesn't supported this ecc mode.\n");
			BUG();
		}		
	}
}

static void calculate_ecc_mode(void)
{
	int avail_size;
	
	printk("ChipSize=%dMBytes, BlockSize=%dKBytes, PageSize=%dBytes, SpareSize=%dBytes\n",	\
		gNfcInfo.nand->ChipSize, gNfcInfo.nand->BlockSize, gNfcInfo.nand->PageSize, gNfcInfo.nand->SpareSize);
		
	/* calculate available oob size per 1024 bytes */
	gNfcInfo.eccSteps = gNfcInfo.nand->PageSize >> 10;
	avail_size = gNfcInfo.nand->SpareSize / gNfcInfo.eccSteps;

	/* according to ftl, mtd also doen't support ECC_MODE_30 */
	if (avail_size >= 46)
		gNfcInfo.ecc_mode = ECC_MODE_24;
	else if (avail_size >= 32)
		gNfcInfo.ecc_mode = ECC_MODE_16;
	else
		BUG();

	if (gNfcInfo.bus_type == DOUBLE_CHANS_16BITS)
		gNfcInfo.eccSteps <<= 1;

	printk("ecc_steps=%d, ecc_mode=%d\n", gNfcInfo.eccSteps, gNfcInfo.ecc_mode);
}

static void calculate_timing_sequence(void)
{
	int hclk, tacls, twr, twrph0, twrph1;
	struct clk *nfc_clk = NULL;

	nfc_clk = clk_get(NULL, "nfc");

	if (nfc_clk == NULL) {
		printk("fail to get nfc clk, use 320MHz\n");
		hclk = 1000000000 / 320000000; 
	} else 
		hclk = 1000000000 / clk_get_rate(nfc_clk);	/* ns */	
	
	printk("HCLK=%d, tCLS=%d, tALS=%d, tWC=%d, tWP=%d, tWH=%d, tRC=%d, tRP=%d, tREH=%d\n",	\
		hclk, gNfcInfo.nand->tCLS, gNfcInfo.nand->tALS, gNfcInfo.nand->tWC, gNfcInfo.nand->tWP, gNfcInfo.nand->tWH, gNfcInfo.nand->tRC, gNfcInfo.nand->tRP, 
			gNfcInfo.nand->tREH);

	tacls = (gNfcInfo.nand->tCLS >= gNfcInfo.nand->tALS)? (gNfcInfo.nand->tCLS - gNfcInfo.nand->tWP) : (gNfcInfo.nand->tALS - gNfcInfo.nand->tWP);
	gNfcInfo.TACLS = (tacls + hclk - 1) / hclk;

	twr = (gNfcInfo.nand->tWC >= gNfcInfo.nand->tRC)? gNfcInfo.nand->tWC : gNfcInfo.nand->tRC;
	twrph0 = (gNfcInfo.nand->tWP >= gNfcInfo.nand->tRP)? gNfcInfo.nand->tWP : gNfcInfo.nand->tRP;
	twrph1 = (gNfcInfo.nand->tWH >= gNfcInfo.nand->tREH)? gNfcInfo.nand->tWH : gNfcInfo.nand->tREH;
	gNfcInfo.TWRPH0 = (twrph0 + hclk - 1) / hclk;	
	gNfcInfo.TWRPH1 = (twrph1 + hclk - 1) / hclk;
	
	if (gNfcInfo.TWRPH0 * hclk + gNfcInfo.TWRPH1 * hclk < twr) {
		int i = 0;

		while (1) {
			if (!(i & 0x1)) /* even */
				gNfcInfo.TWRPH0++;
			else /* odd */
				gNfcInfo.TWRPH1++;

			if (gNfcInfo.TWRPH0 * hclk + gNfcInfo.TWRPH1 * hclk >= twr)	
				break;
			
			i++;
		}		
	}

	printk("TACLS=%d, TWRPH0=%d, TWRPH1=%d\n", gNfcInfo.TACLS, gNfcInfo.TWRPH0, gNfcInfo.TWRPH1);	
}

static inline void clear_nfc_register(void)
{
	/* clear NFC_CFG nand NFC_CTRL register */
	writel(0, NAND_CFG_V);
	writel(0, NAND_CTRL_V);

	/* clear all interrupt flag */
	writel(0xffffffff, NAND_STATUS_V);	
}

static int get_nand_dev_info(void)
{
	int i, j;
	uint16_t id_code[6];	
	enum SupportedNandMaker maker_num;
	int match_count;
	bool found_nand;
	NAND_FEATURES *tmp_nf;

	clear_nfc_register();

	writel( NFC_ECC_PASS(1) | NFC_ECC_MODE(0)  | NFC_CS(0) 			\
		  | NFC_TACLS(0x3f) | NFC_TWRPH0(0x3f) | NFC_TWRPH1(0x3f) 	\
		  | NFC_PAGESZ(0)   | NFC_CHIP_NUM(0)  | NFC_FLASH_WIDTH(0),  NAND_CFG_V);	
	writel( NFC_CE_CARE(0)         | NFC_CE_REG(0)     | NFC_EN_TRANS_DONE_INT(0)	\
		  | NFC_EN_DEC_DONE_INT(0) | NFC_EN_RnB_INT(1) | NFC_RnB_TRAN_MODE(1), NAND_CTRL_V);	
		
	/* reset the chip, required by some chips (e.g. Micron MT29FxGxxxxx) after power-up */
	sep_nand_cmdfunc(NULL, NAND_CMD_RESET, -1, -1);

	/* send the command for reading device ID */
	sep_nand_cmdfunc(NULL, NAND_CMD_READID, 0x00, -1);

	/* read entire ID string */
	for (i = 0; i < 6; i++)
		id_code[i] = readb(NAND_SDATA_V);

	/* check maker code */
	maker_num = MAX_SUPPORTED_NAND_MAKER;
	for (i = 0; i < MAX_SUPPORTED_NAND_MAKER; i++) {
		if ((id_code[0] & 0xff) == DevSupported.MakerCode[i]) {
			maker_num = (enum SupportedNandMaker)i;
			tmp_nf = DevSupported.flash[i];
			break;
		}			
	}
	
	if (maker_num == MAX_SUPPORTED_NAND_MAKER) {
		printk("no device found\n");
		return -ENODEV;
	}

	/* check device code */
	found_nand = false;
	for (i = 0; i < DevSupported.NandNum[maker_num]; i++) {
		match_count = 0;

		for (j = 1; j < 6; j++) {
			if (tmp_nf->ID[j] == (id_code[j] & 0xff)) 
				match_count++;
			else if (tmp_nf->ID[j] == 0x00)
				match_count++;
			else
				break;
		}

		if (match_count == 5) {
			found_nand = true;
			break;
		}
		else
			tmp_nf++;		
	}

	if (found_nand == false) {
		printk("no device found\n");
		return -ENODEV;
	}	
	
	if (tmp_nf->attribute & A_16BIT)
		gNfcInfo.bus_type = SINGLE_CHAN_16BITS;
	else {
#ifndef CONFIG_FORCE_NAND_SINGLE_CHANNEL
		uint8_t LByte;
		uint8_t HByte;
	
		/* check if compositin of nand is double channels 16 bits */
		gNfcInfo.bus_type = DOUBLE_CHANS_16BITS;
		writel(readl(NAND_CFG_V) | NFC_CHIP_NUM(0) | NFC_FLASH_WIDTH(1), NAND_CFG_V);

		sep_nand_cmdfunc(NULL, NAND_CMD_RESET, -1, -1);
		sep_nand_cmdfunc(NULL, NAND_CMD_READID, 0x00, -1);

		for (i = 0; i < 6; i++)
			id_code[i] = readw(NAND_SDATA_V);		
		 
  		for (i = 0; i < 6; i++)	{
  			if (tmp_nf->ID[i] == 0x00)
				continue;
			LByte = id_code[i] & 0xff;
			HByte = (id_code[i] >> 8) & 0xff;
			if (tmp_nf->ID[i] != LByte) {
				printk("no device found\n");
				return -ENODEV;
			} else if (LByte != HByte) {
				gNfcInfo.bus_type = SINGLE_CHAN_8BITS;
				break;
			}
		}
#else
		gNfcInfo.bus_type = SINGLE_CHAN_8BITS;  
#endif
	}	

	printk("ID: 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x, 0x%04x\n", id_code[0], id_code[1], id_code[2], id_code[3], id_code[4], id_code[5]);
	printk("bus_type=%d\n", gNfcInfo.bus_type);
	
	gNfcInfo.nand = tmp_nf; 

	calculate_ecc_mode();

	calculate_timing_sequence();

	return 0;
}

static void nfc_init(void) 
{	
	uint8_t ps;	/* pagesize */
	uint8_t cn, bw; /* chipnum, buswidth */
	uint8_t em = gNfcInfo.ecc_mode;	/* ecc mode */
	uint8_t tacls = gNfcInfo.TACLS + 1;
	uint8_t twrph0 = gNfcInfo.TWRPH0 + 1;
	uint8_t twrph1 = gNfcInfo.TWRPH1 + 1;  
	
#ifdef SEP0611_NAND_WP_
    sep0611_gpio_cfgpin(SEP0611_NAND_WP_, SEP0611_GPIO_IO);		/* special function */
    sep0611_gpio_dirpin(SEP0611_NAND_WP_, SEP0611_GPIO_OUT);	/* output */
    sep0611_gpio_setpin(SEP0611_NAND_WP_, GPIO_HIGH);			/* high level */
#endif

	switch (gNfcInfo.nand->PageSize) {
		case 2048: ps = 0; break;
		case 4096: ps = 1; break;
		default: ps = 2;	/* PageSize = 8192 */
	}

	switch (gNfcInfo.bus_type) {
		case SINGLE_CHAN_8BITS: cn = 0; bw = 0; break;
		case DOUBLE_CHANS_16BITS: cn = 0; bw = 1; break;
		default: cn = 1; bw = 1;	/* SINGLE_CHAN_16BITS */
	}

	clear_nfc_register();

	/* configure NFC_CFG register */
	writel( NFC_ECC_PASS(1)  | NFC_ECC_MODE(em)   | NFC_CS(0)			\
		  | NFC_TACLS(tacls) | NFC_TWRPH0(twrph0) | NFC_TWRPH1(twrph1)	\
		  | NFC_PAGESZ(ps)   | NFC_CHIP_NUM(cn)   | NFC_FLASH_WIDTH(bw), NAND_CFG_V);	

	/* enable RnB rising edge trigger interrupt */
	writel( NFC_CE_CARE(0)         | NFC_CE_REG(0)     | NFC_EN_TRANS_DONE_INT(0)	\
		  | NFC_EN_DEC_DONE_INT(0) | NFC_EN_RnB_INT(1) | NFC_RnB_TRAN_MODE(1), NAND_CTRL_V);	
}

/*************add by zy**************/
#ifdef CONFIG_CPU_FREQ
struct notifier_block   sep_nand_freq_transition;
static int sep_nand_cpufreq_transition(struct notifier_block *nb,
                      unsigned long val, void *data)
{
//	printk("%s\n",__func__);

//	calculate_timing_sequence();
	writel(NFC_ECC_PASS(1) | NFC_CS(0) | NFC_TACLS(3) | NFC_TWRPH0(6) | NFC_TWRPH1(6) , NAND_CFG_V);
//	writel(NFC_ECC_PASS(1) | NFC_CS(0) | NFC_TACLS(nfc.TACLS + 1) | NFC_TWRPH0(nfc.TWRPH0 + 2) | NFC_TWRPH1(nfc.TWRPH1 + 2) , NAND_CFG_V);
//	printk("out: %s\n",__func__);

    return 0;
}

static inline int sep_nand_cpufreq_register(void)
{
    sep_nand_freq_transition.notifier_call = sep_nand_cpufreq_transition;

    return cpufreq_register_notifier(&sep_nand_freq_transition,
                     CPUFREQ_TRANSITION_NOTIFIER);
}

static inline void sep_nand_cpufreq_deregister(void)
{
    cpufreq_unregister_notifier(&sep_nand_freq_transition,
                    CPUFREQ_TRANSITION_NOTIFIER);
}

#else
static inline int sep_nand_cpufreq_register(void)
{
    return 0;
}

static inline void sep_nand_cpufreq_deregister(void)
{
}
#endif
/*************add by zy**************/

static int sep_nand_probe(struct platform_device *pdev)
{
	struct mtd_info *mtd = NULL;
	struct nand_chip *this = NULL;
	int ret;

	mtd = kzalloc(sizeof(struct mtd_info), GFP_KERNEL);
	if (mtd == NULL) {
		printk("unable to allocate mtd_info structure.\n");
		ret = -ENOMEM;
		return ret;
   	}
	this = kzalloc(sizeof(struct nand_chip), GFP_KERNEL);
	if (this == NULL) {
		printk("unable to allocate nand_chip structure.\n");
		ret = -ENOMEM;
		goto error_kzalloc_nand_chip;
	}
	/* link the private data with the MTD structure */
	mtd->priv = this;

	sep_nfc = mtd;

	platform_set_drvdata(pdev, mtd);

	/* request IRQ */
	ret = request_irq(INTSRC_NAND, nfc_irq_handler, IRQF_DISABLED, "nand", NULL);
	if (ret)
		goto error_request_irq;
	init_completion(&gNfcInfo.nand_ready_request);
	mutex_init(&gNfcInfo.mutex_sepnfc);

	/* request double dma buffer, the size of each buffer is 1024 */
	gNfcInfo.doubleDma_virAddr_0 = (char *)dma_alloc_coherent(NULL, 2048, (dma_addr_t *)&gNfcInfo.doubleDma_busAddr_0, GFP_KERNEL);
	if (gNfcInfo.doubleDma_virAddr_0 == NULL) {
		printk("dma_alloc_coherent fail.\n");
		ret = -ENOMEM;
		goto error_dma_alloc_coherent;
	}
	gNfcInfo.doubleDma_virAddr_1 = gNfcInfo.doubleDma_virAddr_0 + 1024;
	gNfcInfo.doubleDma_busAddr_1 = gNfcInfo.doubleDma_busAddr_0 + 1024;

	this->IO_ADDR_R = (void __iomem *)(NAND_SDATA_V);
	this->IO_ADDR_W = (void __iomem *)(NAND_SDATA_V);
	this->read_byte = sep_nand_read_byte;
	this->read_word = sep_nand_read_word;
	this->write_buf = sep_nand_write_buf;
	this->read_buf = sep_nand_read_buf;
	this->select_chip = sep_nand_select_chip;
	this->dev_ready = sep_nand_dev_ready;
	this->cmdfunc = sep_nand_cmdfunc;
	this->waitfunc = sep_nand_waitfunc;
	
	/* try to get nand info */
	if (get_nand_dev_info()) {
		ret = -ENXIO;
		goto error_get_nand_dev_info;
	}
	/* must be below get_nand_dev_info() */
	set_nand_ecclayout();
	this->ecc.mode = NAND_ECC_HW;
	this->ecc.layout = &gNfcInfo.nand_ooblayout;
	this->ecc.write_page = sep_nand_write_page_hwecc;
	this->ecc.read_page = sep_nand_read_page_hwecc;		
	
	/* inital nfc */
	nfc_init();

	/* scan to find existence of nand device */
	if (nand_scan(mtd, 4)) {
		printk("nand_scan failed\n");
		ret = -ENXIO;
		goto error_nand_scan;
	}

	/* register MTD partitions */
#ifdef CONFIG_MTD_PARTITIONS	
	if (sep0611_partitions.num != 0) {
		add_mtd_partitions(mtd, sep0611_partitions.table, sep0611_partitions.num);
	} else {
		printk("doesn't found mtd partitons, use default.\n");
		add_mtd_partitions(mtd, default_mtd_parts, ARRAY_SIZE(default_mtd_parts));
	}
#endif	
	return 0;   

 error_nand_scan:
 error_get_nand_dev_info:
	dma_free_coherent(NULL, 2048, gNfcInfo.doubleDma_virAddr_0, gNfcInfo.doubleDma_busAddr_0);
 error_dma_alloc_coherent:	
	free_irq(INTSRC_NAND, NULL);
 error_request_irq:
	kfree(this);
 error_kzalloc_nand_chip:
	kfree(mtd);
	return ret;
}

static int sep_nand_remove(struct platform_device *pdev)
{
	struct mtd_info *mtd = platform_get_drvdata(pdev);
	struct nand_chip *this = mtd->priv;

	kfree(mtd);
	kfree(this);
	free_irq(INTSRC_NAND, NULL);
	dma_free_coherent(NULL, 2048, gNfcInfo.doubleDma_virAddr_0, gNfcInfo.doubleDma_busAddr_0);

	return 0;
}

#ifdef CONFIG_PM
static int sep_nand_suspend(struct platform_device *dev, pm_message_t pm)
{
	printk("%s\n", __func__);
	mutex_lock(&gNfcInfo.mutex_sepnfc);
	mutex_unlock(&gNfcInfo.mutex_sepnfc);

	return 0;
}

static int sep_nand_resume(struct platform_device *dev)
{
	printk("%s\n", __func__);
	/* re-init nfc */
	nfc_init();
	
	/* reset the chip, required by some chips (e.g. Micron MT29FxGxxxxx) after power-up */
	sep_nand_cmdfunc(NULL, NAND_CMD_RESET, -1, -1);

	return 0;
}
#else
#define 	sep_nand_suspend	NULL
#define		sep_nand_resume		NULL
#endif

static struct platform_driver sep_nand_driver = {
	.probe		= sep_nand_probe,
	.remove		= sep_nand_remove,
	.suspend	= sep_nand_suspend,
	.resume		= sep_nand_resume,
	.driver		= {
		.name	= "sep0611_nandflash",
		.owner	= THIS_MODULE,
	},
};

static int __init sep_nand_init(void)
{
	printk("SEP0611 NAND Driver v1.1\n");
	return platform_driver_register(&sep_nand_driver);
}

static void __exit sep_nand_exit(void)
{
	platform_driver_unregister(&sep_nand_driver);
}

module_init(sep_nand_init);
module_exit(sep_nand_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("fjj <fanjianjun@wxseuic.com>");
MODULE_DESCRIPTION("Board-sepcific NAND Flash driver on SEP0611 board");
