#ifndef _INCLUDE_FRODO_H_
#define _INCLUDE_FRODO_H_

/*
 * linux/include/asm-arm/arch-sa1100/frodo.h
 *
 * Author: Abraham van der Merwe <abraham@2d3d.co.za>
 *
 * This file contains the hardware specific definitions for 2d3D, Inc.
 * SA-1110 Development Board.
 *
 * Only include this file from SA1100-specific files.
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * History:
 *
 *   2002/07/17   Protect accesses to CPLD memory with a global lock
 *                to prevent races.
 *
 *   2002/06/25   PCMCIA support
 *
 *   2002/06/06   Added Real-Time Clock IRQ
 *                Added IRQs for UARTs
 *
 *   2002/05/20   Added I2C definitions
 *                Updated USB port definitions
 *                Removed scratchpad register
 *                Added definitions for second UART
 *
 *   2002/04/19   Added USB definitions
 *
 *   2002/04/17   Added flow control definitions for UART1
 *
 *   2002/03/14   Added ethernet reset definitions
 *
 *   2002/02/28   Ethernet (cs89x0) support
 *
 *   2002/02/27   IDE support
 *
 *   2002/02/22   Added some CPLD registers to control backlight and
 *                general purpose LEDs
 *
 *   2002/01/31   Initial version
 */

/* CPLD memory */
#define	FRODO_CPLD_BASE				0x40000000
#define FRODO_CPLD_LENGTH			0x00100000

/* CPLD registers */
#define FRODO_CPLD_PCMCIA_COMMAND	0x00000
#define FRODO_CPLD_PCMCIA_STATUS	0x04000
#define FRODO_CPLD_IDE				0x08000
#define FRODO_CPLD_UART1			0x0c000
#define FRODO_CPLD_USB				0x10000
#define FRODO_CPLD_ETHERNET			0x14000
#define FRODO_CPLD_UART2			0x18000
#define FRODO_CPLD_GENERAL			0x04004
#define FRODO_CPLD_I2C				0x08004

/* functions to access those registers */
#ifndef __ASSEMBLY__
#include <linux/types.h>
extern u16 frodo_cpld_read (u32 reg);
extern void frodo_cpld_write (u32 reg,u16 value);
extern void frodo_cpld_set (u32 reg,u16 mask);
extern void frodo_cpld_clear (u32 reg,u16 mask);
#endif	/* #ifndef __ASSEMBLY__ */

/* general command/status register */
#define FRODO_LCD_BACKLIGHT			0x0400		/* R/W */
#define FRODO_LED1					0x0100		/* R/W */
#define FRODO_LED2					0x0200		/* R/W */
#define FRODO_PUSHBUTTON			0x8000		/* R/O */

/* ethernet register */
#define FRODO_ETH_RESET				0x8000		/* R/W */

/* IDE related definitions */
#define FRODO_IDE_GPIO				GPIO_GPIO23
#define FRODO_IDE_IRQ				IRQ_GPIO23
#define FRODO_IDE_CTRL				0xf0038004
#define FRODO_IDE_DATA				0xf0020000

/* Ethernet related definitions */
#define FRODO_ETH_GPIO				GPIO_GPIO20
#define FRODO_ETH_IRQ				IRQ_GPIO20
#define FRODO_ETH_MEMORY			0xf0060000
#define FRODO_ETH_IO				0xf0070000

/* USB device controller */
#define FRODO_USB_DC_GPIO			GPIO_GPIO19
#define FRODO_USB_DC_IRQ			IRQ_GPIO19
#define FRODO_USB_DC_CTRL			0xf0040006
#define FRODO_USB_DC_DATA			0xf0040004

/* USB host controller */
#define FRODO_USB_HC_GPIO			GPIO_GPIO18
#define FRODO_USB_HC_IRQ			IRQ_GPIO18
#define FRODO_USB_HC_CTRL			0xf0040002
#define FRODO_USB_HC_DATA			0xf0040000

/* This UART supports all the funky things */
#define FRODO_UART1_RI				0x0100		/* R/O */
#define FRODO_UART1_DCD				0x0200		/* R/O */
#define FRODO_UART1_CTS				0x0400		/* R/O */
#define FRODO_UART1_DSR				0x0800		/* R/O */
#define FRODO_UART1_DTR				0x2000		/* R/W */
#define FRODO_UART1_RTS				0x4000		/* R/W */
#define FRODO_UART1_IRQEN			0x8000		/* R/W */
#define FRODO_UART1_GPIO			GPIO_GPIO25
#define FRODO_UART1_IRQ				IRQ_GPIO25

/* Console port. Only supports a subset of the control lines */
#define FRODO_UART2_IRQEN			0x0100		/* R/W */
#define FRODO_UART2_CTS				0x1000		/* R/O */
#define FRODO_UART2_RTS				0x8000		/* R/W */
#define FRODO_UART2_GPIO			GPIO_GPIO24
#define FRODO_UART2_IRQ				IRQ_GPIO24

/* USB command register */
#define FRODO_USB_HWAKEUP			0x2000		/* R/W */
#define FRODO_USB_DWAKEUP			0x4000		/* R/W */
#define FRODO_USB_NDPSEL			0x8000		/* R/W */

/* I2C adapter information */
#define FRODO_I2C_SCL_OUT			0x2000		/* R/W */
#define FRODO_I2C_SCL_IN			0x1000		/* R/O */
#define FRODO_I2C_SDA_OUT			0x8000		/* R/W */
#define FRODO_I2C_SDA_IN			0x4000		/* R/O */

/* Real-Time Clock */
#define FRODO_RTC_GPIO				GPIO_GPIO14
#define FRODO_RTC_IRQ				IRQ_GPIO14

/* PCMCIA command register */
#define FRODO_PCMCIA_RESET			0x0004		/* R/W */
#define FRODO_PCMCIA_VCC1			0x0008		/* R/W */
#define FRODO_PCMCIA_VCC0			0x0010		/* R/W */
#define FRODO_PCMCIA_VPP1			0x0020		/* R/W */
#define FRODO_PCMCIA_VPP0			0x0040		/* R/W */
#define FRODO_PCMCIA_CLEAR			0x0080		/* R/W */

/* PCMCIA status register */
#define FRODO_PCMCIA_VS1			0x0001		/* R/O */
#define FRODO_PCMCIA_VS2			0x0002		/* R/O */
#define FRODO_PCMCIA_BVD1			0x0004		/* R/O */
#define FRODO_PCMCIA_BVD2			0x0008		/* R/O */
#define FRODO_PCMCIA_CD1			0x0010		/* R/O */
#define FRODO_PCMCIA_CD2			0x0020		/* R/O */
#define FRODO_PCMCIA_RDYBSY			0x0040		/* R/O */

/* PCMCIA interrupts */
#define FRODO_PCMCIA_STATUS_GPIO	GPIO_GPIO10
#define FRODO_PCMCIA_STATUS_IRQ		IRQ_GPIO10
#define FRODO_PCMCIA_RDYBSY_GPIO	GPIO_GPIO11
#define FRODO_PCMCIA_RDYBSY_IRQ		IRQ_GPIO11

#endif	/* _INCLUDE_FRODO_H_ */
