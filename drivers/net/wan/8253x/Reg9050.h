/* -*- linux-c -*- */
#ifndef __REG9050_H
#define __REG9050_H

/*******************************************************************************
 * Copyright (c) 2001 PLX Technology, Inc.
 * 
 * PLX Technology Inc. licenses this software under specific terms and
 * conditions.  Use of any of the software or derviatives thereof in any
 * product without a PLX Technology chip is strictly prohibited. 
 * 
 * PLX Technology, Inc. provides this software AS IS, WITHOUT ANY WARRANTY,
 * EXPRESS OR IMPLIED, INCLUDING, WITHOUT LIMITATION, ANY WARRANTY OF
 * MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.  PLX makes no guarantee
 * or representations regarding the use of, or the results of the use of,
 * the software and documentation in terms of correctness, accuracy,
 * reliability, currentness, or otherwise; and you rely on the software,
 * documentation and results solely at your own risk.
 * 
 * IN NO EVENT SHALL PLX BE LIABLE FOR ANY LOSS OF USE, LOSS OF BUSINESS,
 * LOSS OF PROFITS, INDIRECT, INCIDENTAL, SPECIAL OR CONSEQUENTIAL DAMAGES
 * OF ANY KIND.  IN NO EVENT SHALL PLX'S TOTAL LIABILITY EXCEED THE SUM
 * PAID TO PLX FOR THE PRODUCT LICENSED HEREUNDER.
 * 
 ******************************************************************************/

/* Modifications and extensions
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 **/


/******************************************************************************
 * 
 * File Name:
 *
 *      Reg9050.h
 *
 * Description:
 *
 *      This file defines all the PLX 9050 chip Registers.
 *
 * Revision:
 *
 *      01-30-01 : PCI SDK v3.20
 *
 ******************************************************************************/


#include "PciRegs.h"


#ifdef __cplusplus
extern "C" {
#endif


/* PCI Configuration Registers */
#define PCI9050_VENDOR_ID            CFG_VENDOR_ID
#define PCI9050_COMMAND              CFG_COMMAND
#define PCI9050_REV_ID               CFG_REV_ID
#define PCI9050_CACHE_SIZE           CFG_CACHE_SIZE
#define PCI9050_PCI_BASE_0           CFG_BAR0
#define PCI9050_PCI_BASE_1           CFG_BAR1
#define PCI9050_PCI_BASE_2           CFG_BAR2
#define PCI9050_PCI_BASE_3           CFG_BAR3
#define PCI9050_PCI_BASE_4           CFG_BAR4
#define PCI9050_PCI_BASE_5           CFG_BAR5
#define PCI9050_CIS_PTR              CFG_CIS_PTR
#define PCI9050_SUB_ID               CFG_SUB_VENDOR_ID
#define PCI9050_PCI_BASE_EXP_ROM     CFG_EXP_ROM_BASE
#define PCI9050_CAP_PTR              CFG_CAP_PTR
#define PCI9050_PCI_RESERVED         CFG_RESERVED2
#define PCI9050_INT_LINE             CFG_INT_LINE


#if 0				/* from PLX header file */
/* Local Configuration Registers */
#define PCI9050_RANGE_SPACE0         0x000
#define PCI9050_RANGE_SPACE1         0x004
#define PCI9050_RANGE_SPACE2         0x008
#define PCI9050_RANGE_SPACE3         0x00C
#define PCI9050_RANGE_EXP_ROM        0x010
#define PCI9050_REMAP_SPACE0         0x014
#define PCI9050_REMAP_SPACE1         0x018
#define PCI9050_REMAP_SPACE2         0x01C
#define PCI9050_REMAP_SPACE3         0x020
#define PCI9050_REMAP_EXP_ROM        0x024
#define PCI9050_DESC_SPACE0          0x028
#define PCI9050_DESC_SPACE1          0x02C
#define PCI9050_DESC_SPACE2          0x030
#define PCI9050_DESC_SPACE3          0x034
#define PCI9050_DESC_EXP_ROM         0x038
#define PCI9050_BASE_CS0             0x03C
#define PCI9050_BASE_CS1             0x040
#define PCI9050_BASE_CS2             0x044
#define PCI9050_BASE_CS3             0x048
#define PCI9050_INT_CTRL_STAT        0x04C
#define PCI9050_EEPROM_CTRL          0x050

#endif


/* Additional register defintions */
#define MAX_PCI9050_REG_OFFSET       0x054

#define PCI9050_EEPROM_SIZE          0x064

/*
 * PLX 9050 registers:
 */

typedef struct plx9050_s 
{
	/* Local Address Space */
	unsigned int	las0;		/* 0x00 */
	unsigned int	las1;		/* 0x04 */
	unsigned int	las2;		/* 0x08 */
	unsigned int	las3;		/* 0x0c */
	/* Expansion ROM range */
	unsigned int	e_rom;		/* 0x10 */
	/* Local Base Adresses */
	unsigned int   lba0;		/* 0x14 */
	unsigned int   lba1;		/* 0x18 */
	unsigned int   lba2;		/* 0x1c */
	unsigned int   lba3;		/* 0x20 */
	unsigned int	e_rom_lba;	/* 0x24 */
	/* Bus region description */
	unsigned int	brd0;		/* 0x28 */
	unsigned int	brd1;		/* 0x2c */
	unsigned int	brd2;		/* 0x30 */
	unsigned int	brd3;		/* 0x34 */
	unsigned int	e_rom_brd;	/* 0x38 */
	/* Chip Select X Base address */
	unsigned int	csba0;		/* 0x3c */
	unsigned int	csba1;		/* 0x40 */
	unsigned int	csba2;		/* 0x44 */
	unsigned int	csba3;		/* 0x48 */
	/* Interupt Control/Status */
	unsigned int   intr;		/* 0x4c */
	/* Control */
	unsigned int	ctrl;		/* 0x50 */
} plx9050_t, PLX9050;

#define PLX_REG_LAS0RR	0x0
#define PLX_REG_LAS1RR	0x4
#define PLX_REG_LAS2RR	0x8
#define PLX_REG_LAS3RR	0x0c
#define PLX_REG_EROMRR	0x10
#define PLX_REG_LAS0BA	0x14
#define PLX_REG_LAS1BA	0x18
#define PLX_REG_LAS2BA	0x1c
#define PLX_REG_LAS3BA	0x20
#define PLX_REG_EROMBA	0x24
#define PLX_REG_LAS0BRD	0x28
#define PLX_REG_LAS1BRD	0x2c
#define PLX_REG_LAS2BRD	0x30
#define PLX_REG_LAS3BRD	0x34
#define PLX_REG_EROMBRD	0x38
#define PLX_REG_CS0BASE	0x3c
#define PLX_REG_CS1BASE	0x40
#define PLX_REG_CS2BASE	0x44
#define PLX_REG_CS3BASE	0x48
#define PLX_REG_INTCSR	0x4c
#define PLX_REG_CTRL	0x50 

/*
 * Bits within those registers:
 */
/* LAS0    */
#define PLX_LAS0_MEM_MASK	0x0ffffff0

/* INTCSR: */
#define PLX_INT_INTR1ENA	0x00000001   /* Local interrupt 1 enable */
#define PLX_INT_INTR1POL	0x00000002   /* Local interrupt 1 polarity */
#define PLX_INT_INTR1STS	0x00000004   /* Local interrupt 1 status */
#define PLX_INT_INTR2ENA	0x00000008   /* Local interrupt 2 enable */
#define PLX_INT_INTR2POL	0x00000010   /* Local interrupt 2 polarity */
#define PLX_INT_INTR2STS	0x00000020   /* Local interrupt 2 status */
#define PLX_INT_PCIINTRENA	0x00000040   /* PCI interrupt enable */
#define PLX_INT_SOFTINTR	0x00000080   /* Software interrupt */
#if 0
#define PLX_INT_ON	        (PLX_INT_INTR1ENA | PLX_INT_PCIINTRENA | PLX_INT_INTR2POL)
#define PLX_INT_OFF	        PLX_INT_INTR2POL
#elif 0
#define PLX_INT_ON	        PLX_INT_PCIINTRENA
#define PLX_INT_OFF	        0
#else
#define PLX_INT_ON	(PLX_INT_INTR1ENA | PLX_INT_PCIINTRENA)
#define PLX_INT_OFF	0x0000
#endif

/* BRD */
#define PLX_BRD_BIGEND          0x01000000
#define PLX_BRD_BIGEND_LANE     0x02000000


     /* CTRL: */	      /*  87654321 */
#define PLX_CTRL_RESET		0x40000000
#define PLX_CTRL_USERIO3DIR	0x00000400
#define PLX_CTRL_USERIO3DATA	0x00000800
#define PLX_CTRL_SEPCLK		0x01000000
#define PLX_CTRL_SEPCS		0x02000000
#define PLX_CTRL_SEPWD		0x04000000
#define PLX_CTRL_SEPRD		0x08000000

/* Definition for the EPROM */
	/* # of addressing bits for NM93CS06, NM93CS46 */
#define NM93_ADDRBITS	6
#define NM93_WENCMD	((u8) 0x00)
#define NM93_WRITECMD	((u8) 0x01)
#define NM93_READCMD	((u8) 0x02)
#define NM93_WRALLCMD	((u8) 0x00) /* same as WEN */
#define NM93_WDSCMD	((u8) 0x00) /* ditto */

#define NM93_WDSADDR	((u8) 0x00)
#define NM93_WRALLADDR	((u8) 0x10)
#define NM93_WENADDR	((u8) 0x30)

#define NM93_BITS_PER_BYTE	8
#define NM93_BITS_PER_WORD	16

#define EPROMPREFETCHOFFSET 	9
#define PREFETCHBIT		0x0008

#define EPROM9050_SIZE			0x40 /* for loading the 9050 */

#define AURORA_MULTI_EPROM_SIZE		0x40	/* serial EPROM size */
/* serial EPROM offsets */
#define AURORA_MULTI_EPROM_CLKLSW	0x36	/* clock speed LSW */
#define AURORA_MULTI_EPROM_CLKMSW	0x37	/* clock speed MSW */
#define AURORA_MULTI_EPROM_REV		0x38	/* revision begins here */
#define AURORA_MULTI_EPROM_REVLEN	0x0f	/* length (in bytes) */
#define AURORA_MULTI_EPROM_SPDGRD	0x3f	/* speed grade is here */

#ifdef __cplusplus
}
#endif

#endif
