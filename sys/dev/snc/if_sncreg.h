/*	$FreeBSD$	*/
/*	$NecBSD: if_snreg.h,v 1.3 1999/01/24 01:39:52 kmatsuda Exp $	*/
/*	$NetBSD$	*/
  
/*-
 * Copyright (c) 1997, 1998, 1999
 *	Kouichi Matsuda.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Kouichi Matsuda for
 *      NetBSD/pc98.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
 * Modified for NetBSD/pc98 1.2.1 from NetBSD/mac68k 1.2D by Kouichi Matsuda.
 * Make adapted for NEC PC-9801-83, 84, PC-9801-103, 104, PC-9801N-25 and
 * PC-9801N-J02R, which uses National Semiconductor DP83934AVQB as
 * Ethernet Controller and National Semiconductor NS46C46 as (64 * 16 bits)
 * Microwire Serial EEPROM.
 */

/*
 * XXX: Should not be HERE. (Should be shared with...)
 */

/*
 * NEC/SONIC port mappings, offset from iobase.
 */
#define	SNEC_CTRL	0	/* SONIC control port (word) */
#define	SNEC_CTRLB	1	/* NEC/SONIC control port (byte) */
#define	SNEC_RSVD0	2	/* not used */
#define	SNEC_ADDR	3	/* SONIC, NEC/SONIC register address set port */
#define	SNEC_RSVD1	4	/* not used */
#define	SNEC_RSVD2	5	/* not used */

#define	SNEC_NREGS	6

/* bank memory size */
#define	SNEC_NMEMS	(NBPG * 2)
/* how many bank */
#define	SNEC_NBANK	0x10
/* internal buffer size */
#define	SNEC_NBUF	(SNEC_NMEMS * SNEC_NBANK)


/*
 * NEC/SONIC specific internal registers.
 */

/*
 *	Memory Bank Select Register (MEMBS)
 */
#define	SNECR_MEMBS	0x80
#define	SNECR_MEMBS_BSEN	0x01	/* enable memory bank select */
#define	SNECR_MEMBS_EBNMSK	0x1c	/* encoded bank select number */
/* Translate bank number to encoded bank select number. */
#define	SNECR_MEMBS_B2EB(bank)	(bank << 2)
#define	SNECR_MEMBS_PCMCIABUS	0x80	/* bus type identification */

/*
 *	Memory Base Address Select Register (MEMSEL)
 */
#define	SNECR_MEMSEL	0x82
/* Translate base phys address to encoded select number. */
#define	SNECR_MEMSEL_PHYS2EN(maddr)	((maddr >> 13) & 0x0f)

/*
 *	Encoded Irq Select Register (IRQSEL)
 */
#define	SNECR_IRQSEL	0x84

/*
 *	EEPROM Access Register (EEP)
 */
#define	SNECR_EEP	0x86
#define	SNECR_EEP_DI		0x10	/* EEPROM Serial Data Input (high) */
#define	SNECR_EEP_CS		0x20	/* EEPROM Chip Select (high) */
#define	SNECR_EEP_SK		0x40	/* EEPROM Serial Data Clock (high) */
#define	SNECR_EEP_DO		0x80	/* EEPROM Serial Data Output (high) */

/* EEPROM data locations */
#define	SNEC_EEPROM_KEY0	6	/* Station Address Check Sum Key #1 */
#define	SNEC_EEPROM_KEY1	7	/* Station Address Check Sum Key #2 */
#define	SNEC_EEPROM_SA0		8	/* Station Address #1 */
#define	SNEC_EEPROM_SA1		9	/* Station Address #2 */
#define	SNEC_EEPROM_SA2		10	/* Station Address #3 */
#define	SNEC_EEPROM_SA3		11	/* Station Address #4 */
#define	SNEC_EEPROM_SA4		12	/* Station Address #5 */
#define	SNEC_EEPROM_SA5		13	/* Station Address #6 */
#define	SNEC_EEPROM_CKSUM	14	/* Station Address Check Sum */

#define	SNEC_EEPROM_SIZE	32	/* valid EEPROM data (max 128 bytes) */

/*
 *	Bus and Mode Identification Register (IDENT)
 */
#define	SNECR_IDENT	0x88
	/* Bit 0: Bus Identification. */
#define	SNECR_IDENT_CBUS	0x01	/* on PC-98 C-Bus */
#define	SNECR_IDENT_PCMCIABUS	0x00	/* on PCMCIA Bus */
	/* Bit 2: always 1 */
#define	SNECR_IDENT_MAGIC	0x04
	/* Bit 4: Bus Configuration Mode Identification. */
#define	SNECR_IDENT_PNP		0x10	/* Plug and Play (C-Bus and PCMCIA) */
#define	SNECR_IDENT_LEGACY	0x00	/* Legacy C-Bus */

#define	SNECR_IDENT_LEGACY_CBUS		\
	(SNECR_IDENT_LEGACY | SNECR_IDENT_MAGIC | SNECR_IDENT_CBUS)
#define	SNECR_IDENT_PNP_CBUS		\
	(SNECR_IDENT_PNP | SNECR_IDENT_MAGIC | SNECR_IDENT_CBUS)
#define	SNECR_IDENT_PNP_PCMCIABUS	\
	(SNECR_IDENT_PNP | SNECR_IDENT_MAGIC | SNECR_IDENT_PCMCIABUS)

/*
 * XXX: parent bus type aliases
 */
#define	SNEC_TYPE_LEGACY	0
#define	SNEC_TYPE_PNP		1

