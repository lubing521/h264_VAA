/* linux/drivers/mtd/nand/nand_test.c
 * * Copyright (c) 2009-2011 SEUIC
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
#include <linux/string.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/cdev.h>

#include <asm/sizes.h>
#include <asm/io.h>
#include <asm/uaccess.h>

#include <mach/nand.h>
#include <mach/hardware.h>


#include "ftl_global.h"
uint32_t start_blk = 0;
uint32_t count_blk = 12;

static void write_4bytes_dummy_data_per_step(void)
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

static void read_4bytes_dummy_data_per_step(void)
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

static void write_ecc_code_per_step(void)
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
			printk("sep ftl doesn't supported this ecc mode.\n");
			BUG();
		}	
	}
}

static void read_ecc_code_per_step(void)
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
			printk("sep ftl doesn't supported this ecc mode.\n");
			BUG();
		}
	}				
}

static uint8_t ecc_correct_by_step(uint8_t * databuf)
{
	uint8_t i;
	uint16_t *buf = (uint16_t *)databuf;
	uint32_t error_sum, error_reg, error_word, error_bit;

	if (readl(NAND_STATUS_V) & (1 << 10)) {			
		printk("[nand_test]exceeds correction range!\n");
		printk("magickeyfornand_test\n");
		return FLASH_TOO_MANY_ERROR;
	} else {
  		error_sum = (readl(NAND_STATUS_V) >> 12) & 0x1f;
		if(error_sum != 0)
			printk("%d error bits occured\n", error_sum);
		if(databuf == NULL)
			return 0;/*dummy read, we dont need to correct it*/
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
	//printk("[SEP_SelectCe_f], ceNo: %d\n", ceNo);	
	
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
}

static inline void FTL_Low_Driver_Out(void)
{
	mutex_unlock(&gNfcInfo.mutex_sepnfc);	
}

static inline void SEP_WriteCmd_f(uint8_t cmd)
{
	writeb(cmd, NAND_CMD_V);
}

static inline void SEP_WriteAddr_f(int32_t colAddr, int32_t rowAddr)
{
	uint8_t i;

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
	//printk("[SEP_Random_Data_Input], offset: %d\n", offset);

	SEP_WriteCmd_f(0x85);
	SEP_WriteAddr_f(offset, -1); 
}

static inline void SEP_Random_Data_output(int32_t offset)
{	
	//printk("[SEP_Random_Data_output], offset: %d\n", offset);

	SEP_WriteCmd_f(0x05);
	SEP_WriteAddr_f(offset, -1);	
	SEP_WriteCmd_f(0xE0); 
}

static inline uint8_t SEP_WaitforReady_f(uint8_t statusCMD)
{	
	uint8_t ret = 0;
	uint16_t fail_bit;
	enum FtlState state;
	uint16_t ready_bit = 0x40;

	//printk("[SEP_WaitforReady_f], statusCMD: 0x%x\n", statusCMD);	

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

	if (gNfcInfo.bus_type == DOUBLE_CHANS_16BITS) {
		fail_bit |= fail_bit << 8;
	}

	/* issue read status command */
	SEP_WriteCmd_f(statusCMD);
	ndelay(100);
	/* temporary, it is a dead-loop */
	while (!(readl(NAND_SDATA_V) & ready_bit));
	
	switch (state) {			
		case FTL_ERASING:
			if (readl(NAND_SDATA_V) & fail_bit) {
				printk("FLASH_ERASE_FAIL\n");
				//ret = FLASH_ERASE_FAIL;
			}
			break;
			
		case FTL_WRITING:
			if (readl(NAND_SDATA_V) & fail_bit) {
				printk("FLASH_PROGRAM_FAIL\n");
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
	//printk("[SEP_ResetChip], statusCMD: 0x%x\n", statusCMD);

	/* issue reset command */
	SEP_WriteCmd_f(0xFF);
	/* wait for chip ready */
	SEP_WaitforReady_f(statusCMD);
}

static void SEP_Write_Page_f(uint8_t *bufData)
{
	uint8_t step;
	int i;
	
	SEP_Random_Data_Input(0);

	for (step = 0; step < 8; step++) {		
		/* open ecc module */
		writel(readl(NAND_CFG_V) & (~(1 << 29)), NAND_CFG_V);
		/* init ecc module */				
		writel(0x1, NAND_INIT_V);
		
		/* write 1024 bytes user data */
		if(bufData == NULL)
		{
			for(i = 0; i < 1024; i++)
			{
				writeb(0xa5, NAND_SDATA_V);/*FIXME:*/
			}
		}else{
			for(i = 0; i < 1024; i++)
			{
				writeb(*(bufData + i), NAND_SDATA_V);/*FIXME:*/
			}
		}
		
		/* write 4 bytes dummy data */
		write_4bytes_dummy_data_per_step();	

		/* close ecc module */
		writel(readl(NAND_CFG_V) | (1 << 29), NAND_CFG_V);
		/* write ecc code */			
		write_ecc_code_per_step();
		
		/* pointer to next 1024 bytes user data start address */
		if(bufData != NULL)
			bufData += 1024;/*FIXME:we dont write the real data now*/
	}
}

static uint8_t SEP_Read_Page_f(uint8_t *bufData)
{
	uint8_t ret = 0;
	uint8_t step;
	int i;
	
	SEP_Random_Data_output(0);

	for (step = 0; step < 8; step++) {
		/* open ecc module */
		writel(readl(NAND_CFG_V) & (~(1 << 29)), NAND_CFG_V);
		/* init ecc module */				
		writel(0x1, NAND_INIT_V);
	
		/* read 1024 bytes user data */				
		if(bufData == NULL)
		{
			for(i = 0; i < 1024; i++)
			{
				readb(NAND_SDATA_V);
			}
		}else{
			for(i = 0; i < 1024; i++)
			{
				*(bufData + i) = readb(NAND_SDATA_V);
			}
		}
		
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
		ret = ecc_correct_by_step(bufData);
		
		/* pointer next 1024 bytes boundary */	
		if(bufData != NULL)
			bufData += 1024;	
	}
	return ret;	
}

uint8_t FLASH_Erase_f(uint8_t ceNo, uint32_t rowAddr)
{
	uint8_t ret;
	
	//printk("\n[FLASH_Erase_f], ceNo: %d, rowAddr: 0x%x\n", 
					//ceNo, rowAddr);
	
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	/* issue block erase setup command */
	SEP_WriteCmd_f(0x60);
	/* issue row address */
	SEP_WriteAddr_f(-1, rowAddr);
	/* issue block erase confirm command */
	SEP_WriteCmd_f(0xD0);	
	/* record flash state */
	gNfcInfo.state = FTL_ERASING;
	
	/* wait for tBERS */
	ret = SEP_WaitforReady_f(0x70);
	if (ret)
	{
		SEP_ResetChip(0x70);
		printk("erase failed\n");
	}

	return ret;
}

uint8_t FLASH_Write_f(uint8_t ceNo, uint32_t rowAddr, uint8_t *dataBufPtr)
{
	uint8_t ret;

	/* select the specified CE */
	SEP_SelectCe_f(ceNo);

	/* issue serial data input command */
	SEP_WriteCmd_f(0x80);	
	/* issue address */
	SEP_WriteAddr_f(0, rowAddr);
	/* involk write a physical page function */
	SEP_Write_Page_f(NULL);
	/* issue program confirm command */
	SEP_WriteCmd_f(0x10);	
	
	/* wait for tPROG */
	ret = SEP_WaitforReady_f(0x70);
	if (ret)
		SEP_ResetChip(0x70);

	return ret;
}

uint8_t FLASH_Read_f(uint8_t ceNo, uint32_t rRowAddr, uint8_t *dataBufPtr)
{
	uint8_t ret;
	
	/* select the specified CE */
	SEP_SelectCe_f(ceNo);
	
	/* issue read setup command */ 
	SEP_WriteCmd_f(0x00);
	/* issue address */
	SEP_WriteAddr_f(0, rRowAddr);
	/* issue read confirm command */
	SEP_WriteCmd_f(0x30);
	
	/* wait for tR */
	ret = SEP_WaitforReady_f(0x70);
	if (ret) {
		SEP_ResetChip(0x70);
		printk("time out occured!while waiting.\n");
		return -11;
	}	
		
	/* issue read data command after read status (must) */
	SEP_WriteCmd_f(0x00);
	
	/* read remains data */
	ret = SEP_Read_Page_f(dataBufPtr);
	if(ret == FLASH_TOO_MANY_ERROR)
		printk("Block Num:%d, pageoffset:%d\n", rRowAddr/256, rRowAddr%256);

	return ret;
}
void printInfo(void)
{
	printk("NAND Info:\n");
	printk("    Cycle:");
	printk("         Row:%d, Col:%d\n", gNfcInfo.nand->RowCycle, gNfcInfo.nand->ColCycle);
	printk("    ECCMOD:");
	switch (gNfcInfo.ecc_mode) {  
		case ECC_MODE_16: {
			printk("ECC_MODE_16\n");
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS) {
				printk("    Channel WIDTH:SINGLE_CHAN_8BITS\n");
			} else {
				printk("    Channel WIDTH:SINGLE_CHAN_16BITS\n");
			}	
			break;
		}
					
		case ECC_MODE_24: {
			printk("ECC_MODE_24\n");
			if (gNfcInfo.bus_type == SINGLE_CHAN_8BITS)	{
				printk("    Channel WIDTH:SINGLE_CHAN_8BITS\n");
			} else {
				printk("    Channel WIDTH:SINGLE_CHAN_16BITS\n");
			}	
			break;
		}
		default: {
			printk("sep ftl doesn't supported this ecc mode.\n");
		}	
	}
}

int nand_test(void)
{
	uint32_t row_start, row_count, i;
	printk("[nand_test]:begin!\n");
	if(start_blk == 0 && count_blk == 0)
	{
		printk("zero count detected! Using default\n");
		start_blk = 0;
		count_blk = 12;
	}
	row_start = start_blk*256;
	row_count = count_blk*256;

	printInfo();

	FTL_Low_Driver_Into();
	/*erase block*/
	printk("startBLK: %d, count: %d\n", start_blk, count_blk);
	printk("erasing....\n");
	for(i = start_blk; i < start_blk+count_blk; i++)
	{
		printk("    block:%d\n", i);
		FLASH_Erase_f(0, (i << 8));
	}

	printk("writing....\n");
	for(i = row_start; i < row_start+row_count; i++)
	{
		if(i%256 == 0)
			printk("    block:%d\n", i/256);
		FLASH_Write_f(0, i, NULL);/*write any data to NAND*/
	}

	printk("reading....\n");
	for(i = row_start; i <  row_start+row_count; i++)
	{
		if(i%256 == 0)
			printk("    block:%d\n", i/256);
		FLASH_Read_f(0, i, NULL);/*read any data to NAND*/
	}
	FTL_Low_Driver_Out();		
	return 0;
}

static int nand_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int nand_read(struct file *file, char __user *buf, size_t size, loff_t *loff)
{
	char *tempChar = "reading......";
	printk("[kernel]:%s", tempChar);
	printk("size:%d, offset:%d\n", size, *loff);
	copy_to_user(buf, tempChar, 8);
	nand_test();
	return 0;
}         

static int nand_write(struct file *file, const char __user *buf, size_t size, loff_t *loff)
{
	return 0;
}         

struct file_operations nand_test_fops = {
	.owner =	THIS_MODULE,
	.open =		nand_open,
	.read =		nand_read,
	.write =	nand_write,
};

static int __init nand_test_init(void)
{
	int result;
	dev_t nand_t;
	struct cdev *nand_test_dev;
	result = alloc_chrdev_region(&nand_t, 1, 1, "nand_test");
	if(result < 0)
	{
		printk("allocate char device id error!\n");
		return result;
	}
	nand_test_dev = cdev_alloc();
	nand_test_dev->ops = &nand_test_fops;
	cdev_init(nand_test_dev, &nand_test_fops);
	nand_test_dev->owner = THIS_MODULE;
	cdev_add(nand_test_dev, nand_t, 1);
	printk("this is nand1!\n");
	nand_test();
	printk("this is nand2!\n");
	return 0;
}

static void  __exit nand_test_exit(void)
{
	printk("GoodBye!\n");
}
module_init(nand_test_init);
module_exit(nand_test_exit);

module_param(start_blk, int, 0);
MODULE_PARM_DESC(start_row, "start block of nand flash");
module_param(count_blk, int, 0);
MODULE_PARM_DESC(count_row, "block count for test");
MODULE_LICENSE("GPL");
