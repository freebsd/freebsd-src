/*
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * Author: Hartmut Brandt <harti@freebsd.org>
 *
 * $FreeBSD$
 *
 * Register definitions for the following chips:
 *	IDT 77105
 */
#ifndef _DEV_UTOPIA_IDTPHY_H
#define	_DEV_UTOPIA_IDTPHY_H

#define	IDTPHY_REGO_MCR		0x00
#define	IDTPHY_REGN_MCR		"Master Control Register"
#define	IDTPHY_REGX_MCR		"\020\010UPLO\7DREC\6ECEI\5TDPC\4DRIC\3HALTTX\2BYTEM\1EI"
#define	IDTPHY_REGM_MCR_UPL0	0x80
#define	IDTPHY_REGM_MCR_DREC	0x40
#define	IDTPHY_REGM_MCR_ECEI	0x20
#define	IDTPHY_REGM_MCR_TDPC	0x10
#define	IDTPHY_REGM_MCR_DRIC	0x08
#define	IDTPHY_REGM_MCR_HALTTX	0x04
#define	IDTPHY_REGM_MCR_BYTEM	0x02
#define	IDTPHY_REGM_MCR_EI	0x01

#define	IDTPHY_REGO_ISTAT	0x01
#define	IDTPHY_REGN_ISTAT	"Interrupt Status"
#define	IDTPHY_REGX_ISTAT	"\020\7GOOD\6HECE\5SCRE\4TPE\3RSCC\2RSE\1RFO"
#define	IDTPHY_REGM_ISTAT_GOOD	0x40	/* good signal bit */
#define	IDTPHY_REGM_ISTAT_HECE	0x20	/* HEC error */
#define	IDTPHY_REGM_ISTAT_SCRE	0x10	/* short cell received error */
#define	IDTPHY_REGM_ISTAT_TPE	0x08	/* transmit parity error */
#define	IDTPHY_REGM_ISTAT_RSCC	0x04	/* receive signal condition change */
#define	IDTPHY_REGM_ISTAT_RSE	0x02	/* receive symbol error */
#define	IDTPHY_REGM_ISTAT_RFO	0x01	/* read FIFO overrun */

#define	IDTPHY_REGO_DIAG	0x02
#define	IDTPHY_REGN_DIAG	"Diagnostic Control"
#define	IDTPHY_REGX_DIAG	"\020\010FTD\7ROS\6MULTI\5RFLUSH\4ITPE\3IHECE\11\3\0NORM\11\3\2PLOOP\11\3\3LLOOP"
#define	IDTPHY_REGM_DIAG_FTD	0x80	/* Force TxClav Deassert */
#define	IDTPHY_REGM_DIAG_ROS	0x40	/* RxClav Operation Select */
#define	IDTPHY_REGM_DIAG_MULTI	0x20	/* Multi-phy operation */
#define	IDTPHY_REGM_DIAG_RFLUSH	0x10	/* clear receive Fifo */
#define	IDTPHY_REGM_DIAG_ITPE	0x08	/* insert transmit payload error */
#define	IDTPHY_REGM_DIAG_IHECE	0x04	/* insert transmit HEC error */
#define	IDTPHY_REGM_DIAG_LOOP	0x03	/* loopback mode */
#define	IDTPHY_REGM_DIAG_LOOP_NONE	0x00	/* normal */
#define	IDTPHY_REGM_DIAG_LOOP_PHY	0x02	/* PHY loopback */
#define	IDTPHY_REGM_DIAG_LOOP_LINE	0x03	/* Line loopback */

#define	IDTPHY_REGO_LHEC	0x03
#define	IDTPHY_REGN_LHEC	"LED Driver and HEC Status/Control"
#define	IDTPHY_REGX_LHEC	"\020\7DRHEC\6DTHEC\11\x18\0CYC1\11\x18\1CYC2\11\x18\2CYC4\11\x18\3CYC8\3FIFOE\2TXLED\1RXLED"
#define	IDTPHY_REGM_LHEC_DRHEC	0x40	/* disable receive HEC */
#define	IDTPHY_REGM_LHEC_DTHEC	0x20	/* disable transmit HEC */
#define	IDTPHY_REGM_LHEC_RXREF	0x18	/* RxRef pulse width */
#define	IDTPHY_REGM_LHEC_RXREF1	0x00	/* 1 pulse */
#define	IDTPHY_REGM_LHEC_RXREF2	0x08	/* 2 pulse */
#define	IDTPHY_REGM_LHEC_RXREF4	0x10	/* 4 pulse */
#define	IDTPHY_REGM_LHEC_RXREF8	0x18	/* 8 pulse */
#define	IDTPHY_REGM_LHEC_FIFOE	0x04	/* Fifo empty */
#define	IDTPHY_REGM_LHEC_TXLED	0x02	/* Tx LED status */
#define	IDTPHY_REGM_LHEC_RXLED	0x01	/* Rx LED status */

#define	IDTPHY_REGO_CNT		0x04	/* +0x05 */
#define	IDTPHY_REGN_CNT		"Counter"

#define	IDTPHY_REGO_CNTS	0x06
#define	IDTPHY_REGN_CNTS	"Counter select"
#define	IDTPHY_REGX_CNTS	"\020\4SEC\3TX\2RX\1HECE"
#define	IDTPHY_REGM_CNTS_SEC	0x08	/* symbol error counter */
#define	IDTPHY_REGM_CNTS_TX	0x04	/* Tx cells */
#define	IDTPHY_REGM_CNTS_RX	0x02	/* Rx cells */
#define	IDTPHY_REGM_CNTS_HECE	0x01	/* HEC errors */

#define	IDTPHY_PRINT_77105					\
	{ /* 00 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_MCR,		\
	  IDTPHY_REGN_MCR,	IDTPHY_REGX_MCR },		\
	{ /* 01 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_ISTAT,		\
	  IDTPHY_REGN_ISTAT,	IDTPHY_REGX_ISTAT },		\
	{ /* 02 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_DIAG,		\
	  IDTPHY_REGN_DIAG,	IDTPHY_REGX_DIAG },		\
	{ /* 03 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_LHEC,		\
	  IDTPHY_REGN_LHEC,	IDTPHY_REGX_LHEC },		\
	{ /* 04, 05 */						\
	  UTP_REGT_INT16,	IDTPHY_REGO_CNT,		\
	  IDTPHY_REGN_CNT,	NULL },				\
	{ /* 06 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_CNTS,		\
	  IDTPHY_REGN_CNTS,	IDTPHY_REGX_CNTS }

#endif	/* _DEV_UTOPIA_IDTPHY_H */
