/* $FreeBSD$ */
/*	$NetBSD: tcreg.h,v 1.1 1995/12/20 00:48:36 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef __DEV_TC_TCREG_H__
#define __DEV_TC_TCREG_H__

/*
 * TurboChannel bus and register definitions.
 */

#define	TC_ROM_LLEN		8
#define	TC_ROM_SLEN		4
#define	TC_ROM_TEST_SIZE	16

#define	TC_SLOT_ROM		0x000003e0
#define	TC_SLOT_PROTOROM	0x003c03e0

typedef struct tc_padchar {
	u_int8_t	v;
	u_int8_t	pad[3];
} tc_padchar_t;

struct tc_rommap {
	tc_padchar_t	tcr_width;
	tc_padchar_t	tcr_stride;
	tc_padchar_t	tcr_rsize;
	tc_padchar_t	tcr_ssize;
	u_int8_t	tcr_test[TC_ROM_TEST_SIZE];
	tc_padchar_t	tcr_rev[TC_ROM_LLEN];
	tc_padchar_t	tcr_vendname[TC_ROM_LLEN];
	tc_padchar_t	tcr_modname[TC_ROM_LLEN];
	tc_padchar_t	tcr_firmtype[TC_ROM_SLEN];
};



/*
 * TurboChannel-specific functions and structures for 3000_300.
 */
#define	TC_3000_300_IR			KV(0x00000001e0000000)	/* Dense */
#define	TC_3000_300_CSR			KV(0x00000001e0000008)	/* Dense */
#define	TC_3000_300_MCR			KV(0x00000001e0000010)	/* Dense */
#define	TC_3000_300_LED			KV(0x00000001e0000018)	/* Dense */

/* Interrupt bits. */
#define	TC_3000_300_IR_CXTURBO		0x00000004	/* TC CXTURBO */
#define	TC_3000_300_IR_TCDS		0x00000008	/* TC Dual SCSI */
#define	TC_3000_300_IR_IOASIC		0x00000010	/* TC IOASIC */
#define	TC_3000_300_IR_BCTAGPARITY	0x08000000	/* BC tag par. err. */
#define	TC_3000_300_IR_TCOVERRUN	0x10000000	/* TC overrun */
#define	TC_3000_300_IR_TCTIMEOUT	0x20000000	/* TC timeout on I/O */
#define	TC_3000_300_IR_BCACHEPARITY	0x40000000	/* Bcache par. err. */
#define	TC_3000_300_IR_MEMPARITY	0x80000000	/* Memory par. err. */

/* Device number "cookies." */
#define	TC_3000_300_DEV_OPT0	0
#define	TC_3000_300_DEV_OPT1	1
#define	TC_3000_300_DEV_TCDS	2
#define	TC_3000_300_DEV_IOASIC	3
#define	TC_3000_300_DEV_CXTURBO	4

#define TC_3000_300_DEV_BOGUS	-1

#define	TC_3000_300_NCOOKIES	5

#define TC_3000_500_IOSLOT              KV(0x00000001c2000000)  /* Dense */
#define TC_3000_500_TCCONFIG            KV(0x00000001c2000008)  /* Dense */
#define TC_3000_500_FADR                KV(0x00000001c2000010)  /* Dense */
#define TC_3000_500_TCEREG              KV(0x00000001c2000018)  /* Dense */
#define TC_3000_500_MEMCONF             KV(0x00000001c2200000)  /* Dense */
#define TC_3000_500_IMR_READ            KV(0x00000001c2400000)  /* Dense */
#define TC_3000_500_IMR_WRITE           KV(0x00000001c281fffc)  /* Dense */
#define TC_3000_500_TCRESET             KV(0x00000001c2a00000)  /* Dense */
#define TC_3000_500_IR                  KV(0x00000001d4800000)  /* Sparse */
#define TC_3000_500_IR_CLEAR            KV(0x00000001d4c00000)  /* Sparse */
#define TC_3000_500_SCMAP               KV(0x00000001d5000000)  /* Sparse */

/* Interrupt bits. */
#define TC_3000_500_IR_OPT0             0x00000001      /* TC Option 0 */
#define TC_3000_500_IR_OPT1             0x00000002      /* TC Option 1 */
#define TC_3000_500_IR_OPT2             0x00000004      /* TC Option 2 */
#define TC_3000_500_IR_OPT3             0x00000008      /* TC Option 3 */
#define TC_3000_500_IR_OPT4             0x00000010      /* TC Option 4 */
#define TC_3000_500_IR_OPT5             0x00000020      /* TC Option 5 */
#define TC_3000_500_IR_TCDS             0x00000040      /* TC Dual SCSI */
#define TC_3000_500_IR_IOASIC           0x00000080      /* TC IOASIC */
#define TC_3000_500_IR_CXTURBO          0x00000100      /* TC CXTURBO */
#define TC_3000_500_IR_ERR2             0x00080000      /* Second error */
#define TC_3000_500_IR_DMABE            0x00100000      /* DMA buffer error */
#define TC_3000_500_IR_DMA2K            0x00200000      /* DMA 2K boundary */
#define TC_3000_500_IR_TCRESET          0x00400000      /* TC reset in prog. */
#define TC_3000_500_IR_TCPAR            0x00800000      /* TC parity error */
#define TC_3000_500_IR_DMATAG           0x01000000      /* DMA tag error */
#define TC_3000_500_IR_DMASBE           0x02000000      /* Single-bit error */
#define TC_3000_500_IR_DMADBE           0x04000000      /* Double-bit error */
#define TC_3000_500_IR_TCTIMEOUT        0x08000000      /* TC timeout on I/O */
#define TC_3000_500_IR_DMABLOCK         0x10000000      /* DMA block too long */
#define TC_3000_500_IR_IOADDR           0x20000000      /* Invalid I/O addr */
#define TC_3000_500_IR_DMASG            0x40000000      /* SG invalid */
#define TC_3000_500_IR_SGPAR            0x80000000      /* SG parity error */

/* I/O Slot Configuration (IOSLOT) bits. */
#define IOSLOT_P                0x04    /* Parity enable. */
#define IOSLOT_B                0x02    /* Block-mode write. */
#define IOSLOT_S                0x01    /* DMA scatter/gather mode. */

/* I/O Slot Configuration (IOSLOT) offsets. */
#define TC_IOSLOT_OPT0          0       /* Option 0 PBS offset. */
#define TC_IOSLOT_OPT1          1       /* Option 1 PBS offset. */
#define TC_IOSLOT_OPT2          2       /* Option 2 PBS offset. */
#define TC_IOSLOT_OPT3          3       /* Option 3 PBS offset. */
#define TC_IOSLOT_OPT4          4       /* Option 4 PBS offset. */
#define TC_IOSLOT_OPT5          5       /* Option 5 PBS offset. */
#define TC_IOSLOT_SCSI          6       /* Option SCSI PBS offset. */
#define TC_IOSLOT_IOASIC        7       /* Option IOASIC PBS offset. */
#define TC_IOSLOT_CXTURBO       8       /* Option CXTURBO PBS offset. */

/* Device number "cookies." */
#define TC_3000_500_DEV_OPT0    0
#define TC_3000_500_DEV_OPT1    1
#define TC_3000_500_DEV_OPT2    2
#define TC_3000_500_DEV_OPT3    3
#define TC_3000_500_DEV_OPT4    4
#define TC_3000_500_DEV_OPT5    5
#define TC_3000_500_DEV_TCDS    6
#define TC_3000_500_DEV_IOASIC  7
#define TC_3000_500_DEV_CXTURBO 8

#define TC_3000_500_DEV_BOGUS   -1

#define TC_3000_500_NCOOKIES    9



#endif /* __DEV_TC_TCREG_H__ */
