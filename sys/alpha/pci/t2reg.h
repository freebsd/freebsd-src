/*
 * Copyright (c) 2000, 2001 Doug Rabson & Andrew Gallatin 
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Portions of this file were obtained from Compaq intellectual
 * property which was made available under the following copyright:
 *
 * *****************************************************************
 * *                                                               *
 * *    Copyright Compaq Computer Corporation, 2000                *
 * *                                                               *
 * *   Permission to use, copy, modify, distribute, and sell       *
 * *   this software and its documentation for any purpose is      *
 * *   hereby granted without fee, provided that the above         *
 * *   copyright notice appear in all copies and that both         *
 * *   that copyright notice and this permission notice appear     *
 * *   in supporting documentation, and that the name of           *
 * *   Compaq Computer Corporation not be used in advertising      *
 * *   or publicity pertaining to distribution of the software     *
 * *   without specific, written prior permission.  Compaq         *
 * *   makes no representations about the suitability of this      *
 * *   software for any purpose.  It is provided "AS IS"           *
 * *   without express or implied warranty.                        *
 * *                                                               *
 * *****************************************************************
 *
 * $FreeBSD$
 */




/*
 * Registers in the T2 CBUS-to-PCI bridge as used in the SABLE
 * systems.
 */

#define REGVAL(r)	(*(volatile int32_t *)				\
				ALPHA_PHYS_TO_K0SEG(r + sable_lynx_base)) 
#define REGVAL64(r)	(*(volatile int64_t *)				\
				ALPHA_PHYS_TO_K0SEG(r + sable_lynx_base))

#define SABLE_BASE	0x0UL		/* offset of SABLE CSRs */
#define LYNX_BASE	0x8000000000UL	/* offset of LYNX CSRs */
#define PCI0_BASE	0x38e000000UL
#define PCI1_BASE	0x38f000000UL

#define CBUS_BASE	0x380000000	/* CBUS CSRs */
#define T2_PCI_SIO	0x3a0000000	/* PCI sparse I/O space */
#define T2_PCI_CONF	0x390000000	/* PCI configuration space */
#define T2_PCI_SPARSE	0x200000000	/* PCI sparse memory space */
#define T2_PCI_DENSE	0x3c0000000	/* PCI dense memory space */

#define T2_IOCSR	(CBUS_BASE + 0xe000000)
					/* Low word */
#define	 T2_IOCSRL_EL		0x00000002UL	/* loopback enable */
#define	 T2_IOCSRL_ESMV		0x00000004UL	/* enable state machine visibility */
#define	 T2_IOCSRL_PDBP		0x00000008UL	/* PCI drive bad parity */
#define	 T2_IOCSRL_SLOT0	0x00000030UL	/* PCI slot 0 present bits */
#define	 T2_IOCSRL_PINT		0x00000040UL	/* PCI interrupt */
#define	 T2_IOCSRL_ENTLBEC	0x00000080UL	/* enable TLB error check */
#define	 T2_IOCSRL_ENCCDMA	0x00000100UL	/* enable CXACK for DMA */
#define	 T2_IOCSRL_ENXXCHG	0x00000400UL	/* enable exclusive exchange for EV5 */
#define	 T2_IOCSRL_CAWWP0	0x00001000UL	/* CBUS command/address write wrong parity 0 */
#define	 T2_IOCSRL_CAWWP2	0x00002000UL	/* CBUS command/address write wrong parity 2 */
#define	 T2_IOCSRL_CDWWPE	0x00004000UL	/* CBUS data write wrong parity even */
#define	 T2_IOCSRL_SLOT2	0x00008000UL	/* PCI slot 2 present bit */
#define	 T2_IOCSRL_PSERR	0x00010000UL	/* power supply error */
#define	 T2_IOCSRL_MBA7		0x00020000UL	/* MBA7 asserted */
#define	 T2_IOCSRL_SLOT1	0x000c0000UL	/* PCI slot 1 present bits */
#define	 T2_IOCSRL_PDWWP1	0x00100000UL	/* PCI DMA write wrong parity HW1 */
#define	 T2_IOCSRL_PDWWP0	0x00200000UL	/* PCI DMA write wrong parity HW0 */
#define	 T2_IOCSRL_PBR		0x00400000UL	/* PCI bus reset */
#define	 T2_IOCSRL_PIR		0x00800000UL	/* PCI interface reset */
#define	 T2_IOCSRL_ENCOI	0x01000000UL	/* enable NOACK, CUCERR and out-of-sync int */
#define	 T2_IOCSRL_EPMS		0x02000000UL	/* enable PCI memory space */
#define	 T2_IOCSRL_ETLB		0x04000000UL	/* enable TLB */
#define	 T2_IOCSRL_EACC		0x08000000UL	/* enable atomic CBUS cycles */
#define	 T2_IOCSRL_ITLB		0x10000000UL	/* flush TLB */
#define	 T2_IOCSRL_ECPC		0x20000000UL	/* enable CBUS parity check */
#define	 T2_IOCSRL_CIR		0x40000000UL	/* CBUS interface reset */
#define	 T2_IOCSRL_EPL		0x80000000UL	/* enable PCI lock */
					/* High word */
#define	 T2_IOCSRH_CBBCE	0x00000001UL	/* CBUS back-to-back cycle enable */
#define	 T2_IOCSRH_TM		0x0000000eUL	/* T2 revision number */
#define	 T2_IOCSRH_SMVL		0x00000070UL	/* state machine visibility select */
#define	 T2_IOCSRH_SLOT2	0x00000080UL	/* PCI slot 2 present bit */
#define	 T2_IOCSRH_EPR		0x00000100UL	/* enable passive release */
#define	 T2_IOCSRH_CAWWP1	0x00001000UL	/* cbus command/address write wrong parity 1 */
#define	 T2_IOCSRH_CAWWP3	0x00002000UL	/* cbus command/address write wrong parity 3 */
#define	 T2_IOCSRH_DWWPO	0x00004000UL	/* CBUS data write wrong parity odd */
#define	 T2_IOCSRH_PRM		0x00100000UL	/* PCI read multiple */
#define	 T2_IOCSRH_PWM		0x00200000UL	/* PCI write multiple */
#define	 T2_IOCSRH_FPRDPED	0x00400000UL	/* force PCI RDPE detect */
#define	 T2_IOCSRH_PFAPED	0x00800000UL	/* force PCI APE detect */
#define	 T2_IOCSRH_FPWDPED	0x01000000UL	/* force PCI WDPE detect */
#define	 T2_IOCSRH_EPNMI	0x02000000UL	/* enable PCI NMI */
#define	 T2_IOCSRH_EPDTI	0x04000000UL	/* enable PCI DTI */
#define	 T2_IOCSRH_EPSEI	0x08000000UL	/* enable PCI SERR interrupt */
#define	 T2_IOCSRH_EPPEI	0x10000000UL	/* enable PCI PERR interrupt */
#define	 T2_IOCSRH_ERDPC	0x20000000UL	/* enable PCI RDP interrupt */
#define	 T2_IOCSRH_EADPC	0x40000000UL	/* enable PCI AP interrupt */
#define	 T2_IOCSRH_EWDPC	0x80000000UL	/* enable PCI WDP interrupt */

#define T2_CERR1	(CBUS_BASE + 0xe000020)
#define T2_CERR2	(CBUS_BASE + 0xe000040)
#define T2_CERR3	(CBUS_BASE + 0xe000060)
#define T2_PERR1	(CBUS_BASE + 0xe000080)
#define	 T2_PERR1_PWDPE		0x00000001	/* PCI write data parity error */
#define	 T2_PERR1_PAPE		0x00000002	/* PCI address parity error */
#define	 T2_PERR1_PRDPE		0x00000004	/* PCI read data parity error */
#define	 T2_PERR1_PPE		0x00000008	/* PCI parity error */
#define	 T2_PERR1_PSE		0x00000010	/* PCI system error */
#define	 T2_PERR1_PDTE		0x00000020	/* PCI device timeout error */
#define	 T2_PERR1_NMI		0x00000040	/* PCI NMI */

#define T2_PERR2	(CBUS_BASE + 0xe0000a0)
#define T2_PSCR		(CBUS_BASE + 0xe0000c0)
#define T2_HAE0_1	(CBUS_BASE + 0xe0000e0)
#define T2_HAE0_2	(CBUS_BASE + 0xe000100)
#define T2_HBASE	(CBUS_BASE + 0xe000120)
#define T2_WBASE1	(CBUS_BASE + 0xe000140)
#define T2_WMASK1	(CBUS_BASE + 0xe000160)
#define T2_TBASE1	(CBUS_BASE + 0xe000180)
#define T2_WBASE2	(CBUS_BASE + 0xe0001a0)
#define T2_WMASK2	(CBUS_BASE + 0xe0001c0)
#define T2_TBASE2	(CBUS_BASE + 0xe0001e0)
#define T2_TLBBR	(CBUS_BASE + 0xe000200)
#define T2_HAE0_3	(CBUS_BASE + 0xe000240)
#define T2_HAE0_4	(CBUS_BASE + 0xe000280)

/*
 * DMA window constants, section 5.2.1.1.1 of the 
 * Sable I/O Specification
 */
 
#define T2_WINDOW_ENABLE	0x00080000
#define T2_WINDOW_DISABLE	0x00000000
#define T2_WINDOW_SG		0x00040000
#define T2_WINDOW_DIRECT	0x00000000

#define T2_WMASK_2G		0x7ff00000
#define T2_WMASK_1G		0x3ff00000
#define T2_WMASK_512M		0x1ff00000
#define T2_WMASK_256M		0x0ff00000
#define T2_WMASK_128M		0x07f00000
#define T2_WMASK_64M		0x03f00000
#define T2_WMASK_32M		0x01f00000
#define T2_WMASK_16M		0x00f00000
#define T2_WMASK_8M		0x00700000
#define T2_WMASK_4M		0x00300000
#define T2_WMASK_2M		0x00100000
#define T2_WMASK_1M		0x00000000


#define T2_WSIZE_2G		0x80000000
#define T2_WSIZE_1G		0x40000000
#define T2_WSIZE_512M		0x20000000
#define T2_WSIZE_256M		0x10000000
#define T2_WSIZE_128M		0x08000000
#define T2_WSIZE_64M		0x04000000
#define T2_WSIZE_32M		0x02000000
#define T2_WSIZE_16M		0x01000000
#define T2_WSIZE_8M		0x00800000
#define T2_WSIZE_4M		0x00400000
#define T2_WSIZE_2M		0x00200000
#define T2_WSIZE_1M		0x00100000
#define T2_WSIZE_0M		0x00000000

#define T2_TBASE_SHIFT		1

#define	MASTER_ICU	0x535
#define	SLAVE0_ICU	0x537
#define	SLAVE1_ICU	0x53b
#define	SLAVE2_ICU	0x53d
#define	SLAVE3_ICU	0x53f


#define T2_EISA_IRQ_TO_STDIO_IRQ( x )	((x) + 7)
#define T2_STDIO_IRQ_TO_EISA_IRQ( x )	((x) - 7)
#define STDIO_PCI0_IRQ_TO_SCB_VECTOR( x )	(( ( x ) * 0x10) + 0x800)
#define STDIO_PCI1_IRQ_TO_SCB_VECTOR( x )	(( ( x ) * 0x10) + 0xC00)

/*
 * T4  Control and Status Registers
 *
 * All CBUS CSRs in the Cbus2 IO subsystems are in the T4 gate array.  The
 * CBUS CSRs in the T4 are all aligned on hexaword boundaries and have 
 * quadword length.  Note, this structure also works for T2 as the T2
 * registers are a proper subset of the T3/T4's.  Just make sure
 * that T2 code does not reference T3/T4-only registers.
 *
 */

typedef struct {
	u_long iocsr;	u_long fill_00[3]; /* I/O Control/Status */
	u_long cerr1;	u_long fill_01[3]; /* Cbus Error Register 1 */
	u_long cerr2;	u_long fill_02[3]; /* Cbus Error Register 2 */
	u_long cerr3;	u_long fill_03[3]; /* Cbus Error Register 3 */
	u_long pcierr1;	u_long fill_04[3]; /* PCI Error Register 1 */
	u_long pcierr2;	u_long fill_05[3]; /* PCI Error Register 2 */
	u_long pciscr;	u_long fill_06[3]; /* PCI Special Cycle  */
	u_long hae0_1;	u_long fill_07[3]; /* High Address Extension 1 */
	u_long hae0_2;	u_long fill_08[3]; /* High Address Extension 2 */
	u_long hbase;	u_long fill_09[3]; /* PCI Hole Base */
	u_long wbase1;	u_long fill_0a[3]; /* Window Base 1 */
	u_long wmask1;	u_long fill_0b[3]; /* Window Mask 1 */
	u_long tbase1;	u_long fill_0c[3]; /* Translated Base 1 */
	u_long wbase2;	u_long fill_0d[3]; /* Window Base 2 */
	u_long wmask2;	u_long fill_0e[3]; /* Window Mask 2 */
	u_long tbase2;	u_long fill_0f[3]; /* Translated Base 2 */
	u_long tlbbr;	u_long fill_10[3]; /* TLB by-pass */
	u_long ivr;	u_long fill_11[3]; /* IVR Passive Rels/Intr Addr  (reserved on T3/T4) */
	u_long hae0_3;	u_long fill_12[3]; /* High Address Extension 3 */
	u_long hae0_4;	u_long fill_13[3]; /* High Address Extension 4 */
	u_long wbase3;	u_long fill_14[3]; /* Window Base 3 */
	u_long wmask3;	u_long fill_15[3]; /* Window Mask 3 */
	u_long tbase3;	u_long fill_16[3]; /* Translated Base 3 */

	u_long rsvd1;	u_long fill_16a[3]; /* unused location */

	u_long tdr0;	u_long fill_17[3]; /* tlb data register 0 */
	u_long tdr1;	u_long fill_18[3]; /* tlb data register 1 */
	u_long tdr2;	u_long fill_19[3]; /* tlb data register 2 */
	u_long tdr3;	u_long fill_1a[3]; /* tlb data register 3 */
	u_long tdr4;	u_long fill_1b[3]; /* tlb data register 4 */
	u_long tdr5;	u_long fill_1c[3]; /* tlb data register 5 */
	u_long tdr6;	u_long fill_1d[3]; /* tlb data register 6 */
	u_long tdr7;	u_long fill_1e[3]; /* tlb data register 7 */
	u_long wbase4;	u_long fill_1f[3]; /* Window Base 4 */
	u_long wmask4;	u_long fill_20[3]; /* Window Mask 4 */
	u_long tbase4;	u_long fill_21[3]; /* Translated Base 4 */
/*
 * The following 4 registers are used to get to the ICIC chip
 */
	u_long air;	u_long fill_22[3]; /* Address Indirection register */
	u_long var;	u_long fill_23[3]; /* Vector access register */
	u_long dir;	u_long fill_24[3]; /* Data Indirection register */
	u_long ice;	u_long fill_25[3]; /* IC enable register Indirection register */

} t2_csr_t;
