/*	$FreeBSD: src/sys/dev/stg/tmc18c30reg.h,v 1.3.18.1 2008/11/25 02:59:29 kensmith Exp $	*/
/*	$NecBSD: tmc18c30reg.h,v 1.4.24.1 2001/06/08 06:27:50 honda Exp $	*/
/*	$NetBSD$	*/

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Kouichi Matsuda. All rights reserved.
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

#ifndef	_TMC18C30REG_H_
#define	_TMC18C30REG_H_

#define	tmc_wdata	0x00
#define	tmc_rdata	0x00

#define	tmc_bctl	0x01
#define		BCTL_BUSFREE	0x00
#define		BCTL_RST	0x01
#define		BCTL_SEL	0x02
#define		BCTL_BSY	0x04
#define		BCTL_ATN	0x08
#define		BCTL_IO		0x10
#define		BCTL_CD		0x20
#define		BCTL_MSG	0x40
#define		BCTL_BUSEN	0x80
#define	tmc_bstat	0x01
#define		BSTAT_BSY	0x01
#define		BSTAT_MSG	0x02
#define		BSTAT_IO	0x04
#define		BSTAT_CMD	0x08
#define		BSTAT_REQ	0x10
#define		BSTAT_SEL	0x20
#define		BSTAT_ACK	0x40

#define	tmc_ictl	0x02
#define		ICTL_FIFO	0x10
#define		ICTL_ARBIT	0x20
#define		ICTL_SEL	0x40
#define		ICTL_CD		0x80
#define		ICTL_ALLINT	(ICTL_ARBIT | ICTL_CD | ICTL_SEL | ICTL_FIFO)
#define	tmc_astat	0x02
#define		ASTAT_INT	0x01
#define		ASTAT_ARBIT	0x02
#define		ASTAT_PARERR	0x04
#define		ASTAT_SCSIRST	0x08
#define		ASTAT_STATMASK	0x0f
#define		ASTAT_FIFODIR	0x10
#define		ASTAT_FIFOEN	0x20
#define		ASTAT_PARENB	0x40
#define		ASTAT_BUSEN	0x80

#define	tmc_ssctl	0x03
#define		SSCTL_FSYNCHEN	0x40
#define		SSCTL_SYNCHEN	0x80
#define	tmc_fstat	0x03

#define	tmc_fctl	0x04
#define		FCTL_CLRFIFO	0x01
#define		FCTL_ARBIT	0x04
#define		FCTL_PARENB	0x08
#define		FCTL_INTEN	0x10
#define		FCTL_CLRINT	0x20
#define		FCTL_FIFOW	0x40
#define		FCTL_FIFOEN	0x80
#define	tmc_icnd	0x04

#define	tmc_mctl	0x05
#define	tmc_idlsb	0x05

#define	tmc_idmsb	0x06

#define	tmc_wlb		0x07
#define	tmc_rlb		0x07

#define	tmc_scsiid	0x08
#define	tmc_sdna	0x08

#define	tmc_istat	0x09
#define		ISTAT_INTEN	0x08
#define		ISTAT_FIFO	0x10
#define		ISTAT_ARBIT	0x20
#define		ISTAT_SEL	0x40
#define		ISTAT_CD	0x80

#define	tmc_cfg1	0x0a

#define	tmc_ioctl	0x0b
#define		IOCTL_IO32	0x80
#define	tmc_cfg2	0x0b

#define	tmc_wfifo	0x0c
#define	tmc_rfifo	0x0c

#define	tmc_fdcnt	0x0e

/* Information transfer phases */
#define	BUSFREE_PHASE		0x00
#define	DATA_OUT_PHASE		(BSTAT_BSY)
#define	DATA_IN_PHASE		(BSTAT_BSY|BSTAT_IO)
#define	COMMAND_PHASE		(BSTAT_CMD|BSTAT_BSY)
#define	STATUS_PHASE		(BSTAT_CMD|BSTAT_BSY|BSTAT_IO)
#define	MESSAGE_OUT_PHASE	(BSTAT_CMD|BSTAT_MSG|BSTAT_BSY)
#define	MESSAGE_IN_PHASE	(BSTAT_CMD|BSTAT_MSG|BSTAT_BSY|BSTAT_IO)
#define	PHASE_RESELECTED	(BSTAT_SEL|BSTAT_IO)

#define	BSTAT_PHMASK		(BSTAT_MSG | BSTAT_IO | BSTAT_CMD)
#define	PHASE_MASK		(BSTAT_SEL | BSTAT_BSY | BSTAT_PHMASK)
#define	RESEL_PHASE_MASK	(BSTAT_SEL | BSTAT_PHMASK)

#define	STG_IS_PHASE_DATA(st) \
	((((st) & PHASE_MASK) & ~BSTAT_IO) == BSTAT_BSY)

/* chip type */
#define	TMCCHIP_UNK		0x00
#define	TMCCHIP_1800		0x01
#define	TMCCHIP_18C50		0x02
#define	TMCCHIP_18C30		0x03

#define	STGIOSZ	0x10

#endif	/* !_TMC18C30REG_H_ */
