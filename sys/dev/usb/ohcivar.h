/*	$NetBSD: ohcivar.h,v 1.21 2000/03/29 01:46:27 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
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

typedef struct ohci_soft_ed {
	ohci_ed_t ed;
	struct ohci_soft_ed *next;
	ohci_physaddr_t physaddr;
} ohci_soft_ed_t;
#define OHCI_SED_SIZE ((sizeof (struct ohci_soft_ed) + OHCI_ED_ALIGN - 1) / OHCI_ED_ALIGN * OHCI_ED_ALIGN)
#define OHCI_SED_CHUNK 128

typedef struct ohci_soft_td {
	ohci_td_t td;
	struct ohci_soft_td *nexttd; /* mirrors nexttd in TD */
	struct ohci_soft_td *dnext; /* next in done list */
	ohci_physaddr_t physaddr;
	LIST_ENTRY(ohci_soft_td) hnext;
	usbd_xfer_handle xfer;
	u_int16_t len;
	u_int16_t flags;
#define OHCI_CALL_DONE	0x0001
#define OHCI_ADD_LEN	0x0002
#define OHCI_TD_HANDLED	0x0004		/* signal process_done has seen it */
} ohci_soft_td_t;
#define OHCI_STD_SIZE ((sizeof (struct ohci_soft_td) + OHCI_TD_ALIGN - 1) / OHCI_TD_ALIGN * OHCI_TD_ALIGN)
#define OHCI_STD_CHUNK 128

typedef struct ohci_soft_itd {
	ohci_itd_t itd;
	struct ohci_soft_itd *nextitd; /* mirrors nexttd in ITD */
	struct ohci_soft_itd *dnext; /* next in done list */
	ohci_physaddr_t physaddr;
	LIST_ENTRY(ohci_soft_itd) hnext;
	usbd_xfer_handle xfer;
	u_int16_t flags;
#ifdef DIAGNOSTIC
	char isdone;
#endif
} ohci_soft_itd_t;
#define OHCI_SITD_SIZE ((sizeof (struct ohci_soft_itd) + OHCI_ITD_ALIGN - 1) / OHCI_ITD_ALIGN * OHCI_ITD_ALIGN)
#define OHCI_SITD_CHUNK 64

#define OHCI_NO_EDS (2*OHCI_NO_INTRS-1)

#define OHCI_HASH_SIZE 128

typedef struct ohci_softc {
	struct usbd_bus sc_bus;		/* base device */
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_size_t sc_size;

#if defined(__FreeBSD__)
	void *ih;

	struct resource *io_res;
	struct resource *irq_res;
#endif

	usb_dma_t sc_hccadma;
	struct ohci_hcca *sc_hcca;
	ohci_soft_ed_t *sc_eds[OHCI_NO_EDS];
	u_int sc_bws[OHCI_NO_INTRS];

	u_int32_t sc_eintrs;		/* enabled interrupts */

	ohci_soft_ed_t *sc_isoc_head;
	ohci_soft_ed_t *sc_ctrl_head;
	ohci_soft_ed_t *sc_bulk_head;

	LIST_HEAD(, ohci_soft_td)  sc_hash_tds[OHCI_HASH_SIZE];
	LIST_HEAD(, ohci_soft_itd) sc_hash_itds[OHCI_HASH_SIZE];

	int sc_noport;
	u_int8_t sc_addr;		/* device address */
	u_int8_t sc_conf;		/* device configuration */

	ohci_soft_ed_t *sc_freeeds;
	ohci_soft_td_t *sc_freetds;
	ohci_soft_itd_t *sc_freeitds;

	SIMPLEQ_HEAD(, usbd_xfer) sc_free_xfers; /* free xfers */

	usbd_xfer_handle sc_intrxfer;

	ohci_soft_itd_t *sc_sidone;
	ohci_soft_td_t  *sc_sdone;

	char sc_vendor[16];
	int sc_id_vendor;

#if defined(__NetBSD__) || defined(__OpenBSD__)
	void *sc_powerhook;		/* cookie from power hook */
	void *sc_shutdownhook;		/* cookie from shutdown hook */
#endif

	device_ptr_t sc_child;
	char sc_dying;
} ohci_softc_t;

usbd_status	ohci_init(ohci_softc_t *);
int		ohci_intr(void *);
#if defined(__NetBSD__) || defined(__OpenBSD__)
int		ohci_detach(ohci_softc_t *, int);
int		ohci_activate(device_ptr_t, enum devact);
#endif

#define MS_TO_TICKS(ms) ((ms) * hz / 1000)
