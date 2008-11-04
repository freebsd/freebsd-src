/* $FreeBSD$ */
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) at
 * Carlstedt Research & Technology.
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

#ifndef _UHCI_H_
#define	_UHCI_H_

/* PCI config registers */
#define	PCI_USBREV		0x60	/* USB protocol revision */
#define	PCI_USB_REV_MASK		0xff
#define	PCI_USB_REV_PRE_1_0	0x00
#define	PCI_USB_REV_1_0		0x10
#define	PCI_USB_REV_1_1		0x11
#define	PCI_LEGSUP		0xc0	/* Legacy Support register */
#define	PCI_LEGSUP_USBPIRQDEN	0x2000	/* USB PIRQ D Enable */
#define	PCI_CBIO		0x20	/* configuration base IO */
#define	PCI_INTERFACE_UHCI	0x00

/* UHCI registers */
#define	UHCI_CMD		0x00
#define	UHCI_CMD_RS		0x0001
#define	UHCI_CMD_HCRESET	0x0002
#define	UHCI_CMD_GRESET		0x0004
#define	UHCI_CMD_EGSM		0x0008
#define	UHCI_CMD_FGR		0x0010
#define	UHCI_CMD_SWDBG		0x0020
#define	UHCI_CMD_CF		0x0040
#define	UHCI_CMD_MAXP		0x0080
#define	UHCI_STS		0x02
#define	UHCI_STS_USBINT		0x0001
#define	UHCI_STS_USBEI		0x0002
#define	UHCI_STS_RD		0x0004
#define	UHCI_STS_HSE		0x0008
#define	UHCI_STS_HCPE		0x0010
#define	UHCI_STS_HCH		0x0020
#define	UHCI_STS_ALLINTRS	0x003f
#define	UHCI_INTR		0x04
#define	UHCI_INTR_TOCRCIE	0x0001
#define	UHCI_INTR_RIE		0x0002
#define	UHCI_INTR_IOCE		0x0004
#define	UHCI_INTR_SPIE		0x0008
#define	UHCI_FRNUM		0x06
#define	UHCI_FRNUM_MASK		0x03ff
#define	UHCI_FLBASEADDR		0x08
#define	UHCI_SOF		0x0c
#define	UHCI_SOF_MASK		0x7f
#define	UHCI_PORTSC1      	0x010
#define	UHCI_PORTSC2      	0x012
#define	UHCI_PORTSC_CCS		0x0001
#define	UHCI_PORTSC_CSC		0x0002
#define	UHCI_PORTSC_PE		0x0004
#define	UHCI_PORTSC_POEDC	0x0008
#define	UHCI_PORTSC_LS		0x0030
#define	UHCI_PORTSC_LS_SHIFT	4
#define	UHCI_PORTSC_RD		0x0040
#define	UHCI_PORTSC_LSDA	0x0100
#define	UHCI_PORTSC_PR		0x0200
#define	UHCI_PORTSC_OCI		0x0400
#define	UHCI_PORTSC_OCIC	0x0800
#define	UHCI_PORTSC_SUSP	0x1000

#define	URWMASK(x)		((x) & (UHCI_PORTSC_SUSP |		\
				UHCI_PORTSC_PR | UHCI_PORTSC_RD |	\
				UHCI_PORTSC_PE))

#define	UHCI_FRAMELIST_COUNT	1024	/* units */
#define	UHCI_FRAMELIST_ALIGN	4096	/* bytes */

/* Structures alignment (bytes) */
#define	UHCI_TD_ALIGN		16
#define	UHCI_QH_ALIGN		16

#if	((USB_PAGE_SIZE < UHCI_TD_ALIGN) || (UHCI_TD_ALIGN == 0) ||	\
	(USB_PAGE_SIZE < UHCI_QH_ALIGN) || (UHCI_QH_ALIGN == 0))
#error	"Invalid USB page size!"
#endif

typedef uint32_t uhci_physaddr_t;

#define	UHCI_PTR_T		0x00000001
#define	UHCI_PTR_TD		0x00000000
#define	UHCI_PTR_QH		0x00000002
#define	UHCI_PTR_VF		0x00000004

#define	UHCI_QH_REMOVE_DELAY	5	/* us - QH remove delay */

/*
 * The Queue Heads (QH) and Transfer Descriptors (TD) are accessed by
 * both the CPU and the USB-controller which run concurrently. Great
 * care must be taken. When the data-structures are linked into the
 * USB controller's frame list, the USB-controller "owns" the
 * td_status and qh_elink fields, which will not be written by the
 * CPU.
 *
 */

struct uhci_td {
/*
 * Data used by the UHCI controller.
 * volatile is used in order to mantain struct members ordering.
 */
	volatile uint32_t td_next;
	volatile uint32_t td_status;
#define	UHCI_TD_GET_ACTLEN(s)	(((s) + 1) & 0x3ff)
#define	UHCI_TD_ZERO_ACTLEN(t)	((t) | 0x3ff)
#define	UHCI_TD_BITSTUFF	0x00020000
#define	UHCI_TD_CRCTO		0x00040000
#define	UHCI_TD_NAK		0x00080000
#define	UHCI_TD_BABBLE		0x00100000
#define	UHCI_TD_DBUFFER		0x00200000
#define	UHCI_TD_STALLED		0x00400000
#define	UHCI_TD_ACTIVE		0x00800000
#define	UHCI_TD_IOC		0x01000000
#define	UHCI_TD_IOS		0x02000000
#define	UHCI_TD_LS		0x04000000
#define	UHCI_TD_GET_ERRCNT(s)	(((s) >> 27) & 3)
#define	UHCI_TD_SET_ERRCNT(n)	((n) << 27)
#define	UHCI_TD_SPD		0x20000000
	volatile uint32_t td_token;
#define	UHCI_TD_PID		0x000000ff
#define	UHCI_TD_PID_IN		0x00000069
#define	UHCI_TD_PID_OUT		0x000000e1
#define	UHCI_TD_PID_SETUP	0x0000002d
#define	UHCI_TD_GET_PID(s)	((s) & 0xff)
#define	UHCI_TD_SET_DEVADDR(a)	((a) << 8)
#define	UHCI_TD_GET_DEVADDR(s)	(((s) >> 8) & 0x7f)
#define	UHCI_TD_SET_ENDPT(e)	(((e) & 0xf) << 15)
#define	UHCI_TD_GET_ENDPT(s)	(((s) >> 15) & 0xf)
#define	UHCI_TD_SET_DT(t)	((t) << 19)
#define	UHCI_TD_GET_DT(s)	(((s) >> 19) & 1)
#define	UHCI_TD_SET_MAXLEN(l)	(((l)-1) << 21)
#define	UHCI_TD_GET_MAXLEN(s)	((((s) >> 21) + 1) & 0x7ff)
#define	UHCI_TD_MAXLEN_MASK	0xffe00000
	volatile uint32_t td_buffer;
/*
 * Extra information needed:
 */
	struct uhci_td *next;
	struct uhci_td *prev;
	struct uhci_td *obj_next;
	struct usb2_page_cache *page_cache;
	struct usb2_page_cache *fix_pc;
	uint32_t td_self;
	uint16_t len;
} __aligned(UHCI_TD_ALIGN);

typedef struct uhci_td uhci_td_t;

#define	UHCI_TD_ERROR	(UHCI_TD_BITSTUFF | UHCI_TD_CRCTO | 		\
			UHCI_TD_BABBLE | UHCI_TD_DBUFFER | UHCI_TD_STALLED)

#define	UHCI_TD_SETUP(len, endp, dev)	(UHCI_TD_SET_MAXLEN(len) |	\
					UHCI_TD_SET_ENDPT(endp) |	\
					UHCI_TD_SET_DEVADDR(dev) |	\
					UHCI_TD_PID_SETUP)

#define	UHCI_TD_OUT(len, endp, dev, dt)	(UHCI_TD_SET_MAXLEN(len) |	\
					UHCI_TD_SET_ENDPT(endp) |	\
					UHCI_TD_SET_DEVADDR(dev) |	\
					UHCI_TD_PID_OUT | UHCI_TD_SET_DT(dt))

#define	UHCI_TD_IN(len, endp, dev, dt)	(UHCI_TD_SET_MAXLEN(len) |	\
					UHCI_TD_SET_ENDPT(endp) |	\
					UHCI_TD_SET_DEVADDR(dev) |	\
					UHCI_TD_PID_IN | UHCI_TD_SET_DT(dt))

struct uhci_qh {
/*
 * Data used by the UHCI controller.
 */
	volatile uint32_t qh_h_next;
	volatile uint32_t qh_e_next;
/*
 * Extra information needed:
 */
	struct uhci_qh *h_next;
	struct uhci_qh *h_prev;
	struct uhci_qh *obj_next;
	struct uhci_td *e_next;
	struct usb2_page_cache *page_cache;
	uint32_t qh_self;
	uint16_t intr_pos;
} __aligned(UHCI_QH_ALIGN);

typedef struct uhci_qh uhci_qh_t;

/* Maximum number of isochronous TD's and QH's interrupt */
#define	UHCI_VFRAMELIST_COUNT	128
#define	UHCI_IFRAMELIST_COUNT	(2 * UHCI_VFRAMELIST_COUNT)

#if	(((UHCI_VFRAMELIST_COUNT & (UHCI_VFRAMELIST_COUNT-1)) != 0) ||	\
	(UHCI_VFRAMELIST_COUNT > UHCI_FRAMELIST_COUNT))
#error	"UHCI_VFRAMELIST_COUNT is not power of two"
#error	"or UHCI_VFRAMELIST_COUNT > UHCI_FRAMELIST_COUNT"
#endif

#if (UHCI_VFRAMELIST_COUNT < USB_MAX_FS_ISOC_FRAMES_PER_XFER)
#error "maximum number of full-speed isochronous frames is higher than supported!"
#endif

struct uhci_config_desc {
	struct usb2_config_descriptor confd;
	struct usb2_interface_descriptor ifcd;
	struct usb2_endpoint_descriptor endpd;
} __packed;

union uhci_hub_desc {
	struct usb2_status stat;
	struct usb2_port_status ps;
	struct usb2_device_descriptor devd;
	uint8_t	temp[128];
};

struct uhci_hw_softc {
	struct usb2_page_cache pframes_pc;
	struct usb2_page_cache isoc_start_pc[UHCI_VFRAMELIST_COUNT];
	struct usb2_page_cache intr_start_pc[UHCI_IFRAMELIST_COUNT];
	struct usb2_page_cache ls_ctl_start_pc;
	struct usb2_page_cache fs_ctl_start_pc;
	struct usb2_page_cache bulk_start_pc;
	struct usb2_page_cache last_qh_pc;
	struct usb2_page_cache last_td_pc;

	struct usb2_page pframes_pg;
	struct usb2_page isoc_start_pg[UHCI_VFRAMELIST_COUNT];
	struct usb2_page intr_start_pg[UHCI_IFRAMELIST_COUNT];
	struct usb2_page ls_ctl_start_pg;
	struct usb2_page fs_ctl_start_pg;
	struct usb2_page bulk_start_pg;
	struct usb2_page last_qh_pg;
	struct usb2_page last_td_pg;
};

typedef struct uhci_softc {
	struct uhci_hw_softc sc_hw;
	struct usb2_bus sc_bus;		/* base device */
	struct usb2_config_td sc_config_td;
	union uhci_hub_desc sc_hub_desc;
	struct usb2_sw_transfer sc_root_ctrl;
	struct usb2_sw_transfer sc_root_intr;

	struct uhci_td *sc_isoc_p_last[UHCI_VFRAMELIST_COUNT];	/* pointer to last TD
								 * for isochronous */
	struct uhci_qh *sc_intr_p_last[UHCI_IFRAMELIST_COUNT];	/* pointer to last QH
								 * for interrupt */
	struct uhci_qh *sc_ls_ctl_p_last;	/* pointer to last QH for low
						 * speed control */
	struct uhci_qh *sc_fs_ctl_p_last;	/* pointer to last QH for full
						 * speed control */
	struct uhci_qh *sc_bulk_p_last;	/* pointer to last QH for bulk */
	struct uhci_qh *sc_reclaim_qh_p;
	struct uhci_qh *sc_last_qh_p;
	struct uhci_td *sc_last_td_p;
	struct resource *sc_io_res;
	struct resource *sc_irq_res;
	void   *sc_intr_hdl;
	device_t sc_dev;
	bus_size_t sc_io_size;
	bus_space_tag_t sc_io_tag;
	bus_space_handle_t sc_io_hdl;

	uint32_t sc_loops;		/* number of QHs that wants looping */

	uint16_t sc_intr_stat[UHCI_IFRAMELIST_COUNT];
	uint16_t sc_saved_frnum;

	uint8_t	sc_addr;		/* device address */
	uint8_t	sc_conf;		/* device configuration */
	uint8_t	sc_isreset;
	uint8_t	sc_saved_sof;
	uint8_t	sc_hub_idata[1];

	char	sc_vendor[16];		/* vendor string for root hub */
} uhci_softc_t;

usb2_bus_mem_cb_t uhci_iterate_hw_softc;

usb2_error_t uhci_init(uhci_softc_t *sc);
void	uhci_suspend(uhci_softc_t *sc);
void	uhci_resume(uhci_softc_t *sc);
void	uhci_reset(uhci_softc_t *sc);
void	uhci_interrupt(uhci_softc_t *sc);

#endif					/* _UHCI_H_ */
