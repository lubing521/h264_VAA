/*************************************************************************************
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

  Name:ftl_global.h
 
  Description:Global variables definations used by flash management aglorithm.
 
  Development/Test Environment:
  		
			  

  Import:  

  Export

______________________________________________________________________________________

  Version      Date         Engineer               Description
  -------   ----------  -----------------  -------------------------------------------
  0.1	    2010-01-10	James			Initial creation.

***************************************************************************************/

#ifndef		__FTL_GLOBAL__
#define		__FTL_GLOBAL__

#include "ftl_def.h"


	typedef struct __FTL_COPYBACK{
		UINT32			wRowAddr[4];
		UINT32			rRowAddr[4];

		UINT8			secAmountOfVPP;//sector amount in a physical page, 4,8...
		UINT8			startOffsetInLPPN;	//begin offset in the LPN(logical physical page).include this offset	
		UINT8			endOffsetInLPPN;	//end offset in the LPN(logical physical page).include this offset
	
	}FTL_COPYBACK,*FTL_COPYBACK_PTR;

	typedef struct __FTL_FLASH_LAYOUT{	
		//flash memory arrangement
		//progpram mode,0-normal, 1-interleaving progpram,2-two planes,5-interleaving two plane
		UINT8			progMode ;//program mode, it will decide the the layout of VPB and VPP
		UINT8			pbPerDie;//2^n,physical block amount in a die
		UINT8			dieAmount;//0--
		UINT8			zoneAmount;//0--n
		UINT8			vpbPerZone;//2^n,virutal physical block in a zone,512,1024,2048--
		UINT16			*vbPerZonePtr;//size=2*zoneAmount,VB amount in each zone,it means the available user capacity of each zone

		UINT16			vppAmountOfVPB;//virtual physical page amount in a VPB,64,128,max 256 now
		UINT8			secPerVPP;//2^n, sector amount in a virtual physical page
		UINT8			secAmountOfPP;//sector amount in a physical page, 4,8...
		UINT8			secAmountOfVPP;//sector amount in a virtual physical page, 16,32...
		UINT8			secPerVPB;//2^n, sectors in a VPB
	}FTL_FLASH_LAYOUT;

	typedef struct __FTL_FLASH_PARA{	
		//flash parameters
		UINT8			flashWidth;//0--8bit, 1--16bit
		UINT8			eraseCycles;
		UINT8			addrCycles;
	
		UINT8			rbWaitCounter;//waiting for r/b interrupt,1<<gpram_rbWaitCounter	

	}FTL_FLASH_PARA;

	typedef struct __FTL_PARA_IN_ZONE{	
		UINT8			zoneID;	//zone ID,considering not all lookup table cached in RAM
		UINT16			emptyVPBAmount;//empty good VPBs in a zone.
		UINT16			discardedVPBAmount;//discarded VPBs in a zone.
		UINT16			startVPB;//points to start good VPB in a zone
		UINT16			endVPB;//points to end good VPB in a zone
		UINT16			startDVPB;//points to start discarded VPB in a zone
		UINT16			endDVPB;//points to end discarded VPB in a zone
		UINT32			vpbCounter;//each writing on first VPP of a VPB,the counter increase one.used as version tag
	
		UINT16			*vb2vpbPtr;//size=2*vpbPerZone*zoneAmount
		UINT16			*vpbLinkerPtr;//size=2*vpbPerZone*zoneAmount			
		UINT8			*plkPtr;//size=vppPerVPB*vpbPerZone*zoneAmount
		UINT8			*vpbAmountPerVBPtr;//VPB amount of each VB,this will be used by GC

#if VPPS_IN_VPB_FOLLOW_256_RULE
		UINT8			*plk256Ptr;//indicates  if there is a LPP was related to the 256th VPP,size=vpbPerZone*zoneAmount*2
									//([0]:indicates if there was a LPP related to the 256th VPP,[0]=1,yes
									//[1]:the LPP number,valid when [0]=1)
#endif
	}FTL_PARA_IN_ONE_ZONE,*FTL_PARA_IN_ONE_ZONE_PTR;

	typedef struct __FTL_CURRENT_OPER{	
		//zone,vpb,vpp,lba offset in vpp,during read/write operation.
		UINT8			lkpID;//lookup table ID related to the zone ID
		UINT8			zoneID;//zone ID
		UINT16			VB;	//virtual block
		UINT8			LPNStatus;//0--this LPN had benn written,1--no data had been written to this LPN
		UINT16			rVPB;//latest data of the lba in the VPB
		UINT16			wVPB;//data of the lba will be written to the vpb
		UINT16			LPN;//logical page number
		UINT16			rVPP;//related to latest LPN,latest data of the lba in the VPP
		UINT16			wVPP;//data of the lba will be written to the wVPP
		UINT16			emptyVPPID;//empty VPP ID
		UINT8			startOffsetInLPPN;	//begin offset in the LPN(logical physical page).include this offset	
		UINT8			endOffsetInLPPN;	//end offset in the LPN(logical physical page).include this offset
		UINT32			lastLBA;	//last LBA
		UINT8			lastOP;//lastOP=1,write data,lastOP=0,read data,lastOP=2,idle,lastOP=3,in GC		
	}FTL_CURRENT_OPER,*FTL_CURRENT_OPER_PTR;



	typedef struct __FTL_SYS_INIF{
		UINT32						sysDataBuf[INTERNAL_DATA_BUF_FOR_FTL];//interal data buf(512B+256B) used by FTL itself.
		UINT32						*userDataBufPtr;//user data's RAM address, if there are several BUF for FTL, this value can be changed before invoking write function.
		UINT32						realCapacity;//system available capcity of FTL expressed by sector amount
		UINT16						cibAddr;//cib block address
		UINT16						cibSwapAddr;//swap block for cib block
		UINT8						cachedTableAmount;//usually equals zoneAmount if there are enough RAM space
		UINT8						powerLossFlag;//indicates if power loss found,0--not found,1--found
		UINT16						reservedVPBAmount;//the least reserved good vpbs for a zone, don't include reserved VPBs for special VB
		UINT8						reservedVPBAmountForSpecialVB;//this value will ensure there are 32 VPBs for each special VB.
		UINT8						inGC;//this value indicates if FTL is in GC progress.1--in GC
		UINT8						*spareDataPtr;//points to an array which holds all parts' spare data used for FTL
		FTL_FLASH_LAYOUT			flashLayout;//flash memory organization		
		FTL_PARA_IN_ONE_ZONE_PTR	paraInOneZonePtr;//lookup tables in a zone
		FTL_CURRENT_OPER			currentOper;//variables used by current read/write functions		
		FTL_FLASH_PARA				flashPara;//flash parameters
	}FTL_SYS_INFO,*FTL_SYS_INFO_PTR;

extern	FTL_SYS_INFO gFTLSysInfo;

#endif	
