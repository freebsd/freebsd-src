/*
 * Copyright (c) KATO Takenori, 1996.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * PC-9801 specific definitions for National Semiconductor DP8390 NIC
 */
#ifndef __PC98_PC98_IF_ED98_H__
#define __PC98_PC98_IF_ED98_H__

/* PC98 only */
#ifndef PC98
#error Why you include if_ed98.h?
#endif

static void pc98_set_register __P((struct pc98_device *dev,
								   int unit, int type));

/*
 * Vendor types
 */
#define ED_VENDOR_MISC		0xf0		/* others */

/*
 * Register offsets/total
 */
#ifdef ED_NOVELL_NIC_OFFSET
#undef ED_NOVELL_NIC_OFFSET
#endif
#define ED_NOVELL_NIC_OFFSET	ed_novell_nic_offset[unit]
#ifdef ED_NOVELL_ASIC_OFFSET
#undef ED_NOVELL_ASIC_OFFSET
#endif
#define ED_NOVELL_ASIC_OFFSET	ed_novell_asic_offset[unit]

/*
 * Remote DMA data register; for reading or writing to the NIC mem
 *	via programmed I/O (offset from ASIC base)
 */
#ifdef ED_NOVELL_DATA
#undef ED_NOVELL_DATA
#endif
#define ED_NOVELL_DATA		ed_novell_data[unit]

/*
 * Reset register; reading from this register causes a board reset
 */
#ifdef ED_NOVELL_RESET
#undef ED_NOVELL_RESET
#endif
#define ED_NOVELL_RESET		ed_novell_reset[unit]

/*
 * Card type
 *
 * Type  Card
 *   0   Allied Telesis CenterCom LA-98-T
 *   1   MELCO LPC-TJ, LPC-TS / IO-DATA PCLA/T
 *   2   PLANET SMART COM 98 EN-2298 / ELECOM LANEED LD-BDN[123]A
 *   3   MELCO EGY-98
 *   4   MELCO LGY-98, IND-SP, IND-SS / MACNICA NE2098(XXX)
 *   5   ICM DT-ET-25, DT-ET-T5, IF-2766ET, IF-2771ET /
 *       D-Link DE-298P{T,CAT}, DE-298{T,TP,CAT}
 *   6   Allied Telesis SIC-98
 *   8   NEC PC-9801-108
 *   9   IO-DATA LA-98
 */
#define ED_TYPE98_BASE		0x10

#define ED_TYPE98_GENERIC	0x10
#define ED_TYPE98_LPC		0x11
#define	ED_TYPE98_BDN		0x12
#define	ED_TYPE98_EGY		0x13
#define	ED_TYPE98_LGY		0x14
#define	ED_TYPE98_ICM		0x15
#define	ED_TYPE98_SIC		0x16
#define ED_TYPE98_108		0x18
#define ED_TYPE98_LA98		0x19

#define ED_TYPE98(x)	(((x->id_flags & 0xffff0000) >> 16) | ED_TYPE98_BASE)

/*
 * Page 0 register offsets
 */
#undef	ED_P0_CR
#define	ED_P0_CR	edp[unit][0x00]

#undef	ED_P0_CLDA0
#define	ED_P0_CLDA0	edp[unit][0x01]
#undef	ED_P0_PSTART
#define	ED_P0_PSTART	edp[unit][0x01]

#undef	ED_P0_CLDA1
#define	ED_P0_CLDA1	edp[unit][0x02]
#undef	ED_P0_PSTOP
#define	ED_P0_PSTOP	edp[unit][0x02]

#undef	ED_P0_BNRY
#define	ED_P0_BNRY	edp[unit][0x03]

#undef	ED_P0_TSR
#define	ED_P0_TSR	edp[unit][0x04]
#undef	ED_P0_TPSR
#define	ED_P0_TPSR	edp[unit][0x04]

#undef	ED_P0_NCR
#define	ED_P0_NCR	edp[unit][0x05]
#undef	ED_P0_TBCR0
#define	ED_P0_TBCR0	edp[unit][0x05]

#undef	ED_P0_FIFO
#define	ED_P0_FIFO	edp[unit][0x06]
#undef	ED_P0_TBCR1
#define	ED_P0_TBCR1	edp[unit][0x06]

#undef	ED_P0_ISR
#define	ED_P0_ISR	edp[unit][0x07]

#undef	ED_P0_CRDA0
#define	ED_P0_CRDA0	edp[unit][0x08]
#undef	ED_P0_RSAR0
#define	ED_P0_RSAR0	edp[unit][0x08]

#undef	ED_P0_CRDA1
#define	ED_P0_CRDA1	edp[unit][0x09]
#undef	ED_P0_RSAR1
#define	ED_P0_RSAR1	edp[unit][0x09]

#undef	ED_P0_RBCR0
#define	ED_P0_RBCR0	edp[unit][0x0a]

#undef	ED_P0_RBCR1
#define	ED_P0_RBCR1	edp[unit][0x0b]

#undef	ED_P0_RSR
#define	ED_P0_RSR	edp[unit][0x0c]
#undef	ED_P0_RCR
#define	ED_P0_RCR	edp[unit][0x0c]

#undef	ED_P0_CNTR0
#define	ED_P0_CNTR0	edp[unit][0x0d]
#undef	ED_P0_TCR
#define	ED_P0_TCR	edp[unit][0x0d]

#undef	ED_P0_CNTR1
#define	ED_P0_CNTR1	edp[unit][0x0e]
#undef	ED_P0_DCR
#define	ED_P0_DCR	edp[unit][0x0e]

#undef	ED_P0_CNTR2
#define	ED_P0_CNTR2	edp[unit][0x0f]
#undef	ED_P0_IMR
#define	ED_P0_IMR	edp[unit][0x0f]

/*
 * Page 1 register offsets
 */
#undef	ED_P1_CR
#define	ED_P1_CR	edp[unit][0x00]
#undef	ED_P1_PAR0
#define	ED_P1_PAR0	edp[unit][0x01]
#undef	ED_P1_PAR1
#define	ED_P1_PAR1	edp[unit][0x02]
#undef	ED_P1_PAR2
#define	ED_P1_PAR2	edp[unit][0x03]
#undef	ED_P1_PAR3
#define	ED_P1_PAR3	edp[unit][0x04]
#undef	ED_P1_PAR4
#define	ED_P1_PAR4	edp[unit][0x05]
#undef	ED_P1_PAR5
#define	ED_P1_PAR5	edp[unit][0x06]
#undef	ED_P1_CURR
#define	ED_P1_CURR	edp[unit][0x07]
#undef	ED_P1_MAR0
#define	ED_P1_MAR0	edp[unit][0x08]
#undef	ED_P1_MAR1
#define	ED_P1_MAR1	edp[unit][0x09]
#undef	ED_P1_MAR2
#define	ED_P1_MAR2	edp[unit][0x0a]
#undef	ED_P1_MAR3
#define	ED_P1_MAR3	edp[unit][0x0b]
#undef	ED_P1_MAR4
#define	ED_P1_MAR4	edp[unit][0x0c]
#undef	ED_P1_MAR5
#define	ED_P1_MAR5	edp[unit][0x0d]
#undef	ED_P1_MAR6
#define	ED_P1_MAR6	edp[unit][0x0e]
#undef	ED_P1_MAR7
#define	ED_P1_MAR7	edp[unit][0x0f]

/*
 * Page 2 register offsets
 */
#undef	ED_P2_CR
#define	ED_P2_CR	edp[unit][0x00]
#undef	ED_P2_PSTART
#define	ED_P2_PSTART	edp[unit][0x01]
#undef	ED_P2_CLDA0
#define	ED_P2_CLDA0	edp[unit][0x01]
#undef	ED_P2_PSTOP
#define	ED_P2_PSTOP	edp[unit][0x02]
#undef	ED_P2_CLDA1
#define	ED_P2_CLDA1	edp[unit][0x02]
#undef	ED_P2_RNPP
#define	ED_P2_RNPP	edp[unit][0x03]
#undef	ED_P2_TPSR
#define	ED_P2_TPSR	edp[unit][0x04]
#undef	ED_P2_LNPP
#define	ED_P2_LNPP	edp[unit][0x05]
#undef	ED_P2_ACU
#define	ED_P2_ACU	edp[unit][0x06]
#undef	ED_P2_ACL
#define	ED_P2_ACL	edp[unit][0x07]
#undef	ED_P2_RCR
#define	ED_P2_RCR	edp[unit][0x0c]
#undef	ED_P2_TCR
#define	ED_P2_TCR	edp[unit][0x0d]
#undef	ED_P2_DCR
#define	ED_P2_DCR	edp[unit][0x0e]
#undef	ED_P2_IMR
#define	ED_P2_IMR	edp[unit][0x0f]

/* PCCARD */
#ifdef ED_PC_MISC
#undef ED_PC_MISC
#endif
#define ED_PC_MISC	ed_pc_misc[unit]
#ifdef ED_PC_RESET
#undef ED_PC_RESET
#endif
#define ED_PC_RESET ed_pc_reset[unit]

/* LPC-T support */
#define LPCT_1d0_ON() \
{ \
	outb(0x2a8e, 0x84); \
	outw(0x4a8e, 0x1d0); \
	outw(0x5a8e, 0x0310); \
}

#define LPCT_1d0_OFF() \
{ \
	outb(0x2a8e, 0xa4); \
	outw(0x4a8e, 0xd0); \
	outw(0x5a8e, 0x0300); \
}


/* register offsets */
static unsigned int *edp[NED];
static unsigned int pc98_io_skip[NED];
static int ed_novell_nic_offset[NED];
static int ed_novell_asic_offset[NED];
static int ed_novell_data[NED];
static int ed_novell_reset[NED];
static int ed_pc_misc[NED];
static int ed_pc_reset[NED];


/* NE2000, LGY-98, ICM, LPC-T */
static unsigned int edp_generic[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

/* EGY-98 */
static unsigned int edp_egy98[16] = {
	0,     0x02,  0x04,  0x06,  0x08,  0x0a,  0x0c,  0x0e,
	0x100, 0x102, 0x104, 0x106, 0x108, 0x10a, 0x10c, 0x10e
};

/* LD-BDN */
static unsigned int edp_bdn98[16] = {
	0x00000, 0x01000, 0x02000, 0x03000, 0x04000, 0x05000, 0x06000, 0x07000,
	0x08000, 0x0a000, 0x0b000, 0x0c000, 0x0d000, 0x0d000, 0x0e000, 0x0f000
};

/* SIC-98 */
static unsigned int edp_sic98[16] = {
	0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600, 0x1800, 0x1a00, 0x1c00, 0x1e00
};

/* IO-DATA LA-98 */
static unsigned int edp_la98[16] = {
	0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000,
	0x8000, 0x9000, 0xa000, 0xb000, 0xc000, 0xd000, 0xe000, 0xf000
};

/* NEC PC-9801-108 */
static unsigned int edp_nec108[16] = {
	0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000a, 0x000c, 0x000e,
	0x1000, 0x1002, 0x1004, 0x1006, 0x1008, 0x100a, 0x100c, 0x100e
};

static void pc98_set_register(struct pc98_device *dev, int unit, int type)
{
	int adj;

	switch (type) {
	case ED_TYPE98_GENERIC:
		edp[unit] = edp_generic;
		pc98_io_skip[unit] = 1;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x0010;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x000f;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_LGY:
		edp[unit] = edp_generic;
		pc98_io_skip[unit] = 1;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x0200;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0100;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_EGY:
		edp[unit] = edp_egy98;
		pc98_io_skip[unit] = 2;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = 0x0200;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0100;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;		

	case ED_TYPE98_ICM:
		edp[unit] = edp_generic;
		pc98_io_skip[unit] = 1;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = 0x0100;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x000f;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_BDN:
		edp[unit] = edp_bdn98;
		pc98_io_skip[unit] = 0x1000;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x0100;
		ED_NOVELL_DATA = 0;
		ED_NOVELL_RESET = 0xc100;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_SIC:
		edp[unit] = edp_sic98;
		pc98_io_skip[unit] = 0x200;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x2000;
		ED_NOVELL_DATA = 0x00;			/* dummy */
		ED_NOVELL_RESET = 0x00;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_LPC:
		edp[unit] = edp_generic;
		pc98_io_skip[unit] = 0x1;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x0100;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0200;
		ED_PC_MISC = 0x108;
		ED_PC_RESET = 0x10f;
		break;

	case ED_TYPE98_108:
		edp[unit] = edp_nec108;
		pc98_io_skip[unit] = 2;
		adj = (dev->id_iobase & 0xf000) / 2;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = (0x888 | adj) - dev->id_iobase;
		ED_NOVELL_DATA = 0;
		ED_NOVELL_RESET = 4;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_LA98:
		edp[unit] = edp_la98;
		pc98_io_skip[unit] = 0x1000;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = 0x100;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0xf000;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;
	}
}

#endif /* __PC98_PC98_IF_ED98_H__ */
