/* $FreeBSD$ */
/* $NetBSD: dwlpxreg.h,v 1.9 1998/03/21 22:02:42 mjacob Exp $ */

/*
 * Copyright (c) 1997, 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Taken from combinations of:
 *
 *	``DWLPA and DWLPB PCI Adapter Technical Manual,
 *	  Order Number: EK-DWLPX-TM.A01''
 *
 *  and
 *
 *	``AlphaServer 8200/8400 System Technical Manual,
 *	  Order Number EK-T8030-TM. A01''
 */

#define	REGVAL(r)	(*(volatile int32_t *)ALPHA_PHYS_TO_K0SEG(r))

/*
 * There are (potentially) 4 I/O hoses, and there are three
 * (electrically distinct) PCI busses per DWLPX (which appear
 * as one logical PCI bus).
 *
 * A CPU to PCI Address Mapping looks (roughly) like this:
 *
 *  39 38........36 35.34 33.....32 31....................5 4.........3 2...0
 *  --------------------------------------------------------------------------
 *  |1| I/O NodeID |Hose#|PCI Space|Byte Aligned I/O <26:0>|Byte Length|0 0 0|
 *  --------------------------------------------------------------------------
 *
 * I/O Node is the TLSB Node ID minus 4. Don't ask.
 */

#define	NHPC	3

/*
 * Address Space Cookies
 *
 * (lacking I/O Node ID and Hose Numbers)
 */

#define	DWLPX_PCI_DENSE		0x000000000LL
#define	DWLPX_PCI_SPARSE	0x100000000LL
#define	DWLPX_PCI_IOSPACE	0x200000000LL
#define	DWLPX_PCI_CONF		0x300000000LL

/*
 * PCIA Interface Adapter Register Addresses (Offsets from Node Address)
 *
 *
 * Addresses are for Hose #0, PCI bus #0. Macros below will offset
 * per bus. I/O Hose and TLSB Node I/D offsets must be added separately.
 */

#define	_PCIA_CTL	0x380000000LL	/* PCI 0 Bus Control */
#define	_PCIA_MRETRY	0x380000080LL	/* PCI 0 Master Retry Limit */
#define	_PCIA_GPR	0x380000100LL	/* PCI 0 General Purpose */
#define	_PCIA_ERR	0x380000180LL	/* PCI 0 Error Summary */
#define	_PCIA_FADR	0x380000200LL	/* PCI 0 Failing Address */
#define	_PCIA_IMASK	0x380000280LL	/* PCI 0 Interrupt Mask */
#define	_PCIA_DIAG	0x380000300LL	/* PCI 0 Diagnostic  */
#define	_PCIA_IPEND	0x380000380LL	/* PCI 0 Interrupt Pending */
#define	_PCIA_IPROG	0x380000400LL	/* PCI 0 Interrupt in Progress */
#define	_PCIA_WMASK_A	0x380000480LL	/* PCI 0 Window Mask A */
#define	_PCIA_WBASE_A	0x380000500LL	/* PCI 0 Window Base A */
#define	_PCIA_TBASE_A	0x380000580LL	/* PCI 0 Window Translated Base A */
#define	_PCIA_WMASK_B	0x380000600LL	/* PCI 0 Window Mask B */
#define	_PCIA_WBASE_B	0x380000680LL	/* PCI 0 Window Base B */
#define	_PCIA_TBASE_B	0x380000700LL	/* PCI 0 Window Translated Base B */
#define	_PCIA_WMASK_C	0x380000780LL	/* PCI 0 Window Mask C */
#define	_PCIA_WBASE_C	0x380000800LL	/* PCI 0 Window Base C */
#define	_PCIA_TBASE_C	0x380000880LL	/* PCI 0 Window Translated Base C */
#define	_PCIA_ERRVEC	0x380000900LL	/* PCI 0 Error Interrupt Vector */
#define	_PCIA_DEVVEC	0x380001000LL	/* PCI 0 Device Interrupt Vector */


#define	PCIA_CTL(hpc)		(_PCIA_CTL	+ (0x200000 * (hpc)))
#define	PCIA_MRETRY(hpc)	(_PCIA_MRETRY	+ (0x200000 * (hpc)))
#define	PCIA_GPR(hpc)		(_PCIA_GPR	+ (0x200000 * (hpc)))
#define	PCIA_ERR(hpc)		(_PCIA_ERR	+ (0x200000 * (hpc)))
#define	PCIA_FADR(hpc)		(_PCIA_FADR	+ (0x200000 * (hpc)))
#define	PCIA_IMASK(hpc)		(_PCIA_IMASK	+ (0x200000 * (hpc)))
#define	PCIA_DIAG(hpc)		(_PCIA_DIAG	+ (0x200000 * (hpc)))
#define	PCIA_IPEND(hpc)		(_PCIA_IPEND	+ (0x200000 * (hpc)))
#define	PCIA_IPROG(hpc)		(_PCIA_IPROG	+ (0x200000 * (hpc)))
#define	PCIA_WMASK_A(hpc)	(_PCIA_WMASK_A	+ (0x200000 * (hpc)))
#define	PCIA_WBASE_A(hpc)	(_PCIA_WBASE_A	+ (0x200000 * (hpc)))
#define	PCIA_TBASE_A(hpc)	(_PCIA_TBASE_A	+ (0x200000 * (hpc)))
#define	PCIA_WMASK_B(hpc)	(_PCIA_WMASK_B	+ (0x200000 * (hpc)))
#define	PCIA_WBASE_B(hpc)	(_PCIA_WBASE_B	+ (0x200000 * (hpc)))
#define	PCIA_TBASE_B(hpc)	(_PCIA_TBASE_B	+ (0x200000 * (hpc)))
#define	PCIA_WMASK_C(hpc)	(_PCIA_WMASK_C	+ (0x200000 * (hpc)))
#define	PCIA_WBASE_C(hpc)	(_PCIA_WBASE_C	+ (0x200000 * (hpc)))
#define	PCIA_TBASE_C(hpc)	(_PCIA_TBASE_C	+ (0x200000 * (hpc)))
#define	PCIA_ERRVEC(hpc)	(_PCIA_ERRVEC	+ (0x200000 * (hpc)))

#define	PCIA_DEVVEC(hpc, subslot, ipin)	\
 (_PCIA_DEVVEC + (0x200000 * (hpc)) + ((subslot) * 0x200) + ((ipin-1) * 0x80))

#define	PCIA_SCYCLE	0x380002000LL	/* PCI Special Cycle */
#define	PCIA_IACK	0x380002080LL	/* PCI Interrupt Acknowledge */

#define	PCIA_PRESENT	0x380800000LL	/* PCI Slot Present */
#define	PCIA_TBIT	0x380A00000LL	/* PCI TBIT */
#define	PCIA_MCTL	0x380C00000LL	/* PCI Module Control */
#define	PCIA_IBR	0x380E00000LL	/* PCI Information Base Repair */

/*
 * Bits in PCIA_CTL register
 */
#define	PCIA_CTL_SG32K	(0<<25)		/* 32K SGMAP entries */
#define PCIA_CTL_SG64K	(1<<25)		/* 64K SGMAP entries */
#define	PCIA_CTL_SG128K	(3<<25)		/* 128K SGMAP entries */
#define	PCIA_CTL_SG0K	(2<<25)		/* disable SGMAP in HPC */
#define	PCIA_CTL_4UP	(0<<23)		/* 4 Up Hose buffers */
#define	PCIA_CTL_1UP	(1<<23)		/* 1 "" */
#define	PCIA_CTL_2UP	(2<<23)		/* 2 "" */
#define	PCIA_CTL_3UP	(3<<23)		/* 3 "" (normal) */
#define	PCIA_CTL_RMM4X	(1<<22)		/* Read Multiple 2X -> 4X */
#define	PCIA_CTL_RMMENA	(1<<21)		/* Read Multiple Enable */
#define	PCIA_CTL_RMMARB	(1<<20)		/* RMM Multiple Arb */
#define	PCIA_CTL_HAEDIS	(1<<19)		/* Hardware Address Ext. Disable */
#define	PCIA_CTL_MHAE(x) ((x&0x1f)<<14)	/* Memory Hardware Address Extension */
#define	PCIA_CTL_IHAE(x) ((x&0x1f)<<9)	/* I/O Hardware Address Extension */
#define	PCIA_CTL_CUTENA	(1<<8)		/* PCI Cut Through */
#define	PCIA_CTL_CUT(x)	((x&0x7)<<4)	/* PCI Cut Through Size */
#define	PCIA_CTL_PRESET	(1<<3)		/* PCI Reset */
#define	PCIA_CTL_DTHROT	(1<<2)		/* DMA downthrottle */
#define	PCIA_CTL_T1CYC	(1<<0)		/* Type 1 Configuration Cycle */

/*
 * Bits in PCIA_ERR. All are "Write 1 to clear".
 */
#define	PCIA_ERR_SERR_L	(1<<18)		/* PCI device asserted SERR_L */
#define	PCIA_ERR_ILAT	(1<<17)		/* Incremental Latency Exceeded */
#define	PCIA_ERR_SGPRTY	(1<<16)		/* CPU access of SG RAM Parity Error */
#define	PCIA_ERR_ILLCSR	(1<<15)		/* Illegal CSR Address Error */
#define	PCIA_ERR_PCINXM	(1<<14)		/* Nonexistent PCI Address Error */
#define	PCIA_ERR_DSCERR	(1<<13)		/* PCI Target Disconnect Error */
#define	PCIA_ERR_ABRT	(1<<12)		/* PCI Target Abort Error */
#define	PCIA_ERR_WPRTY	(1<<11)		/* PCI Write Parity Error */
#define	PCIA_ERR_DPERR	(1<<10)		/* PCI Data Parity Error */
#define	PCIA_ERR_APERR	(1<<9)		/* PCI Address Parity Error */
#define	PCIA_ERR_DFLT	(1<<8)		/* SG Map RAM Invalid Entry Error */
#define	PCIA_ERR_DPRTY	(1<<7)		/* DMA access of SG RAM Parity Error */ 
#define	PCIA_ERR_DRPERR	(1<<6)		/* DMA Read Return Parity Error */
#define	PCIA_ERR_MABRT	(1<<5)		/* PCI Master Abort Error */
#define	PCIA_ERR_CPRTY	(1<<4)		/* CSR Parity Error */
#define	PCIA_ERR_COVR	(1<<3)		/* CSR Overrun Error */
#define	PCIA_ERR_MBPERR	(1<<2)		/* Mailbox Parity Error */
#define	PCIA_ERR_MBILI	(1<<1)		/* Mailbox Illegal Length Error */
#define	PCIA_ERR_ERROR	(1<<0)		/* Summary Error */
#define	PCIA_ERR_ALLERR	((1<<19) - 1)

/*
 * Bits in PCIA_PRESENT.
 */
#define	PCIA_PRESENT_REVSHIFT	25	/* shift by this to get revision */
#define	PCIA_PRESENT_REVMASK	0xf
#define	PCIA_PRESENT_STDIO	0x01000000 /* STD I/O bridge present */
#define	PCIA_PRESENT_SLOTSHIFT(hpc, slot) \
		(((hpc) << 3) + ((slot) << 1))
#define	PCIA_PRESENT_SLOT_MASK	0x3
#define	PCIA_PRESENT_SLOT_NONE	0x0
#define	PCIA_PRESENT_SLOT_25W	0x1
#define	PCIA_PRESENT_SLOT_15W	0x2
#define	PCIA_PRESENT_SLOW_7W	0x3

/*
 * Location of the DWLPx SGMAP page table SRAM.
 */
#define	PCIA_SGMAP_PT	0x381000000UL

/*
 * Values for PCIA_WMASK_x
 */
#define	PCIA_WMASK_MASK	0xffff0000	/* mask of valid bits */
#define	PCIA_WMASK_64K	0x00000000
#define	PCIA_WMASK_128K	0x00010000
#define	PCIA_WMASK_256K	0x00030000
#define	PCIA_WMASK_512K	0x00070000
#define	PCIA_WMASK_1M	0x000f0000
#define	PCIA_WMASK_2M	0x001f0000
#define	PCIA_WMASK_4M	0x003f0000
#define	PCIA_WMASK_8M	0x007f0000
#define	PCIA_WMASK_16M	0x00ff0000
#define	PCIA_WMASK_32M	0x01ff0000
#define	PCIA_WMASK_64M	0x03ff0000
#define	PCIA_WMASK_128M	0x07ff0000
#define	PCIA_WMASK_256M	0x0fff0000
#define	PCIA_WMASK_512M	0x1fff0000
#define	PCIA_WMASK_1G	0x3fff0000
#define	PCIA_WMASK_2G	0x7fff0000
#define	PCIA_WMASK_4G	0xffff0000

/*
 * Values for PCIA_WBASE_x
 */
#define	PCIA_WBASE_MASK	 0xffff0000	/* mask of valid bits in address */
#define	PCIA_WBASE_W_EN	 0x00000002	/* window enable */
#define	PCIA_WBASE_SG_EN 0x00000001	/* SGMAP enable */

/*
 * Values for PCIA_TBASE_x
 *
 * NOTE: Translated Base is only used on direct-mapped DMA on the DWLPx!!
 */
#define	PCIA_TBASE_MASK	 0x00fffffe
#define	PCIA_TBASE_SHIFT 15
