/*
 * arch/ppc/platforms/ocotea.h
 *
 * Ocotea board definitions
 *
 * Matt Porter <mporter@mvista.com>
 *
 * Copyright 2003 MontaVista Software Inc.
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

#ifdef __KERNEL__
#ifndef __ASM_OCOTEA_H__
#define __ASM_OCOTEA_H__

#include <linux/config.h>
#include <platforms/ibm440gx.h>

/* F/W TLB mapping used in bootloader glue to reset EMAC */
#define PPC44x_EMAC0_MR0	0xE0000800

/* Location of MAC addresses in firmware */
#define OCOTEA_MAC_BASE		(OCOTEA_SMALL_FLASH_HIGH+0xb0500)
#define OCOTEA_MAC_SIZE		0x200
#define OCOTEA_MAC1_OFFSET	0x100

/* Default clock rate */
#define OCOTEA_SYSCLK		25000000

/* RTC/NVRAM location */
#define OCOTEA_RTC_ADDR		0x0000000148000000ULL
#define OCOTEA_RTC_SIZE		0x2000

/* Flash */
#define OCOTEA_FPGA_ADDR		0x0000000148300000ULL
#define OCOTEA_BOOT_LARGE_FLASH(x)	(x & 0x40)
#define OCOTEA_SMALL_FLASH_LOW		0x00000001ff900000ULL
#define OCOTEA_SMALL_FLASH_HIGH		0x00000001fff00000ULL
#define OCOTEA_SMALL_FLASH_SIZE		0x100000
#define OCOTEA_LARGE_FLASH_LOW		0x00000001ff800000ULL
#define OCOTEA_LARGE_FLASH_HIGH		0x00000001ffc00000ULL
#define OCOTEA_LARGE_FLASH_SIZE		0x400000

/*
 * Serial port defines
 */
#define RS_TABLE_SIZE	2

/* OpenBIOS defined UART mappings, used before early_serial_setup */
#define UART0_IO_BASE	(u8 *) 0xE0000200
#define UART1_IO_BASE	(u8 *) 0xE0000300

/* This value is used only to fill something in STD_UART_OP definition,
   actual value is determined in ocotea_early_serial_map.   --ebs
*/
#define BASE_BAUD	11059200/16
#define STD_UART_OP(num)					\
	{ 0, BASE_BAUD, 0, UART##num##_INT,			\
		ASYNC_BOOT_AUTOCONF,				\
		iomem_base: UART##num##_IO_BASE,		\
		io_type: SERIAL_IO_MEM},

#define SERIAL_PORT_DFNS	\
	STD_UART_OP(0)		\
	STD_UART_OP(1)

/* PCI support */
#define OCOTEA_PCI_LOWER_IO	0x00000000
#define OCOTEA_PCI_UPPER_IO	0x0000ffff
#define OCOTEA_PCI_LOWER_MEM	0x80000000
#define OCOTEA_PCI_UPPER_MEM	0xffffefff

#define OCOTEA_PCI_CFGREGS_BASE	0x000000020ec00000ULL
#define OCOTEA_PCI_CFGA_PLB32	0x0ec00000
#define OCOTEA_PCI_CFGD_PLB32	0x0ec00004

#define OCOTEA_PCI_IO_BASE	0x0000000208000000ULL
#define OCOTEA_PCI_IO_SIZE	0x00010000
#define OCOTEA_PCI_MEM_OFFSET	0x00000000

#endif				/* __ASM_OCOTEA_H__ */
#endif				/* __KERNEL__ */
