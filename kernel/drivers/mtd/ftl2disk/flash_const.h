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

  Name:flash_const.h
 
  Description:Const definations used for FLASH device driver,including NAND commands,error number,etc.
 
  Development/Test Environment:
  		
			  

  Import:  

  Export

______________________________________________________________________________________

  Version      Date         Engineer               Description
  -------   ----------  -----------------  -------------------------------------------
  0.1	    2010-01-10	James			Initial creation.

***************************************************************************************/

#ifndef __FLASH_CONST__
#define __FLASH_CONST__
	
//command list
#define	CMD_READ_00						0x00		//1st command of normal read,or other read
#define	CMD_READ_30						0x30		//2nd command of normal read, or 3rd command of two plane read
#define	CMD_READ_ID_90					0x90		//Read ID
#define	CMD_READ_STATUS_70				0x70		//Read status
#define CMD_READ_STATUS_F1				0xF1		//Read status for internal chip 1(SAMSUNG)
#define CMD_READ_STATUS_F2				0xF2		//Read status for internal chip 2(SAMSUNG)
#define	CMD_RESET_FF					0xFF		//Reset
#define	CMD_PROG_80						0x80		//1st command of normal page program or two plane program
#define	CMD_PROG_10						0x10		//2nd command of normal page program, 4th command of two plane program
#define CMD_PROG_11						0x11		//2nd cycle command of two plane program 
#define CMD_PROG_81						0x81		//3rd cycle command of two plane program 
#define	CMD_ERASE_60					0x60		//1st cycle command of block erase, or 1st and 2nd for two plane read or erase
#define	CMD_ERASE_D0					0xD0		//2nd cycle command of block erase
#define	CMD_RANDOM_IN_85				0x85		//1st cycle command of random data input,or copy back
#define	CMD_READ_COPY_BACK_35			0x35    	//2nd cycle command of read for copy back,or 3rd for two plane read for copy back
#define	CMD_RANDOM_OUT_05				0x05		//1st cycle command of random data output,or 2nd for two plane random output
#define	CMD_RANDOM_OUT_E0				0xE0		//2nd cycle command of random data output,or 3rd for two plane random output
#define	CMD_PLANE_60					0x60		//1st and 2nd for two plane read or erase


//0x20--0x2F, FLASH status
#define FLASH_PROGRAM_FAIL				0x20
#define FLASH_ERASE_FAIL				0x21

//wait for r/b ready,but timeout
#define FLASH_RB_TIMEOUT				0x22
//there are too many errors after read data out
#define FLASH_TOO_MANY_ERROR			0x23
#define FLASH_COPY_BACK_FAIL			0x24
//data read out are all 0
#define FLASH_ALL_ZERO					0x25

#define MAX_CE_AMOUNT_IN_SYSTEM		1
#define MAX_CHIP_AMOUNT_IN_A_CE		2

#endif
