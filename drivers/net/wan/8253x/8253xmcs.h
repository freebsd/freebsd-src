/* -*- linux-c -*- */
/* 
 * Copyright (C) 2001 By Joachim Martillo, Telford Tools, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 *
 **/

#ifndef _8253XMCS_H_
#define _8253XMCS_H_

#include "8253xctl.h"

/* structures for the multi channel server, the host card, GLINK and
 * extensions cards.  This system uses the AMCC S5920 instead of the
 * PLX 9050 */

/* ------------------------------------------------------------------------- */
/* Useful macros */

/*
 * NOTICE: pciat_identify: pci125c,102 unit 1
 * NOTICE: pciat_probe: pci125c,102 unit 1
 * NOTICE: pciat_attach: pci125c,102 instance 1
 * NOTICE: Reg End Size     Pointer  AccHandle
 * NOTICE:   0 --  00000000 5093a000 5033f160
 * NOTICE:   0 be  00000000 5093c000 5033f128
 * NOTICE:   0 le  00000000 50940000 5033f0f0
 * NOTICE:   1 --  00000080 50942000 5033f0b8
 * NOTICE:   1 be  00000080 50944000 5033f080
 * NOTICE:   1 le  00000080 50946000 5033f048
 * NOTICE:   2 --  00004000 50948000 5033f010
 * NOTICE:   2 be  00004000 5094c000 5033efd8
 * NOTICE:   2 le  00004000 50950000 5033efa0
 * NOTICE:   3 --  00008000 50954000 5033ef68
 * NOTICE:   3 be  00008000 5095c000 5033ef30
 * NOTICE:   3 le  00008000 50964000 5033eef8
 * NOTICE:   4 --  00000800 5096c000 5033eec0
 * NOTICE:   4 be  00000800 5096e000 5033ee88
 * NOTICE:   4 le  00000800 50970000 5033ee50
 * NOTICE: pciat_attach: pci125c,102 1: PCI reg property
 * NOTICE: Idx Bus Dev Fun Reg Spc        Addr              Size
 * NOTICE:   0 000 004 000 000 CFG 00000000 00000000 00000000 00000000
 * NOTICE:   1 000 004 000 010 MEM 00000000 00000000 00000000 00000080
 * NOTICE:   2 000 004 000 014 MEM 00000000 00000000 00000000 00004000
 * NOTICE:   3 000 004 000 018 MEM 00000000 00000000 00000000 00008000
 * NOTICE:   4 000 004 000 01c MEM 00000000 00000000 00000000 00000800
 * PCI-device: pci125c,102@4, pciat #1
 */

/*
 * Serial EPROM information:
 *
 *  + chip speed grade		[ one byte ]
 *  + chip oscillator speed	[ 4 bytes ]
 *  + board revision		[ ascii string ]
 *  + date of manufacture	[ ascii string ]
 *  + location of manufacture	[ ascii string ]
 *  + serial number		[ ascii string ]
 *  + prototype/production flag	[ one bit ]
 *  + sync/async license	[ one bit ]
 *  + CIM type			[ one byte ]
 *  + assembly house		[ ascii string ]
 */

/*
 * Serial EPROM map.
 */

#define MCS_SEP_TYPE		0x00
#define MCS_SEP_FLAGS		0x01
#define MCS_SEP_SPDGRD		0x02
#define MCS_SEP_MAGIC		0x03
#define MCS_SEP_CLKSPD		0x04
#define MCS_SEP_SN		0x10
#define MCS_SEP_SNLEN		0x10
#define MCS_SEP_REV		0x20
#define MCS_SEP_REVLEN		0x10
#define MCS_SEP_MFGLOC		0x30
#define MCS_SEP_MFGLOCLEN	0x10
#define MCS_SEP_MFGDATE		0x40
#define MCS_SEP_MFGDATELEN	0x20

#define MCS_SEP_MAGICVAL	0x65

/* Host NVRAM DEFINES */

#define AMCC_NVR_VENDEVID	0x10 /* offset in 32bit quantities */

/*
 * PCI spaces on the CIM.
 */
#if 0				/* Solaris driver stuff */
#define AMCC_REG		1
#define CIMCMD_REG		2
#define MICCMD_REG		3
#define FIFOCACHE_REG		4
#else
#define AMCC_REG		virtbaseaddress0 /* bridge */
#define CIMCMD_REG		virtbaseaddress1
#define MICCMD_REG		virtbaseaddress2
#define FIFOCACHE_REG		virtbaseaddress3
#endif

/*
 * AMCC registers:
 */

#define AMCC_OMB		0x0c	/* 4 bytes */
#define AMCC_IMB		0x1c	/* 4 bytes */
#define AMCC_MBEF		0x34	/* 4 bytes */
#define AMCC_INTCSR		0x38	/* 4 bytes */
#define	AMCC_INTASSERT		0x00800000	/* RO */
#define	AMCC_AOINTPIN		0x00400000	/* RO */
#define	AMCC_IMINT		0x00020000	/* R/WC */
#define	AMCC_OMINT		0x00010000	/* R/WC */
#define	AMCC_AOINTPINENA	0x00002000	/* R/W */
#define AMCC_RCR		0x3c	/* 4 bytes */
#define	AMCC_NVRACCCTRLMASK	0xe0000000	/* nvRAM Acc. Ctrl */
#define	AMCC_NVRACCFAIL		0x10000000	/* RO */
#define AMCC_NVRBUSY		0x80000000
#define AMCC_NVRWRLA		0x80000000
#define AMCC_NVRWRHA		0xa0000000
#define AMCC_NVRRDDB		0xe0000000
#define	AMCC_NVROPPMASK		0x000f0000	/* R/W */
#define	AMCC_MBXFLGRESET	0x08000000	/* WO */
#define	AMCC_RDFIFORESET	0x02000000	/* WO */
#define	AMCC_AORESET		0x01000000	/* R/W */
#define AMCC_PTCR		0x60	/* 4 bytes */
#define	AMCC_AMWTSTATEMASK	0x07
#define	AMCC_PREFETCHMASK	0x18
#define	AMCC_WRFIFODIS		0x20
#define	AMCC_ENDCONV		0x40
#define	AMCC_PTMODE		0x80

#define AMCC_SIZE		0x80	/* space size, in bytes */
#define AMCC_NVRAM_SIZE	        0x40 /* in shorts just to be consistent with
				      * other eprom and nvram sizes*/

/*
 * CIM Command space	0x0000 - 0x3fff
 */

#define CIMCMD_CHANSHIFT	6	/* shift channel# to the left */
#define CIMCMD_CHANMASK		0x3f	/* 6 bits of mask */
#define CIMCMD_CIMSHIFT		10	/* shift cim# to the left */
#define CIMCMD_CIMMASK		0x3	/* 2 bits of mask */
#define CIMCMD_CTRLSHIFT	1	/* shift control address to the left */
#define CIMCMD_CTRLMASK		0x7	/* 3 bits of mask */
#define CIMCMD_CHIPSHIFT	9

#define CIMCMD_RESET		0x0000
#define CIMCMD_RDINT		0x0002
#define CIMCMD_RDINT_ESCCMASK	0x00ff
#define CIMCMD_WRINT		0x0003
#define CIMCMD_WRINTENA		0x0004
#define CIMCMD_WRINTDIS		0x0006
#define CIMCMD_RESETENA		0x0007	/* assert reset */
#define CIMCMD_RESETDIS		0x0000	/* deassert the reset */
#define CIMCMD_RDFIFOW		0x1000	/* add channel# */
#define CIMCMD_WRFIFOB		0x2002	/* add channel# */
#define CIMCMD_WRFIFOW		0x2000	/* add channel# */
#define CIMCMD_RDREGB		0x1000	/* add channel# and reg# (>= 0x20) */
#define CIMCMD_WRREGB		0x2000	/* add channel# and reg# (>= 0x20) */
#define CIMCMD_RDSETUP		0x3200	/* add cim# and address (word acc) */
#define CIMCMD_WRSETUP		0x3220	/* add cim# and address (word acc) */
#define CIMCMD_RDCIMCSR		0x3000	/* add cim# */
#define CIMCMD_CIMCSR_LED	0x01
#define CIMCMD_CIMCSR_SWI	0x02
#define CIMCMD_CIMCSR_SDA	0x04
#define CIMCMD_CIMCSR_SCL	0x08
#define CIMCMD_CIMCSR_TESTMASK	0xc0
#define CIMCMD_WRCIMCSR		0x3020	/* add cim# */

#define CIMCMD_SIZE		0x4000	/* space size, in bytes */

/*
 * MIC Command space	0x0000 - 0x5fc0
 */

#define MICCMD_CHANSHIFT	6	/* shift channel# to the left */
#define MICCMD_CHANMASK		0x3f	/* 6 bits of mask */

#define MICCMD_MICCSR		0x0000	/* R/W (byte) */
#define MICCMD_MICCSR_END	0x80
#define MICCMD_MICCSR_ENL	0x40
#define MICCMD_MICCSR_LPN	0x20
#define MICCMD_MICCSR_DGM	0x10
#define MICCMD_MICCSR_CPY	0x08
#define MICCMD_MICCSR_GLE	0x04
#define MICCMD_MICCSR_RXE	0x02
#define MICCMD_MICCSR_IRQ	0x01
#define MICCMD_REV		0x0001	/* RO (byte) */
#define MICCMD_CACHETRIG	0x5000	/* WO (byte: #words-1) add channel# */

#define MICCMD_SIZE		0x8000	/* space size, in bytes */

/*
 * FIFO Cache space	0x000 - 0x7ff
 */

#define FIFOCACHE_CHANSHIFT	5	/* shift channel# to the left */
#define FIFOCACHE_CHANMASK	0x3f	/* 6 bits of mask */

#define FIFOCACHE_FIFOCACHE	0x000	/* add channel# and word offset */

#define FIFOCACHE_SIZE		0x800	/* space size, in bytes */

/*
 * Other miscellaneous constants
 */

#define MAX_NCIMS		4	/* maximum of 4 CIMS */
#define CIM_NPORTS		16	/* 16 ports per CIM */
#define CIM_NCHIPS		2	/* 2 ESCC8s/CIM */
#define CHIP_NPORTS		8	/* 8 ports per chip */

#define WANMCS_CLKSPEED	7372800	/* 7.3728 MHz */


/* PCR/PVR (Universal Port) */

/*
 * To summarize the use of the parallel port:
 *                    RS-232
 * Parallel port A -- TxClkdir control	(output) ports 0 - 7
 * Parallel port B -- DTR		(output) ports 0 - 7
 * Parallel port C -- DSR		(input)  ports 0 - 7
 * Parallel port D -- unused
 */

#define WANMCS_PCRAVAL		0x00	/* all output bits */
#define WANMCS_PCRBVAL		0x00	/* all output bits */
#define WANMCS_PCRCVAL		0xff	/* all input bits */
#define WANMCS_PCRDVAL		0x0f	/* 4 input bits */

#define WANMCS_PIMAVAL		0xff	/* all interrupts off */
#define WANMCS_PIMBVAL		0xff	/* all interrupts off */
#define WANMCS_PIMCVAL		0xff	/* all interrupts off */
#define WANMCS_PIMDVAL		0x0f	/* all interrupts off */

#define WANMCS_PVRAVAL		0xff	/* all high */
#define WANMCS_PVRBVAL		0xff	/* all high */

#define ANY_BITS_ARE_ON(x, b)	(((x) & (b)) != 0)
#define ANY_BITS_ARE_OFF(x, b)	((((x) & (b)) ^ (b)) != 0)
#define ALL_BITS_ARE_ON(x, b)	((((x) & (b)) ^ (b)) == 0)

/* ------------------------------------------------------------------------- */
/* New types and type specific macros */

typedef struct _mcs_sep 
{
#if 0
	ddi_acc_handle_t	 s_handle; /* something from Solaris */
#endif
	unsigned char		*s_rdptr;
	unsigned char		*s_wrptr;
	unsigned int		 s_scl;
	unsigned int		 s_sda;
} mcs_sep_t;

/*
 * Per-line private information for wanmcs.
 */

typedef struct _wanmcspriv 
{
	unsigned char		 r_chipunit;	/* [0, 1] or [0, 7] */
	
	/* these items are for accessing the ESCCx registers as bytes */
#if 0
	ddi_acc_handle_t	 r_reghandle;	/* handle for access to ESCCx regs */
#endif
	unsigned char		*r_rdregbase;	/* base for reading ESCCx registers */
	unsigned char		*r_wrregbase;	/* base for writing ESCCx registers */
	
	/* these items are for accessing the ESCCx FIFOs as bytes and words */
#if 0
	ddi_acc_handle_t	 r_fifohandle;
#endif
	unsigned short	*r_rdfifow;	/* read FIFO word */
	unsigned char		*r_wrfifob;	/* write FIFO byte */
	unsigned short	*r_wrfifow;	/* write FIFO word */
	
	/* these items are for accessing the MIC command space */
#if 0
	ddi_acc_handle_t	 r_miccmdhandle;
#endif
	unsigned char		*r_wrcachetrig;	/* the FIFO cache trigger */
	
	/* these itmes are for accessing the FIFO cache space */
#if 0
	ddi_acc_handle_t r_fifocachehandle;
#endif
	unsigned short *r_fifocachebase;    
} wanmcspriv_t;

#define AMCC_INT_OFF 0

extern unsigned int 
amcc_read_nvram(unsigned char* buffer, 
		unsigned length, 
		unsigned char *bridge_space);

extern unsigned int mcs_ciminit(SAB_BOARD *bptr, AURA_CIM *cim);

extern int wanmcs_reset(SAB_BOARD* bptr);

#endif
