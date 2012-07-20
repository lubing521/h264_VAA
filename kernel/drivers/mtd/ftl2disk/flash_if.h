/*************************************************************************************\
*  ���ݺ�����Ϣ�Ƽ����޹�˾������      
*
*  ��Ȩ����(C)2009 ���ݺ�����Ϣ�Ƽ����޹�˾����������Ȩ��
*
*  
*
*
*  ���������ڲ������ݺ�����Ϣ�Ƽ����޹�˾����                             
*                                                                            
*  ����������ڱ���ɵ�����±����Ʋ��ֻ�ȫ����
*
*  �����Ǵ˰�Ȩ����������Զ�����ڱ�����У�
*
*  ����ʹ�ò��ֻ�ȫ������
*
***************************************************************************************/

/**************************************************************************************

  Name:flash_if.h
 
  Description:NAND FLASH access functions, including page read/write and byte read. used by MTD layer.
 
  Development/Test Environment:
  		
			  

  Import:  

  Export

______________________________________________________________________________________

  Version      Date         Engineer               Description
  -------   ----------  -----------------  -------------------------------------------
  0.1	    2010-01-10	James			Initial creation.

***************************************************************************************/

#ifndef __FLASH_IF__
#define __FLASH_IF__

extern __INLINE__  UINT8 FLASH_Init_f(void);

extern __INLINE__  UINT8 FLASH_Erase_f(UINT8 ceNo,UINT32 *rowAddr);
extern __INLINE__  UINT8 FLASH_Erase_Interleaving_f(UINT8 ceNo,UINT32 *rowAddr);
extern __INLINE__  UINT8 FLASH_Erase_TwoPlane_f(UINT8 ceNo,UINT32 *rowAddr);
extern __INLINE__  UINT8 FLASH_Erase_InterleavingTwoPlane_f(UINT8 ceNo,UINT32 *rowAddr);

extern __INLINE__  UINT8 FLASH_Write_f(UINT8 ceNo,UINT32 *rowAddr,UINT8 *dataBufPtr,UINT8 *spareBufPtr);
extern __INLINE__  UINT8 FLASH_Write_Interleaving_f(UINT8 ceNo,UINT32 *rowAddr,UINT8 *dataBufPtr,UINT8 *spareBufPtr);
extern __INLINE__  UINT8 FLASH_Write_TwoPlane_f(UINT8 ceNo,UINT32 *rowAddr,UINT8 *dataBufPtr,UINT8 *spareBufPtr);
extern __INLINE__  UINT8 FLASH_Write_InterleavingTwoPlane_f(UINT8 ceNo,UINT32 *rowAddr,UINT8 *dataBufPtr,UINT8 *spareBufPtr);


extern __INLINE__  UINT8 FLASH_Read_f(UINT8 ceNo,UINT32 *rRowAddr,UINT8 startOffsetInLPPN,UINT8 endOffsetInLPPN,UINT8 *dataBufPtr,UINT8 *spareBufPtr,UINT8 flag);
extern __INLINE__  UINT8 FLASH_Read_Interleaving_f(UINT8 ceNo,UINT32 *rRowAddr,UINT8 startOffsetInLPPN,UINT8 endOffsetInLPPN,UINT8 *dataBufPtr,UINT8 *spareBufPtr,UINT8 flag);
extern __INLINE__  UINT8 FLASH_Read_TwoPlane_f(UINT8 ceNo,UINT32 *rRowAddr,UINT8 startOffsetInLPPN,UINT8 endOffsetInLPPN,UINT8 *dataBufPtr,UINT8 *spareBufPtr,UINT8 flag);
extern __INLINE__  UINT8 FLASH_Read_InterleavingTwoPlane_f(UINT8 ceNo,UINT32 *rRowAddr,UINT8 startOffsetInLPPN,UINT8 endOffsetInLPPN,UINT8 *dataBufPtr,UINT8 *spareBufPtr,UINT8 flag);

extern __INLINE__  UINT8 FLASH_HardCopyBack_f(UINT8 ceNo,FTL_COPYBACK_PTR copyBackPtr,UINT8 *dataBufPtr,UINT8 *spareBufPtr,UINT8 flag);
extern __INLINE__  UINT8 FLASH_HardCopyBack_Interleaving_f(UINT8 ceNo,FTL_COPYBACK_PTR copyBackPtr,UINT8 *dataBufPtr,UINT8 *spareBufPtr,UINT8 flag);
extern __INLINE__  UINT8 FLASH_HardCopyBack_TwoPlane_f(UINT8 ceNo,FTL_COPYBACK_PTR copyBackPtr,UINT8 *dataBufPtr,UINT8 *spareBufPtr,UINT8 flag);
extern __INLINE__  UINT8 FLASH_HardCopyBack_InterleavingTwoPlane_f(UINT8 ceNo,FTL_COPYBACK_PTR copyBackPtr,UINT8 *dataBufPtr,UINT8 *spareBufPtr,UINT8 flag);

#endif
