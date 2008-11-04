/* $FreeBSD$ */
/*-
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

#ifndef _EHCI_H_
#define	_EHCI_H_

/* PCI config registers */
#define	PCI_CBMEM		0x10	/* configuration base MEM */
#define	PCI_INTERFACE_EHCI	0x20
#define	PCI_USBREV		0x60	/* RO USB protocol revision */
#define	PCI_USB_REV_MASK	0xff
#define	PCI_USB_REV_PRE_1_0	0x00
#define	PCI_USB_REV_1_0		0x10
#define	PCI_USB_REV_1_1		0x11
#define	PCI_USB_REV_2_0		0x20
#define	PCI_EHCI_FLADJ		0x61	/* RW Frame len adj, SOF=59488+6*fladj */
#define	PCI_EHCI_PORTWAKECAP	0x62	/* RW Port wake caps (opt)  */

/* EHCI Extended Capabilities */
#define	EHCI_EC_LEGSUP		0x01
#define	EHCI_EECP_NEXT(x)	(((x) >> 8) & 0xff)
#define	EHCI_EECP_ID(x)		((x) & 0xff)

/* Legacy support extended capability */
#define	EHCI_LEGSUP_BIOS_SEM		0x02
#define	EHCI_LEGSUP_OS_SEM		0x03
#define	EHCI_LEGSUP_USBLEGCTLSTS	0x04

/* EHCI capability registers */
#define	EHCI_CAPLENGTH		0x00	/* RO Capability register length field */
/* reserved			0x01 */
#define	EHCI_HCIVERSION		0x02	/* RO Interface version number */
#define	EHCI_HCSPARAMS		0x04	/* RO Structural parameters */
#define	EHCI_HCS_DEBUGPORT(x)	(((x) >> 20) & 0xf)
#define	EHCI_HCS_P_INDICATOR(x) ((x) & 0x10000)
#define	EHCI_HCS_N_CC(x)	(((x) >> 12) & 0xf)	/* # of companion ctlrs */
#define	EHCI_HCS_N_PCC(x)	(((x) >> 8) & 0xf)	/* # of ports per comp. */
#define	EHCI_HCS_PPC(x)		((x) & 0x10)	/* port power control */
#define	EHCI_HCS_N_PORTS(x)	((x) & 0xf)	/* # of ports */
#define	EHCI_HCCPARAMS		0x08	/* RO Capability parameters */
#define	EHCI_HCC_EECP(x)	(((x) >> 8) & 0xff)	/* extended ports caps */
#define	EHCI_HCC_IST(x)		(((x) >> 4) & 0xf)	/* isoc sched threshold */
#define	EHCI_HCC_ASPC(x)	((x) & 0x4)	/* async sched park cap */
#define	EHCI_HCC_PFLF(x)	((x) & 0x2)	/* prog frame list flag */
#define	EHCI_HCC_64BIT(x)	((x) & 0x1)	/* 64 bit address cap */
#define	EHCI_HCSP_PORTROUTE	0x0c	/* RO Companion port route description */

/* EHCI operational registers.  Offset given by EHCI_CAPLENGTH register */
#define	EHCI_USBCMD		0x00	/* RO, RW, WO Command register */
#define	EHCI_CMD_ITC_M		0x00ff0000	/* RW interrupt threshold ctrl */
#define	EHCI_CMD_ITC_1		0x00010000
#define	EHCI_CMD_ITC_2		0x00020000
#define	EHCI_CMD_ITC_4		0x00040000
#define	EHCI_CMD_ITC_8		0x00080000
#define	EHCI_CMD_ITC_16		0x00100000
#define	EHCI_CMD_ITC_32		0x00200000
#define	EHCI_CMD_ITC_64		0x00400000
#define	EHCI_CMD_ASPME		0x00000800	/* RW/RO async park enable */
#define	EHCI_CMD_ASPMC		0x00000300	/* RW/RO async park count */
#define	EHCI_CMD_LHCR		0x00000080	/* RW light host ctrl reset */
#define	EHCI_CMD_IAAD		0x00000040	/* RW intr on async adv door
						 * bell */
#define	EHCI_CMD_ASE		0x00000020	/* RW async sched enable */
#define	EHCI_CMD_PSE		0x00000010	/* RW periodic sched enable */
#define	EHCI_CMD_FLS_M		0x0000000c	/* RW/RO frame list size */
#define	EHCI_CMD_FLS(x)		(((x) >> 2) & 3)	/* RW/RO frame list size */
#define	EHCI_CMD_HCRESET	0x00000002	/* RW reset */
#define	EHCI_CMD_RS		0x00000001	/* RW run/stop */
#define	EHCI_USBSTS		0x04	/* RO, RW, RWC Status register */
#define	EHCI_STS_ASS		0x00008000	/* RO async sched status */
#define	EHCI_STS_PSS		0x00004000	/* RO periodic sched status */
#define	EHCI_STS_REC		0x00002000	/* RO reclamation */
#define	EHCI_STS_HCH		0x00001000	/* RO host controller halted */
#define	EHCI_STS_IAA		0x00000020	/* RWC interrupt on async adv */
#define	EHCI_STS_HSE		0x00000010	/* RWC host system error */
#define	EHCI_STS_FLR		0x00000008	/* RWC frame list rollover */
#define	EHCI_STS_PCD		0x00000004	/* RWC port change detect */
#define	EHCI_STS_ERRINT		0x00000002	/* RWC error interrupt */
#define	EHCI_STS_INT		0x00000001	/* RWC interrupt */
#define	EHCI_STS_INTRS(x)	((x) & 0x3f)

/*
 * NOTE: the doorbell interrupt is enabled, but the doorbell is never
 * used! SiS chipsets require this.
 */
#define	EHCI_NORMAL_INTRS	(EHCI_STS_IAA | EHCI_STS_HSE |	\
				EHCI_STS_PCD | EHCI_STS_ERRINT | EHCI_STS_INT)

#define	EHCI_USBINTR		0x08	/* RW Interrupt register */
#define	EHCI_INTR_IAAE		0x00000020	/* interrupt on async advance
						 * ena */
#define	EHCI_INTR_HSEE		0x00000010	/* host system error ena */
#define	EHCI_INTR_FLRE		0x00000008	/* frame list rollover ena */
#define	EHCI_INTR_PCIE		0x00000004	/* port change ena */
#define	EHCI_INTR_UEIE		0x00000002	/* USB error intr ena */
#define	EHCI_INTR_UIE		0x00000001	/* USB intr ena */

#define	EHCI_FRINDEX		0x0c	/* RW Frame Index register */

#define	EHCI_CTRLDSSEGMENT	0x10	/* RW Control Data Structure Segment */

#define	EHCI_PERIODICLISTBASE	0x14	/* RW Periodic List Base */
#define	EHCI_ASYNCLISTADDR	0x18	/* RW Async List Base */

#define	EHCI_CONFIGFLAG		0x40	/* RW Configure Flag register */
#define	EHCI_CONF_CF		0x00000001	/* RW configure flag */

#define	EHCI_PORTSC(n)		(0x40+(4*(n)))	/* RO, RW, RWC Port Status reg */
#define	EHCI_PS_WKOC_E		0x00400000	/* RW wake on over current ena */
#define	EHCI_PS_WKDSCNNT_E	0x00200000	/* RW wake on disconnect ena */
#define	EHCI_PS_WKCNNT_E	0x00100000	/* RW wake on connect ena */
#define	EHCI_PS_PTC		0x000f0000	/* RW port test control */
#define	EHCI_PS_PIC		0x0000c000	/* RW port indicator control */
#define	EHCI_PS_PO		0x00002000	/* RW port owner */
#define	EHCI_PS_PP		0x00001000	/* RW,RO port power */
#define	EHCI_PS_LS		0x00000c00	/* RO line status */
#define	EHCI_PS_IS_LOWSPEED(x)	(((x) & EHCI_PS_LS) == 0x00000400)
#define	EHCI_PS_PR		0x00000100	/* RW port reset */
#define	EHCI_PS_SUSP		0x00000080	/* RW suspend */
#define	EHCI_PS_FPR		0x00000040	/* RW force port resume */
#define	EHCI_PS_OCC		0x00000020	/* RWC over current change */
#define	EHCI_PS_OCA		0x00000010	/* RO over current active */
#define	EHCI_PS_PEC		0x00000008	/* RWC port enable change */
#define	EHCI_PS_PE		0x00000004	/* RW port enable */
#define	EHCI_PS_CSC		0x00000002	/* RWC connect status change */
#define	EHCI_PS_CS		0x00000001	/* RO connect status */
#define	EHCI_PS_CLEAR		(EHCI_PS_OCC | EHCI_PS_PEC | EHCI_PS_CSC)

#define	EHCI_PORT_RESET_COMPLETE	2	/* ms */

/*
 * Alignment NOTE: structures must be aligned so that the hardware can index
 * without performing addition.
 */
#define	EHCI_FRAMELIST_ALIGN          0x1000	/* bytes */
#define	EHCI_FRAMELIST_COUNT            1024	/* units */
#define	EHCI_VIRTUAL_FRAMELIST_COUNT     128	/* units */

#if ((8*EHCI_VIRTUAL_FRAMELIST_COUNT) < USB_MAX_HS_ISOC_FRAMES_PER_XFER)
#error "maximum number of high-speed isochronous frames is higher than supported!"
#endif

#if (EHCI_VIRTUAL_FRAMELIST_COUNT < USB_MAX_FS_ISOC_FRAMES_PER_XFER)
#error "maximum number of full-speed isochronous frames is higher than supported!"
#endif

/* Link types */
#define	EHCI_LINK_TERMINATE	0x00000001
#define	EHCI_LINK_TYPE(x)	((x) & 0x00000006)
#define	EHCI_LINK_ITD		0x0
#define	EHCI_LINK_QH		0x2
#define	EHCI_LINK_SITD		0x4
#define	EHCI_LINK_FSTN		0x6
#define	EHCI_LINK_ADDR(x)	((x) &~ 0x1f)

/* Structures alignment (bytes) */
#define	EHCI_ITD_ALIGN	128
#define	EHCI_SITD_ALIGN	64
#define	EHCI_QTD_ALIGN	64
#define	EHCI_QH_ALIGN	128
#define	EHCI_FSTN_ALIGN	32
/* Data buffers are divided into one or more pages */
#define	EHCI_PAGE_SIZE	0x1000
#if	((USB_PAGE_SIZE < EHCI_PAGE_SIZE) || (EHCI_PAGE_SIZE == 0) ||	\
	(USB_PAGE_SIZE < EHCI_ITD_ALIGN) || (EHCI_ITD_ALIGN == 0) ||	\
	(USB_PAGE_SIZE < EHCI_SITD_ALIGN) || (EHCI_SITD_ALIGN == 0) ||	\
	(USB_PAGE_SIZE < EHCI_QTD_ALIGN) || (EHCI_QTD_ALIGN == 0) ||	\
	(USB_PAGE_SIZE < EHCI_QH_ALIGN) || (EHCI_QH_ALIGN == 0) ||	\
	(USB_PAGE_SIZE < EHCI_FSTN_ALIGN) || (EHCI_FSTN_ALIGN == 0))
#error	"Invalid USB page size!"
#endif


/*
 * Isochronous Transfer Descriptor.  This descriptor is used for high speed
 * transfers only.
 */
struct ehci_itd {
	volatile uint32_t itd_next;
	volatile uint32_t itd_status[8];
#define	EHCI_ITD_SET_LEN(x)	((x) << 16)
#define	EHCI_ITD_GET_LEN(x)	(((x) >> 16) & 0xFFF)
#define	EHCI_ITD_IOC		(1 << 15)
#define	EHCI_ITD_SET_PG(x)	((x) << 12)
#define	EHCI_ITD_GET_PG(x)	(((x) >> 12) & 0x7)
#define	EHCI_ITD_SET_OFFS(x)	(x)
#define	EHCI_ITD_GET_OFFS(x)	(((x) >> 0) & 0xFFF)
#define	EHCI_ITD_ACTIVE		(1 << 31)
#define	EHCI_ITD_DATABUFERR	(1 << 30)
#define	EHCI_ITD_BABBLE		(1 << 29)
#define	EHCI_ITD_XACTERR	(1 << 28)
	volatile uint32_t itd_bp[7];
	/* itd_bp[0] */
#define	EHCI_ITD_SET_ADDR(x)	(x)
#define	EHCI_ITD_GET_ADDR(x)	(((x) >> 0) & 0x7F)
#define	EHCI_ITD_SET_ENDPT(x)	((x) << 8)
#define	EHCI_ITD_GET_ENDPT(x)	(((x) >> 8) & 0xF)
	/* itd_bp[1] */
#define	EHCI_ITD_SET_DIR_IN	(1 << 11)
#define	EHCI_ITD_SET_DIR_OUT	(0 << 11)
#define	EHCI_ITD_SET_MPL(x)	(x)
#define	EHCI_ITD_GET_MPL(x)	(((x) >> 0) & 0x7FF)
	volatile uint32_t itd_bp_hi[7];
/*
 * Extra information needed:
 */
	uint32_t itd_self;
	struct ehci_itd *next;
	struct ehci_itd *prev;
	struct ehci_itd *obj_next;
	struct usb2_page_cache *page_cache;
} __aligned(EHCI_ITD_ALIGN);

typedef struct ehci_itd ehci_itd_t;

/*
 * Split Transaction Isochronous Transfer Descriptor.  This descriptor is used
 * for full speed transfers only.
 */
struct ehci_sitd {
	volatile uint32_t sitd_next;
	volatile uint32_t sitd_portaddr;
#define	EHCI_SITD_SET_DIR_OUT	(0 << 31)
#define	EHCI_SITD_SET_DIR_IN	(1 << 31)
#define	EHCI_SITD_SET_ADDR(x)	(x)
#define	EHCI_SITD_GET_ADDR(x)	((x) & 0x7F)
#define	EHCI_SITD_SET_ENDPT(x)	((x) << 8)
#define	EHCI_SITD_GET_ENDPT(x)	(((x) >> 8) & 0xF)
#define	EHCI_SITD_GET_DIR(x)	((x) >> 31)
#define	EHCI_SITD_SET_PORT(x)	((x) << 24)
#define	EHCI_SITD_GET_PORT(x)	(((x) >> 24) & 0x7F)
#define	EHCI_SITD_SET_HUBA(x)	((x) << 16)
#define	EHCI_SITD_GET_HUBA(x)	(((x) >> 16) & 0x7F)
	volatile uint32_t sitd_mask;
#define	EHCI_SITD_SET_SMASK(x)	(x)
#define	EHCI_SITD_SET_CMASK(x)	((x) << 8)
	volatile uint32_t sitd_status;
#define	EHCI_SITD_COMPLETE_SPLIT	(1<<1)
#define	EHCI_SITD_START_SPLIT		(0<<1)
#define	EHCI_SITD_MISSED_MICRO_FRAME	(1<<2)
#define	EHCI_SITD_XACTERR		(1<<3)
#define	EHCI_SITD_BABBLE		(1<<4)
#define	EHCI_SITD_DATABUFERR		(1<<5)
#define	EHCI_SITD_ERROR			(1<<6)
#define	EHCI_SITD_ACTIVE		(1<<7)
#define	EHCI_SITD_IOC			(1<<31)
#define	EHCI_SITD_SET_LEN(len)		((len)<<16)
#define	EHCI_SITD_GET_LEN(x)		(((x)>>16) & 0x3FF)
	volatile uint32_t sitd_bp[2];
	volatile uint32_t sitd_back;
	volatile uint32_t sitd_bp_hi[2];
/*
 * Extra information needed:
 */
	uint32_t sitd_self;
	struct ehci_sitd *next;
	struct ehci_sitd *prev;
	struct ehci_sitd *obj_next;
	struct usb2_page_cache *page_cache;
} __aligned(EHCI_SITD_ALIGN);

typedef struct ehci_sitd ehci_sitd_t;

/* Queue Element Transfer Descriptor */
struct ehci_qtd {
	volatile uint32_t qtd_next;
	volatile uint32_t qtd_altnext;
	volatile uint32_t qtd_status;
#define	EHCI_QTD_GET_STATUS(x)	(((x) >>  0) & 0xff)
#define	EHCI_QTD_SET_STATUS(x)  ((x) << 0)
#define	EHCI_QTD_ACTIVE		0x80
#define	EHCI_QTD_HALTED		0x40
#define	EHCI_QTD_BUFERR		0x20
#define	EHCI_QTD_BABBLE		0x10
#define	EHCI_QTD_XACTERR	0x08
#define	EHCI_QTD_MISSEDMICRO	0x04
#define	EHCI_QTD_SPLITXSTATE	0x02
#define	EHCI_QTD_PINGSTATE	0x01
#define	EHCI_QTD_STATERRS	0x74
#define	EHCI_QTD_GET_PID(x)	(((x) >>  8) & 0x3)
#define	EHCI_QTD_SET_PID(x)	((x) <<  8)
#define	EHCI_QTD_PID_OUT	0x0
#define	EHCI_QTD_PID_IN		0x1
#define	EHCI_QTD_PID_SETUP	0x2
#define	EHCI_QTD_GET_CERR(x)	(((x) >> 10) &  0x3)
#define	EHCI_QTD_SET_CERR(x)	((x) << 10)
#define	EHCI_QTD_GET_C_PAGE(x)	(((x) >> 12) &  0x7)
#define	EHCI_QTD_SET_C_PAGE(x)	((x) << 12)
#define	EHCI_QTD_GET_IOC(x)	(((x) >> 15) &  0x1)
#define	EHCI_QTD_IOC		0x00008000
#define	EHCI_QTD_GET_BYTES(x)	(((x) >> 16) &  0x7fff)
#define	EHCI_QTD_SET_BYTES(x)	((x) << 16)
#define	EHCI_QTD_GET_TOGGLE(x)	(((x) >> 31) &  0x1)
#define	EHCI_QTD_SET_TOGGLE(x)	((x) << 31)
#define	EHCI_QTD_TOGGLE_MASK	0x80000000
#define	EHCI_QTD_NBUFFERS	5
#define	EHCI_QTD_PAYLOAD_MAX ((EHCI_QTD_NBUFFERS-1)*EHCI_PAGE_SIZE)
	volatile uint32_t qtd_buffer[EHCI_QTD_NBUFFERS];
	volatile uint32_t qtd_buffer_hi[EHCI_QTD_NBUFFERS];
/*
 * Extra information needed:
 */
	struct ehci_qtd *alt_next;
	struct ehci_qtd *obj_next;
	struct usb2_page_cache *page_cache;
	uint32_t qtd_self;
	uint16_t len;
} __aligned(EHCI_QTD_ALIGN);

typedef struct ehci_qtd ehci_qtd_t;

/* Queue Head Sub Structure */
struct ehci_qh_sub {
	volatile uint32_t qtd_next;
	volatile uint32_t qtd_altnext;
	volatile uint32_t qtd_status;
	volatile uint32_t qtd_buffer[EHCI_QTD_NBUFFERS];
	volatile uint32_t qtd_buffer_hi[EHCI_QTD_NBUFFERS];
} __aligned(4);

/* Queue Head */
struct ehci_qh {
	volatile uint32_t qh_link;
	volatile uint32_t qh_endp;
#define	EHCI_QH_GET_ADDR(x)	(((x) >>  0) & 0x7f)	/* endpoint addr */
#define	EHCI_QH_SET_ADDR(x)	(x)
#define	EHCI_QH_ADDRMASK	0x0000007f
#define	EHCI_QH_GET_INACT(x)	(((x) >>  7) & 0x01)	/* inactivate on next */
#define	EHCI_QH_INACT		0x00000080
#define	EHCI_QH_GET_ENDPT(x)	(((x) >>  8) & 0x0f)	/* endpoint no */
#define	EHCI_QH_SET_ENDPT(x)	((x) <<  8)
#define	EHCI_QH_GET_EPS(x)	(((x) >> 12) & 0x03)	/* endpoint speed */
#define	EHCI_QH_SET_EPS(x)	((x) << 12)
#define	EHCI_QH_SPEED_FULL	0x0
#define	EHCI_QH_SPEED_LOW	0x1
#define	EHCI_QH_SPEED_HIGH	0x2
#define	EHCI_QH_GET_DTC(x)	(((x) >> 14) & 0x01)	/* data toggle control */
#define	EHCI_QH_DTC		0x00004000
#define	EHCI_QH_GET_HRECL(x)	(((x) >> 15) & 0x01)	/* head of reclamation */
#define	EHCI_QH_HRECL		0x00008000
#define	EHCI_QH_GET_MPL(x)	(((x) >> 16) & 0x7ff)	/* max packet len */
#define	EHCI_QH_SET_MPL(x)	((x) << 16)
#define	EHCI_QH_MPLMASK		0x07ff0000
#define	EHCI_QH_GET_CTL(x)	(((x) >> 27) & 0x01)	/* control endpoint */
#define	EHCI_QH_CTL		0x08000000
#define	EHCI_QH_GET_NRL(x)	(((x) >> 28) & 0x0f)	/* NAK reload */
#define	EHCI_QH_SET_NRL(x)	((x) << 28)
	volatile uint32_t qh_endphub;
#define	EHCI_QH_GET_SMASK(x)	(((x) >>  0) & 0xff)	/* intr sched mask */
#define	EHCI_QH_SET_SMASK(x)	((x) <<  0)
#define	EHCI_QH_GET_CMASK(x)	(((x) >>  8) & 0xff)	/* split completion mask */
#define	EHCI_QH_SET_CMASK(x)	((x) <<  8)
#define	EHCI_QH_GET_HUBA(x)	(((x) >> 16) & 0x7f)	/* hub address */
#define	EHCI_QH_SET_HUBA(x)	((x) << 16)
#define	EHCI_QH_GET_PORT(x)	(((x) >> 23) & 0x7f)	/* hub port */
#define	EHCI_QH_SET_PORT(x)	((x) << 23)
#define	EHCI_QH_GET_MULT(x)	(((x) >> 30) & 0x03)	/* pipe multiplier */
#define	EHCI_QH_SET_MULT(x)	((x) << 30)
	volatile uint32_t qh_curqtd;
	struct ehci_qh_sub qh_qtd;
/*
 * Extra information needed:
 */
	struct ehci_qh *next;
	struct ehci_qh *prev;
	struct ehci_qh *obj_next;
	struct usb2_page_cache *page_cache;
	uint32_t qh_self;
} __aligned(EHCI_QH_ALIGN);

typedef struct ehci_qh ehci_qh_t;

/* Periodic Frame Span Traversal Node */
struct ehci_fstn {
	volatile uint32_t fstn_link;
	volatile uint32_t fstn_back;
} __aligned(EHCI_FSTN_ALIGN);

typedef struct ehci_fstn ehci_fstn_t;

struct ehci_hw_softc {
	struct usb2_page_cache pframes_pc;
	struct usb2_page_cache async_start_pc;
	struct usb2_page_cache intr_start_pc[EHCI_VIRTUAL_FRAMELIST_COUNT];
	struct usb2_page_cache isoc_hs_start_pc[EHCI_VIRTUAL_FRAMELIST_COUNT];
	struct usb2_page_cache isoc_fs_start_pc[EHCI_VIRTUAL_FRAMELIST_COUNT];

	struct usb2_page pframes_pg;
	struct usb2_page async_start_pg;
	struct usb2_page intr_start_pg[EHCI_VIRTUAL_FRAMELIST_COUNT];
	struct usb2_page isoc_hs_start_pg[EHCI_VIRTUAL_FRAMELIST_COUNT];
	struct usb2_page isoc_fs_start_pg[EHCI_VIRTUAL_FRAMELIST_COUNT];
};

struct ehci_config_desc {
	struct usb2_config_descriptor confd;
	struct usb2_interface_descriptor ifcd;
	struct usb2_endpoint_descriptor endpd;
} __packed;

union ehci_hub_desc {
	struct usb2_status stat;
	struct usb2_port_status ps;
	struct usb2_device_descriptor devd;
	struct usb2_device_qualifier odevd;
	struct usb2_hub_descriptor hubd;
	uint8_t	temp[128];
};

typedef struct ehci_softc {
	struct ehci_hw_softc sc_hw;
	struct usb2_bus sc_bus;		/* base device */
	struct usb2_config_td sc_config_td;
	struct usb2_callout sc_tmo_pcd;
	union ehci_hub_desc sc_hub_desc;
	struct usb2_sw_transfer sc_root_ctrl;
	struct usb2_sw_transfer sc_root_intr;

	struct resource *sc_io_res;
	struct resource *sc_irq_res;
	struct ehci_qh *sc_async_p_last;
	struct ehci_qh *sc_intr_p_last[EHCI_VIRTUAL_FRAMELIST_COUNT];
	struct ehci_sitd *sc_isoc_fs_p_last[EHCI_VIRTUAL_FRAMELIST_COUNT];
	struct ehci_itd *sc_isoc_hs_p_last[EHCI_VIRTUAL_FRAMELIST_COUNT];
	void   *sc_intr_hdl;
	device_t sc_dev;
	bus_size_t sc_io_size;
	bus_space_tag_t sc_io_tag;
	bus_space_handle_t sc_io_hdl;

	uint32_t sc_eintrs;
	uint32_t sc_cmd;		/* shadow of cmd register during
					 * suspend */

	uint16_t sc_intr_stat[EHCI_VIRTUAL_FRAMELIST_COUNT];
	uint16_t sc_id_vendor;		/* vendor ID for root hub */

	uint8_t	sc_offs;		/* offset to operational registers */
	uint8_t	sc_doorbell_disable;	/* set on doorbell failure */
	uint8_t	sc_noport;
	uint8_t	sc_addr;		/* device address */
	uint8_t	sc_conf;		/* device configuration */
	uint8_t	sc_isreset;
	uint8_t	sc_hub_idata[8];

	char	sc_vendor[16];		/* vendor string for root hub */

} ehci_softc_t;

#define	EREAD1(sc, a)	bus_space_read_1((sc)->sc_io_tag, (sc)->sc_io_hdl, (a))
#define	EREAD2(sc, a)	bus_space_read_2((sc)->sc_io_tag, (sc)->sc_io_hdl, (a))
#define	EREAD4(sc, a)	bus_space_read_4((sc)->sc_io_tag, (sc)->sc_io_hdl, (a))
#define	EWRITE1(sc, a, x)						\
	    bus_space_write_1((sc)->sc_io_tag, (sc)->sc_io_hdl, (a), (x))
#define	EWRITE2(sc, a, x)						\
	    bus_space_write_2((sc)->sc_io_tag, (sc)->sc_io_hdl, (a), (x))
#define	EWRITE4(sc, a, x)						\
	    bus_space_write_4((sc)->sc_io_tag, (sc)->sc_io_hdl, (a), (x))
#define	EOREAD1(sc, a)							\
	    bus_space_read_1((sc)->sc_io_tag, (sc)->sc_io_hdl, (sc)->sc_offs+(a))
#define	EOREAD2(sc, a)							\
	    bus_space_read_2((sc)->sc_io_tag, (sc)->sc_io_hdl, (sc)->sc_offs+(a))
#define	EOREAD4(sc, a)							\
	    bus_space_read_4((sc)->sc_io_tag, (sc)->sc_io_hdl, (sc)->sc_offs+(a))
#define	EOWRITE1(sc, a, x)						\
	    bus_space_write_1((sc)->sc_io_tag, (sc)->sc_io_hdl, (sc)->sc_offs+(a), (x))
#define	EOWRITE2(sc, a, x)						\
	    bus_space_write_2((sc)->sc_io_tag, (sc)->sc_io_hdl, (sc)->sc_offs+(a), (x))
#define	EOWRITE4(sc, a, x)						\
	    bus_space_write_4((sc)->sc_io_tag, (sc)->sc_io_hdl, (sc)->sc_offs+(a), (x))

usb2_bus_mem_cb_t ehci_iterate_hw_softc;

usb2_error_t ehci_init(ehci_softc_t *sc);
void	ehci_detach(struct ehci_softc *sc);
void	ehci_suspend(struct ehci_softc *sc);
void	ehci_resume(struct ehci_softc *sc);
void	ehci_shutdown(ehci_softc_t *sc);
void	ehci_interrupt(ehci_softc_t *sc);

#endif					/* _EHCI_H_ */
