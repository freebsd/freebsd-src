#ifndef IF_RDPREG_H
#define IF_RDPREG_H 1
/*
 * Copyright (c) 1998 Joerg Wunsch
 *
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
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPERS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE DEVELOPERS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/i386/isa/if_rdpreg.h,v 1.2 1999/08/28 00:44:49 peter Exp $
 */

/*
 * Part of the definitions here has been copied over from the REDP
 * packet driver's REDPPD.INC file.  This provides us with the same
 * set of acronyms as the packet driver is using.
 *
 * The packet driver had no copyright, and is believed to be in the
 * public domain.  The author seems to be someone who calls himself
 * "Chiu", so that's the only acknowledgment i can give here.
 * Supposedly the author was someone from RealTek.
 */

/*
 * We're hanging upon an LPT port, thus suck in the lpt defs as well.
 */
#include <i386/isa/lptreg.h>

struct rdphdr {
	/* RTL8002 header that is prepended to the actual packet */
	u_char	unused2[2];
	u_short	pktlen;
	u_char	status;		/* copy of RSR for this packet */
	u_char	unused3[3];
};	

/*
 *
 *    8 Data Modes are provided:
 *
 * 	+--------+---------------+-------------+
 * 	|  Mode  |     Read	 |    Write    |
 * 	+--------+---------------+-------------+
 * 	|   0	 |   LptCtrl	 |   LptData   |
 * 	+--------+---------------+-------------+
 * 	|   1	 |   LptCtrl	 |   LptCtrl   |
 * 	+--------+---------------+-------------+
 * 	|   2	 |   LptCtrl*2	 |   LptData   |
 * 	+--------+---------------+-------------+
 * 	|   3	 |   LptCtrl*2	 |   LptCtrl   |
 * 	+--------+---------------+-------------+
 * 	|   4	 |   LptData	 |   LptData   |
 * 	+--------+---------------+-------------+
 * 	|   5	 |   LptData	 |   LptCtrl   |
 * 	+--------+---------------+-------------+
 * 	|   6	 |   LptData*2	 |   LptData   |
 * 	+--------+---------------+-------------+
 * 	|   7	 |   LptData*2	 |   LptCtrl   |
 * 	+--------+---------------+-------------+
 *
 * Right now, this driver only implements mode 0 (which ought to work
 * on any standard parallel interface).
 *
 */

/*
 * Page 0 of EPLC registers
 */
#define	IDR0	0x00		/* Ethernet ID register (R/W) */
#define	IDR1	0x01
#define	IDR2	0x02
#define	IDR3	0x03
#define	IDR4	0x04
#define	IDR5	0x05
#define	TBCR0	0x06		/* transmit byte count (W), 11 bits valid */
#define	TBCR1	0x07
#define	TSR	0x08		/* transmit status (R), cleared upon next tx */
# define TSR_TOK	1	/* transmit OK */
# define TSR_TABT	2	/* transmit aborted (excessive collisions) */
# define TSR_COL	4	/* collision detected */
# define TSR_CDH	8	/* CD heartbeat detected */
#define	RSR	0x09		/*
				 * receiver status (R), cleared upon next
				 * received packet (but stored in rx buffer
				 * header anyway)
				 */
# define RSR_ROK	1	/* receive OK */
# define RSR_CRC	2	/* CRC error */
# define RSR_FA		4	/* frame alignment error (not multiple of 8) */
# define RSR_BUFO	0x10	/* rx buffer overflow, packet discarded */
# define RSR_PUN	0x20	/* packet count underflow (jump command issued
				 * but rx buffer was empty) */
# define RSR_POV	0x40	/* packet count overflow (more than 254 (?)
				 * packets still in buffer) */
#define	ISR	0x0A		/* interrupt status register (R), writing
				 * clears the written bits */
# define ISR_TOK	1	/* transmission OK (~ TSR_TOK) */
# define ISR_TER	2	/* transmitter error (~ TSR_TABT) */
# define ISR_ROK	4	/* receive OK (~ RSR_ROK) */
# define ISR_RER	8	/* receiver error (~ RSR_CRC|RSR_FA) */
# define ISR_RBER	0x10	/* rx buffer overflow (POV|PUN|BUFO) */
#define	IMR	0x0B		/* interrupt mask register (R/W), bit as ISR */
#define	CMR1	0x0C		/* command register 1 (R/W) */
# define CMR1_BUFE	1	/* (R) rx buffer empty */
# define CMR1_IRQ	2	/* (R) interrupt request */
# define CMR1_TRA	4	/* (R) transmission in progress */
				/* (W) transmit start */
# define CMR1_TE	0x10	/* (R/W) transmitter enable */
# define CMR1_RE	0x20	/* (R/W) receiver enable */
# define CMR1_RST	0x40	/* (R/W) reset; sticks until reset completed */
# define CMR1_RDPAC	1	/* (W) `rx jump packet', prepare for reading
				 * next packet from ring buffer */
# define CMR1_WRPAC	2	/* (W) `tx jump packet', packet in tx buffer
				 * is complete and can be sent */
# define CMR1_RETX	8	/* (W) retransmit (must be accomp'ed by TRA) */
# define CMR1_MUX	0x80	/* (W) RTL8012: tell the printer MUX to
				 * connect the output pins to the host */
#define	CMR2	0x0D		/* command register 2 (R/W) */
# define CMR2_IRQOUT	1	/* interrupt signal output enabled */
# define CMR2_RAMTST	2	/* enable RAM test */
# define CMR2_PAGE	4	/* select register page #1 */
# define CMR2_IRQINV	8	/* make active IRQ `low' */
# define CMR2_AMbits	0x30	/* address mode bits: */
#  define CMR2_AM_NONE	0x00	/* 0: accept nothing */
#  define CMR2_AM_PHYS	0x10	/* 1: only physical addr */
#  define CMR2_AM_PB	0x20	/* 2: phys + broadcast */
#  define CMR2_AM_ALL	0x30	/* 3: promiscuous */
# define CMR2_LBK	0x40	/* enable loopback */
# define CMR2_SER	0x80	/* save error packet */
#define	MAR	0x0E		/* memory access register (?), used for
				 * remote DMA to the 8002's buffer */
#define	PNR	TBCR0		/* received packet number (R) */
#define	COLR	TBCR1		/* collision count (R) (4 bit valid) */

/*
 * Page 1 of EPLC registers -- EEPROM control
 */
#define	PCMR	TBCR0		/* port command register */
/* bits for 93C46 control -- add HNib */
#define	PCMR_SK	0x04		/* serial clock for EEPROM */
#define	PCMR_CS	0x02		/* chip select for EEPROM */
#define	PCMR_DO	0x01		/* DI to EEPROM */

/* EEPROM data, nibbles for 74S288, bits for 93C46 */
#define	PDR	TBCR1		/* DO from EEPROM, only bit 0 valid for
				 * serial EEPROM */

/*
 * The following definitionss define remote DMA command through LptCtrl
 */
#define	ATFD	3	/* ATFD bit in Lpt's Control register                */
			/* -> ATFD bit is added for Xircom's MUX             */
#define	Ctrl_LNibRead	(0x08+ATFD)	/* specify low  nibble               */
#define	Ctrl_HNibRead	(0+ATFD)	/* specify high nibble               */
#define	Ctrl_SelData	(0x04+ATFD)	/* not through LptCtrl but through   */
					/* LptData                           */
#define	Ctrl_IRQEN	0x10		/* set IRQEN of lpt control register */

/* Here define constants to construct the required read/write commands */
#define	WrAddr	0x40	/* set address of EPLC write register   */
#define	RdAddr	0x0C0	/* set address of EPLC read register    */
#define	EOR	0x20	/* ORed to make 'end of read',set CSB=1 */
#define	EOW	0x0E0	/* end of write, R/WB=A/DB=CSB=1        */
#define	EOC	0x0E0	/* End Of r/w Command, R/WB=A/DB=CSB=1  */
#define	HNib	0x10

#define MkHi(value) (((value) >> 4) | HNib)

#endif /* IF_RDPREG_H */
