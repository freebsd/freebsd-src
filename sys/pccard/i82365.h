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
 * $FreeBSD: src/sys/pccard/i82365.h,v 1.10 2000/03/10 05:43:28 imp Exp $
 */

#define	PCIC_I82365	0		/* Intel chip */
#define	PCIC_IBM	1		/* IBM clone */
#define	PCIC_VLSI	2		/* VLSI chip */
#define	PCIC_PD672X	3		/* Cirrus logic 627x */
#define	PCIC_PD6710	4		/* Cirrus logic 6710 */
#define	PCIC_CL6729	5		/* Cirrus logic 6729 */
#define	PCIC_VG365	6		/* Vadem 365 */
#define	PCIC_VG465      7		/* Vadem 465 */
#define	PCIC_VG468	8		/* Vadem 468 */
#define	PCIC_VG469	9		/* Vadem 469 */
#define	PCIC_RF5C396	10		/* Ricoh RF5C396 */
#define	PCIC_IBM_KING	11		/* IBM KING PCMCIA Controller */
#define	PCIC_PC98	12		/* NEC PC98 PCMCIA Controller */
#define	PCIC_TI1130	13		/* TI PCI1130 CardBus */

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
#define	PCIC_VPP	0x0C	/* Vpp control bits */
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

/* For Misc. Control Register 1 */
#define PCIC_SPKR_EN	0x10	/* Cirrus PD672x: speaker enable */

/* For Global Control register (PCIC_GLO_CTRL) */
#define PCIC_PWR_DOWN	0x01	/* power down */
#define PCIC_LVL_MODE	0x02	/* level mode interrupt enable */
#define PCIC_WB_CSCINT	0x04	/* explicit write-back csc intr */
#define PCIC_IRQ0_LEVEL 0x08	/* irq 14 pulse mode enable */
#define PCIC_IRQ1_LEVEL 0x10

/* For Misc. Control Register 2 */
#define PCIC_LPDM_EN	0x02	/* Cirrus PD672x: low power dynamic mode */

/*
 *	Mask of allowable interrupts.
 *	Ints are 3,4,5,7,9,10,11,12,14,15
 */
#define	PCIC_INT_MASK_ALLOWED	0xDEB8

#define	PCIC_IO_WIN	2
#define	PCIC_MEM_WIN	5

#define	PCIC_CARD_SLOTS	4
#define PCIC_MAX_CARDS	2
#define PCIC_MAX_SLOTS (PCIC_MAX_CARDS * PCIC_CARD_SLOTS)
