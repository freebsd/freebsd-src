
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010 Hans Petter Selasky. All rights reserved.
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

#ifndef _XHCIREG_H_
#define	_XHCIREG_H_

/* XHCI PCI config registers */
#define	PCI_XHCI_CBMEM		0x10	/* configuration base MEM */
#define	PCI_XHCI_USBREV		0x60	/* RO USB protocol revision */
#define	PCI_USB_REV_3_0		0x30	/* USB 3.0 */
#define	PCI_XHCI_FLADJ		0x61	/* RW frame length adjust */

#define	PCI_XHCI_INTEL_XUSB2PR	0xD0	/* Intel USB2 Port Routing */
#define	PCI_XHCI_INTEL_USB2PRM	0xD4	/* Intel USB2 Port Routing Mask */
#define	PCI_XHCI_INTEL_USB3_PSSEN 0xD8	/* Intel USB3 Port SuperSpeed Enable */
#define	PCI_XHCI_INTEL_USB3PRM	0xDC	/* Intel USB3 Port Routing Mask */

/* XHCI capability registers */
#define	XHCI_CAPLENGTH		0x00	/* RO capability */
#define	XHCI_RESERVED		0x01	/* Reserved */
#define	XHCI_HCIVERSION		0x02	/* RO Interface version number */
#define	XHCI_HCIVERSION_0_9	0x0090	/* xHCI version 0.9 */
#define	XHCI_HCIVERSION_1_0	0x0100	/* xHCI version 1.0 */
#define	XHCI_HCSPARAMS1		0x04	/* RO structural parameters 1 */
#define	XHCI_HCS1_DEVSLOT_MAX(x)((x) & 0xFF)
#define	XHCI_HCS1_IRQ_MAX(x)	(((x) >> 8) & 0x3FF)
#define	XHCI_HCS1_N_PORTS(x)	(((x) >> 24) & 0xFF)
#define	XHCI_HCSPARAMS2		0x08	/* RO structural parameters 2 */
#define	XHCI_HCS2_IST(x)	((x) & 0xF)
#define	XHCI_HCS2_ERST_MAX(x)	(((x) >> 4) & 0xF)
#define	XHCI_HCS2_SPR(x)	(((x) >> 26) & 0x1)
#define	XHCI_HCS2_SPB_MAX(x)	((((x) >> 16) & 0x3E0) | (((x) >> 27) & 0x1F))
#define	XHCI_HCSPARAMS3		0x0C	/* RO structural parameters 3 */
#define	XHCI_HCS3_U1_DEL(x)	((x) & 0xFF)
#define	XHCI_HCS3_U2_DEL(x)	(((x) >> 16) & 0xFFFF)
#define	XHCI_HCCPARAMS1		0x10	/* RO capability parameters */
#define	XHCI_HCS0_AC64(x)	((x) & 0x1)		/* 64-bit capable */
#define	XHCI_HCS0_BNC(x)	(((x) >> 1) & 0x1)	/* BW negotiation */
#define	XHCI_HCS0_CSZ(x)	(((x) >> 2) & 0x1)	/* context size */
#define	XHCI_HCS0_PPC(x)	(((x) >> 3) & 0x1)	/* port power control */
#define	XHCI_HCS0_PIND(x)	(((x) >> 4) & 0x1)	/* port indicators */
#define	XHCI_HCS0_LHRC(x)	(((x) >> 5) & 0x1)	/* light HC reset */
#define	XHCI_HCS0_LTC(x)	(((x) >> 6) & 0x1)	/* latency tolerance msg */
#define	XHCI_HCS0_NSS(x)	(((x) >> 7) & 0x1)	/* no secondary sid */
#define	XHCI_HCS0_PSA_SZ_MAX(x)	(((x) >> 12) & 0xF)	/* max pri. stream array size */
#define	XHCI_HCS0_XECP(x)	(((x) >> 16) & 0xFFFF)	/* extended capabilities pointer */
#define	XHCI_DBOFF		0x14	/* RO doorbell offset */
#define	XHCI_RTSOFF		0x18	/* RO runtime register space offset */

/* XHCI operational registers.  Offset given by XHCI_CAPLENGTH register */
#define	XHCI_USBCMD		0x00	/* XHCI command */
#define	XHCI_CMD_RS		0x00000001	/* RW Run/Stop */
#define	XHCI_CMD_HCRST		0x00000002	/* RW Host Controller Reset */
#define	XHCI_CMD_INTE		0x00000004	/* RW Interrupter Enable */
#define	XHCI_CMD_HSEE		0x00000008	/* RW Host System Error Enable */
#define	XHCI_CMD_LHCRST		0x00000080	/* RO/RW Light Host Controller Reset */
#define	XHCI_CMD_CSS		0x00000100	/* RW Controller Save State */
#define	XHCI_CMD_CRS		0x00000200	/* RW Controller Restore State */
#define	XHCI_CMD_EWE		0x00000400	/* RW Enable Wrap Event */
#define	XHCI_CMD_EU3S		0x00000800	/* RW Enable U3 MFINDEX Stop */
#define	XHCI_USBSTS		0x04	/* XHCI status */
#define	XHCI_STS_HCH		0x00000001	/* RO - Host Controller Halted */
#define	XHCI_STS_HSE		0x00000004	/* RW - Host System Error */
#define	XHCI_STS_EINT		0x00000008	/* RW - Event Interrupt */
#define	XHCI_STS_PCD		0x00000010	/* RW - Port Change Detect */
#define	XHCI_STS_SSS		0x00000100	/* RO - Save State Status */
#define	XHCI_STS_RSS		0x00000200	/* RO - Restore State Status */
#define	XHCI_STS_SRE		0x00000400	/* RW - Save/Restore Error */
#define	XHCI_STS_CNR		0x00000800	/* RO - Controller Not Ready */
#define	XHCI_STS_HCE		0x00001000	/* RO - Host Controller Error */
#define	XHCI_PAGESIZE		0x08	/* XHCI page size mask */
#define	XHCI_PAGESIZE_4K	0x00000001	/* 4K Page Size */
#define	XHCI_PAGESIZE_8K	0x00000002	/* 8K Page Size */
#define	XHCI_PAGESIZE_16K	0x00000004	/* 16K Page Size */
#define	XHCI_PAGESIZE_32K	0x00000008	/* 32K Page Size */
#define	XHCI_PAGESIZE_64K	0x00000010	/* 64K Page Size */
#define	XHCI_DNCTRL		0x14	/* XHCI device notification control */
#define	XHCI_DNCTRL_MASK(n)	(1U << (n))
#define	XHCI_CRCR_LO		0x18	/* XHCI command ring control */
#define	XHCI_CRCR_LO_RCS	0x00000001	/* RW - consumer cycle state */
#define	XHCI_CRCR_LO_CS		0x00000002	/* RW - command stop */
#define	XHCI_CRCR_LO_CA		0x00000004	/* RW - command abort */
#define	XHCI_CRCR_LO_CRR	0x00000008	/* RW - command ring running */
#define	XHCI_CRCR_LO_MASK	0x0000000F
#define	XHCI_CRCR_HI		0x1C	/* XHCI command ring control */
#define	XHCI_DCBAAP_LO		0x30	/* XHCI dev context BA pointer */
#define	XHCI_DCBAAP_HI		0x34	/* XHCI dev context BA pointer */
#define	XHCI_CONFIG		0x38
#define	XHCI_CONFIG_SLOTS_MASK	0x000000FF	/* RW - number of device slots enabled */

/* XHCI port status registers */
#define	XHCI_PORTSC(n)		(0x3F0 + (0x10 * (n)))	/* XHCI port status */
#define	XHCI_PS_CCS		0x00000001	/* RO - current connect status */
#define	XHCI_PS_PED		0x00000002	/* RW - port enabled / disabled */
#define	XHCI_PS_OCA		0x00000008	/* RO - over current active */
#define	XHCI_PS_PR		0x00000010	/* RW - port reset */
#define	XHCI_PS_PLS_GET(x)	(((x) >> 5) & 0xF)	/* RW - port link state */
#define	XHCI_PS_PLS_SET(x)	(((x) & 0xF) << 5)	/* RW - port link state */
#define	XHCI_PS_PP		0x00000200	/* RW - port power */
#define	XHCI_PS_SPEED_GET(x)	(((x) >> 10) & 0xF)	/* RO - port speed */
#define	XHCI_PS_SPEED_FULL	0x1			/* Full Speed USB */
#define	XHCI_PS_SPEED_LOW	0x2			/* Low Speed USB */
#define	XHCI_PS_SPEED_HIGH	0x3			/* High Speed USB */
#define	XHCI_PS_SPEED_SS	0x4			/* Super Speed USB */
#define	XHCI_PS_PIC_GET(x)	(((x) >> 14) & 0x3)	/* RW - port indicator */
#define	XHCI_PS_PIC_SET(x)	(((x) & 0x3) << 14)	/* RW - port indicator */
#define	XHCI_PS_LWS		0x00010000	/* RW - port link state write strobe */
#define	XHCI_PS_CSC		0x00020000	/* RW - connect status change */
#define	XHCI_PS_PEC		0x00040000	/* RW - port enable/disable change */
#define	XHCI_PS_WRC		0x00080000	/* RW - warm port reset change */
#define	XHCI_PS_OCC		0x00100000	/* RW - over-current change */
#define	XHCI_PS_PRC		0x00200000	/* RW - port reset change */
#define	XHCI_PS_PLC		0x00400000	/* RW - port link state change */
#define	XHCI_PS_CEC		0x00800000	/* RW - config error change */
#define	XHCI_PS_CAS		0x01000000	/* RO - cold attach status */
#define	XHCI_PS_WCE		0x02000000	/* RW - wake on connect enable */
#define	XHCI_PS_WDE		0x04000000	/* RW - wake on disconnect enable */
#define	XHCI_PS_WOE		0x08000000	/* RW - wake on over-current enable */
#define	XHCI_PS_DR		0x40000000	/* RO - device removable */
#define	XHCI_PS_WPR		0x80000000U	/* RW - warm port reset */
#define	XHCI_PS_CLEAR		0x80FF01FFU	/* command bits */

#define	XHCI_PORTPMSC(n)	(0x3F4 + (0x10 * (n)))	/* XHCI status and control */
#define	XHCI_PM3_U1TO_GET(x)	(((x) >> 0) & 0xFF)	/* RW - U1 timeout */
#define	XHCI_PM3_U1TO_SET(x)	(((x) & 0xFF) << 0)	/* RW - U1 timeout */
#define	XHCI_PM3_U2TO_GET(x)	(((x) >> 8) & 0xFF)	/* RW - U2 timeout */
#define	XHCI_PM3_U2TO_SET(x)	(((x) & 0xFF) << 8)	/* RW - U2 timeout */
#define	XHCI_PM3_FLA		0x00010000	/* RW - Force Link PM Accept */
#define	XHCI_PM2_L1S_GET(x)	(((x) >> 0) & 0x7)	/* RO - L1 status */
#define	XHCI_PM2_RWE		0x00000008		/* RW - remote wakup enable */
#define	XHCI_PM2_HIRD_GET(x)	(((x) >> 4) & 0xF)	/* RW - host initiated resume duration */
#define	XHCI_PM2_HIRD_SET(x)	(((x) & 0xF) << 4)	/* RW - host initiated resume duration */
#define	XHCI_PM2_L1SLOT_GET(x)	(((x) >> 8) & 0xFF)	/* RW - L1 device slot */
#define	XHCI_PM2_L1SLOT_SET(x)	(((x) & 0xFF) << 8)	/* RW - L1 device slot */
#define	XHCI_PM2_HLE		0x00010000		/* RW - hardware LPM enable */
#define	XHCI_PORTLI(n)		(0x3F8 + (0x10 * (n)))	/* XHCI port link info */
#define	XHCI_PLI3_ERR_GET(x)	(((x) >> 0) & 0xFFFF)	/* RO - port link errors */
#define	XHCI_PORTRSV(n)		(0x3FC + (0x10 * (n)))	/* XHCI port reserved */

/* XHCI runtime registers.  Offset given by XHCI_CAPLENGTH + XHCI_RTSOFF registers */
#define	XHCI_MFINDEX		0x0000		/* RO - microframe index */
#define	XHCI_MFINDEX_GET(x)	((x) & 0x3FFF)
#define	XHCI_IMAN(n)		(0x0020 + (0x20 * (n)))	/* XHCI interrupt management */
#define	XHCI_IMAN_INTR_PEND	0x00000001	/* RW - interrupt pending */
#define	XHCI_IMAN_INTR_ENA	0x00000002	/* RW - interrupt enable */
#define	XHCI_IMOD(n)		(0x0024 + (0x20 * (n)))	/* XHCI interrupt moderation */
#define	XHCI_IMOD_IVAL_GET(x)	(((x) >> 0) & 0xFFFF)	/* 250ns unit */
#define	XHCI_IMOD_IVAL_SET(x)	(((x) & 0xFFFF) << 0)	/* 250ns unit */
#define	XHCI_IMOD_ICNT_GET(x)	(((x) >> 16) & 0xFFFF)	/* 250ns unit */
#define	XHCI_IMOD_ICNT_SET(x)	(((x) & 0xFFFF) << 16)	/* 250ns unit */
#define	XHCI_IMOD_DEFAULT	0x000001F4U	/* 8000 IRQs/second */
#define	XHCI_IMOD_DEFAULT_LP 	0x000003F8U	/* 4000 IRQs/second - LynxPoint */
#define	XHCI_ERSTSZ(n)		(0x0028 + (0x20 * (n)))	/* XHCI event ring segment table size */
#define	XHCI_ERSTS_GET(x)	((x) & 0xFFFF)
#define	XHCI_ERSTS_SET(x)	((x) & 0xFFFF)
#define	XHCI_ERSTBA_LO(n)	(0x0030 + (0x20 * (n)))	/* XHCI event ring segment table BA */
#define	XHCI_ERSTBA_HI(n)	(0x0034 + (0x20 * (n)))	/* XHCI event ring segment table BA */
#define	XHCI_ERDP_LO(n)	(0x0038 + (0x20 * (n)))	/* XHCI event ring dequeue pointer */
#define	XHCI_ERDP_LO_SINDEX(x)	((x) & 0x7)	/* RO - dequeue segment index */
#define	XHCI_ERDP_LO_BUSY	0x00000008	/* RW - event handler busy */
#define	XHCI_ERDP_HI(n)	(0x003C + (0x20 * (n)))	/* XHCI event ring dequeue pointer */

/* XHCI doorbell registers. Offset given by XHCI_CAPLENGTH + XHCI_DBOFF registers */
#define	XHCI_DOORBELL(n)	(0x0000 + (4 * (n)))
#define	XHCI_DB_TARGET_GET(x)	((x) & 0xFF)		/* RW - doorbell target */
#define	XHCI_DB_TARGET_SET(x)	((x) & 0xFF)		/* RW - doorbell target */
#define	XHCI_DB_SID_GET(x)	(((x) >> 16) & 0xFFFF)	/* RW - doorbell stream ID */
#define	XHCI_DB_SID_SET(x)	(((x) & 0xFFFF) << 16)	/* RW - doorbell stream ID */

/* XHCI legacy support */
#define	XHCI_XECP_ID(x)		((x) & 0xFF)
#define	XHCI_XECP_NEXT(x)	(((x) >> 8) & 0xFF)
#define	XHCI_XECP_BIOS_SEM	0x0002
#define	XHCI_XECP_OS_SEM	0x0003

/* XHCI capability ID's */
#define	XHCI_ID_USB_LEGACY	0x0001
#define	XHCI_ID_PROTOCOLS	0x0002
#define	XHCI_ID_POWER_MGMT	0x0003
#define	XHCI_ID_VIRTUALIZATION	0x0004
#define	XHCI_ID_MSG_IRQ		0x0005
#define	XHCI_ID_USB_LOCAL_MEM	0x0006
/* values 7-9 are reserved */
#define	XHCI_ID_USB_DEBUG	0x000a
/* values 11-16 are reserved */
#define	XHCI_ID_EXT_MSI		0x0011
#define	XHCI_ID_USB3_TUN	0x0012

/*
 * XHCI Debug Capability
 * From Section 7.6 of xHCI April 2023 Revision 1.2b.
 */
#define	XHCI_DCID			0x0000

#define	XHCI_DCDB			0x0004		/* Doorbell */
#define	XHCI_DCDB_OUT			0x00000000
#define	XHCI_DCDB_IN			0x00000100
#define	XHCI_DCDB_INVAL			0x0000FE00

#define	XHCI_DCERSTSZ			0x0008	/* Event Ring Segment Table size */
#define	XHCI_DCERSTBA_LO		0x0010
#define	XHCI_DCERSTBA_HI		0x0014
#define	XHCI_DCERDP_LO			0x0018
#define	XHCI_DCERDP_HI			0x001C

#define	XHCI_DCCTRL			0x0020		/* Debug Control */
#define	XHCI_DCCTRL_DCR			0x00000001
#define	XHCI_DCCTRL_DCR_GET(x)		(((x)      ) & 0x01)
#define	XHCI_DCCTRL_LSE			0x00000002
#define	XHCI_DCCTRL_LSE_GET(x)		(((x) >>  1) & 0x01)
#define	XHCI_DCCTRL_HOT			0x00000004
#define	XHCI_DCCTRL_HOT_GET(x)		(((x) >>  2) & 0x01)
#define	XHCI_DCCTRL_HIT			0x00000008
#define	XHCI_DCCTRL_HIT_GET(x)		(((x) >>  3) & 0x01)
#define	XHCI_DCCTRL_DRC			0x00000010
#define	XHCI_DCCTRL_DRC_GET(x)		(((x) >>  4) & 0x01)
#define	XHCI_DCCTRL_MBS_GET(x)		(((x) >> 16) & 0xFF)
#define	XHCI_DCCTRL_ADDR_GET(x)		(((x) >> 24) & 0x7F)
#define	XHCI_DCCTRL_DCE			0x80000000
#define	XHCI_DCCTRL_DCE_GET(x)		(((x) >> 31) & 0x01)

#define	XHCI_DCST			0x0024	/* Status */
#define	XHCI_DCST_ER			0x00000001
#define	XHCI_DCST_ER_GET(x)		(((x)      ) & 0x01)
#define	XHCI_DCST_SBR			0x00000002
#define	XHCI_DCST_SBR_GET(x)		(((x) >>  1) & 0x01)
#define	XHCI_DCST_PORT_GET(x)		(((x) >> 24) & 0xFF)

#define	XHCI_DCPORTSC			0x0028	/* Port Control */
#define	XHCI_DCPORTSC_CCS		0x00000001
#define	XHCI_DCPORTSC_CCS_GET(x)	(((x)      ) & 0x01)
#define	XHCI_DCPORTSC_PED		0x00000002
#define	XHCI_DCPORTSC_PED_GET(x)	(((x) >>  1) & 0x01)
#define	XHCI_DCPORTSC_PR		0x00000010
#define	XHCI_DCPORTSC_PR_GET(x)		(((x) >>  4) & 0x01)
#define	XHCI_DCPORTSC_PLS_GET(x)	(((x) >>  5) & 0x0F)
#define	XHCI_DCPORTSC_PLS_U0		0x00
#define	XHCI_DCPORTSC_PLS_U1		0x01
#define	XHCI_DCPORTSC_PLS_U2		0x02
#define	XHCI_DCPORTSC_PLS_U3		0x03
#define	XHCI_DCPORTSC_PLS_DISABLED	0x04
#define	XHCI_DCPORTSC_PLS_RXDETECTED	0x05
#define	XHCI_DCPORTSC_PLS_INACTIVE	0x06
#define	XHCI_DCPORTSC_PLS_POLLING	0x07
#define	XHCI_DCPORTSC_PLS_RECOVERY	0x08
#define	XHCI_DCPORTSC_PLS_HOTRESET	0x09
#define	XHCI_DCPORTSC_SPEED_GET(x)	(((x) >> 10) & 0x0F)
#define	XHCI_DCPORTSC_CSC		0x00020000
#define	XHCI_DCPORTSC_CSC_GET(x)	(((x) >> 17) & 0x01)
#define	XHCI_DCPORTSC_PRC		0x00200000
#define	XHCI_DCPORTSC_PRC_GET(x)	(((x) >> 21) & 0x01)
#define	XHCI_DCPORTSC_PLC		0x00400000
#define	XHCI_DCPORTSC_PLC_GET(x)	(((x) >> 22) & 0x01)
#define	XHCI_DCPORTSC_CEC		0x00800000
#define	XHCI_DCPORTSC_CEC_GET(x)	(((x) >> 23) & 0x01)
#define	XHCI_DCCP_LO			0x0030	/* Context Pointer */
#define	XHCI_DCCP_HI			0x0034
#define	XHCI_DCDDI1			0x0038	/* Device Descriptor Info */
#define	XHCI_DCDDI2			0x003C	/* Device Descriptor Info */

/* DbC CIC offset in uint32 */
#define	XHCI_DCDBCIC_STR0DESC_LO		0x0000
#define	XHCI_DCDBCIC_STR0DESC_HI		0x0001
#define	XHCI_DCDBCIC_MANUDESC_LO		0x0002
#define	XHCI_DCDBCIC_MANUDESC_HI		0x0003
#define	XHCI_DCDBCIC_PRODDESC_LO		0x0004
#define	XHCI_DCDBCIC_PRODDESC_HI		0x0005
#define	XHCI_DCDBCIC_SERIALDESC_LO		0x0006
#define	XHCI_DCDBCIC_SERIALDESC_HI		0x0007
#define	XHCI_DCDBCIC_DESCLEN			0x0008
#define	XHCI_DCDBCIC_STR0DESC_LEN_GET(x)	(((x) >> 0) & 0xff)
#define	XHCI_DCDBCIC_STR0DESC_LEN_SET(x)	(((x) & 0xff) << 0)
#define	XHCI_DCDBCIC_MANUDESC_LEN_GET(x)	(((x) >> 8) & 0xff)
#define	XHCI_DCDBCIC_MANUDESC_LEN_SET(x)	(((x) & 0xff) << 8)
#define	XHCI_DCDBCIC_PRODDESC_LEN_GET(x)	(((x) >> 16) & 0xff)
#define	XHCI_DCDBCIC_PRODDESC_LEN_SET(x)	(((x) & 0xff) << 16)
#define	XHCI_DCDBCIC_SERIALDESC_LEN_GET(x)	(((x) >> 24) & 0xff)
#define	XHCI_DCDBCIC_SERIALDESC_LEN_SET(x)	(((x) & 0xff) << 24)

#define	XHCI_DCSTATUS(ctrl, portsc) \
	(XHCI_DCCTRL_DCE_GET(ctrl) << 4 | \
	XHCI_DCPORTSC_CCS_GET(portsc) << 3 | \
	XHCI_DCPORTSC_PED_GET(portsc) << 2 | \
	XHCI_DCPORTSC_PR_GET(portsc)  << 1 | \
	XHCI_DCCTRL_DCR_GET(ctrl))
#define XHCI_DCPORTSC_ACK_MASK	\
	(XHCI_DCPORTSC_PED | \
	 XHCI_DCPORTSC_CSC | XHCI_DCPORTSC_PRC | \
	 XHCI_DCPORTSC_PLC | XHCI_DCPORTSC_CEC)

#define	XHCI_DCPORT_ST_OFF			0x00
#define	XHCI_DCPORT_ST_DISCONNECTED		0x10	/* DCE only */
#define	XHCI_DCPORT_ST_DISCONNECTED_RUNNING	0x11	/* XXX: DCE + DCR */
#define	XHCI_DCPORT_ST_DISABLED			0x18	/* DCE + CCS */
#define	XHCI_DCPORT_ST_RESETTING		0x1a	/* DCE + CCS + PR */
#define	XHCI_DCPORT_ST_ENABLED			0x1c	/* DCE + CCS + PED */
#define	XHCI_DCPORT_ST_CONFIGURED		0x1d	/* DCE + CCS + PED + DCR */

#define	XHCI_DC_MAXPACKETLEN			1024
/*
 * While Sec 7.6.3.2 describes Endpoint IDs should be 0 or 1,
 * Intel chips use Device Context Index (Sec 4.5.1) instead.
 */
#define	XHCI_DC_EPID_OUT			0
#define	XHCI_DC_EPID_IN				1
#define	XHCI_DC_EPID_OUT_INTEL			2
#define	XHCI_DC_EPID_IN_INTEL			3
#define	XHCI_DC_SLOT				1

/* XHCI register R/W wrappers */
#define	XREAD1(sc, what, a) \
	bus_space_read_1((sc)->sc_io_tag, (sc)->sc_io_hdl, \
		(a) + (sc)->sc_##what##_off)
#define	XREAD2(sc, what, a) \
	bus_space_read_2((sc)->sc_io_tag, (sc)->sc_io_hdl, \
		(a) + (sc)->sc_##what##_off)
#define	XREAD4(sc, what, a) \
	bus_space_read_4((sc)->sc_io_tag, (sc)->sc_io_hdl, \
		(a) + (sc)->sc_##what##_off)
#define	XWRITE1(sc, what, a, x) \
	bus_space_write_1((sc)->sc_io_tag, (sc)->sc_io_hdl, \
		(a) + (sc)->sc_##what##_off, (x))
#define	XWRITE2(sc, what, a, x) \
	bus_space_write_2((sc)->sc_io_tag, (sc)->sc_io_hdl, \
		(a) + (sc)->sc_##what##_off, (x))
#define	XWRITE4(sc, what, a, x) \
	bus_space_write_4((sc)->sc_io_tag, (sc)->sc_io_hdl, \
		(a) + (sc)->sc_##what##_off, (x))

#endif	/* _XHCIREG_H_ */
