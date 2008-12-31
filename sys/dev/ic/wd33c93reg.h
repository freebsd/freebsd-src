/* $FreeBSD: src/sys/dev/ic/wd33c93reg.h,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $ */
/*	$NecBSD: wd33c93reg.h,v 1.21.24.1 2001/06/13 05:52:05 honda Exp $	*/
/*	$NetBSD$	*/
/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *  Copyright (c) 1996, 1997, 1998
 *	Naofumi Honda. All rights reserved.
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

#ifndef	_WD33C93REG_H_
#define	_WD33C93REG_H_

/* wd33c93 register */
#define	wd3s_oid		0x00
#define	IDR_FS_16_20		0x80
#define	IDR_FS_12_15		0x40
#define	IDR_FS_8_10		0x00
#define	IDR_RAF			0x20
#define	IDR_EHP			0x10
#define	IDR_EAF			0x08
#define	IDR_IDM			0x07

#define	wd3s_ctrl		0x01
#define	CR_DMA			0x80
#define	CR_DMAD			0x40
#define	CR_HLT_HOST_PARITY	0x10
#define	CR_DIS_INT		0x08
#define	CR_IDIS_INT		0x04
#define	CR_HLT_ATN		0x02
#define	CR_HLT_BUS_PARITY	0x01
#define	CR_DEFAULT		(CR_DIS_INT | CR_IDIS_INT)
#define	CR_DEFAULT_HP		(CR_DEFAULT | CR_HLT_BUS_PARITY)

#define	wd3s_tout		0x02
#define	wd3s_cdb		0x03
#define	wd3s_lun		0x0f
#define	wd3s_cph		0x10
#define	wd3s_synch		0x11
#define	wd3s_cnt		0x12
#define	wd3s_did		0x15
#define	DIDR_SCC		0x80
#define	DIDR_DPD		0x40

#define	wd3s_sid		0x16
#define	SIDR_RESEL		0x80
#define	SIDR_SEL		0x40
#define	SIDR_VALID		0x08
#define	SIDR_IDM		0x07

#define	wd3s_stat		0x17

#define	BSR_CM			0xf0
#define	BSR_CMDCPL		0x10
#define	BSR_CMDABT		0x20
#define	BSR_CMDERR		0x40
#define	BSR_CMDREQ		0x80

#define	BSR_SM			0x0f
#define	BSR_PM			0x07
#define	BSR_PHVALID		0x08
#define	BSR_IOR			0x01
#define	BSR_DATAOUT		0x00
#define	BSR_DATAIN		0x01
#define	BSR_CMDOUT		0x02
#define	BSR_STATIN		0x03
#define	BSR_UNSPINFO0		0x04
#define	BSR_UNSPINFO1		0x05
#define	BSR_MSGOUT		0x06
#define	BSR_MSGIN		0x07

#define	BSR_RESET		0x00
#define	BSR_AFM_RESET		0x01
#define	BSR_SELECTED		0x11
#define	BSR_SATFIN		0x16
#define	BSR_ACKREQ		0x20
#define	BSR_SATSDP		0x21
#define	BSR_RESEL		0x80
#define	BSR_AFM_RESEL		0x81
#define	BSR_DISC		0x85

#define	wd3s_cmd		0x18
#define	wd3s_data		0x19
#define	wd3s_qtag		0x1a

#define	wd3s_mbank		0x30
#define	MBR_RST			0x02
#define	MBR_IEN			0x04

#define	wd3s_mwin		0x31
#define	wd3s_auxc		0x33
#define	AUXCR_HIDM		0x07
#define	AUXCR_INTM		0x38
#define	AUXCR_RRST		0x80

/* status port */
#define	STR_INT			0x80
#define	STR_LCI			0x40
#define	STR_BSY			0x20
#define	STR_CIP			0x10
#define	STR_PE			0x02
#define	STR_DBR			0x01
#define	STR_BUSY		0xf0

/* cmd port */
#define	CMDP_DMES		0x01
#define	CMDP_DMER		0x02
#define	CMDP_TCMS		0x04
#define	CMDP_TCMR		0x08
#define	CMDP_TCIR		0x10

/* wd33c93 chip cmds */
#define	WD3S_SBT		0x80
#define	WD3S_RESET		0x00
#define	WD3S_ABORT		0x01
#define	WD3S_ASSERT_ATN		0x02
#define	WD3S_NEGATE_ACK		0x03
#define	WD3S_DISCONNECT		0x04
#define	WD3S_RESELECT		0x05
#define	WD3S_SELECT_ATN		0x06
#define	WD3S_SELECT_NO_ATN	0x07
#define	WD3S_SELECT_ATN_TFR	0x08
#define	WD3S_SELECT_NO_ATN_TFR	0x09
#define	WD3S_RESELECT_RCV_DATA	0x0a
#define	WD3S_RESELECT_SEND_DATA	0x0b
#define	WD3S_WAIT_SELECT_RCV	0x0c
#define	WD3S_CMD_COMPSEQ	0x0d
#define	WD3S_SEND_DISC_MSG	0x0e
#define	WD3S_SET_IDI		0x0f
#define	WD3S_RCV_CMD		0x10
#define	WD3S_RCV_DATA		0x11
#define	WD3S_RCV_MSG_OUT	0x12
#define	WD3S_RCV_UNSP_INFO_OUT	0x13
#define	WD3S_SEND_STATUS	0x14
#define	WD3S_SEND_DATA		0x15
#define	WD3S_SEND_MSG_IN	0x16
#define	WD3S_SEND_UNSP_INFO_IN	0x17
#define	WD3S_TRANSLATE_ADDRESS	0x18
#define	WD3S_TFR_INFO		0x20

#endif	/* !_WD33C93REG_H_ */
