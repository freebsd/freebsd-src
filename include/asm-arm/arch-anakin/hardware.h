/*
 *  linux/include/asm-arm/arch-anakin/hardware.h
 *
 *  Copyright (C) 2001 Aleph One Ltd. for Acunia N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  Changelog:
 *   10-Apr-2001 TTC	Created
 */

#ifndef __ASM_ARCH_HARDWARE_H
#define __ASM_ARCH_HARDWARE_H

/*
 * Memory map
 */
#define SRAM_START		0x00000000
#define SRAM_SIZE		0x00100000
#define SRAM_BASE		0xdf000000

#define SDRAM_START		0x20000000
#define SDRAM_SIZE		0x04000000
#define SDRAM_BASE		0xc0000000

#define IO_START		0x40000000
#define IO_SIZE			0x00100000
#define IO_BASE			0xe0000000

#define FLASH_START		0x60000000
#define FLASH_SIZE		0x00080000
#define FLASH_BASE		0xe8000000

#define VGA_START		0x80000000
#define VGA_SIZE		0x0002db40
#define VGA_BASE		0xf0000000

/*
 * IO map
 */
#define IO_CONTROLLER		0x00000
#define INTERRUPT_CONTROLLER	0x02000
#define UART0			0x04000
#define UART1			0x06000
#define UART2			0x08000
#define CODEC			0x0a000
#define UART4			0x0c000
#define UART3			0x0e000
#define DISPLAY_CONTROLLER	0x10000
#define DAB			0x12000
#define STATE_CONTROLLER	0x14000
#define CAN			0x23000
#define COMPACTFLASH		0x24000

/*
 * Use SRAM for D-cache flush
 */
#define FLUSH_BASE_PHYS		SRAM_START
#define FLUSH_BASE		SRAM_BASE
#define UNCACHEABLE_ADDR	(SRAM_BASE + 0x10000)

/*
 * Use SDRAM for memory
 */
#define MEM_SIZE		SDRAM_SIZE

#endif
