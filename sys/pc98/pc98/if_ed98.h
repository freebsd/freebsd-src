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

static void pc98_set_register __P((struct isa_device *dev, int type));

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
#define ED_NOVELL_NIC_OFFSET	sc->edreg.nic_offset
#ifdef ED_NOVELL_ASIC_OFFSET
#undef ED_NOVELL_ASIC_OFFSET
#endif
#define ED_NOVELL_ASIC_OFFSET	sc->edreg.asic_offset

/*
 * Remote DMA data register; for reading or writing to the NIC mem
 *	via programmed I/O (offset from ASIC base)
 */
#ifdef ED_NOVELL_DATA
#undef ED_NOVELL_DATA
#endif
#define ED_NOVELL_DATA		sc->edreg.data

/*
 * Reset register; reading from this register causes a board reset
 */
#ifdef ED_NOVELL_RESET
#undef ED_NOVELL_RESET
#endif
#define ED_NOVELL_RESET		sc->edreg.reset

/*
 * Card type
 *
 * Type  Card
 * 0x00  Allied Telesis CenterCom LA-98-T
 * 0x10  MELCO LPC-TJ, LPC-TS / IO-DATA PCLA/T
 * 0x20  PLANET SMART COM 98 EN-2298 / ELECOM LANEED LD-BDN[123]A
 * 0x30  MELCO EGY-98 / Contec C-NET(98)E-A/L-A
 * 0x40  MELCO LGY-98, IND-SP, IND-SS / MACNICA NE2098(XXX)
 * 0x50  ICM DT-ET-25, DT-ET-T5, IF-2766ET, IF-2771ET /
 *       D-Link DE-298P{T,CAT}, DE-298{T,TP,CAT}
 * 0x60  Allied Telesis SIC-98
 * 0x80  NEC PC-9801-108
 * 0x90  IO-DATA LA-98
 * 0xa0  Contec C-NET(98)
 * 0xb0  Contec C-NET(98)E/L
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
#define ED_TYPE98_CNET98	0x1a
#define ED_TYPE98_CNET98EL	0x1b
#define ED_TYPE98_UE2212	0x1c

#define ED_TYPE98(x)	(((x & 0xffff0000) >> 20) | ED_TYPE98_BASE)
#define ED_TYPE98SUB(x)	((x & 0xf0000) >> 16)


/*
 * Page 0 register offsets
 */
#undef	ED_P0_CR
#define	ED_P0_CR	sc->edreg.port[0x00]

#undef	ED_P0_CLDA0
#define	ED_P0_CLDA0	sc->edreg.port[0x01]
#undef	ED_P0_PSTART
#define	ED_P0_PSTART	sc->edreg.port[0x01]

#undef	ED_P0_CLDA1
#define	ED_P0_CLDA1	sc->edreg.port[0x02]
#undef	ED_P0_PSTOP
#define	ED_P0_PSTOP	sc->edreg.port[0x02]

#undef	ED_P0_BNRY
#define	ED_P0_BNRY	sc->edreg.port[0x03]

#undef	ED_P0_TSR
#define	ED_P0_TSR	sc->edreg.port[0x04]
#undef	ED_P0_TPSR
#define	ED_P0_TPSR	sc->edreg.port[0x04]

#undef	ED_P0_NCR
#define	ED_P0_NCR	sc->edreg.port[0x05]
#undef	ED_P0_TBCR0
#define	ED_P0_TBCR0	sc->edreg.port[0x05]

#undef	ED_P0_FIFO
#define	ED_P0_FIFO	sc->edreg.port[0x06]
#undef	ED_P0_TBCR1
#define	ED_P0_TBCR1	sc->edreg.port[0x06]

#undef	ED_P0_ISR
#define	ED_P0_ISR	sc->edreg.port[0x07]

#undef	ED_P0_CRDA0
#define	ED_P0_CRDA0	sc->edreg.port[0x08]
#undef	ED_P0_RSAR0
#define	ED_P0_RSAR0	sc->edreg.port[0x08]

#undef	ED_P0_CRDA1
#define	ED_P0_CRDA1	sc->edreg.port[0x09]
#undef	ED_P0_RSAR1
#define	ED_P0_RSAR1	sc->edreg.port[0x09]

#undef	ED_P0_RBCR0
#define	ED_P0_RBCR0	sc->edreg.port[0x0a]

#undef	ED_P0_RBCR1
#define	ED_P0_RBCR1	sc->edreg.port[0x0b]

#undef	ED_P0_RSR
#define	ED_P0_RSR	sc->edreg.port[0x0c]
#undef	ED_P0_RCR
#define	ED_P0_RCR	sc->edreg.port[0x0c]

#undef	ED_P0_CNTR0
#define	ED_P0_CNTR0	sc->edreg.port[0x0d]
#undef	ED_P0_TCR
#define	ED_P0_TCR	sc->edreg.port[0x0d]

#undef	ED_P0_CNTR1
#define	ED_P0_CNTR1	sc->edreg.port[0x0e]
#undef	ED_P0_DCR
#define	ED_P0_DCR	sc->edreg.port[0x0e]

#undef	ED_P0_CNTR2
#define	ED_P0_CNTR2	sc->edreg.port[0x0f]
#undef	ED_P0_IMR
#define	ED_P0_IMR	sc->edreg.port[0x0f]

/*
 * Page 1 register offsets
 */
#undef	ED_P1_CR
#define	ED_P1_CR	sc->edreg.port[0x00]
#undef	ED_P1_PAR0
#define	ED_P1_PAR0	sc->edreg.port[0x01]
#undef	ED_P1_PAR1
#define	ED_P1_PAR1	sc->edreg.port[0x02]
#undef	ED_P1_PAR2
#define	ED_P1_PAR2	sc->edreg.port[0x03]
#undef	ED_P1_PAR3
#define	ED_P1_PAR3	sc->edreg.port[0x04]
#undef	ED_P1_PAR4
#define	ED_P1_PAR4	sc->edreg.port[0x05]
#undef	ED_P1_PAR5
#define	ED_P1_PAR5	sc->edreg.port[0x06]
#undef	ED_P1_CURR
#define	ED_P1_CURR	sc->edreg.port[0x07]
#undef	ED_P1_MAR0
#define	ED_P1_MAR0	sc->edreg.port[0x08]
#undef	ED_P1_MAR1
#define	ED_P1_MAR1	sc->edreg.port[0x09]
#undef	ED_P1_MAR2
#define	ED_P1_MAR2	sc->edreg.port[0x0a]
#undef	ED_P1_MAR3
#define	ED_P1_MAR3	sc->edreg.port[0x0b]
#undef	ED_P1_MAR4
#define	ED_P1_MAR4	sc->edreg.port[0x0c]
#undef	ED_P1_MAR5
#define	ED_P1_MAR5	sc->edreg.port[0x0d]
#undef	ED_P1_MAR6
#define	ED_P1_MAR6	sc->edreg.port[0x0e]
#undef	ED_P1_MAR7
#define	ED_P1_MAR7	sc->edreg.port[0x0f]

/*
 * Page 2 register offsets
 */
#undef	ED_P2_CR
#define	ED_P2_CR	sc->edreg.port[0x00]
#undef	ED_P2_PSTART
#define	ED_P2_PSTART	sc->edreg.port[0x01]
#undef	ED_P2_CLDA0
#define	ED_P2_CLDA0	sc->edreg.port[0x01]
#undef	ED_P2_PSTOP
#define	ED_P2_PSTOP	sc->edreg.port[0x02]
#undef	ED_P2_CLDA1
#define	ED_P2_CLDA1	sc->edreg.port[0x02]
#undef	ED_P2_RNPP
#define	ED_P2_RNPP	sc->edreg.port[0x03]
#undef	ED_P2_TPSR
#define	ED_P2_TPSR	sc->edreg.port[0x04]
#undef	ED_P2_LNPP
#define	ED_P2_LNPP	sc->edreg.port[0x05]
#undef	ED_P2_ACU
#define	ED_P2_ACU	sc->edreg.port[0x06]
#undef	ED_P2_ACL
#define	ED_P2_ACL	sc->edreg.port[0x07]
#undef	ED_P2_RCR
#define	ED_P2_RCR	sc->edreg.port[0x0c]
#undef	ED_P2_TCR
#define	ED_P2_TCR	sc->edreg.port[0x0d]
#undef	ED_P2_DCR
#define	ED_P2_DCR	sc->edreg.port[0x0e]
#undef	ED_P2_IMR
#define	ED_P2_IMR	sc->edreg.port[0x0f]

/* PCCARD */
#ifdef ED_PC_MISC
#undef ED_PC_MISC
#endif
#define ED_PC_MISC	sc->edreg.pc_misc
#ifdef ED_PC_RESET
#undef ED_PC_RESET
#endif
#define ED_PC_RESET sc->edreg.pc_reset

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


/*
 * C-NET(98)
 */
#define ED_CNET98_INIT_ADDR	0xaaed          /* 0xaaed reset register  */
                                            /* 0xaaef i/o address set */
/* offset NIC address */
#define ED_CNET98_MAP_REG0L	 1              /* MAPPING register0 Low  */
#define ED_CNET98_MAP_REG1L	 3              /* MAPPING register1 Low  */
#define ED_CNET98_MAP_REG2L	 5              /* MAPPING register2 Low  */
#define ED_CNET98_MAP_REG3L	 7              /* MAPPING register3 Low  */
#define ED_CNET98_MAP_REG0H	 9              /* MAPPING register0 Hi   */
#define ED_CNET98_MAP_REG1H	11              /* MAPPING register1 Hi   */
#define ED_CNET98_MAP_REG2H	13              /* MAPPING register2 Hi   */
#define ED_CNET98_MAP_REG3H	15              /* MAPPING register3 Hi   */
#define ED_CNET98_WIN_REG	(0x400 +  1)    /* window register        */
#define ED_CNET98_INT_LEV	(0x400 +  3)    /* init level register    */
#define ED_CNET98_INT_REQ	(0x400 +  5)    /* init request register  */
#define ED_CNET98_INT_MASK	(0x400 +  7)    /* init mask register     */
#define ED_CNET98_INT_STAT	(0x400 +  9)    /* init status register   */
#define ED_CNET98_INT_CLR	(0x400 +  9)    /* init clear register    */
#define ED_CNET98_RESERVE1	(0x400 + 11)
#define ED_CNET98_RESERVE2	(0x400 + 13)
#define ED_CNET98_RESERVE3	(0x400 + 15)


/*
 * C-NET(98)E/L
 */
/*
 * NIC Initial Register(on board JP1)
 */
#define ED_CNET98EL_INIT        0xaaed
#define ED_CNET98EL_INIT2       0x55ed

#define ED_CNET98EL_NIC_OFFSET  0
#define ED_CNET98EL_ASIC_OFFSET 0x400   /* offset to nic i/o regs */
#define ED_CNET98EL_PAGE_OFFSET 0x0000  /* page offset for NIC access to mem */
/*
 * XXX - The I/O address range is fragmented in the CNET98E/L; this is the
 *    number of regs at iobase.
 */
#define ED_CNET98EL_IO_PORTS    16      /* # of i/o addresses used */
/*
 *    Interrupt Configuration Register (offset from ASIC base)
 */
#define ED_CNET98EL_ICR         0x02

#define ED_CNET98EL_ICR_IRQ3    0x01    /* Interrupt request 3 select */
#define ED_CNET98EL_ICR_IRQ5    0x02    /* Interrupt request 5 select */
#define ED_CNET98EL_ICR_IRQ6    0x04    /* Interrupt request 6 select */
#define ED_CNET98EL_ICR_IRQ12   0x20    /* Interrupt request 12 select */
/*
 *    Interrupt Mask Register (offset from ASIC base)
 */
#define ED_CNET98EL_IMR         0x04
/*
 *    Interrupt Status Register (offset from ASIC base)
 */
#define ED_CNET98EL_ISR         0x05

/* NE2000, LGY-98, ICM, LPC-T, C-NET(98)E/L */
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

/* Contec C-NET(98) */
static unsigned int edp_cnet98[16] = {
	0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000a, 0x000c, 0x000e,
	0x0400, 0x0402, 0x0404, 0x0406, 0x0408, 0x040a, 0x040c, 0x040e
};


static void pc98_set_register(struct isa_device *dev, int type)
{
	struct ed_softc *sc = &ed_softc[dev->id_unit];
	int adj;

	switch (type) {
	case ED_TYPE98_GENERIC:
		sc->edreg.port = edp_generic;
		sc->edreg.ioskip = 1;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x0010;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x000f;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_LGY:
		sc->edreg.port = edp_generic;
		sc->edreg.ioskip = 1;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x0200;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0100;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_EGY:
		sc->edreg.port = edp_egy98;
		sc->edreg.ioskip = 2;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = 0x0200;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0100;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;		

	case ED_TYPE98_ICM:
		sc->edreg.port = edp_generic;
		sc->edreg.ioskip = 1;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = 0x0100;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x000f;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_BDN:
		sc->edreg.port = edp_bdn98;
		sc->edreg.ioskip = 0x1000;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x0100;
		ED_NOVELL_DATA = 0;
		ED_NOVELL_RESET = 0xc100;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_SIC:
		sc->edreg.port = edp_sic98;
		sc->edreg.ioskip = 0x200;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x2000;
		ED_NOVELL_DATA = 0x00;			/* dummy */
		ED_NOVELL_RESET = 0x00;
		ED_PC_MISC = 0x18;				/* dummy */
		ED_PC_RESET = 0x1f;				/* dummy */
		break;

	case ED_TYPE98_LPC:
		sc->edreg.port = edp_generic;
		sc->edreg.ioskip = 0x1;
		ED_NOVELL_NIC_OFFSET = 0x0000;
		ED_NOVELL_ASIC_OFFSET = 0x0100;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0200;
		ED_PC_MISC = 0x108;
		ED_PC_RESET = 0x10f;
		break;

	case ED_TYPE98_108:
		sc->edreg.port = edp_nec108;
		sc->edreg.ioskip = 2;
		adj = (dev->id_iobase & 0xf000) / 2;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = (0x888 | adj) - dev->id_iobase;
		ED_NOVELL_DATA = 0;
		ED_NOVELL_RESET = 4;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_LA98:
		sc->edreg.port = edp_la98;
		sc->edreg.ioskip = 0x1000;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = 0x100;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0xf000;
		ED_PC_MISC = 0x18;
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_CNET98EL:
		sc->edreg.port = edp_generic;
		sc->edreg.ioskip = 1;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = 0x0400;
		ED_NOVELL_DATA = 0x000e;
		ED_NOVELL_RESET = 0x0000;	/* dummy */
		ED_PC_RESET = 0x1f;
		break;

	case ED_TYPE98_CNET98:
		sc->edreg.port = edp_cnet98;
		sc->edreg.ioskip = 2;
		ED_NOVELL_NIC_OFFSET = 0;
		ED_NOVELL_ASIC_OFFSET = 0x0400;
		ED_NOVELL_DATA = 0x000e;
		ED_NOVELL_RESET = 0x0000;	/* dummy */
		ED_PC_RESET = 0x1f;
		break;
	}
}

#endif /* __PC98_PC98_IF_ED98_H__ */
