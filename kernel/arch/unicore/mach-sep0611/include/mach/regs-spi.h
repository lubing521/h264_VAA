/* arch/unicore/mach-sep0611/include/mach/regs-spi.h
*
* sep0611 SPI register definition
*  
* Copyright (C) 2010 SEU ASIC.
 *
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
 *
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
 *
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <mach/hardware.h>

#ifndef __ASM_ARCH_REGS_SPI_H
#define __ASM_ARCH_REGS_SPI_H

#define SEP0611_CONTROL0   (0x00)
#define SEP0611_CONTROL0_TMOD_TR	  (0<<8)	/* TR mode */
#define SEP0611_CONTROL0_TMOD_TO	  (1<<8)	/* TO mode */
#define SEP0611_CONTROL0_TMOD_RO   (2<<8)	/* RO mode */

#define SEP0611_CONTROL0_SCPOL_HIGH  (1<<7)	/* Clock polarity select */
#define SEP0611_CONTROL0_SCPOL_LOW  (0<<7)	/* Clock polarity select */

#define SEP0611_CONTROL0_CPHA_FMTB	  (1<<6)	  /* Clock Phase Select */
#define SEP0611_CONTROL0_CPHA_FMTA	  (0<<6)	  /* Clock Phase Select */

#define SEP0611_CONTROL0_FRF_SPI	  (0<<4)	  /* Select THE SPI MODE */
#define SEP0611_CONTROL0_DFL_INIT	  (0x7)	  /* DATA FRAME LONG INITIAL */


#define SEP0611_CONTROL1   (0x04)


#define SEP0611_SSIENR   (0x08)
#define SEP0611_SSIENR_ENR 1
#define SEP0611_SSIENR_DIS  0

#define SEP0611_SER   (0x10)
#define SEP0611_SER_NUM1  2      /*SELECT THE NO1 */
#define SEP0611_SER_NUM0  1      /*SELECT THE NO0 */
#define SEP0611_SER_NONE  0      /*SELECT NO SLAVE */

#define SEP0611_BAUDR   (0x14)
#define SEP0611_TXFTLR   (0x18)    /*FIFO THRESHOLD REGISTER*/
#define SEP0611_RXFTLR   (0x1C)
#define SEP0611_TXFLR   (0x20)    /*FIFO STATUS REGISTER*/
#define SEP0611_RXFLR   (0x24)

#define SEP0611_SR        (0x28)
#define SEP0611_SR_DCOL   (1<<6)   /*DATA COLLITION ERROR*/
#define SEP0611_SR_BUSY   1          /*SSI BUSY*/

#define SEP0611_IMR      (0x2C)
#define SEP0611_IMR_NOMASK      (0x1f)

#define SEP0611_ISR      (0x30)
#define SEP0611_RISR     (0x34)

#define SEP0611_TXOICR   (0x38)
#define SEP0611_RXOICR   (0x3C)
#define SEP0611_RXUICR   (0x40)
#define SEP0611_MSTICR   (0x44)
#define SEP0611_ICR    (0x48)

#define SEP0611_DMACR            (0x4C)
#define SEP0611_DMACR_ENA    (0x03)
#define SEP0611_DMACR_DIS     (0x0)
#define SEP0611_DMATDLR         (0x50)
#define SEP0611_DMARDLR         (0x54)
#define SEP0611_IDR             (0x58)
#define SEP0611_SSI_COMP_VERSION         (0x5C)

#define SEP0611_DR              (0x60)
//#define SEP0611_DRRX               (0x7e)



#define SEP0611_GPIO_OUTPUT  (0xFFFFFFF1)

#define SEP0611_GPD2_DATA  (GPIO_PORTD_DATA_V|0X02)
#define SEP0611_GPD2_DIR   (GPIO_PORTD_DIR_V&(~(1<<2)))
#define SEP0611_GPD2_SEL   (GPIO_PORTD_SEL_V&(~(1<<2)))

#define CSL *(volatile unsigned long*)GPIO_PORTD_DATA_V &= ~(0x1<<3); //cs 片选信号拉低,使能
#define CSH *(volatile unsigned long*)GPIO_PORTD_DATA_V |= 0x1<<3;		//cs 片选信号拉高  ，不使能
/*
#define SEP0611_GPD4_DATA  (GPIO_PORTD_DATA_V|0X04)
#define SEP0611_GPD4_DIR   (GPIO_PORTD_DIR_V&(~(1<<4)))
#define SEP0611_GPD4_SEL   (GPIO_PORTD_SEL_V&(~(1<<4)))

#define SEP0611_GPD1_DATA  (GPIO_PORTD_DATA_V|0X01)
#define SEP0611_GPD1_DIR   (GPIO_PORTD_DIR_V&(~(1<<1)))
#define SEP0611_GPD1_SEL   (GPIO_PORTD_SEL_V&(~(1<<1)))

#define SEP0611_GPD0_DATA  (GPIO_PORTD_DATA_V|0X0)
#define SEP0611_GPD0_DIR   (GPIO_PORTD_DIR_V&(~(1<<0)))
#define SEP0611_GPD0_SEL   (GPIO_PORTD_SEL_V&(~(1<<0)))
*/

#endif /* __ASM_ARCH_REGS_SPI_H */
