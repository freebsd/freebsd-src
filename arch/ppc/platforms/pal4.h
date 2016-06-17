/*
 * arch/ppc/platforms/pal4.h
 *
 * Definitions for SBS Palomar IV board
 *
 * Author: Dan Cox
 *
 * 2002 (c) MontaVista, Software, Inc.  This file is licensed under
 * the terms of the GNU General Public License version 2.  This program
 * is licensed "as is" without any warranty of any kind, whether express
 * or implied.
 */

#ifndef __PPC_PLATFORMS_PAL4_H
#define __PPC_PLATFORMS_PAL4_H

#include <asm/io.h>

#define CPC700_MEM_CFGADDR    0xff500008
#define CPC700_MEM_CFGDATA    0xff50000c

#define CPC700_MB0SA            0x38
#define CPC700_MB0EA            0x58
#define CPC700_MB1SA            0x3c
#define CPC700_MB1EA            0x5c
#define CPC700_MB2SA            0x40
#define CPC700_MB2EA            0x60
#define CPC700_MB3SA            0x44
#define CPC700_MB3EA            0x64
#define CPC700_MB4SA            0x48
#define CPC700_MB4EA            0x68

extern inline long
cpc700_read_memreg(int reg)
{
	out_be32((volatile unsigned int *) CPC700_MEM_CFGADDR, reg);
	return in_be32((volatile unsigned int *) CPC700_MEM_CFGDATA);
}


#define PAL4_NVRAM             0xfffc0000
#define PAL4_NVRAM_SIZE        0x8000

#define PAL4_DRAM              0xfff80000
#define  PAL4_DRAM_BR_MASK     0xc0
#define  PAL4_DRAM_BR_SHIFT    6
#define  PAL4_DRAM_RESET       0x10
#define  PAL4_DRAM_EREADY      0x40

#define PAL4_MISC              0xfff80004
#define  PAL4_MISC_FB_MASK     0xc0
#define  PAL4_MISC_FLASH       0x40  /* StratFlash mapping: 1->0xff80, 0->0xfff0 */
#define  PAL4_MISC_MISC        0x08
#define  PAL4_MISC_BITF        0x02
#define  PAL4_MISC_NVKS        0x01

#define PAL4_L2                0xfff80008
#define  PAL4_L2_MASK          0x07

#define PAL4_PLDR              0xfff8000c

/* Only two Ethernet devices on the board... */
#define PAL4_ETH               31
#define PAL4_INTA              20

#endif /* __PPC_PLATFORMS_PAL4_H */
