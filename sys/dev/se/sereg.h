/*-
 * Copyright (c) 2002 Jake Burkholder.
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
 * $FreeBSD$
 */

#ifndef _DEV_SE_SEREG_H_
#define	_DEV_SE_SEREG_H_

#define	SE_DIV(n, m)	(((m) << 6) | ((n) - 1))
#define	SE_DIV_9600	SE_DIV(48, 2)
#define	SE_DIV_19200	SE_DIV(48, 1)
#define	SE_DIV_38400	SE_DIV(24, 1)
#define	SE_DIV_115200	SE_DIV(8, 1)

#define	SE_CHA		0x0	/* channel a offset */
#define	SE_CHB		0x40	/* channel b offset */

#define	SE_RFIFO	0x0	/* receive fifo */
#define	SE_XFIFO	0x0	/* transmit fifo */

#define	SE_STAR		0x20	/* status register */
#define	STAR_CTS	0x2	/*  clear to send state */
#define	STAR_CEC	0x4	/*  command executing */
#define	STAR_TEC	0x8	/*  tic executing */
#define	STAR_FCS	0x10	/*  flow control status */
#define	STAR_RFNE	0x20	/*  receive fifo not empty */
#define	STAR_XFW	0x40	/*  transmit fifo write enable */
#define	STAR_XDOV	0x80	/*  transmit data overflow */

#define	SE_CMDR		0x20	/* command register */
#define	CMDR_XRES	0x1	/*  transmitter reset */
#define	CMDR_XF		0x8	/*  transmit frame */
#define	CMDR_STI	0x10	/*  start timer */
#define	CMDR_RFRD	0x20	/*  receive fifo read enable */
#define	CMDR_RRES	0x40	/*  reveiver reset */
#define	CMDR_RMC	0x80	/*  receive message complete */

#define	SE_MODE		0x22	/* mode register */
#define	MODE_TLP	0x1	/*  test loop */
#define	MODE_TRS	0x2	/*  timer resolution */
#define	MODE_RTS	0x4	/*  request to send */
#define	MODE_RAC	0x8	/*  receiver active */
#define	MODE_FLON	0x10	/*  flow control on */
#define	MODE_FCTS	0x20	/*  flow control using cts */
#define	MODE_FRTS	0x40	/*  flow control using rts */

#define	SE_TIMR		0x23	/* timer register */
#define	SE_XON		0x24	/* xon character */
#define	SE_XOFF		0x25	/* xoff character */
#define	SE_TCR		0x26	/* transmit character register */

#define	SE_DAFO		0x27	/* data format */
#define	DAFO_CHL	0x2	/*  character length */
#define	DAFO_CHL_8	0x0	/*   8 bits */
#define	DAFO_CHL_7	0x1	/*   7 bits */
#define	DAFO_CHL_6	0x2	/*   6 bits */
#define	DAFO_CHL_5	0x3	/*   5 bits */
#define	DAFO_PARE	0x4	/*  parity enable */
#define	DAFO_PAR	0x18	/*  parity format */
#define	DAFO_STOP	0x20	/*  stop bit */
#define	DAFO_XBRK	0x40	/*  transmit break */

#define	SE_RFC		0x28	/* rfifo control register */
#define	RFC_TCDE	0x1	/*  termination character detection enable */
#define	RFC_RFTH	0xc	/*  rfifo threshold level */
#define	RFC_RFTH_2	0x0	/*   2 bytes */
#define	RFC_RFTH_4	0x4	/*   4 bytes */
#define	RFC_RFTH_16	0x8	/*   16 bytes */
#define	RFC_RFTH_32	0xc	/*   32 bytes */
#define	RFC_RFDF	0x10	/*  rfifo data format */
#define	RFC_DXS		0x20	/*  disable storage of xon/xoff characters */
#define	RFC_DPS		0x40	/*  disable parity storage */

#define	SE_RBCL		0x2a	/* receive byte count low */
#define	SE_XBCL		0x2a	/* transmit byte count low */
#define	SE_RBCH		0x2b	/* receive byte count high */
#define	SE_XBCH		0x2b	/* transmit byte count high */

#define	SE_CCR0		0x2c	/* channel configuration register 0 */
#define	CCR0_SM		0x3	/*  serial mode */
#define	CCR0_SM_HDLC	0x0	/*   hdlc/sdlc mode */
#define	CCR0_SM_SDLC	0x1	/*   sdlc loop mode */
#define	CCR0_SM_BISYNC	0x2	/*   bisync mode */
#define	CCR0_SM_ASYNC	0x3	/*   async mode */
#define	CCR0_SC		0x1c	/*  serial configuration */
#define	CCR0_SC_NRZ	0x0	/*   nrz data encoding */
#define	CCR0_SC_NRZI	0x2	/*   nrzi data encoding */
#define	CCR0_SC_FM0	0x4	/*   fm0 data encoding */
#define	CCR0_SC_FM1	0x5	/*   fm1 data encoding */
#define	CCR0_SC_MCHSTR	0x6	/*   manchester data encoding */
#define	CCR0_MCE	0x40	/*  master clock enable */
#define	CCR0_PU		0x80	/*  power up */

#define	SE_CCR1		0x2d	/* channel configuration register 1 */
#define	CCR1_CM		0x7	/*  clock mode */
#define	CCR1_CM_7	0x7	/*   clock mode 7 */
#define	CCR1_BCR	0x8	/*  bit clock rate */
#define	CCR1_ODS	0x10	/*  output driver select */

#define	SE_CCR2		0x2e	/* channel configuration register 2 */
#define	CCR2_DIV	0x1	/*  data inversion */
#define	CCR2_RWX	0x4	/*  read/write exchange */
#define	CCR2_TOE	0x8	/*  txclk ouput enable */
#define	CCR2_SSEL	0x10	/*  clock source select */
#define	CCR2_BDF	0x20	/*  baud rate division factor */
#define	CCR2_BR8	0x40	/*  baud rate 8 */
#define	CCR2_BR9	0x80	/*  baud rate 9 */
#define	CCR2_RCS0	0x10	/*  receive clock shift 0 (5) */
#define	CCR2_XCS0	0x20	/*  transmit clock shift 0 (5) */
#define	CCR2_SOC0	0x40	/*  special output control 0 (0a, 1, 4, 5) */
#define	CCR2_SOC1	0x80	/*  special output control 1 (0a, 1, 4, 5) */

#define	SE_CCR3		0x2f	/* channel configuration register 3 */
#define	CCR3_PSD	0x1	/*  dpll phase shift disable */

#define	SE_TSAX		0x30	/* transmit timeslot assignment register */
#define	SE_TSAR		0x31	/* receive timeslot assignment register */
#define	SE_XCCR		0x32	/* transmit channel capacity register */
#define	SE_RCCR		0x33	/* receive channel capacity register */

#define	SE_VSTR		0x34	/* version status register */
#define	VSTR_VN		0xf	/*  version number 0 */
#define	VSTR_DPLA	0x40	/*  dpll asynchronous */
#define	VSTR_CD		0x80	/*  carrier detect */

#define	SE_BGR		0x34	/* baud rate generator register */
#define	SE_TIC		0x35	/* trasmit immediate character */
#define	SE_MXN		0x36	/* mask xon character */
#define	SE_MXF		0x37	/* mask xoff character */

#define	SE_GIS		0x38	/* global interrupt status */
#define	GIS_ISB0	0x1	/*  interrupt status channel B 0 */
#define	GIS_ISB1	0x2	/*  interrupt status channel B 1 */
#define	GIS_ISA0	0x4	/*  interrupt status channel A 0 */
#define	GIS_ISA1	0x8	/*  interrupt status channel A 1 */
#define	GIS_PI		0x80	/*  univerisal port interrupt */

#define	SE_IVA		0x38	/* interrupt vector address */

#define	SE_IPC		0x39	/* interrupt port configuration */
#define	IPC_IC0		0x1	/*  interrupt configuration 0 */
#define	IPC_IC1		0x2	/*  interrupt configuration 1 */
#define	IPC_CASM	0x4	/*  cascading mode */
#define	IPC_SLA0	0x8	/*  slave address 0 */
#define	IPC_SLA1	0x10	/*  slave address 1 */
#define	IPC_VIS		0x80	/*  masked interrupts visible */

#define	SE_ISR0		0x3a	/* interrupt status 0 */
#define	ISR0_RPF	0x1	/*  receive pool full */
#define	ISR0_RFO	0x2	/*  receive frame overflow */
#define	ISR0_CDSC	0x4	/*  carrier detect status change */
#define	ISR0_PLLA	0x8	/*  dpll asynchronous */
#define	ISR0_FERR	0x10	/*  framing error */
#define	ISR0_PERR	0x20	/*  parity error */
#define	ISR0_TIME	0x40	/*  time out */
#define	ISR0_TCD	0x80	/*  termination character detected */

#define	SE_IMR0		0x3a	/* interrupt mask 0 */

#define	SE_ISR1		0x3b	/* interrupt status 1 */
#define	ISR1_XPR	0x1	/*  transmit pool ready */
#define	ISR1_XON	0x2	/*  transmit message repeat */
#define	ISR1_CSC	0x4	/*  clear to send status change */
#define	ISR1_TIN	0x8	/*  timer interrupt */
#define	ISR1_XOFF	0x10	/*  xoff character detected */
#define	ISR1_ALLS	0x20	/*  all sent */
#define	ISR1_BRKT	0x40	/*  break terminated */
#define	ISR1_BRK	0x80	/*  break */

#define	SE_IMR1		0x3b	/* interrupt mask 1 */
#define	SE_PVR		0x3c	/* port value register */
#define	SE_PIS		0x3d	/* port interrupt status */
#define	SE_PIM		0x3d	/* port interrupt mask */
#define	SE_PCR		0x3e	/* port configuration register */

#define	SE_CCR4		0x3f	/* channel configuration register 4 */
#define	CCR4_ICD	0x10	/*  invert polarity of carrier detect signal */
#define	CCR4_TST1	0x20	/*  test pin */
#define	CCR4_EBRG	0x40	/*  enhanced baud rate generator mode */
#define	CCR4_MCK4	0x80	/*  master clock divide by 4 */

#endif
