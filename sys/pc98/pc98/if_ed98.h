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
 *
 * $FreeBSD$
 */

/*
 * PC-9801 specific definitions for DP8390/SMC8216 NICs.
 */
#ifndef __PC98_PC98_IF_ED98_H__
#define __PC98_PC98_IF_ED98_H__

/* PC98 only */
#ifndef PC98
#error Why you include if_ed98.h?
#endif

static	int pc98_set_register __P((struct isa_device *dev, int type));
static	int pc98_set_register_unit __P((struct ed_softc *sc, int type, int iobase));

/*
 * Vendor types
 */
#define	ED_VENDOR_MISC		0xf0		/* others */

/*
 * Register offsets
 */
#undef	ED_NOVELL_ASIC_OFFSET
#define	ED_NOVELL_ASIC_OFFSET	sc->edreg.asic_offset

/*
 * Remote DMA data register; for reading or writing to the NIC mem
 * via programmed I/O (offset from ASIC base).
 */
#undef	ED_NOVELL_DATA
#define	ED_NOVELL_DATA		sc->edreg.data

/*
 * Reset register; reading from this register causes a board reset.
 */
#undef	ED_NOVELL_RESET
#define	ED_NOVELL_RESET		sc->edreg.reset

/*
 * Card types.
 *
 * Type  Card
 * 0x00  Allied Telesis CenterCom LA-98-T.
 * 0x10  NE2000 PCMCIA on old 98Note.
 * 0x20  PLANET SMART COM 98 EN-2298 / ELECOM LANEED LD-BDN[123]A.
 * 0x30  MELCO EGY-98 / Contec C-NET(98)E-A/L-A.
 * 0x40  MELCO LGY-98, IND-SP, IND-SS / MACNICA NE2098(XXX).
 * 0x50  ICM DT-ET-25, DT-ET-T5, IF-2766ET, IF-2771ET /
 *       D-Link DE-298P{T,CAT}, DE-298{T,TP,CAT}.
 * 0x60  Allied Telesis SIC-98.
 * 0x70  ** RESERVED **
 * 0x80  NEC PC-9801-108.
 * 0x90  IO-DATA LA-98.
 * 0xa0  Contec C-NET(98).
 * 0xb0  Contec C-NET(98)E/L.
 * 0xc0  ** RESERVED **
 * 0xd0  Networld EC/EP-98X.
 */
#define	ED_TYPE98_BASE		0x80

#define	ED_TYPE98_GENERIC	0x80
#define	ED_TYPE98_PCIC98	0x81	/* OLD NOTE PCMCIA */
#define	ED_TYPE98_BDN		0x82
#define	ED_TYPE98_EGY		0x83
#define	ED_TYPE98_LGY		0x84
#define	ED_TYPE98_ICM		0x85
#define	ED_TYPE98_SIC		0x86
#define	ED_TYPE98_108		0x88
#define	ED_TYPE98_LA98		0x89
#define	ED_TYPE98_CNET98	0x8a
#define	ED_TYPE98_CNET98EL	0x8b
#define	ED_TYPE98_NW98X		0x8d

#define	ED_TYPE98(x)	(((x & 0xffff0000) >> 20) | ED_TYPE98_BASE)
#define	ED_TYPE98SUB(x)	((x & 0xf0000) >> 16)


/*
 * Page 0 register offsets.
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
 * Page 1 register offsets.
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
#undef	ED_P1_PAR
#define	ED_P1_PAR(i)	sc->edreg.port[0x01 + i]
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
#undef	ED_P1_MAR
#define	ED_P1_MAR(i)	sc->edreg.port[0x08 + i]

/*
 * Page 2 register offsets.
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

/* PCIC98 support */
#define	ED_PCIC98_16BIT_ON()    outb(0x2a8e, 0x94)
#define	ED_PCIC98_16BIT_OFF()   outb(0x2a8e, 0xb4)

/*
 * C-NET(98) & C-NET(98)EL
 */
/*
 * NIC Initial Register(on board JP1).
 */
#define	ED_CNET98_INIT          0xaaed
#define	ED_CNET98_INIT2         0x55ed

/*
 * C-NET(98)
 */
#define	ED_CNET98_IO_PORTS      16

/* offset NIC address */
#define	ED_CNET98_MAP_REG0L	 1              /* MAPPING register0 Low.  */
#define	ED_CNET98_MAP_REG1L	 3              /* MAPPING register1 Low.  */
#define	ED_CNET98_MAP_REG2L	 5              /* MAPPING register2 Low.  */
#define	ED_CNET98_MAP_REG3L	 7              /* MAPPING register3 Low.  */
#define	ED_CNET98_MAP_REG0H	 9              /* MAPPING register0 Hi.   */
#define	ED_CNET98_MAP_REG1H	11              /* MAPPING register1 Hi.   */
#define	ED_CNET98_MAP_REG2H	13              /* MAPPING register2 Hi.   */
#define	ED_CNET98_MAP_REG3H	15              /* MAPPING register3 Hi.   */
#define	ED_CNET98_WIN_REG	(0x400 +  1)    /* Window register.        */
#define	ED_CNET98_INT_LEV	(0x400 +  3)    /* Init level register.    */
#define	ED_CNET98_INT_REQ	(0x400 +  5)    /* Init request register.  */
#define	ED_CNET98_INT_MASK	(0x400 +  7)    /* Init mask register.     */
#define	ED_CNET98_INT_STAT	(0x400 +  9)    /* Init status register.   */
#define	ED_CNET98_INT_CLR	(0x400 +  9)    /* Init clear register.    */
#define	ED_CNET98_RESERVE1	(0x400 + 11)
#define	ED_CNET98_RESERVE2	(0x400 + 13)
#define	ED_CNET98_RESERVE3	(0x400 + 15)
#define	ED_CNET98_INT_IRQ3      0x01            /* INT 0 */
#define	ED_CNET98_INT_IRQ5      0x02            /* INT 1 */
#define	ED_CNET98_INT_IRQ6      0x04            /* INT 2 */
#define	ED_CNET98_INT_IRQ9      0x08            /* INT 3 */
#define	ED_CNET98_INT_IRQ12     0x20            /* INT 5 */
#define	ED_CNET98_INT_IRQ13     0x40            /* INT 6 */


/* C-NET(98)E/L */
#define	ED_CNET98EL_NIC_OFFSET  0
#define	ED_CNET98EL_ASIC_OFFSET 0x400   /* Offset to nic i/o regs. */
#define	ED_CNET98EL_PAGE_OFFSET 0x0000  /* Page offset for NIC access to mem. */
/*
 * XXX - The I/O address range is fragmented in the CNET98E/L; this is the
 *    number of regs at iobase.
 */
#define	ED_CNET98EL_IO_PORTS    16      /* # of i/o addresses used. */
/*
 *    Interrupt Configuration Register (offset from ASIC base).
 */
#define	ED_CNET98EL_ICR         0x02

#define	ED_CNET98EL_ICR_IRQ3    0x01    /* Interrupt request 3 select.  */
#define	ED_CNET98EL_ICR_IRQ5    0x02    /* Interrupt request 5 select.  */
#define	ED_CNET98EL_ICR_IRQ6    0x04    /* Interrupt request 6 select.  */
#define	ED_CNET98EL_ICR_IRQ12   0x20    /* Interrupt request 12 select. */
/*
 *    Interrupt Mask Register (offset from ASIC base).
 */
#define	ED_CNET98EL_IMR         0x04
/*
 *    Interrupt Status Register (offset from ASIC base).
 */
#define	ED_CNET98EL_ISR         0x05

/*
 * Networld EC/EP-98X
 */
/*
 * Interrupt Status Register (offset from ASIC base).
 */
#define	ED_NW98X_IRQ            0x1000

#define	ED_NW98X_IRQ3           0x04
#define	ED_NW98X_IRQ5           0x06
#define	ED_NW98X_IRQ6           0x08
#define	ED_NW98X_IRQ12          0x0a
#define	ED_NW98X_IRQ13          0x02

/* NE2000, LGY-98, ICM, C-NET(98)E/L */
static	u_short edp_generic[16] = {
	0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15
};

/* EGY-98 */
static	u_short edp_egy98[16] = {
	0,     0x02,  0x04,  0x06,  0x08,  0x0a,  0x0c,  0x0e,
	0x100, 0x102, 0x104, 0x106, 0x108, 0x10a, 0x10c, 0x10e
};

/* SIC-98 */
static	u_short edp_sic98[16] = {
	0x0000, 0x0200, 0x0400, 0x0600, 0x0800, 0x0a00, 0x0c00, 0x0e00,
	0x1000, 0x1200, 0x1400, 0x1600, 0x1800, 0x1a00, 0x1c00, 0x1e00
};

/* IO-DATA LA-98, ELECOM LD-BDN */
static	u_short edp_la98[16] = {
	0x0000, 0x1000, 0x2000, 0x3000, 0x4000, 0x5000, 0x6000, 0x7000,
	0x8000, 0x9000, 0xa000, 0xb000, 0xc000, 0xd000, 0xe000, 0xf000
};

/* NEC PC-9801-108 */
static	u_short edp_nec108[16] = {
	0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000a, 0x000c, 0x000e,
	0x1000, 0x1002, 0x1004, 0x1006, 0x1008, 0x100a, 0x100c, 0x100e
};

/* Contec C-NET(98) */
static	u_short edp_cnet98[16] = {
	0x0000, 0x0002, 0x0004, 0x0006, 0x0008, 0x000a, 0x000c, 0x000e,
	0x0400, 0x0402, 0x0404, 0x0406, 0x0408, 0x040a, 0x040c, 0x040e
};

/* Networld EC/EP-98X */
static	u_short edp_nw98x[16] = {
	0x0000, 0x0100, 0x0200, 0x0300, 0x0400, 0x0500, 0x0600, 0x0700,
	0x0800, 0x0900, 0x0a00, 0x0b00, 0x0c00, 0x0d00, 0x0e00, 0x0f00
};


static int
pc98_set_register(struct isa_device *dev, int type)
{
	return pc98_set_register_unit(&ed_softc[dev->id_unit], type, dev->id_iobase);
}

static int
pc98_set_register_unit(struct ed_softc *sc, int type, int iobase)
{
	int	adj;
	int	nports;

	sc->type = type;

	switch (type) {
	default:
	case ED_TYPE98_GENERIC:
		sc->edreg.port = edp_generic;
		ED_NOVELL_ASIC_OFFSET = 0x0010;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x000f;
		nports = 32;
		break;

	case ED_TYPE98_LGY:
		sc->edreg.port = edp_generic;
		ED_NOVELL_ASIC_OFFSET = 0x0200;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0100;
		nports = 16;
		break;

	case ED_TYPE98_EGY:
		sc->edreg.port = edp_egy98;
		ED_NOVELL_ASIC_OFFSET = 0x0200;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0100;
		nports = 16;
		break;		

	case ED_TYPE98_ICM:
	case ED_TYPE98_PCIC98:
		sc->edreg.port = edp_generic;
		ED_NOVELL_ASIC_OFFSET = 0x0100;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x000f;
		nports = 16;
		break;

	case ED_TYPE98_BDN:
		sc->edreg.port = edp_la98;
		ED_NOVELL_ASIC_OFFSET = 0x0100;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0xc000;
		nports = 1;
		break;

	case ED_TYPE98_SIC:
		sc->edreg.port = edp_sic98;
		ED_NOVELL_ASIC_OFFSET = 0x2000;
		ED_NOVELL_DATA = 0;	/* dummy */
		ED_NOVELL_RESET = 0;	/* dummy */
		nports = 1;
		break;

	case ED_TYPE98_108:
		sc->edreg.port = edp_nec108;
		adj = (iobase & 0xf000) / 2;
		ED_NOVELL_ASIC_OFFSET = (0x0888 | adj) - iobase;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0002;
		nports = 16;
		break;

	case ED_TYPE98_LA98:
		sc->edreg.port = edp_la98;
		ED_NOVELL_ASIC_OFFSET = 0x0100;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0xf000;
		nports = 1;
		break;

	case ED_TYPE98_CNET98EL:
		sc->edreg.port = edp_generic;
		ED_NOVELL_ASIC_OFFSET = 0x0400;
		ED_NOVELL_DATA = 0x000e;
		ED_NOVELL_RESET = 0;	/* dummy */
		nports = 16;
		break;

	case ED_TYPE98_CNET98:
		sc->edreg.port = edp_cnet98;
		ED_NOVELL_ASIC_OFFSET = 0x0400;
		ED_NOVELL_DATA = 0;	/* dummy */
		ED_NOVELL_RESET = 0;	/* dummy */
		nports = 16;
		break;

	case ED_TYPE98_NW98X:
		sc->edreg.port = edp_nw98x;
		ED_NOVELL_ASIC_OFFSET = 0x1000;
		ED_NOVELL_DATA = 0x0000;
		ED_NOVELL_RESET = 0x0f00;
		nports = 1;
		break;
	}
	return nports;
}

/*
 * SMC EtherEZ98(SMC8498BTA)
 *
 * A sample of kernel conf is as follows.
 * #device ed0 at isa? port 0x10d0 net irq 6 iomem 0xc8000 vector edintr
 */
#undef	ED_WD_NIC_OFFSET
#define ED_WD_NIC_OFFSET	0x100		/* I/O base offset to NIC */
#undef	ED_WD_ASIC_OFFSET
#define ED_WD_ASIC_OFFSET	0		/* I/O base offset to ASIC */
/*
 * XXX - The I/O address range is fragmented in the EtherEZ98;
 *	it occupies 16*2 I/O addresses, by the way.
 */
#undef	ED_WD_IO_PORTS
#define ED_WD_IO_PORTS		16		/* # of i/o addresses used */

#endif /* __PC98_PC98_IF_ED98_H__ */
