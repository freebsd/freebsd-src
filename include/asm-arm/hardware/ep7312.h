/*
 *  linux/include/asm-arm/hardware/ep7312.h
 *
 *  This file contains the hardware definitions of the EP7312 internal
 *  registers.
 *
 *  Copyright (C) 2003 Roland A.I. Rosier <roland.rosier@lycos.co.uk>.
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 */
#ifndef __ASM_HARDWARE_EP7312_H
#define __ASM_HARDWARE_EP7312_H

#include <linux/config.h>

/*
 * Include CLPS7111 and EP7212 register definitions
 */

#include <asm/hardware/clps7111.h>
#include <asm/hardware/ep7212.h>

/*
 * define EP7312_BASE to be the base address of the region
 * you want to access.
 */

#define EP7312_PHYS_BASE        (0x80000000)

#define SDCONF   0x2300	/* SDRAM Configuration register */
#define SDRFPR   0x2340	/* SDRAM Refresh period register */
#define UNIQID   0x2440	/* 32 bit unique ID for the EP73xx device */
#define DAI64FS  0x2600	/* DAI 64Fs Control Register */
#define PLLW     0x2610	/* Write Register for PLL Multiplier */
#define PLLR     0xA5A8	/* Read Register for PLL Multiplier */
#define RANDID0	 0x2700	/* Bits 31-0 of 128-bit random ID */
#define RANDID1	 0x2704	/* Bits 63-32 of 128-bit random ID */
#define RANDID2	 0x2708	/* Bits 95-64 of 128-bit random ID */
#define RANDID3	 0x270C	/* Bits 127-96 of 128-bit random ID */

#define SDCONF_ACTIVE           (1 << 10)
#define SDCONF_CLKCTL           (1 << 9)
#define SDCONF_WIDTH_4          (0 << 7)
#define SDCONF_WIDTH_8          (1 << 7)
#define SDCONF_WIDTH_16         (2 << 7)
#define SDCONF_WIDTH_32         (3 << 7)
#define SDCONF_SIZE_16          (0 << 5)
#define SDCONF_SIZE_64          (1 << 5)
#define SDCONF_SIZE_128         (2 << 5)
#define SDCONF_SIZE_256         (3 << 5)
#define SDCONF_CASLAT_2         (2)
#define SDCONF_CASLAT_3         (3)

#endif /* __ASM_HARDWARE_EP7312_H */
