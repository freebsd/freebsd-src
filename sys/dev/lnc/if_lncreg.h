/*-
 * Copyright (c) 1994-2000
 *	Paul Richards.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    verbatim and that no modifications are made prior to this
 *    point in the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name Paul Richards may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Am7990, Local Area Network Controller for Ethernet (LANCE)
 *
 * The LANCE has four Control and Status Registers(CSRs) which are accessed
 * through two bus addressable ports, the address port (RAP) and the data
 * port (RDP).
 *
 */

#define CSR0	0
#define CSR1	1
#define CSR2	2
#define CSR3	3
#define CSR88	88
#define CSR89	89

#define BCR49	49
#define BCR32	32
#define BCR33	33
#define BCR34	34


/* Control and Status Register Masks */

/* CSR0 */

#define ERR	0x8000
#define BABL	0x4000
#define CERR	0x2000
#define MISS	0x1000
#define MERR	0x0800
#define RINT	0x0400
#define TINT	0x0200
#define IDON	0x0100
#define INTR	0x0080
#define INEA	0x0040
#define RXON	0x0020
#define TXON	0x0010
#define TDMD	0x0008
#define STOP	0x0004
#define STRT	0x0002
#define INIT	0x0001

/*
 * CSR3
 *
 * Bits 3-15 are reserved.
 *
 */

#define BSWP	0x0004
#define ACON	0x0002
#define BCON	0x0001

/* ISA Bus Configuration Registers */
#define MSRDA   0x0000  /* ISACSR0: Master Mode Read Activity */
#define MSWRA   0x0001  /* ISACSR1: Master Mode Write Activity */
#define MC      0x0002  /* ISACSR2: Miscellaneous Configuration */

#define LED1    0x0005  /* ISACSR5: LED1 Status */
#define LED2    0x0006  /* ISACSR6: LED2 Status */
#define LED3    0x0007  /* ISACSR7: LED3 Status */

#define LED_PSE         0x0080  /* Pulse Stretcher */
#define LED_XMTE        0x0010  /* Transmit Status */
#define LED_RVPOLE      0x0008  /* Receive Polarity */
#define LED_RCVE        0x0004  /* Receive Status */
#define LED_JABE        0x0002  /* Jabber */
#define LED_COLE        0x0001  /* Collision */

/* Initialisation block */

struct init_block {
	u_short mode;		/* Mode register			*/
	u_char  padr[6];	/* Ethernet address			*/
	u_char  ladrf[8];	/* Logical address filter (multicast)	*/
	u_short rdra;		/* Low order pointer to receive ring	*/
	u_short rlen;		/* High order pointer and no. rings	*/
	u_short tdra;		/* Low order pointer to transmit ring	*/
	u_short tlen;		/* High order pointer and no rings	*/
};

/* Initialisation Block Mode Register Masks */

#define PROM      0x8000   /* Promiscuous Mode */
#define DRCVBC    0x4000   /* Disable Receive Broadcast */
#define DRCVPA    0x2000   /* Disable Receive Physical Address */
#define DLNKTST	0x1000   /* Disable Link Status */
#define DAPC      0x0800   /* Disable Automatic Polarity Correction */
#define MENDECL   0x0400   /* MENDEC Loopback Mode */
#define LRT       0x0200   /* Low Receive Threshold (T-MAU mode only) */
#define TSEL      0x0200   /* Transmit Mode Select  (AUI mode only) */
#define PORTSEL   0x0180   /* Port Select bits */
#define INTL      0x0040   /* Internal Loopback */
#define DRTY      0x0020   /* Disable Retry */
#define FCOLL     0x0010   /* Force Collision */
#define DXMTFCS   0x0008   /* Disable transmit CRC (FCS) */
#define LOOP      0x0004   /* Loopback Enabl */
#define DTX       0x0002   /* Disable the transmitter */
#define DRX       0x0001   /* Disable the receiver */

/*
 * Message Descriptor Structure
 *
 * Each transmit or receive descriptor ring entry (RDRE's and TDRE's)
 * is composed of 4, 16-bit, message descriptors. They contain the following
 * information.
 *
 * 1. The address of the actual message data buffer in user (host) memory.
 * 2. The length of that message buffer.
 * 3. The status information for that particular buffer. The eight most
 *    significant bits of md1 are collectively termed the STATUS of the
 *    descriptor.
 *
 * Descriptor md0 contains LADR 0-15, the low order 16 bits of the 24-bit
 * address of the actual data buffer.  Bits 0-7 of descriptor md1 contain
 * HADR, the high order 8-bits of the 24-bit data buffer address. Bits 8-15
 * of md1 contain the status flags of the buffer. Descriptor md2 contains the
 * buffer byte count in bits 0-11 as a two's complement number and must have
 * 1's written to bits 12-15. For the receive entry md3 has the Message Byte
 * Count in bits 0-11, this is the length of the received message and is valid
 * only when ERR is cleared and ENP is set. For the transmit entry it contains
 * more status information.
 *
 */

struct mds {
	u_short md0;
	u_short md1;
	short   md2;
	u_short md3;
};

/* Receive STATUS flags for md1 */

#define OWN	0x8000		/* Owner bit, 0=host, 1=Lance   */
#define MDERR	0x4000		/* Error                        */
#define FRAM	0x2000		/* Framing error error          */
#define OFLO	0x1000		/* Silo overflow                */
#define CRC	0x0800		/* CRC error                    */
#define RBUFF	0x0400		/* Buffer error                 */
#define STP	0x0200		/* Start of packet              */
#define ENP	0x0100		/* End of packet                */
#define HADR	0x00FF		/* High order address bits	*/

/* Receive STATUS flags for md2 */

#define BCNT	0x0FFF		/* Size of data buffer as 2's comp. no. */

/* Receive STATUS flags for md3 */

#define MCNT	0x0FFF		/* Total size of data for received packet */

/* Transmit STATUS flags for md1 */

#define ADD_FCS	0x2000		/* Controls generation of FCS	*/
#define MORE	0x1000		/* Indicates more than one retry was needed */
#define ONE	0x0800		/* Exactly one retry was needed */
#define DEF	0x0400		/* Packet transmit deferred -- channel busy */

/*
 * Transmit status flags for md2
 *
 * Same as for receive descriptor.
 *
 * BCNT   0x0FFF         Size of data buffer as 2's complement number.
 *
 */

/* Transmit status flags for md3 */

#define TBUFF	0x8000		/* Buffer error         */
#define UFLO	0x4000		/* Silo underflow       */
#define LCOL	0x1000		/* Late collision       */
#define LCAR	0x0800		/* Loss of carrier      */
#define RTRY	0x0400		/* Tried 16 times       */
#define TDR 	0x03FF		/* Time domain reflectometry */
