/*	$NetBSD: ohcivar.h,v 1.4 1998/12/26 12:53:01 augustss Exp $	*/
/*	$FreeBSD$	*/

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (augustss@carlstedt.se) at
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
	ohci_ed_t *ed;
	struct ohci_soft_ed *next;
	ohci_physaddr_t physaddr;
} ohci_soft_ed_t;
#define OHCI_ED_CHUNK 256

typedef struct ohci_soft_td {
	ohci_td_t *td;
	struct ohci_soft_td *nexttd; /* mirrors nexttd in TD */
	struct ohci_soft_td *dnext; /* next in done list */
	ohci_physaddr_t physaddr;
	LIST_ENTRY(ohci_soft_td) hnext;
	/*ohci_soft_ed_t *sed;*/
	usbd_request_handle reqh;
	u_int16_t len;
} ohci_soft_td_t;
#define OHCI_TD_CHUNK 256

#define OHCI_NO_EDS (2*OHCI_NO_INTRS-1)

#define OHCI_HASH_SIZE 128

typedef struct ohci_softc {
	struct usbd_bus sc_bus;		/* base device */
#if defined(__NetBSD__)
	void *sc_ih;			/* interrupt vectoring */
	bus_space_tag_t iot;
	bus_space_handle_t ioh;

	bus_dma_tag_t sc_dmatag;	/* DMA tag */
	/* XXX should keep track of all DMA memory */

#elif defined(__FreeBSD__)
        int             sc_iobase;
#endif /* __FreeBSD__ */

	usb_dma_t sc_hccadma;
	struct ohci_hcca *sc_hcca;
	ohci_soft_ed_t *sc_eds[OHCI_NO_EDS];
	u_int sc_bws[OHCI_NO_INTRS];

	u_int32_t sc_eintrs;
	ohci_soft_ed_t *sc_ctrl_head;
	ohci_soft_ed_t *sc_bulk_head;

	LIST_HEAD(, ohci_soft_td) sc_hash_tds[OHCI_HASH_SIZE];

	int sc_noport;
	u_int8_t sc_addr;		/* device address */
	u_int8_t sc_conf;		/* device configuration */

	ohci_soft_ed_t *sc_freeeds;
	ohci_soft_td_t *sc_freetds;

	usbd_request_handle sc_intrreqh;

	int sc_intrs;
	char sc_vendor[16];
} ohci_softc_t;

usbd_status	ohci_init __P((ohci_softc_t *));
int		ohci_intr __P((void *));

#define MS_TO_TICKS(ms) ((ms) * hz / 1000)

