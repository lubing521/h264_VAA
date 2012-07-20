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

  Name:ftl_if.h
 
  Description:FTL interface functions provide for upper applications.
 
  Development/Test Environment:
  		
			  

  Import:  

  Export

______________________________________________________________________________________

  Version      Date         Engineer               Description
  -------   ----------  -----------------  -------------------------------------------
  0.1	    2010-01-10	James			Initial creation.

***************************************************************************************/

#ifndef __FTL_IF_H__
#define __FTL_IF_H__

#include "ftl_def.h"

/**************************************************************************************
Description:initialize flash management running env. 
Inputs:
	flag--0,indicates that power on init;1 means re-init
Output:
	0--successful
	others--failure.
**************************************************************************************/

extern UINT8 FTL_Init_f(UINT8 flag);

/**************************************************************************************
Description:Read a sector data from flash. Before read from flash, this function will write
			back the cached data left by previous writing to flash.
NOTE: read can be modified to only read out part of data from flash instead whole PP
Inputs:
	lba,begining logical block address .
	sectorAmount,total sector amount want to be read,
				max sector amount at this read limitted by sector amount of a VPP
Output:
	0--failure
	others--real valid sector amount in data buf
**************************************************************************************/
extern UINT8 FTL_ReadSector_f(UINT32 lba,UINT16 sectorAmount);
/**************************************************************************************
Description: Prepare to write data to flash. 
			if current lba doesn't continue to last lba, finish last write first
			if lookup table related to current lba is not cached,build the lookup table
			if there is not available empty VPP for write, get an empty VPB for write
Inputs:
	lba,start logical block address data will be written to.
	sectorAmount, the sector amount will be written to.this value is optional.
Output:
	0------successfully
	other--failure.
**************************************************************************************/
extern UINT8 FTL_WriteOpen_f(UINT32 lba,UINT16 sectorAmount);
/**************************************************************************************
Description:Write a sector data to flash.The data received from host must be saved in correct offset in RAM
Inputs:
	lba,logical block address data will be written to.
	
Output:
	0------successfully
	WAIT_FOR_NEXT_LBA---rerutn directly, not end of the LPN, wait for next write.
	other--failure.
**************************************************************************************/
extern UINT8 FTL_WriteSector_f(UINT32 lba);
/**************************************************************************************
Description:Write back the cached data to flash. At most time, we don't know the exact 
			data packet will be written to flash, to improve performance we cached the data 
			if the data wasn't align the page boundary, wait for next packet coming in.
			Usually this function will be invoked during we make sure the disk was removed.
Inputs:	
Output:
	0------successfully
	other--failure.
**************************************************************************************/
extern UINT8 FTL_WriteClose_f(void);

#endif

