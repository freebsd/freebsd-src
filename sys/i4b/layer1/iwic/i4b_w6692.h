/*-
 * Copyright (c) 1999, 2000 Dave Boyce. All rights reserved.
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
 */

/*---------------------------------------------------------------------------
 *
 *      i4b_iwic - isdn4bsd Winbond W6692 driver
 *      ----------------------------------------
 *
 * $FreeBSD: src/sys/i4b/layer1/iwic/i4b_w6692.h,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $
 *
 *      last edit-date: [Sun Jan 21 11:09:46 2001]
 *
 *---------------------------------------------------------------------------*/

#ifndef _I4B_W6692_H_
#define _I4B_W6692_H_

#define IWIC_BCH_A       0       /* channel A */
#define IWIC_BCH_B       1       /* channel B */

/*---------------------------------------------------------------------------*
 *	FIFO depths
 *---------------------------------------------------------------------------*/
#define IWIC_DCHAN_FIFO_LEN	64
#define IWIC_BCHAN_FIFO_LEN	64

/*---------------------------------------------------------------------------*
 *	D-Channel register offsets
 *---------------------------------------------------------------------------*/
#define D_RFIFO		0x00	/* D channel receive FIFO */
#define D_XFIFO		0x04	/* D channel transmit FIFO */
#define D_CMDR		0x08	/* D channel command register */
#define D_MODE		0x0c	/* D channel mode control */
#define D_TIMR		0x10	/* D channel timer control */
#define D_EXIR		0x1c	/* D channel extended interrupt */
#define D_EXIM		0x20	/* D channel extended interrupt mask */
#define D_STAR		0x24	/* D channel status register */
#define D_RSTA		0x28	/* D channel receive status */
#define D_SAM		0x2c	/* D channel address mask 1 */
#define D_SAP1		0x30	/* D channel individual SAPI 1 */
#define D_SAP2		0x34	/* D channel individual SAPI 2 */
#define D_TAM		0x38	/* D channel address mask 2 */
#define D_TEI1		0x3c	/* D channel individual TEI 1 */
#define D_TEI2		0x40	/* D channel individual TEI 2 */
#define D_RBCH		0x44	/* D channel receive frame byte count high */
#define D_RBCL		0x48	/* D channel receive frame byte count low */
#define D_CTL		0x54	/* D channel control register */

/*---------------------------------------------------------------------------*
 *	B-channel base offsets
 *---------------------------------------------------------------------------*/
#define B1_CHAN_OFFSET	0x80	/* B1 channel offset */
#define B2_CHAN_OFFSET	0xc0	/* B2 channel offset */

/*---------------------------------------------------------------------------*
 *	B-channel register offsets, from base
 *---------------------------------------------------------------------------*/
#define B_RFIFO		0x00	/* B channel receive FIFO */
#define B_XFIFO		0x04	/* B channel transmit FIFO */
#define B_CMDR		0x08	/* B channel command register */
#define B_MODE		0x0c	/* B channel mode control */
#define B_EXIR		0x10	/* B channel extended interrupt */
#define B_EXIM		0x14	/* B channel extended interrupt mask */
#define B_STAR		0x18	/* B channel status register */
#define B_ADM1		0x1c	/* B channel address mask 1 */
#define B_ADM2		0x20	/* B channel address mask 2 */
#define B_ADR1		0x24	/* B channel address 1 */
#define B_ADR2		0x28	/* B channel address 2 */
#define B_RBCL		0x2c	/* B channel receive frame byte count high */
#define B_RBCH		0x30	/* B channel receive frame byte count low */

/*---------------------------------------------------------------------------*
 * 	Remaining control register offsets.
 *---------------------------------------------------------------------------*/
#define ISTA		0x14	/* Interrupt status register */
#define IMASK		0x18	/* Interrupt mask register */
#define TIMR2		0x4c	/* Timer 2 */
#define L1_RC		0x50	/* GCI layer 1 ready code */
#define CIR		0x58	/* Command/Indication receive */
#define CIX		0x5c	/* Command/Indication transmit */
#define SQR		0x60	/* S/Q channel receive register */
#define SQX		0x64	/* S/Q channel transmit register */
#define PCTL		0x68	/* Peripheral control register */
#define MOR		0x6c	/* Monitor receive channel */
#define MOX		0x70	/* Monitor transmit channel */
#define MOSR		0x74	/* Monitor channel status register */
#define MOCR		0x78	/* Monitor channel control register */
#define GCR		0x7c	/* GCI mode control register */
#define XADDR		0xf4	/* Peripheral address register */
#define XDATA		0xf8	/* Peripheral data register */
#define EPCTL		0xfc	/* Serial EEPROM control */

/*---------------------------------------------------------------------------*
 *	register bits
 *---------------------------------------------------------------------------*/
#define D_CMDR_RACK	0x80
#define D_CMDR_RRST	0x40
#define D_CMDR_STT	0x10
#define D_CMDR_XMS	0x08
#define D_CMDR_XME	0x02
#define D_CMDR_XRST	0x01

#define D_MODE_MMS	0x80
#define D_MODE_RACT	0x40
#define D_MODE_TMS	0x10
#define D_MODE_TEE	0x08
#define D_MODE_MFD	0x04
#define D_MODE_DLP	0x02
#define D_MODE_RLP	0x01

#define D_TIMR_CNT(i)	(((i) >> 5) & 0x07)
#define D_TIMR_VAL(i)   ((i) & 0x1f)

#define ISTA_D_RMR	0x80
#define ISTA_D_RME	0x40
#define ISTA_D_XFR	0x20
#define ISTA_XINT1	0x10
#define ISTA_XINT0	0x08
#define ISTA_D_EXI	0x04
#define ISTA_B1_EXI	0x02
#define ISTA_B2_EXI	0x01

#define IMASK_D_RMR	0x80
#define IMASK_D_RME	0x40
#define IMASK_D_XFR	0x20
#define IMASK_XINT1	0x10
#define IMASK_XINT0	0x08
#define IMASK_D_EXI	0x04
#define IMASK_B1_EXI	0x02
#define IMASK_B2_EXI	0x01

#define D_EXIR_RDOV	0x80
#define D_EXIR_XDUN	0x40
#define D_EXIR_XCOL	0x20
#define D_EXIR_TIN2	0x10
#define D_EXIR_MOC	0x08
#define D_EXIR_ISC	0x04
#define D_EXIR_TEXP	0x02
#define D_EXIR_WEXP	0x01

#define D_EXIM_RDOV	0x80
#define D_EXIM_XDUN	0x40
#define D_EXIM_XCOL	0x20
#define D_EXIM_TIM2	0x10
#define D_EXIM_MOC	0x08
#define D_EXIM_ISC	0x04
#define D_EXIM_TEXP	0x02
#define D_EXIM_WEXP	0x01

#define D_STAR_XDOW	0x80
#define D_STAR_XBZ	0x20
#define D_STAR_DRDY	0x10

#define D_RSTA_RDOV	0x40
#define D_RSTA_CRCE	0x20
#define D_RSTA_RMB	0x10

#define D_RBCH_VN(i)	(((i) >> 6) & 0x03)
#define D_RBCH_LOV	0x20
#define D_RBC(h,l)      (((((h) & 0x1f)) << 8) + (l))

#define D_TIMR2_TMD	0x80
#define D_TIMR2_TBCN(i)	((i) & 0x3f)

#define L1_RC_RC(i)	((i) & 0x0f)

#define D_CTL_WTT(i)	(((i) > 6) & 0x03)
#define D_CTL_SRST	0x20
#define D_CTL_TPS	0x04
#define D_CTL_OPS(i)	((i) & 0x03)

#define CIR_SCC		0x80
#define CIR_ICC		0x40
#define CIR_CODR(i)	((i) & 0x0f)

#define CIX_ECK		0x00
#define CIX_RST		0x01
#define CIX_SCP		0x04
#define CIX_SSP		0x02
#define CIX_AR8		0x08
#define CIX_AR10       	0x09
#define CIX_EAL		0x0a
#define CIX_DRC		0x0f

#define CIR_CE		0x07
#define CIR_DRD		0x00
#define CIR_LD		0x04
#define CIR_ARD		0x08
#define CIR_TI		0x0a
#define CIR_ATI		0x0b
#define CIR_AI8		0x0c
#define CIR_AI10	0x0d
#define CIR_CD		0x0f

#define SQR_XIND1	0x80
#define SQR_XIND0	0x40
#define SQR_MSYN	0x20
#define SQR_SCIE	0x10
#define SQR_S(i)	((i) & 0x0f)

#define SQX_SCIE	0x10
#define SQX_Q(i)	((i) & 0x0f)


#define B_CMDR_RACK	0x80
#define B_CMDR_RRST	0x40
#define B_CMDR_RACT	0x20
#define B_CMDR_XMS 	0x04
#define B_CMDR_XME 	0x02
#define B_CMDR_XRST	0x01

#define B_MODE_MMS	0x80
#define B_MODE_ITF	0x40
#define B_MODE_EPCM	0x20
#define B_MODE_BSW1	0x10
#define B_MODE_BSW0	0x08
#define B_MODE_SW56	0x04
#define B_MODE_FTS1	0x02
#define B_MODE_FTS0	0x01

#define B_EXIR_RMR	0x40
#define B_EXIR_RME	0x20
#define B_EXIR_RDOV	0x10
#define B_EXIR_XFR	0x02
#define B_EXIR_XDUN	0x01

#define B_EXIM_RMR	0x40
#define B_EXIM_RME	0x20
#define B_EXIM_RDOV	0x10
#define B_EXIM_XFR	0x02
#define B_EXIM_XDUN	0x01

#define B_STAR_RDOV	0x40
#define B_STAR_CRCE	0x20
#define B_STAR_RMB	0x10
#define B_STAR_XDOW	0x04
#define B_STAR_XBZ	0x01

#define B_RBC(h,l)      (((((h) & 0x1f)) << 8) + (l))

#endif /* _I4B_W6692_H_ */
