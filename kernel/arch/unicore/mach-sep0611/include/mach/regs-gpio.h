/* linux/arch/unicore/mach-sep0611/include/mach/regs-gpio.h
 *
 * Copyright (c) 2009-2011 SEUIC
 *  cgm <chenguangming@wxseuic.com>
 *
 * Southeast University ASIC SoC support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Modifications:
 *  05-31-2011	cgm initial version
 *
 */

#ifndef __ASM_ARCH_REGS_GPIO_H
#define __ASM_ARCH_REGS_GPIO_H

#include <mach/hardware.h>
#include <mach/gpio.h>

#define SEP0611_GPIO_BASE(pin)			((pin < PORT_AO) ? ((pin >> 5) * 0x20 + GPIO_PORTA_V) : (GPIO_AO_V))
#define SEP0611_GPIO_OFFSET(pin)		(pin & 31)

/*
 * SEL
 */
#define SEP0611_GPIO_IO					0
#define SEP0611_GPIO_SFN				1
#define SEP0611_GPIO_SFN2				2

/*
 * DIR
 */
#define SEP0611_GPIO_OUT				0
#define SEP0611_GPIO_IN					1

/*
 * get gpio pin's line number.
 */
#define SEP0611_GPIONO(port, offset)	(port + offset)


/*
 * all the GPIO pins' line number, start with PORTA
 */
#define SEP0611_GPA0	SEP0611_GPIONO(PORT_A, 0)
#define SRAM_ADDR0		SEP0611_GPIO_SFN
#define SD2_WP			SEP0611_GPIO_SFN2

#define SEP0611_GPA1	SEP0611_GPIONO(PORT_A, 1)
#define SRAM_ADDR1		SEP0611_GPIO_SFN
#define SD2_DAT0		SEP0611_GPIO_SFN2

#define SEP0611_GPA2	SEP0611_GPIONO(PORT_A, 2)
#define SRAM_ADDR2		SEP0611_GPIO_SFN
#define SD2_DAT1		SEP0611_GPIO_SFN2

#define SEP0611_GPA3	SEP0611_GPIONO(PORT_A, 3)
#define SRAM_ADDR3		SEP0611_GPIO_SFN
#define SD2_DAT2		SEP0611_GPIO_SFN2

#define SEP0611_GPA4	SEP0611_GPIONO(PORT_A, 4)
#define SRAM_ADDR4		SEP0611_GPIO_SFN
#define SD2_DAT3		SEP0611_GPIO_SFN2

#define SEP0611_GPA5	SEP0611_GPIONO(PORT_A, 5)
#define SRAM_ADDR5		SEP0611_GPIO_SFN
#define SD2_CMD			SEP0611_GPIO_SFN2

#define SEP0611_GPA6	SEP0611_GPIONO(PORT_A, 6)
#define SRAM_ADDR6		SEP0611_GPIO_SFN
#define SD2_CLK			SEP0611_GPIO_SFN2

#define SEP0611_GPA7	SEP0611_GPIONO(PORT_A, 7)
#define SRAM_ADDR7		SEP0611_GPIO_SFN
#define SSI3_RXD		SEP0611_GPIO_SFN2

#define SEP0611_GPA8	SEP0611_GPIONO(PORT_A, 8)
#define SRAM_ADDR8		SEP0611_GPIO_SFN
#define SSI3_TXD		SEP0611_GPIO_SFN2

#define SEP0611_GPA9	SEP0611_GPIONO(PORT_A, 9)
#define SRAM_ADDR9		SEP0611_GPIO_SFN
#define SSI3_CS			SEP0611_GPIO_SFN2

#define SEP0611_GPA10	SEP0611_GPIONO(PORT_A, 10)
#define SRAM_ADDR10		SEP0611_GPIO_SFN
#define SSI3_CLK		SEP0611_GPIO_SFN2

#define SEP0611_GPA11	SEP0611_GPIONO(PORT_A, 11)
#define SRAM_NBE0		SEP0611_GPIO_SFN
#define I2S_SDO1		SEP0611_GPIO_SFN2

#define SEP0611_GPA12	SEP0611_GPIONO(PORT_A, 12)
#define SRAM_NBE1		SEP0611_GPIO_SFN
#define I2S_SDO2		SEP0611_GPIO_SFN2

#define SEP0611_GPA13	SEP0611_GPIONO(PORT_A, 13)
#define SRAM_NOE		SEP0611_GPIO_SFN
#define I2S_SDO3		SEP0611_GPIO_SFN2

#define SEP0611_GPA14	SEP0611_GPIONO(PORT_A, 14)
#define SRAM_NWE		SEP0611_GPIO_SFN

#define SEP0611_GPA15	SEP0611_GPIONO(PORT_A, 15)
#define SRAM_NCS0		SEP0611_GPIO_SFN

#define SEP0611_GPA16	SEP0611_GPIONO(PORT_A, 16)
#define SRAM_NCS1		SEP0611_GPIO_SFN


/*
 * PORTB
 */
#define SEP0611_GPB0	SEP0611_GPIONO(PORT_B, 0)
#define SM_FL_DATA0		SEP0611_GPIO_SFN

#define SEP0611_GPB1	SEP0611_GPIONO(PORT_B, 1)
#define SM_FL_DATA1		SEP0611_GPIO_SFN

#define SEP0611_GPB2	SEP0611_GPIONO(PORT_B, 2)
#define SM_FL_DATA2		SEP0611_GPIO_SFN

#define SEP0611_GPB3	SEP0611_GPIONO(PORT_B, 3)
#define SM_FL_DATA3		SEP0611_GPIO_SFN

#define SEP0611_GPB4	SEP0611_GPIONO(PORT_B, 4)
#define SM_FL_DATA4		SEP0611_GPIO_SFN

#define SEP0611_GPB5	SEP0611_GPIONO(PORT_B, 5)
#define SM_FL_DATA5		SEP0611_GPIO_SFN

#define SEP0611_GPB6	SEP0611_GPIONO(PORT_B, 6)
#define SM_FL_DATA5		SEP0611_GPIO_SFN

#define SEP0611_GPB7	SEP0611_GPIONO(PORT_B, 7)
#define SM_FL_DATA7		SEP0611_GPIO_SFN

#define SEP0611_GPB8	SEP0611_GPIONO(PORT_B, 8)
#define SM_FL_DATA8		SEP0611_GPIO_SFN

#define SEP0611_GPB9	SEP0611_GPIONO(PORT_B, 9)
#define SM_FL_DATA9		SEP0611_GPIO_SFN

#define SEP0611_GPB10	SEP0611_GPIONO(PORT_B, 10)
#define SM_FL_DATA10	SEP0611_GPIO_SFN

#define SEP0611_GPB11	SEP0611_GPIONO(PORT_B, 11)
#define SM_FL_DATA11	SEP0611_GPIO_SFN

#define SEP0611_GPB12	SEP0611_GPIONO(PORT_B, 12)
#define SM_FL_DATA12	SEP0611_GPIO_SFN

#define SEP0611_GPB13	SEP0611_GPIONO(PORT_B, 13)
#define SM_FL_DATA13	SEP0611_GPIO_SFN

#define SEP0611_GPB14	SEP0611_GPIONO(PORT_B, 14)
#define SM_FL_DATA14	SEP0611_GPIO_SFN

#define SEP0611_GPB15	SEP0611_GPIONO(PORT_B, 15)
#define SM_FL_DATA15	SEP0611_GPIO_SFN


/*
 * PORTC
 */
#define SEP0611_GPC0	SEP0611_GPIONO(PORT_C, 0)
#define I2S_SDI0		SEP0611_GPIO_SFN

#define SEP0611_GPC1	SEP0611_GPIONO(PORT_C, 1)
#define I2S_SDO0		SEP0611_GPIO_SFN

#define SEP0611_GPC2	SEP0611_GPIONO(PORT_C, 2)
#define I2S_WS			SEP0611_GPIO_SFN

#define SEP0611_GPC3	SEP0611_GPIONO(PORT_C, 3)
#define I2S_CLK			SEP0611_GPIO_SFN

#define SEP0611_GPC4	SEP0611_GPIONO(PORT_C, 4)
#define NAND_R_B0		SEP0611_GPIO_SFN

#define SEP0611_GPC5	SEP0611_GPIONO(PORT_C, 5)
#define NAND_R_B1		SEP0611_GPIO_SFN

#define SEP0611_GPC6	SEP0611_GPIONO(PORT_C, 6)
#define NAND_R_B2		SEP0611_GPIO_SFN

#define SEP0611_GPC7	SEP0611_GPIONO(PORT_C, 7)
#define NAND_R_B3		SEP0611_GPIO_SFN

#define SEP0611_GPC8	SEP0611_GPIONO(PORT_C, 8)
#define NAND_FALE		SEP0611_GPIO_SFN

#define SEP0611_GPC9	SEP0611_GPIONO(PORT_C, 9)
#define NAND_FCLE		SEP0611_GPIO_SFN

#define SEP0611_GPC10	SEP0611_GPIONO(PORT_C, 10)
#define NAND_NFWE		SEP0611_GPIO_SFN

#define SEP0611_GPC11	SEP0611_GPIONO(PORT_C, 11)
#define NAND_NFRE		SEP0611_GPIO_SFN

#define SEP0611_GPC12	SEP0611_GPIONO(PORT_C, 12)
#define NAND_NCE0		SEP0611_GPIO_SFN

#define SEP0611_GPC13	SEP0611_GPIONO(PORT_C, 13)
#define NAND_NCE1		SEP0611_GPIO_SFN

#define SEP0611_GPC14	SEP0611_GPIONO(PORT_C, 14)
#define NAND_NCE2		SEP0611_GPIO_SFN

#define SEP0611_GPC15	SEP0611_GPIONO(PORT_C, 15)
#define NAND_NCE3		SEP0611_GPIO_SFN



/*
 * PORTD
 */
#define SEP0611_GPD0	SEP0611_GPIONO(PORT_D, 0)
#define UART4_CTS		SEP0611_GPIO_SFN

#define SEP0611_GPD1	SEP0611_GPIONO(PORT_D, 1)
#define UART4_RTS		SEP0611_GPIO_SFN

#define SEP0611_GPD2	SEP0611_GPIONO(PORT_D, 2)
#define UART4_RXD		SEP0611_GPIO_SFN

#define SEP0611_GPD3	SEP0611_GPIONO(PORT_D, 3)
#define UART4_TXD		SEP0611_GPIO_SFN

#define SEP0611_GPD4	SEP0611_GPIONO(PORT_D, 4)
#define UART3_CTS		SEP0611_GPIO_SFN

#define SEP0611_GPD5	SEP0611_GPIONO(PORT_D, 5)
#define UART3_RTS		SEP0611_GPIO_SFN

#define SEP0611_GPD6	SEP0611_GPIONO(PORT_D, 6)
#define UART3_RXD		SEP0611_GPIO_SFN

#define SEP0611_GPD7	SEP0611_GPIONO(PORT_D, 7)
#define UART3_TXD		SEP0611_GPIO_SFN

#define SEP0611_GPD8	SEP0611_GPIONO(PORT_D, 8)
#define UART2_RXD		SEP0611_GPIO_SFN

#define SEP0611_GPD9	SEP0611_GPIONO(PORT_D, 9)
#define UART2_TXD		SEP0611_GPIO_SFN

#define SEP0611_GPD10	SEP0611_GPIONO(PORT_D, 10)
#define UART1_RXD		SEP0611_GPIO_SFN

#define SEP0611_GPD11	SEP0611_GPIONO(PORT_D, 11)
#define UART1_TXD		SEP0611_GPIO_SFN

#define SEP0611_GPD12	SEP0611_GPIONO(PORT_D, 12)
#define SD1_WP			SEP0611_GPIO_SFN

#define SEP0611_GPD13	SEP0611_GPIONO(PORT_D, 13)
#define SD1_DAT0		SEP0611_GPIO_SFN

#define SEP0611_GPD14	SEP0611_GPIONO(PORT_D, 14)
#define SD1_DAT1		SEP0611_GPIO_SFN

#define SEP0611_GPD15	SEP0611_GPIONO(PORT_D, 15)
#define SD1_DAT2		SEP0611_GPIO_SFN

#define SEP0611_GPD16	SEP0611_GPIONO(PORT_D, 16)
#define SD1_DAT3		SEP0611_GPIO_SFN

#define SEP0611_GPD17	SEP0611_GPIONO(PORT_D, 17)
#define SD1_CMD			SEP0611_GPIO_SFN

#define SEP0611_GPD18	SEP0611_GPIONO(PORT_D, 18)
#define SD1_CLK			SEP0611_GPIO_SFN


/*
 * PORTE
 */
#define SEP0611_GPE0	SEP0611_GPIONO(PORT_E, 0)
#define JTAG_TDO		SEP0611_GPIO_SFN

#define SEP0611_GPE1	SEP0611_GPIONO(PORT_E, 1)
#define JTAG_TDI		SEP0611_GPIO_SFN

#define SEP0611_GPE2	SEP0611_GPIONO(PORT_E, 2)
#define JTAG_TCK		SEP0611_GPIO_SFN

#define SEP0611_GPE3	SEP0611_GPIONO(PORT_E, 3)
#define JTAG_TMS		SEP0611_GPIO_SFN

#define SEP0611_GPE4	SEP0611_GPIONO(PORT_E, 4)
#define UART3_PWM0		SEP0611_GPIO_SFN
#define NAND_ECC_MODE0	SEP0611_GPIO_SFN2

#define SEP0611_GPE5	SEP0611_GPIONO(PORT_E, 5)
#define UART3_PWM1		SEP0611_GPIO_SFN
#define NAND_ECC_MODE1	SEP0611_GPIO_SFN2

#define SEP0611_GPE6	SEP0611_GPIONO(PORT_E, 6)
#define GPS_SIGN		SEP0611_GPIO_SFN

#define SEP0611_GPE7	SEP0611_GPIONO(PORT_E, 7)
#define GPS_MAG			SEP0611_GPIO_SFN

#define SEP0611_GPE8	SEP0611_GPIONO(PORT_E, 8)
#define SSI2_RXD		SEP0611_GPIO_SFN

#define SEP0611_GPE9	SEP0611_GPIONO(PORT_E, 9)
#define SSI2_TXD		SEP0611_GPIO_SFN

#define SEP0611_GPE10	SEP0611_GPIONO(PORT_E, 10)
#define SSI2_CS			SEP0611_GPIO_SFN

#define SEP0611_GPE11	SEP0611_GPIONO(PORT_E, 11)
#define SSI2_CLK		SEP0611_GPIO_SFN

#define SEP0611_GPE12	SEP0611_GPIONO(PORT_E, 12)
#define SSI1_RXD		SEP0611_GPIO_SFN

#define SEP0611_GPE13	SEP0611_GPIONO(PORT_E, 13)
#define SSI1_TXD		SEP0611_GPIO_SFN

#define SEP0611_GPE14	SEP0611_GPIONO(PORT_E, 14)
#define SSI1_CS			SEP0611_GPIO_SFN

#define SEP0611_GPE15	SEP0611_GPIONO(PORT_E, 15)
#define SSI1_CLK		SEP0611_GPIO_SFN

#define SEP0611_GPE16	SEP0611_GPIONO(PORT_E, 16)
#define I2C3_DAT		SEP0611_GPIO_SFN

#define SEP0611_GPE17	SEP0611_GPIONO(PORT_E, 17)
#define I2C3_CLK		SEP0611_GPIO_SFN

#define SEP0611_GPE18	SEP0611_GPIONO(PORT_E, 18)
#define I2C2_DAT		SEP0611_GPIO_SFN

#define SEP0611_GPE19	SEP0611_GPIONO(PORT_E, 19)
#define I2C2_CLK		SEP0611_GPIO_SFN

#define SEP0611_GPE20	SEP0611_GPIONO(PORT_E, 20)
#define I2C1_DAT		SEP0611_GPIO_SFN

#define SEP0611_GPE21	SEP0611_GPIONO(PORT_E, 21)
#define I2C1_CLK		SEP0611_GPIO_SFN


/*
 * PORTF
 */
#define SEP0611_GPF0	SEP0611_GPIONO(PORT_F, 0)
#define LCD_CONTRAST	SEP0611_GPIO_SFN

#define SEP0611_GPF1	SEP0611_GPIONO(PORT_F, 1)
#define LCD_DEN			SEP0611_GPIO_SFN

#define SEP0611_GPF2	SEP0611_GPIONO(PORT_F, 2)
#define LCD_VSYNC		SEP0611_GPIO_SFN

#define SEP0611_GPF3	SEP0611_GPIONO(PORT_F, 3)
#define LCD_HSYNC		SEP0611_GPIO_SFN

#define SEP0611_GPF4	SEP0611_GPIONO(PORT_F, 4)

#define SEP0611_GPF5	SEP0611_GPIONO(PORT_F, 5)
#define LCD_LD0			SEP0611_GPIO_SFN

#define SEP0611_GPF6	SEP0611_GPIONO(PORT_F, 6)
#define LCD_LD1			SEP0611_GPIO_SFN

#define SEP0611_GPF7	SEP0611_GPIONO(PORT_F, 7)
#define LCD_LD2			SEP0611_GPIO_SFN

#define SEP0611_GPF8	SEP0611_GPIONO(PORT_F, 8)
#define LCD_LD3			SEP0611_GPIO_SFN

#define SEP0611_GPF9	SEP0611_GPIONO(PORT_F, 9)
#define LCD_LD4			SEP0611_GPIO_SFN

#define SEP0611_GPF10	SEP0611_GPIONO(PORT_F, 10)
#define LCD_LD5			SEP0611_GPIO_SFN

#define SEP0611_GPF11	SEP0611_GPIONO(PORT_F, 11)
#define LCD_LD6			SEP0611_GPIO_SFN

#define SEP0611_GPF12	SEP0611_GPIONO(PORT_F, 12)
#define LCD_LD7			SEP0611_GPIO_SFN

#define SEP0611_GPF13	SEP0611_GPIONO(PORT_F, 13)
#define LCD_LD8			SEP0611_GPIO_SFN

#define SEP0611_GPF14	SEP0611_GPIONO(PORT_F, 14)
#define LCD_LD9			SEP0611_GPIO_SFN

#define SEP0611_GPF15	SEP0611_GPIONO(PORT_F, 15)
#define LCD_LD10		SEP0611_GPIO_SFN

#define SEP0611_GPF16	SEP0611_GPIONO(PORT_F, 16)
#define LCD_LD11		SEP0611_GPIO_SFN

#define SEP0611_GPF17	SEP0611_GPIONO(PORT_F, 17)
#define LCD_LD12		SEP0611_GPIO_SFN

#define SEP0611_GPF18	SEP0611_GPIONO(PORT_F, 18)
#define LCD_LD13		SEP0611_GPIO_SFN

#define SEP0611_GPF19	SEP0611_GPIONO(PORT_F, 19)
#define LCD_LD14		SEP0611_GPIO_SFN

#define SEP0611_GPF20	SEP0611_GPIONO(PORT_F, 20)
#define LCD_LD15		SEP0611_GPIO_SFN

#define SEP0611_GPF21	SEP0611_GPIONO(PORT_F, 21)
#define LCD_LD16		SEP0611_GPIO_SFN

#define SEP0611_GPF22	SEP0611_GPIONO(PORT_F, 22)
#define LCD_LD17		SEP0611_GPIO_SFN

#define SEP0611_GPF23	SEP0611_GPIONO(PORT_F, 23)
#define LCD_LD18		SEP0611_GPIO_SFN

#define SEP0611_GPF24	SEP0611_GPIONO(PORT_F, 24)
#define LCD_LD19		SEP0611_GPIO_SFN

#define SEP0611_GPF25	SEP0611_GPIONO(PORT_F, 25)
#define LCD_LD20		SEP0611_GPIO_SFN

#define SEP0611_GPF26	SEP0611_GPIONO(PORT_F, 26)
#define LCD_LD21		SEP0611_GPIO_SFN

#define SEP0611_GPF27	SEP0611_GPIONO(PORT_F, 27)
#define LCD_LD22		SEP0611_GPIO_SFN

#define SEP0611_GPF28	SEP0611_GPIONO(PORT_F, 28)
#define LCD_LD23		SEP0611_GPIO_SFN


/*
 * PORTI
 */
#define SEP0611_GPI0	SEP0611_GPIONO(PORT_I, 0)

#define SEP0611_GPI1	SEP0611_GPIONO(PORT_I, 1)

#define SEP0611_GPI2	SEP0611_GPIONO(PORT_I, 2)

#define SEP0611_GPI3	SEP0611_GPIONO(PORT_I, 3)

#define SEP0611_GPI4	SEP0611_GPIONO(PORT_I, 4)
#define EXTINT4			SEP0611_GPIO_IO

#define SEP0611_GPI5	SEP0611_GPIONO(PORT_I, 5)
#define EXTINT5			SEP0611_GPIO_IO

#define SEP0611_GPI6	SEP0611_GPIONO(PORT_I, 6)
#define EXTINT6			SEP0611_GPIO_IO

#define SEP0611_GPI7	SEP0611_GPIONO(PORT_I, 7)
#define EXTINT7			SEP0611_GPIO_IO

#define SEP0611_GPI8	SEP0611_GPIONO(PORT_I, 8)
#define EXTINT8			SEP0611_GPIO_IO

#define SEP0611_GPI9	SEP0611_GPIONO(PORT_I, 9)
#define EXTINT9			SEP0611_GPIO_IO

#define SEP0611_GPI10	SEP0611_GPIONO(PORT_I, 10)
#define EXTINT10		SEP0611_GPIO_IO

#define SEP0611_GPI11	SEP0611_GPIONO(PORT_I, 11)
#define EXTINT11		SEP0611_GPIO_IO

#define SEP0611_GPI12	SEP0611_GPIONO(PORT_I, 12)
#define EXTINT12		SEP0611_GPIO_IO

#define SEP0611_GPI13	SEP0611_GPIONO(PORT_I, 13)
#define EXTINT13		SEP0611_GPIO_IO

#define SEP0611_GPI14	SEP0611_GPIONO(PORT_I, 14)
#define EXTINT14		SEP0611_GPIO_IO

#define SEP0611_GPI15	SEP0611_GPIONO(PORT_I, 15)
#define EXTINT15		SEP0611_GPIO_IO


/*
 * PORT_AO
 */
#define SEP0611_AO0		SEP0611_GPIONO(PORT_AO, 0)
#define AO_EXTINT0		SEP0611_GPIO_IO

#define SEP0611_AO1		SEP0611_GPIONO(PORT_AO, 1)
#define AO_EXTINT1		SEP0611_GPIO_IO

#define SEP0611_AO2		SEP0611_GPIONO(PORT_AO, 2)
#define AO_EXTINT2		SEP0611_GPIO_IO

#define SEP0611_AO3		SEP0611_GPIONO(PORT_AO, 3)
#define AO_EXTINT3		SEP0611_GPIO_IO

#define SEP0611_AO4		SEP0611_GPIONO(PORT_AO, 4)

#define SEP0611_AO5		SEP0611_GPIONO(PORT_AO, 5)

#define SEP0611_AO6		SEP0611_GPIONO(PORT_AO, 6)

#define SEP0611_AO7		SEP0611_GPIONO(PORT_AO, 7)
#define GPS_CLK			SEP0611_GPIO_SFN


#endif /* __ASM_ARCH_REGS_GPIO_H */
