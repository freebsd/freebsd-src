/*	$NetBSD: ehcireg.h,v 1.13 2001/11/23 01:16:27 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * The EHCI 0.96 spec can be found at
 * http://developer.intel.com/technology/usb/download/ehci-r096.pdf
 * and the USB 2.0 spec at
 * http://www.usb.org/developers/data/usb_20.zip
 */

#ifndef _DEV_PCI_EHCIREG_H_
#define _DEV_PCI_EHCIREG_H_

/*** PCI config registers ***/

#define PCI_CBMEM		0x10	/* configuration base MEM */

#define PCI_INTERFACE_EHCI	0x20

#define PCI_USBREV		0x60	/* RO USB protocol revision */
#define  PCI_USBREV_MASK	0xff
#define  PCI_USBREV_PRE_1_0	0x00
#define  PCI_USBREV_1_0		0x10
#define  PCI_USBREV_1_1		0x11
#define  PCI_USBREV_2_0		0x20

#define PCI_EHCI_FLADJ		0x61	/*RW Frame len adj, SOF=59488+6*fladj */

#define PCI_EHCI_PORTWAKECAP	0x62	/* RW Port wake caps (opt)  */

/* Regs ar EECP + offset */
#define PCI_EHCI_USBLEGSUP	0x00
#define PCI_EHCI_USBLEGCTLSTS	0x04

/*** EHCI capability registers ***/

#define EHCI_CAPLENGTH		0x00	/*RO Capability register length field */
/* reserved			0x01 */
#define EHCI_HCIVERSION		0x02	/* RO Interface version number */

#define EHCI_HCSPARAMS		0x04	/* RO Structural parameters */
#define  EHCI_HCS_DEBUGPORT(x)	(((x) >> 20) & 0xf)
#define  EHCI_HCS_P_INCICATOR(x) ((x) & 0x10000)
#define  EHCI_HCS_N_CC(x)	(((x) >> 12) & 0xf) /* # of companion ctlrs */
#define  EHCI_HCS_N_PCC(x)	(((x) >> 8) & 0xf) /* # of ports per comp. */
#define  EHCI_HCS_PPC(x)	((x) & 0x10) /* port power control */
#define  EHCI_HCS_N_PORTS(x)	((x) & 0xf) /* # of ports */

#define EHCI_HCCPARAMS		0x08	/* RO Capability parameters */
#define  EHCI_HCC_EECP(x)	(((x) >> 8) & 0xff) /* extended ports caps */
#define  EHCI_HCC_IST(x)	(((x) >> 4) & 0xf) /* isoc sched threshold */
#define  EHCI_HCC_ASPC(x)	((x) & 0x4) /* async sched park cap */
#define  EHCI_HCC_PFLF(x)	((x) & 0x2) /* prog frame list flag */
#define  EHCI_HCC_64BIT(x)	((x) & 0x1) /* 64 bit address cap */

#define EHCI_HCSP_PORTROUTE	0x0c	/*RO Companion port route description */

/* EHCI operational registers.  Offset given by EHCI_CAPLENGTH register */
#define EHCI_USBCMD		0x00	/* RO, RW, WO Command register */
#define  EHCI_CMD_ITC_M		0x00ff0000 /* RW interrupt threshold ctrl */
#define   EHCI_CMD_ITC_1	0x00010000
#define   EHCI_CMD_ITC_2	0x00020000
#define   EHCI_CMD_ITC_4	0x00040000
#define   EHCI_CMD_ITC_8	0x00080000
#define   EHCI_CMD_ITC_16	0x00100000
#define   EHCI_CMD_ITC_32	0x00200000
#define   EHCI_CMD_ITC_64	0x00400000
#define  EHCI_CMD_ASPME		0x00000800 /* RW/RO async park enable */
#define  EHCI_CMD_ASPMC		0x00000300 /* RW/RO async park count */
#define  EHCI_CMD_LHCR		0x00000080 /* RW light host ctrl reset */
#define  EHCI_CMD_IAAD		0x00000040 /* RW intr on async adv door bell */
#define  EHCI_CMD_ASE		0x00000020 /* RW async sched enable */
#define  EHCI_CMD_PSE		0x00000010 /* RW periodic sched enable */
#define  EHCI_CMD_FLS_M		0x0000000c /* RW/RO frame list size */
#define  EHCI_CMD_FLS(x)	(((x) >> 2) & 3) /* RW/RO frame list size */
#define  EHCI_CMD_HCRESET	0x00000002 /* RW reset */
#define  EHCI_CMD_RS		0x00000001 /* RW run/stop */

#define EHCI_USBSTS		0x04	/* RO, RW, RWC Status register */
#define  EHCI_STS_ASS		0x00008000 /* RO async sched status */
#define  EHCI_STS_PSS		0x00004000 /* RO periodic sched status */
#define  EHCI_STS_REC		0x00002000 /* RO reclamation */
#define  EHCI_STS_HCH		0x00001000 /* RO host controller halted */
#define  EHCI_STS_IAA		0x00000020 /* RWC interrupt on async adv */
#define  EHCI_STS_HSE		0x00000010 /* RWC host system error */
#define  EHCI_STS_FLR		0x00000008 /* RWC frame list rollover */
#define  EHCI_STS_PCD		0x00000004 /* RWC port change detect */
#define  EHCI_STS_ERRINT	0x00000002 /* RWC error interrupt */
#define  EHCI_STS_INT		0x00000001 /* RWC interrupt */
#define  EHCI_STS_INTRS(x)	((x) & 0x3f)

#define EHCI_NORMAL_INTRS (EHCI_STS_IAA | EHCI_STS_HSE | EHCI_STS_PCD | EHCI_STS_ERRINT | EHCI_STS_INT)

#define EHCI_USBINTR		0x08	/* RW Interrupt register */
#define EHCI_INTR_IAAE		0x00000020 /* interrupt on async advance ena */
#define EHCI_INTR_HSEE		0x00000010 /* host system error ena */
#define EHCI_INTR_FLRE		0x00000008 /* frame list rollover ena */
#define EHCI_INTR_PCIE		0x00000004 /* port change ena */
#define EHCI_INTR_UEIE		0x00000002 /* USB error intr ena */
#define EHCI_INTR_UIE		0x00000001 /* USB intr ena */

#define EHCI_FRINDEX		0x0c	/* RW Frame Index register */

#define EHCI_CTRLDSSEGMENT	0x10	/* RW Control Data Structure Segment */

#define EHCI_PERIODICLISTBASE	0x14	/* RW Periodic List Base */
#define EHCI_ASYNCLISTADDR	0x18	/* RW Async List Base */

#define EHCI_CONFIGFLAG		0x40	/* RW Configure Flag register */
#define  EHCI_CONF_CF		0x00000001 /* RW configure flag */

#define EHCI_PORTSC(n)		(0x40+4*(n)) /* RO, RW, RWC Port Status reg */
#define  EHCI_PS_WKOC_E		0x00400000 /* RW wake on over current ena */
#define  EHCI_PS_WKDSCNNT_E	0x00200000 /* RW wake on disconnect ena */
#define  EHCI_PS_WKCNNT_E	0x00100000 /* RW wake on connect ena */
#define  EHCI_PS_PTC		0x000f0000 /* RW port test control */
#define  EHCI_PS_PIC		0x0000c000 /* RW port indicator control */
#define  EHCI_PS_PO		0x00002000 /* RW port owner */
#define  EHCI_PS_PP		0x00001000 /* RW,RO port power */
#define  EHCI_PS_LS		0x00000c00 /* RO line status */
#define  EHCI_PS_IS_LOWSPEED(x)	(((x) & EHCI_PS_LS) == 0x00000400)
#define  EHCI_PS_PR		0x00000100 /* RW port reset */
#define  EHCI_PS_SUSP		0x00000080 /* RW suspend */
#define  EHCI_PS_FPR		0x00000040 /* RW force port resume */
#define  EHCI_PS_OCC		0x00000020 /* RWC over current change */
#define  EHCI_PS_OCA		0x00000010 /* RO over current active */
#define  EHCI_PS_PEC		0x00000008 /* RWC port enable change */
#define  EHCI_PS_PE		0x00000004 /* RW port enable */
#define  EHCI_PS_CSC		0x00000002 /* RWC connect status change */
#define  EHCI_PS_CS		0x00000001 /* RO connect status */
#define  EHCI_PS_CLEAR		(EHCI_PS_OCC|EHCI_PS_PEC|EHCI_PS_CSC)

#define EHCI_PORT_RESET_COMPLETE 2 /* ms */

#define EHCI_FLALIGN_ALIGN	0x1000

/* No data structure may cross a page boundary. */
#define EHCI_PAGE_SIZE 0x1000
#define EHCI_PAGE(x) ((x) &~ 0xfff)
#define EHCI_PAGE_OFFSET(x) ((x) & 0xfff)
#if defined(__FreeBSD__)
#define EHCI_PAGE_MASK(x) ((x) & 0xfff)
#endif

typedef u_int32_t ehci_link_t;
#define EHCI_LINK_TERMINATE	0x00000001
#define EHCI_LINK_TYPE(x)	((x) & 0x00000006)
#define  EHCI_LINK_ITD		0x0
#define  EHCI_LINK_QH		0x2
#define  EHCI_LINK_SITD		0x4
#define  EHCI_LINK_FSTN		0x6
#define EHCI_LINK_ADDR(x)	((x) &~ 0x1f)

typedef u_int32_t ehci_physaddr_t;

/* Isochronous Transfer Descriptor */
typedef struct {
	ehci_link_t	itd_next;
	/* XXX many more */
} ehci_itd_t;
#define EHCI_ITD_ALIGN 32

/* Split Transaction Isochronous Transfer Descriptor */
typedef struct {
	ehci_link_t	sitd_next;
	/* XXX many more */
} ehci_sitd_t;
#define EHCI_SITD_ALIGN 32

/* Queue Element Transfer Descriptor */
#define EHCI_QTD_NBUFFERS 5
typedef struct {
	ehci_link_t	qtd_next;
	ehci_link_t	qtd_altnext;
	u_int32_t	qtd_status;
#define EHCI_QTD_GET_STATUS(x)	(((x) >>  0) & 0xff)
#define  EHCI_QTD_ACTIVE	0x80
#define  EHCI_QTD_HALTED	0x40
#define  EHCI_QTD_BUFERR	0x20
#define  EHCI_QTD_BABBLE	0x10
#define  EHCI_QTD_XACTERR	0x08
#define  EHCI_QTD_MISSEDMICRO	0x04
#define  EHCI_QTD_SPLITXSTATE	0x02
#define  EHCI_QTD_PINGSTATE	0x01
#define  EHCI_QTD_STATERRS	0x7c
#define EHCI_QTD_GET_PID(x)	(((x) >>  8) & 0x3)
#define EHCI_QTD_SET_PID(x)	((x) <<  8)
#define  EHCI_QTD_PID_OUT	0x0
#define  EHCI_QTD_PID_IN	0x1
#define  EHCI_QTD_PID_SETUP	0x2
#define EHCI_QTD_GET_CERR(x)	(((x) >> 10) &  0x3)
#define EHCI_QTD_SET_CERR(x)	((x) << 10)
#define EHCI_QTD_GET_C_PAGE(x)	(((x) >> 12) &  0x7)
#define EHCI_QTD_SET_C_PAGE(x)	((x) << 12)
#define EHCI_QTD_GET_IOC(x)	(((x) >> 15) &  0x1)
#define EHCI_QTD_IOC		0x00008000
#define EHCI_QTD_GET_BYTES(x)	(((x) >> 16) &  0x7fff)
#define EHCI_QTD_SET_BYTES(x)	((x) << 16)
#define EHCI_QTD_GET_TOGGLE(x)	(((x) >> 31) &  0x1)
#define EHCI_QTD_TOGGLE		0x80000000
	ehci_physaddr_t	qtd_buffer[EHCI_QTD_NBUFFERS];
	ehci_physaddr_t qtd_buffer_hi[EHCI_QTD_NBUFFERS];
} ehci_qtd_t;
#define EHCI_QTD_ALIGN 32

/* Queue Head */
typedef struct {
	ehci_link_t	qh_link;
	u_int32_t	qh_endp;
#define EHCI_QH_GET_ADDR(x)	(((x) >>  0) & 0x7f) /* endpoint addr */
#define EHCI_QH_SET_ADDR(x)	(x)
#define EHCI_QH_ADDRMASK	0x0000007f
#define EHCI_QH_GET_INACT(x)	(((x) >>  7) & 0x01) /* inactivate on next */
#define EHCI_QH_INACT		0x00000080
#define EHCI_QH_GET_ENDPT(x)	(((x) >>  8) & 0x0f) /* endpoint no */
#define EHCI_QH_SET_ENDPT(x)	((x) <<  8)
#define EHCI_QH_GET_EPS(x)	(((x) >> 12) & 0x03) /* endpoint speed */
#define EHCI_QH_SET_EPS(x)	((x) << 12)
#define  EHCI_QH_SPEED_FULL	0x0
#define  EHCI_QH_SPEED_LOW	0x1
#define  EHCI_QH_SPEED_HIGH	0x2
#define EHCI_QH_GET_DTC(x)	(((x) >> 14) & 0x01) /* data toggle control */
#define EHCI_QH_DTC		0x00004000
#define EHCI_QH_GET_HRECL(x)	(((x) >> 15) & 0x01) /* head of reclamation */
#define EHCI_QH_HRECL		0x00008000
#define EHCI_QH_GET_MPL(x)	(((x) >> 16) & 0x7ff) /* max packet len */
#define EHCI_QH_SET_MPL(x)	((x) << 16)
#define EHCI_QG_MPLMASK		0x07ff0000
#define EHCI_QH_GET_CTL(x)	(((x) >> 26) & 0x01) /* control endpoint */
#define EHCI_QH_CTL		0x08000000
#define EHCI_QH_GET_NRL(x)	(((x) >> 28) & 0x0f) /* NAK reload */
#define EHCI_QH_SET_NRL(x)	((x) << 28)
	u_int32_t	qh_endphub;
#define EHCI_QH_GET_SMASK(x)	(((x) >>  0) & 0xff) /* intr sched mask */
#define EHCI_QH_SET_SMASK(x)	((x) <<  0)
#define EHCI_QH_GET_CMASK(x)	(((x) >>  8) & 0xff) /* split completion mask */
#define EHCI_QH_SET_CMASK(x)	((x) <<  8)
#define EHCI_QH_GET_HUBA(x)	(((x) >> 16) & 0x7f) /* hub address */
#define EHCI_QH_SET_HUBA(x)	((x) << 16)
#define EHCI_QH_GET_PORT(x)	(((x) >> 23) & 0x7f) /* hub port */
#define EHCI_QH_SET_PORT(x)	((x) << 23)
#define EHCI_QH_GET_MULT(x)	(((x) >> 30) & 0x03) /* pipe multiplier */
#define EHCI_QH_SET_MULT(x)	((x) << 30)
	ehci_link_t	qh_curqtd;
	ehci_qtd_t	qh_qtd;
} ehci_qh_t;
#define EHCI_QH_ALIGN 32

/* Periodic Frame Span Traversal Node */
typedef struct {
	ehci_link_t	fstn_link;
	ehci_link_t	fstn_back;
} ehci_fstn_t;
#define EHCI_FSTN_ALIGN 32

#endif /* _DEV_PCI_EHCIREG_H_ */
