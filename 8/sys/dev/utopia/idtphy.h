/*-
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
 *	IDT 77155
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

#define	IDTPHY_REGO_MRID	0x00
#define	IDTPHY_REGN_MRID	"Master Reset & ID"
#define	IDTPHY_REGM_MRID_RESET	0x80	/* software reset */
#define	IDTPHY_REGM_MRID_TYPE	0x70	/* type */
#define	IDTPHY_REGM_MRID_155	0x30	/* idt77155 type */
#define	IDTPHY_REGM_MRID_ID	0x0f	/* revision */
#define	IDTPHY_REGX_MRID	\
	    "\020\010mstReset\12\x70\12type\12\xf\12id"

#define	IDTPHY_REGO_CONF	0x01
#define	IDTPHY_REGN_CONF	"Configuration"
#define	IDTPHY_REGM_CONF_AFEBE	0x40	/* autoFEBE */
#define	IDTPHY_REGM_CONF_ALRDI	0x20	/* autoLRDI */
#define	IDTPHY_REGM_CONF_APRDI	0x10	/* autoPRDI */
#define	IDTPHY_REGM_CONF_TCAIN	0x08	/* TCAInv */
#define	IDTPHY_REGM_CONF_RCAIN	0x04	/* RCAInv */
#define	IDTPHY_REGM_CONF_RXDIN	0x02	/* RXDInv */
#define	IDTPHY_REGM_CONF_RESV	0x81
#define	IDTPHY_REGX_CONF	\
	    "\020\07autoFEBE\6autoLRDI\5autoPRDI\4TCAInv\3RCAInv\2RXDInv"

#define	IDTPHY_REGO_INT		0x02
#define	IDTPHY_REGN_INT		"Interrupt"
#define	IDTPHY_REGM_INT_TXOOLI	0x80	/* txOOLInt */
#define	IDTPHY_REGM_INT_RXLOCI	0x40	/* rxLOCInt */
#define	IDTPHY_REGM_INT_RXOOLI	0x20	/* rxOOLInt */
#define	IDTPHY_REGM_INT_TXCDI	0x10	/* txCDi */
#define	IDTPHY_REGM_INT_RXCDI	0x08	/* rxCDi */
#define	IDTPHY_REGM_INT_RXPOHI	0x04	/* rxPOHi */
#define	IDTPHY_REGM_INT_RXLOHI	0x02	/* rxLOHi */
#define	IDTPHY_REGM_INT_RXSOHI	0x01	/* rxSOHi */
#define	IDTPHY_REGX_INT		\
	    "\020\10txOOLInt\7rxLOCInt\6rxOOLInt\5txCDi\4rxCDi\3rxPOHi" \
	    "\2rxLOHi\1rxSOHi"

#define	IDTPHY_REGO_MCM		0x04
#define	IDTPHY_REGN_MCM		"Master Clock Monitor"
#define	IDTPHY_REGM_MCM_RRCLK	0x08	/* rrclkReg */
#define	IDTPHY_REGM_MCM_TRCLK	0x04	/* trclkReg */
#define	IDTPHY_REGM_MCM_RCLK	0x02	/* rclkReg */
#define	IDTPHY_REGM_MCM_TCLK	0x01	/* tclkReg */
#define	IDTPHY_REGM_MCM_RESV	0xf0
#define	IDTPHY_REGX_MCM		\
	    "\020\4rrclkReg\3trclkReg\2rclkReg\1tclkReg"

#define	IDTPHY_REGO_MCTL	0x05
#define	IDTPHY_REGN_MCTL	"Master Control"
#define	IDTPHY_REGM_MCTL_LOCI	0x80	/* rxLOCIEn */
#define	IDTPHY_REGM_MCTL_LOC	0x40	/* LOC */
#define	IDTPHY_REGM_MCTL_FIXP	0x20	/* txFixptr */
#define	IDTPHY_REGM_MCTL_LLOOP	0x04	/* txLLoop */
#define	IDTPHY_REGM_MCTL_DLOOP	0x02	/* rxDLoop */
#define	IDTPHY_REGM_MCTL_TLOOP	0x01	/* rxLoopT */
#define	IDTPHY_REGM_MCTL_RESV	0x18
#define	IDTPHY_REGX_MCTL	\
	    "\020\10rxLOCIEn\7LOC\6txFixptr\3txLLoop\2rxDLoop\1rxLoopT"

#define	IDTPHY_REGO_TXC		0x06
#define	IDTPHY_REGN_TXC		"Transmit Clock Synthesis C/S"
#define	IDTPHY_REGM_TXC_TXOOL	0x08	/* txOOL */
#define	IDTPHY_REGM_TXC_TXOOLI	0x02	/* txOOLIEn */
#define	IDTPHY_REGM_TXC_TXREF	0x01	/* txrefSel */
#define	IDTPHY_REGM_TXC_RESV	0xf4
#define	IDTPHY_REGX_TXC		\
	    "\020\4txOOL\2txOOLIEn\1txrefSel"

#define	IDTPHY_REGO_RXC		0x07
#define	IDTPHY_REGN_RXC		"Receive Clock/Data Recovery C/S"
#define	IDTPHY_REGM_RXC_RXOOL	0x08	/* rxOOL */
#define	IDTPHY_REGM_RXC_RXOOLI	0x02	/* rxOOLIEn */
#define	IDTPHY_REGM_RXC_RXREF	0x01	/* rxrefSel */
#define	IDTPHY_REGM_RXC_RESV	0xf4
#define	IDTPHY_REGX_RXC		\
	    "\020\4rxOOL\2rxOOLIEn\1rxrefSel"

#define	IDTPHY_REGO_RSOC	0x10
#define	IDTPHY_REGN_RSOC	"Receive Overhead Control"
#define	IDTPHY_REGM_RSOC_DSCR	0x40	/* scrDis */
#define	IDTPHY_REGM_RSOC_FOOF	0x20	/* frcOOF */
#define	IDTPHY_REGM_RSOC_B1IE	0x08	/* B1ErrIEn */
#define	IDTPHY_REGM_RSOC_LOSI	0x04	/* LOSIEn */
#define	IDTPHY_REGM_RSOC_LOFI	0x02	/* LOFIEn */
#define	IDTPHY_REGM_RSOC_OOFI	0x01	/* OOFIEn */
#define	IDTPHY_REGM_RSOC_RESV	0x90
#define	IDTPHY_REGX_RSOC	\
	    "\020\7scrDis\6frcOOF\4B1ErrIEn\3LOSIEn\2LOFIEn\1OOFIEn"

#define	IDTPHY_REGO_RSOS	0x11
#define	IDTPHY_REGN_RSOS	"Receive Overhead Status"
#define	IDTPHY_REGM_RSOS_C1INT	0x80	/* C1Int */
#define	IDTPHY_REGM_RSOS_B1INT	0x40	/* B1ErrInt */
#define	IDTPHY_REGM_RSOS_LOSI	0x20	/* LOSInt */
#define	IDTPHY_REGM_RSOS_LOFI	0x10	/* LOFInt */
#define	IDTPHY_REGM_RSOS_OOFI	0x08	/* OOFInt */
#define	IDTPHY_REGM_RSOS_LOS	0x04	/* LOS */
#define	IDTPHY_REGM_RSOS_LOF	0x02	/* LOF */
#define	IDTPHY_REGM_RSOS_OOF	0x01	/* OOF */
#define	IDTPHY_REGX_RSOS	\
	    "\020\10C1Int\7B1ErrInt\6LOSInt\5LOFInt\4OOFint\3LOS\2LOF\1OOF"

#define	IDTPHY_REGO_BIPC	0x12	/* + 0x13 LE */
#define	IDTPHY_REGN_BIPC	"Receive Section BIP Errors"

#define	IDTPHY_REGO_TSOC	0x14
#define	IDTPHY_REGN_TSOC	"Transmit Overhead Control"
#define	IDTPHY_REGM_TSOC_DSCR	0x40	/* scrDis */
#define	IDTPHY_REGM_TSOC_LAISI	0x01	/* LAISIns */
#define	IDTPHY_REGM_TSOC_RESV	0xbe
#define	IDTPHY_REGX_TSOC	\
	    "\020\7scrDis\1LAISIns"

#define	IDTPHY_REGO_TSOC2	0x15
#define	IDTPHY_REGN_TSOC2	"Transmit Overhead Control 2"
#define	IDTPHY_REGM_TSOC2_LOSI	0x04	/* LOSIns */
#define	IDTPHY_REGM_TSOC2_B1INV	0x02	/* B1Inv */
#define	IDTPHY_REGM_TSOC2_IFE	0x01	/* frErrIns */
#define	IDTPHY_REGM_TSOC2_RESV	0xf8
#define	IDTPHY_REGX_TSOC2	\
	    "\020\3LOSIns\2B1Inv\1frErrIns"

#define	IDTPHY_REGO_RLOS	0x18
#define	IDTPHY_REGN_RLOS	"Receive Line Overhead Status"
#define	IDTPHY_REGM_RLOS_B2W	0x80	/* B2Word */
#define	IDTPHY_REGM_RLOS_LAIS	0x02	/* LAIS */
#define	IDTPHY_REGM_RLOS_LRDI	0x01	/* LRDI */
#define	IDTPHY_REGM_RLOS_RESV	0x7c
#define	IDTPHY_REGX_RLOS	\
	    "\020\10B2Word\2LAIS\1LRDI"

#define	IDTPHY_REGO_RLOI	0x19
#define	IDTPHY_REGN_RLOI	"Receive Line Overhead Interrupt"
#define	IDTPHY_REGM_RLOI_LFEBEE	0x80	/* LFEBEIEn */
#define	IDTPHY_REGM_RLOI_B2EE	0x40	/* B2ErrIEn */
#define	IDTPHY_REGM_RLOI_LAISE	0x20	/* LAISIEn */
#define	IDTPHY_REGM_RLOI_LRDIE	0x10	/* LRDIIEn */
#define	IDTPHY_REGM_RLOI_LFEBEI	0x08	/* LFEBEInt */
#define	IDTPHY_REGM_RLOI_B2EI	0x04	/* B2ErrInt */
#define	IDTPHY_REGM_RLOI_LAISI	0x02	/* LAISInt */
#define	IDTPHY_REGM_RLOI_LRDII	0x01	/* LRDIInt */
#define	IDTPHY_REGX_RLOI	\
	    "\020\10LFEBEIEn\7B2ErrIEn\6LAISIEn\5LRDIIEn\4LFEBEInt\3B2ErrInt" \
	    "\2LAISInt\1LRDIInt"

#define	IDTPHY_REGO_B2EC	0x1a	/* + 0x1b, 0x1c, 20bit LE */
#define	IDTPHY_REGN_B2EC	"B2 Errors"

#define	IDTPHY_REGO_FEBEC	0x1d	/* + 0x1e, 0x1f, 20bit LE */
#define	IDTPHY_REGN_FEBEC	"Line FEBE Errors"

#define	IDTPHY_REGO_TLOS	0x20
#define	IDTPHY_REGN_TLOS	"Transmit Line Overhead Status"
#define	IDTPHY_REGM_TLOS_LRDI	0x01	/* LRDI */
#define	IDTPHY_REGM_TLOS_RESV	0xfe
#define	IDTPHY_REGX_TLOS	\
	    "\020\1LRDI"

#define	IDTPHY_REGO_TLOC	0x21
#define	IDTPHY_REGN_TLOC	"Transmit Line Overhead Control"
#define	IDTPHY_REGM_TLOC_B2INV	0x01	/* B2Inv */
#define	IDTPHY_REGM_TLOC_RESV	0xfe
#define	IDTPHY_REGX_TLOC	\
	    "\020\1B2Inv"

#define	IDTPHY_REGO_TK1		0x24
#define	IDTPHY_REGN_TK1		"Transmit K1"

#define	IDTPHY_REGO_TK2		0x25
#define	IDTPHY_REGN_TK2		"Transmit K2"

#define	IDTPHY_REGO_RK1		0x26
#define	IDTPHY_REGN_RK1		"Receive K1"

#define	IDTPHY_REGO_RK2		0x27
#define	IDTPHY_REGN_RK2		"Receive K2"

#define	IDTPHY_REGO_RPOS	0x30
#define	IDTPHY_REGN_RPOS	"Receive Path Overhead Status"
#define	IDTPHY_REGM_RPOS_LOP	0x20	/* LOP */
#define	IDTPHY_REGM_RPOS_PAIS	0x08	/* PAIS */
#define	IDTPHY_REGM_RPOS_PRDI	0x04	/* PRDI */
#define	IDTPHY_REGM_RPOS_RESV	0xd3
#define	IDTPHY_REGX_RPOS	\
	    "\020\6LOP\4PAIS\3PRDI"

#define	IDTPHY_REGO_RPOI	0x31
#define	IDTPHY_REGN_RPOI	"Receive Path Overhead Interrupt"
#define	IDTPHY_REGM_RPOI_C2I	0x80	/* C2Int */
#define	IDTPHY_REGM_RPOI_LOPI	0x20	/* LOPInt */
#define	IDTPHY_REGM_RPOI_PAISI	0x08	/* PAISInt */
#define	IDTPHY_REGM_RPOI_PRDII	0x04	/* PRDIInt */
#define	IDTPHY_REGM_RPOI_B3EI	0x02	/* B3ErrInt */
#define	IDTPHY_REGM_RPOI_PFEBEI	0x01	/* PFEBEInt */
#define	IDTPHY_REGM_RPOI_RESV	0x50
#define	IDTPHY_REGX_RPOI	\
	    "\020\10C2Int\6LOPInt\4PAISInt\3PRDIInt\2B3ErrInt\1PFEBEInt"

#define	IDTPHY_REGO_RPIE	0x33
#define	IDTPHY_REGN_RPIE	"Receive Path Interrupt Enable"
#define	IDTPHY_REGM_RPIE_C2E	0x80	/* C2IEn */
#define	IDTPHY_REGM_RPIE_LOPE	0x20	/* LOPIEn */
#define	IDTPHY_REGM_RPIE_PAISE	0x08	/* PAISIEn */
#define	IDTPHY_REGM_RPIE_PRDIE	0x04	/* PRDIIEn */
#define	IDTPHY_REGM_RPIE_B3EE	0x02	/* B3ErrIEn */
#define	IDTPHY_REGM_RPIE_PFEBEE	0x01	/* PFEBEIEn */
#define	IDTPHY_REGM_RPIE_RESV	0x50
#define	IDTPHY_REGX_RPIE	\
	    "\020\10CSIEn\6LOPIEn\4PAISIEn\3PRDIIEn\2B3ErrIEn\1PFEBEIEn"

#define	IDTPHY_REGO_RC2		0x37
#define	IDTPHY_REGN_RC2		"Receive C2"

#define	IDTPHY_REGO_B3EC	0x38	/* + 0x39, LE, 16bit */
#define	IDTPHY_REGN_B3EC	"B3 Errors"

#define	IDTPHY_REGO_PFEBEC	0x3a	/* + 0x3b, LE, 16bit */
#define	IDTPHY_REGN_PFEBEC	"Path FEBE Errors"

#define	IDTPHY_REGO_RPEC	0x3d
#define	IDTPHY_REGN_RPEC	"Receive Path BIP Error Control"
#define	IDTPHY_REGM_RPEC_B3C	0x20	/* blkBIP */
#define	IDTPHY_REGM_RPEC_RESV	0xdf
#define	IDTPHY_REGX_RPEC	\
	    "\020\6blkBIP"

#define	IDTPHY_REGO_TPOC	0x40
#define	IDTPHY_REGN_TPOC	"Transmit Path Control"
#define	IDTPHY_REGM_TPOC_B3INV	0x02	/* B3Inv */
#define	IDTPHY_REGM_TPOC_PAISI	0x01	/* PAISIns */
#define	IDTPHY_REGM_TPOC_RESC	0xfc
#define	IDTPHY_REGX_TPOC	\
	   "\020\2B3Inv\1PAISIns"

#define	IDTPHY_REGO_TPTC	0x41
#define	IDTPHY_REGN_TPTC	"Transmit Pointer Control"
#define	IDTPHY_REGM_TPTC_FPTR	0x40	/* frcPtr */
#define	IDTPHY_REGM_TPTC_STUFF	0x20	/* stuffCtl */
#define	IDTPHY_REGM_TPTC_PTR	0x10	/* Ptr */
#define	IDTPHY_REGM_TPTC_NDF	0x08	/* NDF */
#define	IDTPHY_REGM_TPTC_DECP	0x04	/* decPtr */
#define	IDTPHY_REGM_TPTC_INCP	0x02	/* incPtr */
#define	IDTPHY_REGM_TPTC_RESV	0x81
#define	IDTPHY_REGX_TPTC	\
	    "\020\7frcPtr\6stuffCtl\5Ptr\4NDF\3decPtr\2incPtr"

#define	IDTPHY_REGO_PTRL	0x45
#define	IDTPHY_REGN_PTRL	"Transmit Pointer LSB"
#define	IDTPHY_REGX_PTRL	\
	    "\020\12\xff\20arbPtr"

#define	IDTPHY_REGO_PTRM	0x46
#define	IDTPHY_REGN_PTRM	"Transmit Pointer MSB"
#define	IDTPHY_REGM_PTRM_NDF	0xf0	/* NDFVal */
#define	IDTPHY_REGS_PTRM_NDF	4
#define	IDTPHY_REGM_PTRM_SS	0x0c	/* ssBit */
#define	IDTPHY_REGM_PTRM_SONET	0x00
#define	IDTPHY_REGM_PTRM_SDH	0x08
#define	IDTPHY_REGM_PTRM_PTR	0x03
#define	IDTPHY_REGX_PTRM	\
	    "\020\12\xf0\20NDFVal\12\xc\20ssBit\12\x3\20arbPtr"

#define	IDTPHY_REGO_TC2		0x48
#define	IDTPHY_REGN_TC2		"Transmit C2"

#define	IDTPHY_REGO_TPOC2	0x49
#define	IDTPHY_REGN_TPOC2	"Transmit Path Control 2"
#define	IDTPHY_REGM_TPOC2_FEBE	0xf0	/* PFEBEIns */
#define	IDTPHY_REGS_TPOC2_FEBE	4
#define	IDTPHY_REGM_TPOC2_PRDII	0x08	/* PRDIIns */
#define	IDTPHY_REGM_TPOC2_G1	0x07	/* G1Ins */
#define	IDTPHY_REGX_TPOC2	\
	    "\020\12\xf0\20PFEBEIns\4PRDIIns\12\x7\20G1Ins"

#define	IDTPHY_REGO_RCC		0x50
#define	IDTPHY_REGN_RCC		"Receive Cell Control"
#define	IDTPHY_REGM_RCC_OCD	0x80	/* OCD */
#define	IDTPHY_REGM_RCC_PARTY	0x40	/* parity */
#define	IDTPHY_REGM_RCC_PASS	0x20	/* pass */
#define	IDTPHY_REGM_RCC_DCOR	0x10	/* corDis */
#define	IDTPHY_REGM_RCC_DHEC	0x08	/* HECdis */
#define	IDTPHY_REGM_RCC_ADD	0x04	/* csetAdd */
#define	IDTPHY_REGM_RCC_DSCR	0x02	/* scrDis */
#define	IDTPHY_REGM_RCC_RFIFO	0x01	/* rxFIFOrst */
#define	IDTPHY_REGX_RCC		\
	    "\020\10OCD\7parity\6pass\5corDis\4HECdis\3csetAdd" \
	    "\2scrDis\1rxFIFOrst"

#define	IDTPHY_REGO_RCI		0x51
#define	IDTPHY_REGN_RCI		"Receive Cell Interrupt"
#define	IDTPHY_REGM_RCI_OCDE	0x80	/* OCDIEn */
#define	IDTPHY_REGM_RCI_HECE	0x40	/* HECIEn */
#define	IDTPHY_REGM_RCI_OVFE	0x20	/* ovfIEn */
#define	IDTPHY_REGM_RCI_OCDI	0x10	/* OCDInt */
#define	IDTPHY_REGM_RCI_CORI	0x08	/* corInt */
#define	IDTPHY_REGM_RCI_UCORI	0x04	/* uncorInt */
#define	IDTPHY_REGM_RCI_OVFI	0x02	/* ovfInt */
#define	IDTPHY_REGM_RCI_RESV	0x01
#define	IDTPHY_REGX_RCI		\
	    "\020\10OCDIEn\7HECIEn\6ovfIEn\5OCDInt\4corInt\3uncorInt\2ovfInt"

#define	IDTPHY_REGO_CMH		0x52
#define	IDTPHY_REGN_CMH		"Receive Cell Match Header"
#define	IDTPHY_REGM_CMH_GFC	0xf0	/* GFC */
#define	IDTPHY_REGS_CMH_GFC	4
#define	IDTPHY_REGM_CMH_PTI	0x0e	/* PTI */
#define	IDTPHY_REGS_CMH_PTI	1
#define	IDTPHY_REGM_CMH_CLP	0x01	/* CLP */
#define	IDTPHY_REGX_CMH		\
	    "\020\12\xf0\20GFC\12\xe\20PTI\12\x1\20CLP"

#define	IDTPHY_REGO_CMHM	0x53
#define	IDTPHY_REGN_CMHM	"Receive Cell Match Header Mask"
#define	IDTPHY_REGM_CMHM_GFC	0xf0	/* mskGFC */
#define	IDTPHY_REGS_CMHM_GFC	4
#define	IDTPHY_REGM_CMHM_PTI	0x0e	/* mskPTI */
#define	IDTPHY_REGS_CMHM_PTI	1
#define	IDTPHY_REGM_CMHM_CLP	0x01	/* mskCLP */
#define	IDTPHY_REGX_CMHM	\
	    "\020\12\xf0\20mskGFC\12\xe\20mskPTI\12\x1\20mskCLP"

#define	IDTPHY_REGO_CEC		0x54
#define	IDTPHY_REGN_CEC		"Correctable Errors"

#define	IDTPHY_REGO_UEC		0x55
#define	IDTPHY_REGN_UEC		"Uncorrectable Errors"

#define	IDTPHY_REGO_RCCNT	0x56	/* +0x57, 0x58, LE, 19bit */
#define	IDTPHY_REGN_RCCNT	"Receive Cells"

#define	IDTPHY_REGO_RCCF	0x59
#define	IDTPHY_REGN_RCCF	"Receive Cell Configuration"
#define	IDTPHY_REGM_RCCF_GFCE	0xf0	/* GFCen */
#define	IDTPHY_REGS_RCCF_GFCE	4
#define	IDTPHY_REGM_RCCF_FIXS	0x08	/* FixSen */
#define	IDTPHY_REGM_RCCF_RCAL	0x04	/* RCAlevel */
#define	IDTPHY_REGM_RCCF_HECF	0x03	/* HECftr */
#define	IDTPHY_REGX_RCCF	\
	    "\020\12\xf0\20GFCen\4FixSen\3RCAlevel\12\x3\20HECftr"

#define	IDTPHY_REGO_RXID	0x5a
#define	IDTPHY_REGN_RXID	"Receive ID Address"
#define	IDTPHY_REGM_RXID_ID	0x03	/* IDAddr */
#define	IDTPHY_REGM_RXID_RESV	0xfc
#define	IDTPHY_REGX_RXID	\
	    "\020\12\x3\20IDAddr"

#define	IDTPHY_REGO_TCC		0x60
#define	IDTPHY_REGN_TCC		"Transmit Cell Control"
#define	IDTPHY_REGM_TCC_FIFOE	0x80	/* fovrIEn */
#define	IDTPHY_REGM_TCC_SOCI	0x40	/* socInt */
#define	IDTPHY_REGM_TCC_FIFOI	0x20	/* fovrInt */
#define	IDTPHY_REGM_TCC_HECINV	0x10	/* HECInv */
#define	IDTPHY_REGM_TCC_HECDIS	0x08	/* HECDis */
#define	IDTPHY_REGM_TCC_ADD	0x04	/* csetAdd */
#define	IDTPHY_REGM_TCC_DSCR	0x02	/* scrDis */
#define	IDTPHY_REGM_TCC_FIFOR	0x01	/* txFIFOrst */
#define	IDTPHY_REGX_TCC		\
	    "\020\10fovrIEn\7socInt\6fovrInt\5HECInv\4HECDis\3csetAdd" \
	    "\2scrDis\1txFIFOrst"

#define	IDTPHY_REGO_TCHP	0x61
#define	IDTPHY_REGN_TCHP	"Transmit Idle Cell Header"
#define	IDTPHY_REGM_TCHP_GFC	0xf0	/* GFCtx */
#define	IDTPHY_REGS_TCHP_GFC	4
#define	IDTPHY_REGM_TCHP_PTI	0x0e	/* PTItx */
#define	IDTPHY_REGS_TCHP_PTI	1
#define	IDTPHY_REGM_TCHP_CLP	0x01	/* CLPtx */
#define	IDTPHY_REGX_TCHP	\
	    "\020\12\xf0\20GFCtx\12\xe\20PTItx\12\x1\20CLPtx"

#define	IDTPHY_REGO_TPLD	0x62
#define	IDTPHY_REGN_TPLD	"Transmit Idle Cell Payload"

#define	IDTPHY_REGO_TCC2	0x63
#define	IDTPHY_REGN_TCC2	"Transmit Cell Configuration 2"
#define	IDTPHY_REGM_TCC2_PARITY	0x80	/* parity */
#define	IDTPHY_REGM_TCC2_PARE	0x40	/* parIEn */
#define	IDTPHY_REGM_TCC2_PARI	0x10	/* parInt */
#define	IDTPHY_REGM_TCC2_FIFO	0x0c	/* FIFOdpth */
#define	IDTPHY_REGS_TCC2_FIFO	2
#define	IDTPHY_REGM_TCC2_TCAL	0x02	/* TCAlevel */
#define	IDTPHY_REGM_TCC2_RESV	0x01
#define	IDTPHY_REGX_TCC2	\
	    "\020\10parity\7parIEn\5parInt\12\xc\20FIFOdpth\2TCAlevel"

#define	IDTPHY_REGO_TXCNT	0x64	/* +65,66 LE 19bit */
#define	IDTPHY_REGN_TXCNT	"Transmit Cells"

#define	IDTPHY_REGO_TCC3	0x67
#define	IDTPHY_REGN_TCC3	"Transmit Cell Configuration 3"
#define	IDTPHY_REGM_TCC3_GFCE	0xf0	/* txGFCen */
#define	IDTPHY_REGS_TCC3_GFCE	4
#define	IDTPHY_REGM_TCC3_FIXE	0x08	/* txFixSen */
#define	IDTPHY_REGM_TCC3_H4ID	0x04	/* H4InsDis */
#define	IDTPHY_REGM_TCC3_FIXB	0x03	/* fixByte */
#define	IDTPHY_REGM_TCC3_FIX00	0x00	/* 0x00 */
#define	IDTPHY_REGM_TCC3_FIX55	0x01	/* 0x55 */
#define	IDTPHY_REGM_TCC3_FIXAA	0x02	/* 0xAA */
#define	IDTPHY_REGM_TCC3_FIXFF	0x03	/* 0xFF */
#define	IDTPHY_REGX_TCC3	\
	    "\020\12\xf0\20txGFCen\4txFixSen\3H4InsDis" \
	    "\11\x3\x0FIX00\11\x3\x1FIX55\11\x3\x2FIXAA\11\x3\x3FIXFF"

#define	IDTPHY_REGO_TXID	0x68
#define	IDTPHY_REGN_TXID	"Transmit ID Address"
#define	IDTPHY_REGM_TXID_ID	0x03	/* txIDAddr */
#define	IDTPHY_REGM_TXID_RESV	0xfc
#define	IDTPHY_REGX_TXID	\
	    "\020\12\x3\20txIDAddr"

#define	IDTPHY_REGO_RBER	0x70
#define	IDTPHY_REGN_RBER	"Receive BER S/C"
#define	IDTPHY_REGM_RBER_FAILE	0x08	/* FailIEn */
#define	IDTPHY_REGM_RBER_WARNE	0x04	/* WarnIEn */
#define	IDTPHY_REGM_RBER_FAIL	0x02	/* BERfail */
#define	IDTPHY_REGM_RBER_WARN	0x01	/* BERwarn */
#define	IDTPHY_REGM_RBER_RESV	0xf0
#define	IDTPHY_REGX_RBER	\
	    "\020\4FailIEn\3WarnIEn\2BERfail\1BERwarn"

#define	IDTPHY_REGO_BFTH	0x71
#define	IDTPHY_REGN_BFTH	"Receive BER Fail Threshold"

#define	IDTPHY_REGO_BFWIN	0x72
#define	IDTPHY_REGN_BFWIN	"Receive BER Fail Window"

#define	IDTPHY_REGO_BFDEN	0x73	/* +74, 16bit LE */
#define	IDTPHY_REGN_BFDEN	"Receive BER Fail Denominator"

#define	IDTPHY_REGO_BWTH	0x75
#define	IDTPHY_REGN_BWTH	"Receive BER Warn Threshold"

#define	IDTPHY_REGO_BWWIN	0x76
#define	IDTPHY_REGN_BWWIN	"Receive BER Warn Window"

#define	IDTPHY_REGO_BWDEN	0x77	/* +78, 16bit LE */
#define	IDTPHY_REGN_BWDEN	"Receive BER Warn Denomiator"

#define	IDTPHY_REGO_OPEC	0x7f
#define	IDTPHY_REGN_OPEC	"Output PECL Control"
#define	IDTPHY_REGM_OPEC_TXC	0x04	/* pcctl_tc */
#define	IDTPHY_REGM_OPEC_TXD	0x02	/* pcctl_td */
#define	IDTPHY_REGM_OPEC_RXDO	0x01	/* pcctl_r */
#define	IDTPHY_REGM_OPEC_RESV	0xf8
#define	IDTPHY_REGX_OPEC	\
	    "\020\3pctl_tc\2pcctl_td\1pcctl_r"

#define	IDTPHY_PRINT_77155					\
	{ /* 00 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_MRID,		\
	  IDTPHY_REGN_MRID,	IDTPHY_REGX_MRID },		\
	{ /* 01 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_CONF,		\
	  IDTPHY_REGN_CONF,	IDTPHY_REGX_CONF },		\
	{ /* 02 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_INT,		\
	  IDTPHY_REGN_INT,	IDTPHY_REGX_INT },		\
	  /* 03 unused */					\
	{ /* 04 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_MCM,		\
	  IDTPHY_REGN_MCM,	IDTPHY_REGX_MCM },		\
	{ /* 05 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_MCTL,		\
	  IDTPHY_REGN_MCTL,	IDTPHY_REGX_MCTL },		\
	{ /* 06 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TXC,		\
	  IDTPHY_REGN_TXC,	IDTPHY_REGX_TXC },		\
	{ /* 07 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RXC,		\
	  IDTPHY_REGN_RXC,	IDTPHY_REGX_RXC },		\
	  /* 08-0f unused */					\
	{ /* 10 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RSOC,		\
	  IDTPHY_REGN_RSOC,	IDTPHY_REGX_RSOC },		\
	{ /* 11 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RSOS,		\
	  IDTPHY_REGN_RSOS,	IDTPHY_REGX_RSOS },		\
	{ /* 12, 13 */						\
	  UTP_REGT_INT16,	IDTPHY_REGO_BIPC,		\
	  IDTPHY_REGN_BIPC,	NULL },				\
	{ /* 14 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TSOC,		\
	  IDTPHY_REGN_TSOC,	IDTPHY_REGX_TSOC },		\
	{ /* 15 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TSOC2,		\
	  IDTPHY_REGN_TSOC2,	IDTPHY_REGX_TSOC2 },		\
	  /* 16, 17 unused */					\
	{ /* 18 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RLOS,		\
	  IDTPHY_REGN_RLOS,	IDTPHY_REGX_RLOS },		\
	{ /* 19 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RLOI,		\
	  IDTPHY_REGN_RLOI,	IDTPHY_REGX_RLOI },		\
	{ /* 1a-1c */						\
	  UTP_REGT_INT20,	IDTPHY_REGO_B2EC,		\
	  IDTPHY_REGN_B2EC,	NULL },				\
	{ /* 1d-1f */						\
	  UTP_REGT_INT20,	IDTPHY_REGO_FEBEC,		\
	  IDTPHY_REGN_FEBEC,	NULL },				\
	{ /* 20 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TLOS,		\
	  IDTPHY_REGN_TLOS,	IDTPHY_REGX_TLOS },		\
	{ /* 21 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TLOC,		\
	  IDTPHY_REGN_TLOC,	IDTPHY_REGX_TLOC },		\
	  /* 22, 23 unused */					\
	{ /* 24 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_TK1,		\
	  IDTPHY_REGN_TK1,	NULL },				\
	{ /* 25 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_TK2,		\
	  IDTPHY_REGN_TK2,	NULL },				\
	{ /* 26 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_RK1,		\
	  IDTPHY_REGN_RK1,	NULL },				\
	{ /* 27 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_RK2,		\
	  IDTPHY_REGN_RK2,	NULL },				\
	  /* 28-2f unused */					\
	{ /* 30 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RPOS,		\
	  IDTPHY_REGN_RPOS,	IDTPHY_REGX_RPOS },		\
	{ /* 31 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RPOI,		\
	  IDTPHY_REGN_RPOI,	IDTPHY_REGX_RPOI },		\
	  /* 32 unused */					\
	{ /* 33 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RPIE,		\
	  IDTPHY_REGN_RPIE,	IDTPHY_REGX_RPIE },		\
	  /* 34-36 unused */					\
	{ /* 37 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_RC2,		\
	  IDTPHY_REGN_RC2,	NULL },				\
	{ /* 38-39 */						\
	  UTP_REGT_INT16,	IDTPHY_REGO_B3EC,		\
	  IDTPHY_REGN_B3EC,	NULL },				\
	{ /* 3a-3b */						\
	  UTP_REGT_INT16,	IDTPHY_REGO_PFEBEC,		\
	  IDTPHY_REGN_PFEBEC,	NULL },				\
	  /* 3c unused */					\
	{ /* 3d */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RPEC,		\
	  IDTPHY_REGN_RPEC,	IDTPHY_REGX_RPEC },		\
	  /* 3e, 3f unused */					\
	{ /* 40 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TPOC,		\
	  IDTPHY_REGN_TPOC,	IDTPHY_REGX_TPOC },		\
	{ /* 41 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TPTC,		\
	  IDTPHY_REGN_TPTC,	IDTPHY_REGX_TPTC },		\
	  /* 42-44 unused */					\
	{ /* 45 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_PTRL,		\
	  IDTPHY_REGN_PTRL,	IDTPHY_REGX_PTRL },		\
	{ /* 46 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_PTRM,		\
	  IDTPHY_REGN_PTRM,	IDTPHY_REGX_PTRM },		\
	  /* 47 unused */					\
	{ /* 48 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_TC2,		\
	  IDTPHY_REGN_TC2,	NULL },				\
	{ /* 49 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TPOC2,		\
	  IDTPHY_REGN_TPOC2,	IDTPHY_REGX_TPOC2 },		\
	  /* 4a-4f unused */					\
	{ /* 50 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RCC,		\
	  IDTPHY_REGN_RCC,	IDTPHY_REGX_RCC },		\
	{ /* 51 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RCI,		\
	  IDTPHY_REGN_RCI,	IDTPHY_REGX_RCI },		\
	{ /* 52 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_CMH,		\
	  IDTPHY_REGN_CMH,	IDTPHY_REGX_CMH },		\
	{ /* 53 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_CMHM,		\
	  IDTPHY_REGN_CMHM,	IDTPHY_REGX_CMHM },		\
	{ /* 54 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_CEC,		\
	  IDTPHY_REGN_CEC,	NULL },				\
	{ /* 55 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_UEC,		\
	  IDTPHY_REGN_UEC,	NULL },				\
	{ /* 56-58 */						\
	  UTP_REGT_INT19,	IDTPHY_REGO_RCCNT,		\
	  IDTPHY_REGN_RCCNT,	NULL },				\
	{ /* 59 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RCCF,		\
	  IDTPHY_REGN_RCCF,	IDTPHY_REGX_RCCF },		\
	{ /* 5a */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RXID,		\
	  IDTPHY_REGN_RXID,	IDTPHY_REGX_RXID },		\
	  /* 5b-5f unused */					\
	{ /* 60 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TCC,		\
	  IDTPHY_REGN_TCC,	IDTPHY_REGX_TCC },		\
	{ /* 61 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TCHP,		\
	  IDTPHY_REGN_TCHP,	IDTPHY_REGX_TCHP },		\
	{ /* 62 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_TPLD,		\
	  IDTPHY_REGN_TPLD,	NULL },				\
	{ /* 63 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TCC2,		\
	  IDTPHY_REGN_TCC2,	IDTPHY_REGX_TCC2 },		\
	{ /* 64-66 */						\
	  UTP_REGT_INT19,	IDTPHY_REGO_TXCNT,		\
	  IDTPHY_REGN_TXCNT,	NULL },				\
	{ /* 67 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TCC3,		\
	  IDTPHY_REGN_TCC3,	IDTPHY_REGX_TCC3 },		\
	{ /* 68 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_TXID,		\
	  IDTPHY_REGN_TXID,	IDTPHY_REGX_TXID },		\
	  /* 69-6f unused */					\
	{ /* 70 */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_RBER,		\
	  IDTPHY_REGN_RBER,	IDTPHY_REGX_RBER },		\
	{ /* 71 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_BFTH,		\
	  IDTPHY_REGN_BFTH,	NULL },				\
	{ /* 72 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_BFWIN,		\
	  IDTPHY_REGN_BFWIN,	NULL },				\
	{ /* 73,74 */						\
	  UTP_REGT_INT16,	IDTPHY_REGO_BFDEN,		\
	  IDTPHY_REGN_BFDEN,	NULL },				\
	{ /* 75 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_BWTH,		\
	  IDTPHY_REGN_BWTH,	NULL },				\
	{ /* 76 */						\
	  UTP_REGT_INT8,	IDTPHY_REGO_BWWIN,		\
	  IDTPHY_REGN_BWWIN,	NULL },				\
	{ /* 77,78 */						\
	  UTP_REGT_INT16,	IDTPHY_REGO_BWDEN,		\
	  IDTPHY_REGN_BWDEN,	NULL },				\
	  /* 79-7e unused */					\
	{ /* 7f */						\
	  UTP_REGT_BITS,	IDTPHY_REGO_OPEC,		\
	  IDTPHY_REGN_OPEC,	IDTPHY_REGX_OPEC }


#endif	/* _DEV_UTOPIA_IDTPHY_H */
