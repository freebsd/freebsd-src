/*
 *	i82365.h - Definitions for Intel 82365 PCIC
 *	PCMCIA Card Interface Controller
 *
 * originally by Barry Jaspan; hacked over by Keith Moore
 * hacked to unrecognisability by Andrew McRae (andrew@mega.com.au)
 *
 * Updated 3/3/95 to include Cirrus Logic stuff.
 *-------------------------------------------------------------------------
 *
 * Copyright (c) 1995 Andrew McRae.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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

#define	PCIC_I82365	0		/* Intel chip */
#define	PCIC_IBM	1		/* IBM clone */
#define	PCIC_VLSI	2		/* VLSI chip */
#define	PCIC_PD672X	3		/* Cirrus logic 672x */
#define	PCIC_PD6710	4		/* Cirrus logic 6710 */
#define	PCIC_VG365	5		/* Vadem 365 */
#define	PCIC_VG465      6		/* Vadem 465 */
#define	PCIC_VG468	7		/* Vadem 468 */
#define	PCIC_VG469	8		/* Vadem 469 */
#define	PCIC_RF5C396	9		/* Ricoh RF5C396 */
#define	PCIC_IBM_KING	10		/* IBM KING PCMCIA Controller */
#define	PCIC_PC98	11		/* NEC PC98 PCMCIA Controller */
/* These last ones aren't in normal freebsd */
#define	PCIC_TI1130	12		/* TI PCI1130 CardBus */

/*
 *	Address of the controllers. Each controller can manage
 *	two PCMCIA slots. Up to 8 slots are supported in total.
 *	The PCIC controller is accessed via an index port and a
 *	data port. The index port has the 8 bit address of the
 *	register accessed via the data port. How I long for
 *	real memory mapped I/O!
 *	The top two bits of the index address are used to
 *	identify the port number, and the lower 6 bits
 *	select one of the 64 possible data registers.
 */
#define PCIC_INDEX_0	0x3e0			/* index reg, chips 0 and 1 */
#define PCIC_DATA_0	(PCIC_INDEX_0 + 1)	/* data reg, chips 0 and 1 */
#define PCIC_INDEX_1	(PCIC_INDEX_0 + 2)	/* index reg, chips 2 and 3 */
#define PCIC_DATA_1	(PCIC_INDEX_1 + 1)	/* data reg, chips 2 and 3 */

/*
 *	Register index addresses.
 */
#define PCIC_ID_REV	0x00	/* Identification and Revision */
#define PCIC_STATUS	0x01	/* Interface Status */
#define PCIC_POWER	0x02	/* Power and RESETDRV control */
#define PCIC_INT_GEN	0x03	/* Interrupt and General Control */
#define PCIC_STAT_CHG	0x04	/* Card Status Change */
#define PCIC_STAT_INT	0x05	/* Card Status Change Interrupt Config */
#define PCIC_ADDRWINE	0x06	/* Address Window Enable */
#define PCIC_IOCTL	0x07	/* I/O Control */
#define PCIC_IO0	0x08	/* I/O Address 0 */
#define PCIC_IO1	0x0c	/* I/O Address 1 */
#define	PCIC_MEMBASE	0x10	/* Base of memory window registers */
#define PCIC_CDGC	0x16	/* Card Detect and General Control */
#define PCIC_MISC1	0x16	/* PD672x: Misc control register 1 per slot */
#define PCIC_GLO_CTRL	0x1e	/* Global Control Register */
#define PCIC_MISC2	0x1e	/* PD672x: Misc control register 2 per chip */
#define PCIC_CLCHIP	0x1f	/* PD67xx: Chip I/D */
#define PCIC_CVSR	0x2f	/* Vadem: Voltage select register */

#define PCIC_VMISC	0x3a	/* Vadem: Misc control register */

#define	PCIC_TIME_SETUP0	0x3a
#define	PCIC_TIME_CMD0		0x3b
#define	PCIC_TIME_RECOV0	0x3c
#define	PCIC_TIME_SETUP1	0x3d
#define	PCIC_TIME_CMD1		0x3e
#define	PCIC_TIME_RECOV1	0x3f

#define	PCIC_SLOT_SIZE	0x40	/* Size of register set for one slot */

/* Now register bits, ordered by reg # */

/* For Identification and Revision (PCIC_ID_REV) */
#define PCIC_INTEL0	0x82	/* Intel 82365SL Rev. 0; Both Memory and I/O */
#define PCIC_INTEL1	0x83	/* Intel 82365SL Rev. 1; Both Memory and I/O */
#define PCIC_VLSI82C146	0x84	/* VLSI 82C146 */
#define PCIC_IBM1	0x88	/* IBM PCIC clone; Both Memory and I/O */
#define PCIC_IBM2	0x89	/* IBM PCIC clone; Both Memory and I/O */
#define PCIC_IBM3	0x8a	/* IBM KING PCIC clone; Both Memory and I/O */

/* For Interface Status register (PCIC_STATUS) */
#define PCIC_VPPV	0x80	/* Vpp_valid */
#define PCIC_POW	0x40	/* PC Card power active */
#define PCIC_READY	0x20	/* Ready/~Busy */
#define PCIC_MWP	0x10	/* Memory Write Protect */
#define PCIC_CD		0x0C	/* Both card detect bits */
#define PCIC_BVD	0x03	/* Both Battery Voltage Detect bits */

/* For the Power and RESETDRV register (PCIC_POWER) */
#define PCIC_OUTENA	0x80	/* Output Enable */
#define PCIC_DISRST	0x40	/* Disable RESETDRV */
#define PCIC_APSENA	0x20	/* Auto Pwer Switch Enable */
#define PCIC_PCPWRE	0x10	/* PC Card Power Enable */
#define	PCIC_VCC	0x18	/* Vcc control bits */
#define	PCIC_VCC_5V	0x10	/* 5 volts */
#define	PCIC_VCC_3V	0x18	/* 3 volts */
#define	PCIC_VCC_5V_KING	0x14	/* 5 volts for KING PCIC */
#define	PCIC_VPP	0x03	/* Vpp control bits */
#define	PCIC_VPP_5V	0x01	/* 5 volts */
#define	PCIC_VPP_12V	0x02	/* 12 volts */

/* For the Interrupt and General Control register (PCIC_INT_GEN) */
#define PCIC_CARDTYPE	0x20	/* Card Type 0 = memory, 1 = I/O */
#define		PCIC_IOCARD	0x20
#define		PCIC_MEMCARD	0x00
#define PCIC_CARDRESET	0x40	/* Card reset 0 = Reset, 1 = Normal */
#define	PCIC_INTR_ENA	0x10	/* Interrupt enable */

/* For the Card Status Change register (PCIC_STAT_CHG) */
#define PCIC_CDTCH	0x08	/* Card Detect Change */
#define PCIC_RDYCH	0x04	/* Ready Change */
#define PCIC_BATWRN	0x02	/* Battery Warning */
#define PCIC_BATDED	0x01	/* Battery Dead */

/*
 * For the Address Window Enable Register (PCIC_ADDRWINE)
 * The lower 6 bits contain enable bits for the memory
 * windows (LSB = memory window 0).
 */
#define PCIC_MEMCS16	0x20	/* ~MEMCS16 Decode A23-A12 */
#define PCIC_IO0_EN	0x40	/* I/O Window 0 Enable */
#define PCIC_IO1_EN	0x80	/* I/O Window 1 Enable */

/*
 * For the I/O Control Register (PCIC_IOCTL)
 * The lower nybble is the flags for I/O window 0
 * The upper nybble is the flags for I/O window 1
 */
#define PCIC_IO_16BIT	0x01	/* I/O to this segment is 16 bit */
#define PCIC_IO_CS16	0x02	/* I/O cs16 source is the card */
#define PCIC_IO_0WS	0x04	/* zero wait states added on 8 bit cycles */
#define PCIC_IO_WS	0x08	/* Wait states added for 16 bit cycles */

/*
 *	The memory window registers contain the start and end
 *	physical host address that the PCIC maps to the card,
 *	and an offset calculated from the card memory address.
 *	All values are shifted down 12 bits, so allocation is
 *	done in 4Kb blocks. Only 12 bits of each value is
 *	stored, limiting the range to the ISA address size of
 *	24 bits. The upper 4 bits of the most significant byte
 *	within the values are used for various flags.
 *
 *	The layout is:
 *
 *	base+0 : lower 8 bits of system memory start address
 *	base+1 : upper 4 bits of system memory start address + flags
 *	base+2 : lower 8 bits of system memory end address
 *	base+3 : upper 4 bits of system memory end address + flags
 *	base+4 : lower 8 bits of offset to card address
 *	base+5 : upper 4 bits of offset to card address + flags
 *
 *	The following two bytes are reserved for other use.
 */
#define	PCIC_MEMSIZE	8
/*
 *	Flags for system memory start address upper byte
 */
#define PCIC_ZEROWS	0x40	/* Zero wait states */
#define PCIC_DATA16	0x80	/* Data width is 16 bits */

/*
 *	Flags for system memory end address upper byte
 */
#define PCIC_MW0	0x40	/* Wait state bit 0 */
#define PCIC_MW1	0x80	/* Wait state bit 1 */

/*
 *	Flags for card offset upper byte
 */
#define PCIC_REG	0x40	/* Attribute/Common select (why called Reg?) */
#define PCIC_WP		0x80	/* Write-protect this window */

/* For Card Detect and General Control register (PCIC_CDGC) */
#define PCIC_16_DL_INH	0x01	/* 16-bit memory delay inhibit */
#define PCIC_CNFG_RST_EN 0x02	/* configuration reset enable */
#define PCIC_GPI_EN	0x04	/* GPI Enable */
#define PCIC_GPI_TRANS	0x08	/* GPI Transition Control */
#define PCIC_CDRES_EN	0x10	/* card detect resume enable */
#define PCIC_SW_CD_INT	0x20	/* s/w card detect interrupt */

/* CL-PD67[12]x: For 3.3V cards, etc. (PCIC_MISC1) */
#define PCIC_MISC1_5V_DETECT 0x01	/* PD6710 only */
#define PCIC_MISC1_VCC_33    0x02	/* Set Vcc is 3.3V, else 5.0V */
#define PCIC_MISC1_PMINT     0x04	/* Pulse management intr */
#define PCIC_MISC1_PCINT     0x08	/* Pulse card interrupt */
#define PCIC_MISC1_SPEAKER   0x10	/* Enable speaker */
#define PCIC_MISC1_INPACK    0x80	/* INPACK throttles data */

/* i82365B and newer (!PD67xx) Global Control register (PCIC_GLO_CTRL) */
#define PCIC_PWR_DOWN	0x01	/* power down */
#define PCIC_LVL_MODE	0x02	/* level mode interrupt enable */
#define PCIC_WB_CSCINT	0x04	/* explicit write-back csc intr */
/* Rev B only */
#define PCIC_IRQ0_LEVEL 0x08	/* irq 14 pulse mode enable */
#define PCIC_IRQ1_LEVEL 0x10

/* CL-PD67[12]x: For Misc. Control Register 2 (PCIC_MISC2) */
#define PCIC_LPDM_EN	0x02	/* Cirrus PD672x: low power dynamic mode */

/* CL-PD67[12]x: Chip info (PCIC_CLCHIP) */
#define PCIC_CLC_TOGGLE 0xc0		/* These bits toggle 1 -> 0 */
#define PCIC_CLC_DUAL	0x20		/* Single/dual socket version */

/* Vadem: Card Voltage Select register (PCIC_CVSR) */
#define PCIC_CVSR_VS	0x03		/* Voltage select */
#define PCIC_CVSR_VS_5	0x00		/* 5.0 V */
#define PCIC_CVSR_VS_33a 0x01		/* alt 3.3V */
#define PCIC_CVSR_VS_XX	0x02		/* X.XV when available */
#define PCIC_CVSR_VS_33 0x03		/* 3.3V */

/* Vadem: misc register (PCIC_VMISC) */
#define PCIC_VADEMREV	0x40

/*
 *	Mask of allowable interrupts.
 *
 *	For IBM-AT machines, irqs 3, 4, 5, 7, 9, 10, 11, 12, 14, 15 are
 *	allowed.  Nearly all IBM-AT machines with pcic cards or bridges
 *	wire these interrupts (or a subset thereof) to the corresponding
 *	pins on the ISA bus.  Some older laptops are reported to not route
 *	all the interrupt pins to the bus because the designers knew that
 *	some would conflict with builtin devices.
 *
 *	For NEC PC98 machines, irq 3, 5, 6, 9, 10, 11, 12, 13 are allowed.
 *	These correspond to the C-BUS signals INT 0, 1, 2, 3, 41, 42, 5, 6
 *	respectively.  This is with the desktop C-BUS addin card.  I don't
 *	know if this corresponds to laptop usage or not.
 *
 *	I'm not sure the proper way to map these interrupts, but it looks
 *	like pc98 is a subset of ibm-at so no actual mapping is required.
 */
#ifdef PC98
#define	PCIC_INT_MASK_ALLOWED	0x3E68		/* PC98 */
#else
#define	PCIC_INT_MASK_ALLOWED	0xDEB8		/* AT */
#endif

#define	PCIC_IO_WIN	2
#define	PCIC_MEM_WIN	5

#define	PCIC_CARD_SLOTS	4
#define PCIC_MAX_CARDS	2
#define PCIC_MAX_SLOTS (PCIC_MAX_CARDS * PCIC_CARD_SLOTS)
