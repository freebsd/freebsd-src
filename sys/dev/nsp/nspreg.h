/*	$FreeBSD$	*/
/*	$NecBSD: nspreg.h,v 1.4.14.3 2001/06/29 06:27:53 honda Exp $	*/
/*	$NetBSD$	*/

/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef	_NSPREG_H_
#define	_NSPREG_H_

/* base registers */
#define	nsp_irqcr	0
#define	IRQCR_RESCL	0x01
#define	IRQCR_PHCL	0x02
#define	IRQCR_TIMERCL	0x04
#define	IRQCR_FIFOCL	0x08
#define	IRQCR_SCSIIDIS	0x10
#define	IRQCR_EXTIDIS	0x20
#define	IRQCR_TIMERIDIS	0x40
#define	IRQCR_FIFOIDIS	0x80
#define	IRQCR_ALLMASK	0xff
#define	IRQCR_IRQDIS	0xf0

#define	nsp_irqsr	0
#define	IRQSR_SCSI	0x01
#define	IRQSR_EXT	0x02
#define	IRQSR_TIMER	0x04
#define	IRQSR_FIFO	0x08
#define	IRQSR_MASK	0x0f

#define	nsp_ifselr	1
#define	IFSELR_IFSEL	0x01
#define	IFSELR_REGSEL	0x04

#define	nsp_fifosr	1
#define	FIFOSR_CHIPREVM	0x0f
#define	FIFOSR_CHIPIDM	0x70
#define	FIFOSR_FULLEMP	0x80

#define	nsp_idxr	2
#define	nsp_datar	3
#define	nsp_fifodr	4

/* indexed registers */
#define	NSPR_EXTBUSC	0x10

#define	NSPR_CLKDIVR	0x11
#define	CLKDIVR_40M	0x02
#define	CLKDIVR_20M	0x01

#define	NSPR_TERMPWRC	0x13
#define	TERMPWRC_POWON	0x01

#define	NSPR_SCIENR	0x15
#define	SCIENR_SCCHG	0x01
#define	SCIENR_RESEL	0x10
#define	SCIENR_FIFO	0x20
#define	SCIENR_RST	0x40

#define	NSPR_IRQPHS	0x16
#define	IRQPHS_LMSG	0x01
#define	IRQPHS_LIO	0x02
#define	IRQPHS_LCD	0x04
#define	IRQPHS_LBF	0x08
#define	IRQPHS_PCHG	0x10
#define	IRQPHS_RSEL	0x20
#define	IRQPHS_FIFO	0x40
#define	IRQPHS_RST	0x80
#define	IRQPHS_PHMASK   (IRQPHS_LCD | IRQPHS_LMSG | IRQPHS_LIO)

#define	NSPR_TIMERCNT	0x17

#define NSPR_SCBUSCR	0x18
#define	SCBUSCR_SEL	0x01
#define	SCBUSCR_RST	0x02
#define	SCBUSCR_DOUT	0x04
#define	SCBUSCR_ATN	0x08
#define	SCBUSCR_ACK	0x10
#define	SCBUSCR_BSY	0x20
#define	SCBUSCR_ADIR	0x40
#define	SCBUSCR_ACKEN	0x80

#define	NSPR_SCBUSMON	0x19
#define	SCBUSMON_MSG	0x01
#define	SCBUSMON_IO	0x02
#define	SCBUSMON_CD	0x04
#define	SCBUSMON_BSY	0x08
#define	SCBUSMON_ACK	0x10
#define	SCBUSMON_REQ	0x20
#define	SCBUSMON_SEL	0x40
#define SCBUSMON_ATN	0x80

#define	NSPR_SETARBIT	0x1A

#define	NSPR_ARBITS	0x1A
#define	ARBITS_EXEC	0x01
#define	ARBITS_CLR	0x02
#define	ARBITS_WIN	0x02
#define	ARBITS_FAIL	0x04
#define	ARBITS_RESEL	0x08

#define	NSPR_PARITYR	0x1B	/* (W/R) */
#define	PARITYR_ENABLE	0x01
#define	PARITYR_CLEAR	0x02
#define	PARITYR_PE	0x02

#define	NSPR_CMDCR	0x1C	/* (W) */
#define	CMDCR_PTCLR	0x01
#define	CMDCR_EXEC	0x02

#define	NSPR_RESELR	0x1C	/* (R) */
#define	NSPR_CMDDR	0x1D	/* (W/R) */

#define	NSPR_PTCLRR	0x1E	/* (W) */
#define	PTCLRR_PT	0x01
#define	PTCLRR_ACK	0x02
#define	PTCLRR_REQ	0x04
#define	PTCLRR_HOST	0x08
#define	PTCLRR_RSS	0x30
#define	PTCLRR_RSS_ACK	0x00
#define	PTCLRR_RSS_REQ	0x10
#define	PTCLRR_RSS_HOST	0x20

#define	NSPR_XFERCR	0x1E	/* (R) */

#define	NSPR_XFERMR	0x20
#define	XFERMR_MEM8	0x01
#define	XFERMR_MEM32	0x02
#define	XFERMR_ADR24	0x04
#define	XFERMR_ADR32	0x08
#define	XFERMR_IO8	0x10
#define	XFERMR_IO32	0x20
#define	XFERMR_XEN	0x40
#define	XFERMR_FIFOEN	0x80

#define	NSPR_SYNCR	0x21
#define	SYNCR_OFFM	0x0f
#define	SYNCR_PERM	0xf0
#define	SYNCR_PERS	4

#define	NSPR_DATA	0x22
#define	NSPR_DATAACK	0x23
#define	NSPR_OCR	0x26
#define	OCR_ROMEN	0x01
#define	OCR_TERMPWROUT	0x02
#define	OCR_TERMPWRS	0x04

#define NSPR_ACKWIDTH	0x27

/* SCBUSMON phase defs */
#define	SCBUSMON_FREE	0
#define	SCBUSMON_CMD \
	(SCBUSMON_BSY | SCBUSMON_CD | SCBUSMON_REQ)
#define	SCBUSMON_MSGIN \
	(SCBUSMON_BSY | SCBUSMON_MSG | SCBUSMON_IO | SCBUSMON_CD | SCBUSMON_REQ)
#define	SCBUSMON_MSGOUT \
	(SCBUSMON_BSY | SCBUSMON_MSG | SCBUSMON_CD | SCBUSMON_REQ)
#define	SCBUSMON_DATAIN \
	(SCBUSMON_BSY | SCBUSMON_IO | SCBUSMON_REQ)
#define	SCBUSMON_DATAOUT \
	(SCBUSMON_BSY | SCBUSMON_REQ)
#define	SCBUSMON_STATUS \
	(SCBUSMON_BSY | SCBUSMON_IO | SCBUSMON_CD | SCBUSMON_REQ)
#define	SCBUSMON_RESELECT \
	(SCBUSMON_SEL | SCBUSMON_IO)
#define	SCBUSMON_PHMASK \
	(SCBUSMON_SEL | SCBUSMON_CD | SCBUSMON_MSG | SCBUSMON_IO)

/* Data phase */
#define	NSP_IS_PHASE_DATA(ph) \
	((((ph) & SCBUSMON_PHMASK) & ~SCBUSMON_IO) == 0)
#define	NSP_IS_IRQPHS_DATA(ph) \
	((((ph) & IRQPHS_PHMASK) & ~SCBUSMON_IO) == 0)

/* SCSI phase */
#define	PHASE_CMD	(SCBUSMON_CMD & SCBUSMON_PHMASK)
#define	PHASE_DATAIN	(SCBUSMON_DATAIN & SCBUSMON_PHMASK)
#define	PHASE_DATAOUT	(SCBUSMON_DATAOUT & SCBUSMON_PHMASK)
#define	PHASE_STATUS	(SCBUSMON_STATUS & SCBUSMON_PHMASK)
#define	PHASE_MSGIN	(SCBUSMON_MSGIN & SCBUSMON_PHMASK)
#define	PHASE_MSGOUT	(SCBUSMON_MSGOUT & SCBUSMON_PHMASK)
#define	PHASE_SEL	(SCBUSMON_SEL | SCBUSMON_IO)

#define	IRQPHS_CMD	(IRQPHS_LCD)
#define	IRQPHS_DATAIN	(IRQPHS_LIO)
#define	IRQPHS_DATAOUT	(0)
#define	IRQPHS_STATUS	(IRQPHS_LCD | IRQPHS_LIO)
#define	IRQPHS_MSGIN	(IRQPHS_LCD | IRQPHS_LMSG | IRQPHS_LIO)
#define	IRQPHS_MSGOUT	(IRQPHS_LCD | IRQPHS_LMSG)

/* Size */
#define	NSP_MEMSIZE	NBPG
#define	NSP_IOSIZE	16
#define	NSP_BUFFER_SIZE	512
#endif	/* !_NSPREG_H_ */
