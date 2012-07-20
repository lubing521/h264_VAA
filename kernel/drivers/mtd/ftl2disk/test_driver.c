#include <linux/kernel.h>
#include <linux/time.h>
#include <linux/sched.h>
#include "test.h"

#if DRIVER_TEST


static	UINT8 ceNo;
static	UINT32 rowAddr;
static	UINT16 vpb;
static	UINT8 vpp;
static	FTL_COPYBACK copyBack;
static	UINT32 gErrCounter;
static	UINT32 errCounter;
static	UINT16 i;
static	UINT8 *userBuf;//=(UINT8 *)gFTLSysInfo.userDataBufPtr;
static	UINT8 *spareBuf;//=(UINT8 *)g_spareData;
static	UINT16 pbAmountPerDie;

extern UINT8 MTD_Erase_f(UINT8 zoneID,UINT16 vpb);
extern UINT8 MTD_Read_f(FTL_CURRENT_OPER_PTR currentOperPtr,UINT8 *dataBufPtr);
extern UINT8 MTD_Write_f(FTL_CURRENT_OPER_PTR currentOperPtr);

#define WRITE_SIZE SECOTR_PART_SIZE
#define DATA_CNT (SECOTR_PART_SIZE >> 2)

//FAT表起始扇区：64-3327；
void main_test_FTL_time(void)
{
	unsigned int i, j, k;
	unsigned LOOP_CNT = 512;
	unsigned CONTINOUS_LBA_CNT = 32 * 80;
	unsigned int* gFTLbuf = gFTLSysInfo.userDataBufPtr;
	struct timeval tv01, tv02;
	unsigned int spend;
	unsigned long gLBA;
//	PCKC	pCKC;
//	tca_ckc_init();
//	printk_file("[main_test_FTL_time] begin write size: %u MB, cpuclk:%u, busclk:%u, iobusclk:%u \n", (CONTINOUS_LBA_CNT * LOOP_CNT) >> 11, tca_ckc_getcpu(), tca_ckc_getbus(), tca_ckc_getfbusctrl(CLKCTRL4));
//	pCKC = (PCKC)((unsigned int)&HwCLK_BASE); //0xF0400000
//	printk_file("CLKCTRL[0/2/4]:0x%x,0x%x,0x%x;PLLCFG[0-3]:0x%x,0x%x,0x%x,0x%x \n", pCKC->CLK0CTRL, pCKC->CLK2CTRL, pCKC->CLK4CTRL, pCKC->PLL0CFG, pCKC->PLL1CFG, pCKC->PLL2CFG, pCKC->PLL3CFG);
	printk_file("[main_test_FTL_time] begin write size: %u MB \n", (CONTINOUS_LBA_CNT * LOOP_CNT) >> 11);
	
//	gLBA=0xc2;
//	for(i=0;i<CONTINOUS_LBA_CNT;i++)
//	{
//		FTL_ReadSector_f(gLBA, 1);
//		memset32_f((gFTLbuf+(gLBA%FTL_PARTS_PER_VPP)*(WRITE_SIZE>>2)),gLBA,DATA_CNT);
//		FTL_WriteSector_f(gLBA);
//		gLBA++;
//	}
	
	for (k=0; k<3; k++) {
		if (k != 1) gLBA=0x17c2;
		do_gettimeofday(&tv01);
		for(j=0;j<LOOP_CNT;j++)
		{
			FTL_WriteOpen_f(gLBA, FTL_PARTS_PER_VPP - (gLBA%FTL_PARTS_PER_VPP));

			for(i=0;i<CONTINOUS_LBA_CNT;i++)
			{
				memset32_f((gFTLbuf+(gLBA%FTL_PARTS_PER_VPP)*(WRITE_SIZE>>2)),gLBA+0x10101010,DATA_CNT);
				FTL_WriteSector_f(gLBA);
				gLBA++;
			}
			gLBA += 64;
		}
		do_gettimeofday(&tv02);
		spend = (tv02.tv_sec - tv01.tv_sec) * 1000 + (tv02.tv_usec - tv01.tv_usec) / 1000;
		printk_file("[main_test_FTL_time] end write size: %uMB, spend: %ums \n", (CONTINOUS_LBA_CNT * LOOP_CNT) >> 11, spend);
		schedule_timeout(HZ*1);
	}
	
	for (k=0; k<3; k++) {
		if (k != 1) gLBA=0x17c2;
		do_gettimeofday(&tv01);
		for(j=0;j<LOOP_CNT;j++)
		{
			UINT16 sect_num = 32 - (gLBA % 32);
			FTL_ReadSector_f(gLBA, sect_num);
			gLBA += sect_num;
			for(i=1;i<(CONTINOUS_LBA_CNT>>(5+2));i++)
			{
				memset32_f(gFTLbuf,0,DATA_CNT<<5);
				FTL_ReadSector_f(gLBA, 32);
				gLBA += 32;
			}
			sect_num = ((CONTINOUS_LBA_CNT>>2) - sect_num) % 32;
			if (sect_num) {
				FTL_ReadSector_f(gLBA, sect_num);
				gLBA += sect_num;
			}
			gLBA += 64;
		}
		do_gettimeofday(&tv02);
		spend = (tv02.tv_sec - tv01.tv_sec) * 1000 + (tv02.tv_usec - tv01.tv_usec) / 1000;
		printk_file("[main_test_FTL_time] end read size: %uMB, spend: %ums \n", ((CONTINOUS_LBA_CNT>>2) * LOOP_CNT) >> 11, spend);
		schedule_timeout(HZ*1);
	}
}

void main_test_Driver_time(void)
{
	unsigned long i,j,k;
	unsigned LOOP_CNT = 8;
	unsigned CONTINOUS_CNT = 80;
	unsigned int* gFTLbuf = gFTLSysInfo.userDataBufPtr;
	FTL_CURRENT_OPER_PTR currentOper;
	unsigned int* baseAddr;
	struct timeval tv01, tv02;
	unsigned int spend;
//	PCKC	pCKC;
//	tca_ckc_init();
//	printk_file("[test_Drv_time] begin write size: %u MB, cpuclk:%u, busclk:%u, iobusclk:%u \n", (CONTINOUS_CNT * LOOP_CNT) >> 6, tca_ckc_getcpu(), tca_ckc_getbus(), tca_ckc_getfbusctrl(CLKCTRL4));
//	pCKC = (PCKC)((unsigned int)&HwCLK_BASE); //0xF0400000
//	printk_file("CLKCTRL[0/2/4]:0x%x,0x%x,0x%x;PLLCFG[0-3]:0x%x,0x%x,0x%x,0x%x \n", pCKC->CLK0CTRL, pCKC->CLK2CTRL, pCKC->CLK4CTRL, pCKC->PLL0CFG, pCKC->PLL1CFG, pCKC->PLL2CFG, pCKC->PLL3CFG);
	printk_file("[test_Drv_time] begin write size: %u MB \n", (CONTINOUS_CNT * LOOP_CNT) >> 6);

	baseAddr = gFTLbuf;
	for (i=0; i < (FTL_PARTS_PER_VPP<<7); i++) {
		*baseAddr = i+0x10101010;
		baseAddr++;
	}

	currentOper = &gFTLSysInfo.currentOper;
	currentOper->zoneID = 0;
	currentOper->wVPB = gFTLSysInfo.reservedVPBAmount+10;

	for (k=0; k<3; k++) {
		for (j = 0; j < LOOP_CNT; j++,currentOper->wVPB++)
			MTD_Erase_f(currentOper->zoneID,currentOper->wVPB);
		currentOper->wVPB += 3;
	}
	
	currentOper->wVPB = gFTLSysInfo.reservedVPBAmount+10;
	for (k=0; k<3; k++) {
		do_gettimeofday(&tv01);
		for (j = 0; j < LOOP_CNT; j++) {
			for (currentOper->wVPP = 0; currentOper->wVPP < CONTINOUS_CNT; currentOper->wVPP++) {
				currentOper->LPNStatus = 1;
				MTD_Write_f(currentOper);
			}
			currentOper->wVPB ++;
		}
		do_gettimeofday(&tv02);
		spend = (tv02.tv_sec - tv01.tv_sec) * 1000 + (tv02.tv_usec - tv01.tv_usec) / 1000;
		printk_file("[test_Drv_time] end write size: %uMB, spend: %ums \n", (CONTINOUS_CNT * LOOP_CNT) >> 6, spend);
		currentOper->wVPB += 3;
	}
	schedule_timeout(HZ*1);
	currentOper->rVPB = gFTLSysInfo.reservedVPBAmount+10;
	for (k=0; k<3; k++) {
		do_gettimeofday(&tv01);
		for (j = 0; j < LOOP_CNT; j++) {
			for (currentOper->rVPP = 0; currentOper->rVPP < CONTINOUS_CNT; currentOper->rVPP++) {
				currentOper->startOffsetInLPPN = 0;
				currentOper->endOffsetInLPPN = FTL_PARTS_PER_VPP -1;
				currentOper->LPNStatus = 0;
				//read out the data just wrote
				MTD_Read_f(currentOper,(UINT8 *)gFTLbuf);
			}
			currentOper->rVPB ++;
		}
		do_gettimeofday(&tv02);
		spend = (tv02.tv_sec - tv01.tv_sec) * 1000 + (tv02.tv_usec - tv01.tv_usec) / 1000;
		printk_file("[test_Drv_time] end read size: %uMB, spend: %ums \n", (CONTINOUS_CNT * LOOP_CNT) >> 6, spend);
		currentOper->rVPB += 3;
	}

	currentOper->wVPB = gFTLSysInfo.reservedVPBAmount+10;
	for (k=0; k<3; k++) {
		for (j = 0; j < LOOP_CNT; j++)
			MTD_Erase_f(currentOper->zoneID,currentOper->wVPB+j);
		currentOper->wVPB += 3;
	}
	
	return;
}



//erese VPB,write a VPP, read a whole VPP
void DRIVER_test_mode5_1(void)
{
	printk("\nDRIVER,DRIVER_test_mode5_1...\n");
	
	vpp = 0;

	rowAddr=vpb*(FTL_PARTS_PER_VPP/FTL_PARTS_PER_PP)*VPP_AMOUNT_PER_VPB + vpp;

	copyBack.wRowAddr[0] = rowAddr;
	copyBack.wRowAddr[1] = rowAddr + VPP_AMOUNT_PER_VPB;

	rowAddr +=(pbAmountPerDie/2)*VPP_AMOUNT_PER_VPB;
	
	copyBack.wRowAddr[2] = rowAddr;
	copyBack.wRowAddr[3] = rowAddr + VPP_AMOUNT_PER_VPB;

	copyBack.secAmountOfVPP = FTL_PARTS_PER_VPP;

//case 1:erase a VPB
	FLASH_Erase_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr);

	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		memset_f(userBuf+i*SECOTR_PART_SIZE,0x55+i,SECOTR_PART_SIZE);
		memset_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x88+i,FTL_SPARE_SIZE_PER_PART);
	}
//case 2:write a VPP
	FLASH_Write_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,userBuf,spareBuf);

	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		memset_f(userBuf+i*SECOTR_PART_SIZE,0x0,SECOTR_PART_SIZE);
		memset_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,FTL_SPARE_SIZE_PER_PART);
	}
//case 3:read whole VPP
	copyBack.startOffsetInLPPN = 0;
	copyBack.endOffsetInLPPN = FTL_PARTS_PER_VPP-1;
	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,copyBack.startOffsetInLPPN,copyBack.endOffsetInLPPN,userBuf,spareBuf,0x03);

	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area,sector no:%d, errCounter: %d...\n",i,errCounter);
		}

		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x88+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}
}

//read part of VPP
void DRIVER_test_mode5_2(void)
{
	printk("\nDRIVER,DRIVER_test_mode5_2...\n");

	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		memset_f(userBuf+i*SECOTR_PART_SIZE,0x0,SECOTR_PART_SIZE);
		memset_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,FTL_SPARE_SIZE_PER_PART);
	}

	rowAddr=vpb*(FTL_PARTS_PER_VPP/FTL_PARTS_PER_PP)*VPP_AMOUNT_PER_VPB + vpp;

	copyBack.wRowAddr[0] = rowAddr;
	copyBack.wRowAddr[1] = rowAddr + VPP_AMOUNT_PER_VPB;

	rowAddr +=(pbAmountPerDie/2)*VPP_AMOUNT_PER_VPB;
	
	copyBack.wRowAddr[2] = rowAddr;
	copyBack.wRowAddr[3] = rowAddr + VPP_AMOUNT_PER_VPB;

	copyBack.secAmountOfVPP = FTL_PARTS_PER_VPP;

//case 4:read part of VPP, in the same PP
	printk("\nDRIVER,DRIVER_test_mode5_2,read part of VPP, in the same PP...\n");
	copyBack.startOffsetInLPPN = 0;
	copyBack.endOffsetInLPPN = 0;
	
	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,copyBack.startOffsetInLPPN,copyBack.endOffsetInLPPN,userBuf,spareBuf,0x03);

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area,sector no:%d, errCounter: %d...\n",i,errCounter);
		}

		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x88+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

//case 5:read part of VPP, in the different PP,in the same chip
	printk("\nDRIVER,DRIVER_test_mode5_2,read part of VPP, in the different PP,in the same chip...\n");
	copyBack.startOffsetInLPPN = 1;
	copyBack.endOffsetInLPPN = FTL_PARTS_PER_PP*2 -2;
	
	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,copyBack.startOffsetInLPPN,copyBack.endOffsetInLPPN,userBuf,spareBuf,0x03);

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area,sector no:%d, errCounter: %d...\n",i,errCounter);
		}

		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x88+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

//case 6:read part of VPP, in the different PP,in the different chip,but only two consecutive PPs
	printk("\nDRIVER,DRIVER_test_mode5_2,read part of VPP, in the different PP,in the different chip,but only two consecutive PPs...\n");
	copyBack.startOffsetInLPPN = 9;
	copyBack.endOffsetInLPPN = FTL_PARTS_PER_PP*3 -2;
	
	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,copyBack.startOffsetInLPPN,copyBack.endOffsetInLPPN,userBuf,spareBuf,0x03);

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area,sector no:%d, errCounter: %d...\n",i,errCounter);
		}

		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x88+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

//case 7:read part of VPP, in the different PP,in the different chip,cross several PPs
	printk("\nDRIVER,DRIVER_test_mode5_2,read part of VPP, in the different PP,in the different chip,cross several PPs...\n");
	copyBack.startOffsetInLPPN = 1;
	copyBack.endOffsetInLPPN = FTL_PARTS_PER_PP*4 -2;
	
	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,copyBack.startOffsetInLPPN,copyBack.endOffsetInLPPN,userBuf,spareBuf,0x03);

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area,sector no:%d, errCounter: %d...\n",i,errCounter);
		}

		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x88+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}
}

//hard copy back
void DRIVER_test_mode5_3(void)
{
	printk("\nDRIVER,DRIVER_test_mode5_3...\n");

	rowAddr=vpb*(FTL_PARTS_PER_VPP/FTL_PARTS_PER_PP)*VPP_AMOUNT_PER_VPB + vpp;

	copyBack.wRowAddr[0] = rowAddr;
	copyBack.wRowAddr[1] = rowAddr + VPP_AMOUNT_PER_VPB;

	rowAddr +=(pbAmountPerDie/2)*VPP_AMOUNT_PER_VPB;
	
	copyBack.wRowAddr[2] = rowAddr;
	copyBack.wRowAddr[3] = rowAddr + VPP_AMOUNT_PER_VPB;

	copyBack.secAmountOfVPP = FTL_PARTS_PER_VPP;

	//prepare to copy back
	copyBack.rRowAddr[0] = copyBack.wRowAddr[0];
	copyBack.rRowAddr[1] = copyBack.wRowAddr[1];
	copyBack.rRowAddr[2] = copyBack.wRowAddr[2];
	copyBack.rRowAddr[3] = copyBack.wRowAddr[3];

	vpb = 4;
	vpp = 0;

	rowAddr=vpb*(FTL_PARTS_PER_VPP/FTL_PARTS_PER_PP)*VPP_AMOUNT_PER_VPB + vpp;

	copyBack.wRowAddr[0] = rowAddr;
	copyBack.wRowAddr[1] = rowAddr + VPP_AMOUNT_PER_VPB;

	rowAddr +=(pbAmountPerDie/2)*VPP_AMOUNT_PER_VPB;

	copyBack.wRowAddr[2] = rowAddr;
	copyBack.wRowAddr[3] = rowAddr + VPP_AMOUNT_PER_VPB;

//case 8:whole VPP copy back
	printk("\nDRIVER,DRIVER_test_mode5_3,whole VPP copy back...\n");
	FLASH_Erase_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr);

	copyBack.startOffsetInLPPN = 0;
	copyBack.endOffsetInLPPN = FTL_PARTS_PER_VPP-1;
	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		memset_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x88+i,FTL_SPARE_SIZE_PER_PART);
	}
		
	FLASH_HardCopyBack_InterleavingTwoPlane_f(ceNo,&copyBack,userBuf,spareBuf,1);

	memset_f(userBuf,0x0,SECOTR_PART_SIZE * FTL_PARTS_PER_VPP);
	memset_f(spareBuf,0x0,FTL_SPARE_SIZE_PER_PART * FTL_PARTS_PER_VPP);

	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,copyBack.startOffsetInLPPN,copyBack.endOffsetInLPPN,userBuf,spareBuf,0x03);

	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}

		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x88+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area after copy back,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

//case 9:part of VPP copy back, in the same PP
	printk("\nDRIVER,DRIVER_test_mode5_3,part of VPP copy back, in the same PP...\n");
	FLASH_Erase_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr);
	copyBack.startOffsetInLPPN = 1;
	copyBack.endOffsetInLPPN = FTL_PARTS_PER_PP -2;

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		memset_f(userBuf+i*SECOTR_PART_SIZE,0x10+i,SECOTR_PART_SIZE);
	}

	//all spare area will be updated
	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		memset_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x20+i,FTL_SPARE_SIZE_PER_PART);
	}

	FLASH_HardCopyBack_InterleavingTwoPlane_f(ceNo,&copyBack,userBuf,spareBuf,0);

	memset_f(userBuf,0x0,SECOTR_PART_SIZE * FTL_PARTS_PER_VPP);
	memset_f(spareBuf,0x0,FTL_SPARE_SIZE_PER_PART * FTL_PARTS_PER_VPP);
	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,0,FTL_PARTS_PER_VPP-1,userBuf,spareBuf,0x03);

	for(i=0;i<copyBack.startOffsetInLPPN;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x10+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	for(i=copyBack.endOffsetInLPPN + 1;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	//all spare area will be updated
	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x20+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area after copy back,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

//case 10:part of VPP copy back, in the different PP,in the same chip
	printk("\nDRIVER,DRIVER_test_mode5_3,part of VPP copy back,in the different PP,in the same chip...\n");
	FLASH_Erase_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr);
	copyBack.startOffsetInLPPN = 1;
	copyBack.endOffsetInLPPN = FTL_PARTS_PER_PP*2 -2;

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		memset_f(userBuf+i*SECOTR_PART_SIZE,0x10+i,SECOTR_PART_SIZE);
	}
	//all spare area will be updated
	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		memset_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x21+i,FTL_SPARE_SIZE_PER_PART);
	}

	FLASH_HardCopyBack_InterleavingTwoPlane_f(ceNo,&copyBack,userBuf,spareBuf,0);

	memset_f(userBuf,0x0,SECOTR_PART_SIZE * FTL_PARTS_PER_VPP);
	memset_f(spareBuf,0x0,FTL_SPARE_SIZE_PER_PART * FTL_PARTS_PER_VPP);
	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,0,FTL_PARTS_PER_VPP-1,userBuf,spareBuf,0x03);

	for(i=0;i<copyBack.startOffsetInLPPN;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x10+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	for(i=copyBack.endOffsetInLPPN + 1;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	//all spare area will be updated
	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x21+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area after copy back,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}
//case 11:part of VPP copy back, in the different chip,but only two consecutive PPs
	printk("\nDRIVER,DRIVER_test_mode5_3,part of VPP copy back,in the different chip,but only two consecutive PPs...\n");
	FLASH_Erase_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr);
	copyBack.startOffsetInLPPN = 9;
	copyBack.endOffsetInLPPN = FTL_PARTS_PER_PP*3 -2;

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		memset_f(userBuf+i*SECOTR_PART_SIZE,0x10+i,SECOTR_PART_SIZE);
	}
	//all spare area will be updated
	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		memset_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x22+i,FTL_SPARE_SIZE_PER_PART);
	}

	FLASH_HardCopyBack_InterleavingTwoPlane_f(ceNo,&copyBack,userBuf,spareBuf,0);

	memset_f(userBuf,0x0,SECOTR_PART_SIZE * FTL_PARTS_PER_VPP);
	memset_f(spareBuf,0x0,FTL_SPARE_SIZE_PER_PART * FTL_PARTS_PER_VPP);
	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,0,FTL_PARTS_PER_VPP-1,userBuf,spareBuf,0x03);

	for(i=0;i<copyBack.startOffsetInLPPN;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x10+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	for(i=copyBack.endOffsetInLPPN + 1;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}
	//all spare area will be updated
	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x22+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area after copy back,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

//case 12:part of VPP copy back, in the different PP,in the different chip,cross several PPs
	printk("\nDRIVER,DRIVER_test_mode5_3,part of VPP copy back,in the different PP,in the different chip,cross several PPs...\n");
	FLASH_Erase_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr);
	copyBack.startOffsetInLPPN = 1;
	copyBack.endOffsetInLPPN = FTL_PARTS_PER_PP*4 -10;

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		memset_f(userBuf+i*SECOTR_PART_SIZE,0x10+i,SECOTR_PART_SIZE);
	}
	//all spare area will be updated
	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		memset_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x23+i,FTL_SPARE_SIZE_PER_PART);
	}

	FLASH_HardCopyBack_InterleavingTwoPlane_f(ceNo,&copyBack,userBuf,spareBuf,0);

	memset_f(userBuf,0x0,SECOTR_PART_SIZE * FTL_PARTS_PER_VPP);
	memset_f(spareBuf,0x0,FTL_SPARE_SIZE_PER_PART * FTL_PARTS_PER_VPP);
	FLASH_Read_InterleavingTwoPlane_f(ceNo,copyBack.wRowAddr,0,FTL_PARTS_PER_VPP-1,userBuf,spareBuf,0x03);

	for(i=0;i<copyBack.startOffsetInLPPN;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	for(i=copyBack.startOffsetInLPPN;i<copyBack.endOffsetInLPPN + 1;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x10+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	for(i=copyBack.endOffsetInLPPN + 1;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(userBuf+i*SECOTR_PART_SIZE,0x0,0x55+i,SECOTR_PART_SIZE);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in data area after copy back,sector no:%d, errCounter: %d...\n",i,errCounter);
		}
	}

	//all spare area will be updated
	for(i=0;i<FTL_PARTS_PER_VPP;i++)
	{
		errCounter = comp_f(spareBuf+i*FTL_SPARE_SIZE_PER_PART,0x0,0x23+i,FTL_SPARE_SIZE_PER_PART);
		if(errCounter)
		{
			gErrCounter += errCounter;
			printk("\n******Mode 5,difference found in spare area after copy back,secotr no:%d, errCounter: %d...\n",i,errCounter);
		}
	}
}

void DRIVER_test_mode5(void)
{
	printk("\nDRIVER,DRIVER_test_mode5...\n");

	ceNo = 0;
	vpb = 2;
	vpp = 0;
	
	pbAmountPerDie = 1 << FLASH_PB_PER_DIE;
	
	gErrCounter =0;
	errCounter = 0;
	
	userBuf=(UINT8 *)gFTLSysInfo.userDataBufPtr;
	spareBuf=(UINT8 *)g_spareData;
#if 1
	DRIVER_test_mode5_1();


	DRIVER_test_mode5_2();
#endif

	printk("I am here.\n");

	DRIVER_test_mode5_3();
}

void DRIVER_test(void)
{
	FTL_Malloc(0,0,0);
	DRIVER_test_mode5();
}
#endif
