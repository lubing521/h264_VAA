/*************************************************************************************\
*  苏州汉迪信息科技有限公司，机密
*
*  版权所有(C)2009 苏州汉迪信息科技有限公司，保留所有权利
*
*
*
*
*  这个软件属于并由苏州汉迪信息科技有限公司出版
*
*  本软件可以在被许可的情况下被复制部分或全部，
*
*  限制是此版权声明必须永远包含在本软件中，
*
*  不管使用部分或全部代码
*
***************************************************************************************/

/**************************************************************************************

  Name:ftl_format.c

  Description: a sample low format function to format FLASH for FTL.

  Development/Test Environment:



  Import:

  Export

______________________________________________________________________________________

  Version      Date         Engineer               Description
  -------   ----------  -----------------  -------------------------------------------
  0.1	    2010-01-10	James			Initial creation.

***************************************************************************************/

#include <linux/kernel.h>
#include "ftl_global.h"
#include "ftl_util.h"

#ifndef DEBUG_FTL_FORMAT
	#define LOG_OUT(fmt...)
#endif

//if need to do flash low format.0--don't need
#define FTL_LOW_FORMAT	1
#define	DO_BAD_BLOCK_CHECKING

extern UINT8 FTL_Malloc(UINT8 zoneAmount,UINT16 vpbPerZone,UINT8 vppPerVPB);
extern UINT8 MTD_Erase_f(UINT8 zoneID,UINT16 vpb);
extern UINT8 MTD_Read_f(FTL_CURRENT_OPER_PTR currentOperPtr,UINT8 *dataBufPtr);
extern UINT8 MTD_Write_f(FTL_CURRENT_OPER_PTR currentOperPtr);

/**************************************************************************************
Description:low level format on NAND FLASH.
Inputs:
Output:
	0--successful
	others--failure.
**************************************************************************************/
UINT8 FTL_LowFormat_f(void)
{
#if FTL_LOW_FORMAT
	UINT16 vpbPerZone=1;//VPB amount of zone
	UINT8 secPerVPP=1;
	UINT8 i=0;
	UINT16 j=0,k;
	UINT8 badBlockCounter;
	UINT8 temp;
	UINT8 ret;
	UINT8 buf[512];
	UINT8 xbuf[2048];
	UINT8 badPBInZone[128];
	UINT8 *ptrCIB;
	FTL_CURRENT_OPER_PTR currentOper;

	//non-OS environment, memory allocation are static
	FTL_Malloc(0,0,0);
	ptrCIB=(UINT8 *)gFTLSysInfo.userDataBufPtr;
	currentOper = &gFTLSysInfo.currentOper;

	gFTLSysInfo.flashLayout.dieAmount = 0;
	gFTLSysInfo.flashLayout.pbPerDie = FLASH_PB_PER_DIE;
	//sector amount of a physical page
	gFTLSysInfo.flashLayout.secAmountOfPP = FLASH_SECTOR_AMOUNT_PER_PP;

	gFTLSysInfo.flashPara.flashWidth = FLASH_WIDTH;
	gFTLSysInfo.flashPara.eraseCycles = FLASH_ERASE_CYCLE;
	gFTLSysInfo.flashPara.addrCycles = FLASH_ADDR_CYCLE;	
	gFTLSysInfo.flashPara.rbWaitCounter=FLASH_RB_WAIT_COUNTER;

	//program mode setting
	gFTLSysInfo.flashLayout.progMode = 2;//two plane

	//initialize all parameters used by FTL
	gFTLSysInfo.flashLayout.secPerVPB = SHIFT_SECTOR_PER_VPB;	//2^12=4096sectors/block
	gFTLSysInfo.flashLayout.secPerVPP = SHIFT_SECTOR_PER_VPP;
	//zone amount in system.
	gFTLSysInfo.flashLayout.zoneAmount = 1;
	gFTLSysInfo.flashLayout.vpbPerZone = 9;	//512 VPB per zone
	gFTLSysInfo.reservedVPBAmount = 32;	//at least 2, don't include reserved VPBs for special VB

	gFTLSysInfo.cachedTableAmount = gFTLSysInfo.flashLayout.zoneAmount;
	//end program mode seeting

	gFTLSysInfo.cibAddr = 0xFFFF;
	gFTLSysInfo.cibSwapAddr = 0xFFFF;

	vpbPerZone <<=gFTLSysInfo.flashLayout.vpbPerZone;
	secPerVPP <<=gFTLSysInfo.flashLayout.secPerVPP;
	gFTLSysInfo.flashLayout.secAmountOfVPP = secPerVPP;

	printk("\n[FTL LOW FORMAT] program mode: 0x%x, dieAmount: 0x%x,pbPerDie: 0x%x,secPerVPB: 0x%x,secPerVPP: 0x%x,vpbPerZone: 0x%x\n",\
			gFTLSysInfo.flashLayout.progMode ,gFTLSysInfo.flashLayout.dieAmount,gFTLSysInfo.flashLayout.pbPerDie,gFTLSysInfo.flashLayout.secPerVPB,gFTLSysInfo.flashLayout.secPerVPP,gFTLSysInfo.flashLayout.vpbPerZone);

	//below block scan routin, max block number is 4096(occupy 512B)*8=32768,if more than 32768 more larger xbuf will be used.
	for(i=0;i<gFTLSysInfo.flashLayout.zoneAmount;i++)
	{
		if(i==0)//CIB and swap blocks
			badBlockCounter=2;
		else
			badBlockCounter=0;

		currentOper->zoneID = i;
		currentOper->wVPB = 0;
		currentOper->wVPP = 0;

		//memset(buf,0xFF,32);

		//byte n --> pb n*8--n*8+7
		//in a byte, pb format is:
		//|7    |6 	 |5	  |4   |3   |2   |1	  |0   |
		//|VPB7	|VPB6|VPB5|VPB4|VPB3|VPB2|VPB1|VPB0|
		for(j=0;j<(vpbPerZone>>3);j++)
		{
			buf[j]=0xFF;

			for(k=0;k<8;k++)
			{
				temp=1;
				temp <<=k;

				//erase a block
				ret = MTD_Erase_f(currentOper->zoneID,currentOper->wVPB);
#ifdef DO_BAD_BLOCK_CHECKING
				if(ret)
					goto lblBadBlock;

				//prepare data to flash buffer
				memset32_f(gFTLSysInfo.userDataBufPtr,0x5A,((SECOTR_PART_SIZE * secPerVPP)>>2));
				memset_f(gFTLSysInfo.spareDataPtr,0xA5,FTL_SPARE_SIZE_PER_PART * secPerVPP);

				currentOper->LPNStatus = 1;

				//program first page
				ret = MTD_Write_f(currentOper);
				if(ret)
					goto lblBadBlock;

				//clear buf
				memset32_f(gFTLSysInfo.userDataBufPtr,0x0,((SECOTR_PART_SIZE * secPerVPP)>>2));
				memset_f(gFTLSysInfo.spareDataPtr,0x0,FTL_SPARE_SIZE_PER_PART * secPerVPP);

				currentOper->rVPB = currentOper->wVPB;
				currentOper->rVPP = currentOper->wVPP;
				currentOper->startOffsetInLPPN = 0;
				currentOper->endOffsetInLPPN = secPerVPP -1;
				currentOper->LPNStatus = 0;
				//read out the data just wrote
				MTD_Read_f(currentOper,ptrCIB);

				//compare data
				if(comp32_f(gFTLSysInfo.userDataBufPtr,0,0x5A,((SECOTR_PART_SIZE * secPerVPP)>>2)))
				{
lblBadBlock:
					//bad block
					badBlockCounter++;

					//update status
					temp =~temp;
					buf[j] &=temp;
				}
				else
				{
#endif

					buf[j] |=temp;

					//set CIB be first valid VPB
					if(gFTLSysInfo.cibAddr == 0xFFFF)
						gFTLSysInfo.cibAddr = currentOper->wVPB;
					else if(gFTLSysInfo.cibSwapAddr == 0xFFFF)
						gFTLSysInfo.cibSwapAddr = currentOper->wVPB;
#ifdef DO_BAD_BLOCK_CHECKING
				}

				//erase a block
				MTD_Erase_f(currentOper->zoneID,currentOper->wVPB);
#endif
				currentOper->wVPB ++;
			}//for k
			if (buf[j] != 0xFF) printk("[LOW FORMAT] zoneId:%u,vpb:%u, bad block info:%u\n", i, ((unsigned int)j)<<3, buf[j]);
			xbuf[(vpbPerZone/8)*i+j]=buf[j];

		}//for j

		badPBInZone[i]=badBlockCounter;
	}//for i

	memset32_f(gFTLSysInfo.userDataBufPtr,0xFFFFFFFF,((SECOTR_PART_SIZE * secPerVPP)>>2));
	memset_f(gFTLSysInfo.spareDataPtr,0xFF,FTL_SPARE_SIZE_PER_PART * secPerVPP);

	//4 bytes spare data indicate the block status page is valid
	gFTLSysInfo.spareDataPtr[0] = 0xFF;
	gFTLSysInfo.spareDataPtr[1] = 0xFE;
	gFTLSysInfo.spareDataPtr[2] = 0x00;
	gFTLSysInfo.spareDataPtr[3] = 0x01;

	ptrCIB = buf;
	memset32_f((unsigned int *)ptrCIB,0xFFFFFFFF,64); //256B
	//CIB data flag
	ptrCIB[0] = 0xAA;
	ptrCIB[1] = 0x55;
	ptrCIB[2] = 0x5A;
	ptrCIB[3] = 0xA5;
	//CIB block marker
	ptrCIB[4] = 0xFF;
	//swap block for CIB
	ptrCIB[5] = gFTLSysInfo.cibSwapAddr >> 8;
	ptrCIB[6] = gFTLSysInfo.cibSwapAddr & 0xFF  ;
	//CIB' VPB number
	ptrCIB[7] = gFTLSysInfo.cibAddr >> 8;
	ptrCIB[8] = gFTLSysInfo.cibAddr & 0xFF  ;

	ptrCIB[9] = 0xFF;
	ptrCIB[10] = 0xFF;
	ptrCIB[11] = 0xFF;
	ptrCIB[12] = 0xFF;
	ptrCIB[13] = 0xFF;

	ptrCIB[14] = gFTLSysInfo.flashLayout.dieAmount;
	ptrCIB[15] = gFTLSysInfo.flashLayout.pbPerDie;
	//sector amount of a physical page
	ptrCIB[16] = gFTLSysInfo.flashLayout.secAmountOfPP;

	gFTLSysInfo.flashPara.flashWidth = 0;
	ptrCIB[17] = gFTLSysInfo.flashPara.eraseCycles;
	ptrCIB[18] = gFTLSysInfo.flashPara.addrCycles;
	ptrCIB[19] = gFTLSysInfo.flashPara.rbWaitCounter;

	ptrCIB[20] = gFTLSysInfo.flashLayout.progMode;

	//initialize all parameters used by FTL
	ptrCIB[21] = gFTLSysInfo.flashLayout.secPerVPB;
	ptrCIB[22] = gFTLSysInfo.flashLayout.secPerVPP;
	//zone amount in system.
	ptrCIB[23] = gFTLSysInfo.flashLayout.zoneAmount;
	ptrCIB[24] = gFTLSysInfo.flashLayout.vpbPerZone;
	ptrCIB[25] = gFTLSysInfo.reservedVPBAmount >> 8;
	ptrCIB[26] = gFTLSysInfo.reservedVPBAmount & 0xFF;
	ptrCIB[27] = gFTLSysInfo.cachedTableAmount;

	gFTLSysInfo.realCapacity = 0;
	//availble VPBs in a zone can be used for user.gpram_realCapacity must be calcuated by these value.
	for(i=0;i<gFTLSysInfo.flashLayout.zoneAmount;i++)
	{
		k = vpbPerZone-gFTLSysInfo.reservedVPBAmount-badPBInZone[i];
		//in zone,reservedVPBAmountForSpecialVB SHALL be considered,
		if(i == 0)
		{
			//currently, reserved MAX_VPB_AMOUNT_FOR_ONE_SPECIAL_VB VPBs for each special VB
			k -= ((MAX_VPB_AMOUNT_FOR_ONE_SPECIAL_VB + 1) * FTL_SPECIAL_VB_FOR_FS);
		}

		((UINT16 *)(ptrCIB+128))[i]=k;
		gFTLSysInfo.realCapacity +=(k<<gFTLSysInfo.flashLayout.secPerVPB);
	}

		//system user capacity
	*((unsigned long *)(ptrCIB+32)) = gFTLSysInfo.realCapacity;

	memcp32_f((unsigned int *)gFTLSysInfo.userDataBufPtr,(unsigned int *)ptrCIB,64); //256B
	currentOper->zoneID = 0;
	currentOper->wVPB = gFTLSysInfo.cibAddr;
	currentOper->wVPP = 0;
	currentOper->LPNStatus = 1;

	//write CIB to page 0 of CIB
	ret = MTD_Write_f(currentOper);

	//create bad block table
	memcp32_f((unsigned int *)gFTLSysInfo.userDataBufPtr,(unsigned int *)xbuf,512); //2048B

	currentOper->wVPP = 1;

	//write block status to page 1 of CIB
	ret |= MTD_Write_f(currentOper);

	printk("\n[FTL LOW FORMAT] end realCapacity: %u\n",\
			gFTLSysInfo.realCapacity);
#endif  //FTL_LOW_FORMAT
	return ret;

}
