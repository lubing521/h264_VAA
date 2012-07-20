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

  Name:ftl_init.c
 
  Description:Routines used for initializate flash management algorithm.
 
  Development/Test Environment:
  		
			  

  Import:  

  Export

______________________________________________________________________________________

  Version      Date         Engineer               Description
  -------   ----------  -----------------  -------------------------------------------
  0.1	    2010-01-10	James			Initial creation.

***************************************************************************************/
#include "ftl_global.h"
#include <asm/io.h>
#include <linux/kernel.h>
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
extern	unsigned long gpram_databuf;

////////////////////////////////////////////////////////////////////////
//in OS environment, below static memory allocation can be replacd with dynamic memory allocation
FTL_PARA_IN_ONE_ZONE g_FTLParaInZones[CACHED_LOOKUPTABLE_IN_RAM];
UINT16 g_vb2vpb[CACHED_LOOKUPTABLE_IN_RAM][VPB_AMOUNT_IN_A_ZONE];//size=2*vpbPerZone*zoneAmount
UINT16 g_vpbLinker[CACHED_LOOKUPTABLE_IN_RAM][VPB_AMOUNT_IN_A_ZONE];//size=2*vpbPerZone*zoneAmount
UINT32 g_plkp[CACHED_LOOKUPTABLE_IN_RAM][VPB_AMOUNT_IN_A_ZONE * VPP_AMOUNT_PER_VPB/4];//size=vppPerVPB*vpbPerZone*zoneAmount
UINT8 g_vpbAmountPerVB[CACHED_LOOKUPTABLE_IN_RAM][VPB_AMOUNT_IN_A_ZONE];//each VB's VPB amount
UINT16 g_vbPerZone[ZONE_IN_SYSTEM];//VB amount in each zone,it means the available user capacity of each zone
UINT32 g_spareData[FTL_PARTS_PER_VPP*FTL_SPARE_SIZE_PER_PART/4];
//user data aligned with 4 can be accessed with DWORD operations ,additional (FLASH_PART_SIZE-SECOTR_PART_SIZE) RAM is used for ECC
//UINT32 g_userDataBuf[SECOTR_PART_SIZE*FTL_PARTS_PER_VPP/4+(FLASH_PART_SIZE-SECOTR_PART_SIZE)/4];
#if VPPS_IN_VPB_FOLLOW_256_RULE
UINT32 g_plkp256[CACHED_LOOKUPTABLE_IN_RAM][VPB_AMOUNT_IN_A_ZONE * 2];//size=vpbPerZone*zoneAmount*2
#endif

/**************************************************************************************
Description:allocate memory for lookup tables used by FTL, memory size will be 
			calculated by zone amount,vpb per zone, and physical page amount in a PB. 
Inputs:
	zoneAmount--zone amount in system, it will be decided by program mode and FLASH's size
	vpbPerZone--vpb amount in each zone, the vpb amount can be defined according to available RAM space for FTL
				Note:large vpb amount in zone will increase the time of building vb2vpb table 
	vppPerVPB--VPP amount in a VPB, it equals physical page amount in a physical block 
Output:
	0--successful
**************************************************************************************/
UINT8 FTL_Malloc(UINT8 zoneAmount,UINT16 vpbPerZone,UINT8 vppPerVPB)
{
	UINT16 i;

	gFTLSysInfo.paraInOneZonePtr = g_FTLParaInZones;
	gFTLSysInfo.flashLayout.vbPerZonePtr = g_vbPerZone;
	
	gFTLSysInfo.spareDataPtr = (UINT8 *)g_spareData;
	//points to USB data buf
	gFTLSysInfo.userDataBufPtr = (UINT32 *)gpram_databuf;

	//init each lookup table's address
	for(i=0;i<CACHED_LOOKUPTABLE_IN_RAM;i++)
	{
		gFTLSysInfo.paraInOneZonePtr[i].vb2vpbPtr = (UINT16 *)&g_vb2vpb[i];
		gFTLSysInfo.paraInOneZonePtr[i].vpbLinkerPtr = (UINT16 *)&g_vpbLinker[i];
		gFTLSysInfo.paraInOneZonePtr[i].plkPtr = (UINT8 *)&g_plkp[i];
		gFTLSysInfo.paraInOneZonePtr[i].vpbAmountPerVBPtr = (UINT8 *)&g_vpbAmountPerVB[i];
		gFTLSysInfo.paraInOneZonePtr[i].zoneID = 0xFF;
#if VPPS_IN_VPB_FOLLOW_256_RULE
		gFTLSysInfo.paraInOneZonePtr[i].plk256Ptr = (UINT8 *)&g_plkp256[i];
#endif
	}
	return 0;	
}
