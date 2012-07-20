#ifndef __FTL_DEF__
#define __FTL_DEF__

#define TIME_CALC 0
#define CALC_READ_IN_WRITE 0
#define RESERVED_PAGES_NUM 0x40000


//program mode,0:normal,1:normal interleaving,2:normal two plane,3:normal dual channel
//				5:interleaving two plane,6:dual channel interleaving two plane
#define PROG_MODE	2

//************************************************************BEGIN
//below include file, only one can be included
#include "H27UBG8T2A.h"
//*************************************************************END
#define  SHIFT_SECTOR_PER_VPB		13
#define  SHIFT_SECTOR_PER_VPP		5


//************************************************************BEGIN
//global configurations for FTL,PLEASE DON'T CHANGE IF NOT KNOWN WELL.
//these special VBs will be file system space,FTL will make sure there are enough VPBs for these VBs
//the speical VB are from VB 0, the VB from 0 to (FTL_SPECIAL_VB_FOR_FS - 1) will be considered as special VB
#define FTL_SPECIAL_VB_FOR_FS			1

//spare area size of each part is used for FTL.
#define FTL_SPARE_SIZE_PER_PART			2

//reserved bytes in system area,if use ECC for system area, additional bytes will be reserved 
#define FTL_RESERVED_BYTES_IN_SYSTEM_AREA	0

//the size of a sector part in a VPB, if VPP layout is:
//sector 1(512B)+sector 2(512B)+....sector n(512B)+spare area 1(16B)+ spare area 2(16B)...spare area n(16B), the SECOTR_PART_SIZE be 0x200
//if VPP layout is:
//sector 1(512B)+spare area 1(16B)+sector 2(512B)+ spare area 2(16B)+....sector n(512B)+spare area n(16B), the SECOTR_PART_SIZE be (0x200+0x10)
#define SECOTR_PART_SIZE				0x200
#define FLASH_PART_SIZE					(SECOTR_PART_SIZE + FLASH_SPARE_SIZE_PER_SECTOR)


#define VPB_STATUS_SHIFT_IN_VPBLINKER	13
//it means there are max 32 VPBs which can be related to same VB
#define MAX_VPB_AMOUNT_FOR_ONE_VB		0x20

#define MAX_VPB_AMOUNT_FOR_ONE_SPECIAL_VB			0x20

//if a VPB contains 256 VPPs, set VPPS_IN_VPB_FOLLOW_256_RULE=1.if VPPs less than 256 and set VPPS_IN_VPB_FOLLOW_256_RULE=1 will consume more RAM, set 0 is better.
#define VPPS_IN_VPB_FOLLOW_256_RULE				1

//////////////////////////////////////////////////////////////////////
//below definations are different for different program mode,
//they are used by low format routines and written to CIB

//define zone amount in system
#define ZONE_IN_SYSTEM	1

//define VPB amount in a zone, it shall be multiple of 2, 1024,2048,4096 are all OK, 2048 is a better value for system
#define VPB_AMOUNT_IN_A_ZONE	512

//virtual physical page amount in a virtual physical block,usually it equals the PP amount of a PB
#define VPP_AMOUNT_PER_VPB				FLASH_PP_AMOUNT_PER_PB

//the part amount of a PP
#define	FTL_PARTS_PER_PP		(FLASH_SECTOR_AMOUNT_PER_PP)

//the part amount of a VPP
#define	FTL_PARTS_PER_VPP		(1<<SHIFT_SECTOR_PER_VPP)

#define BYTES_PER_PP			(FLASH_PART_SIZE * FTL_PARTS_PER_PP)

//cached lookup table amount in RAM, usually cached lookup table amount equals zone amount in system if there are enough RAM
//if only part of lookup table cached in RAM, the last lookup table will be overwritten during access address whose zoneID>=CACHED_LOOKUPTABLE_IN_RAM-1
#define CACHED_LOOKUPTABLE_IN_RAM		ZONE_IN_SYSTEM

//the max physical block amount will be searched during initialization,default 256
#define MAX_INIT_BAD_BLOCK_AMOUNT		0x100
//these VPBs will be used as swap blocks, each zone at least 2
#define	RESERVED_SWAP_VPB_AMOUNT		0x2

//there is a data buf used by FTL itself,the buf size is INTERNAL_DATA_BUF_FOR_FTL*4
#define INTERNAL_DATA_BUF_FOR_FTL		(256+128)

//1--enable bad block replacement,0--disable
#define	ENABLE_BAD_BLOCK_REPLACEMENT		0

//*************************************************************END

//************************************************************BEGIN
//if the cross linker support inline function, this define can increase some function's performance
//#define __SUPPORT_IN_LINE__
#ifdef __SUPPORT_IN_LINE__
#define	__INLINE__  inline 
#else
#define	__INLINE__
#endif
//*************************************************************END


//************************************************************BEGIN
//data type define
typedef	unsigned char		UINT8;
typedef	unsigned short		UINT16;
typedef	unsigned int		UINT32;
typedef	signed char			INT8;
typedef	signed short		INT16;
typedef	signed int			INT32;
//*************************************************************END

//************************************************************BEGIN
//return value define
#define OPER_OK					0

//0x40--0x4F, return value for FTL logic
//accessed lba is out of available capacity
#define OUT_OF_CAPACITY			0x40
//the program mode is not supported
#define INVALID_PROGRAM_MODE	0x41
//the program mode is not supported
#define WAIT_FOR_NEXT_LBA		0x42
#define NO_RELATED_VPB			0x43
#define NO_RELATED_VPP			0x44
#define	NO_EMPTY_VPP			0x45
#define	NO_EMPTY_VPB			0x46
#define NO_VALID_VPP			0x47
//the flash was not low formatted
#define NO_LOW_FORMAT			0x48


//*************************************************************END


#define READ_USE_USB_BUF 	1
#define BUF_TRANSFER_BY_DMA 	1


//************************************************************BEGIN
//debug switch
//#define _DEBUG

#ifdef _DEBUG
#define __DEBUG__
	extern void log_page_to_file(char* logStr);
	extern char gLogStr[256];
	#define LOG_TMP(fmt...)	{sprintf(gLogStr,fmt);log_page_to_file(gLogStr);}
	#define LOG_OUT_c(fmt...)	
//	#define LOG_OUT_c(fmt...)	{sprintf(gLogStr,fmt);log_page_to_file(gLogStr);}
//	#define LOG_OUT(fmt...)	{sprintf(gLogStr,fmt);log_page_to_file(gLogStr);}
//	#define printk_file(fmt...)  {printk(fmt); LOG_OUT( fmt)}
	#define LOG_OUT(fmt...) 
	#define printk_file(fmt...) printk(fmt)
#else
	#define LOG_OUT_c(fmt...)	
	#define LOG_TMP(fmt...)
	#define LOG_OUT(fmt...)
	#define printk_file(fmt...) printk(fmt)
#endif


//#define		DEBUG_NANDSIM
//#define		DEBUG_DRIVER
//#define		DEBUG_MTD
//#define		DEBUG_FTL_FORMAT
//#define		DEBUG_FTL_LOOKUP
//#define		DEBUG_INIT
//#define		DEBUG_FTL_GC
//#define		DEBUG_FTL_SECTOR

//*************************************************************END

#include "flash_const.h"

#endif
