/*	$NetBSD: uhci.c,v 1.24 1999/02/20 23:26:16 augustss Exp $	*/
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

/*
 * USB Universal Host Controller driver.
 * Handles PIIX3 and PIIX4.
 *
 * Data sheets: ftp://download.intel.com/design/intarch/datashts/29055002.pdf
 *              ftp://download.intel.com/design/intarch/datashts/29056201.pdf
 * UHCI spec: http://www.intel.com/design/usb/uhci11d.pdf
 * USB spec: http://www.usb.org/cgi-usb/mailmerge.cgi/home/usb/docs/developers/
cgiform.tpl
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#if defined(__NetBSD__)
#include <sys/device.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#endif
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/select.h>

#if defined(__FreeBSD__)
#include <machine/bus_pio.h>
#endif
#include <machine/bus.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/uhcireg.h>
#include <dev/usb/uhcivar.h>

#if defined(__FreeBSD__)
#include <machine/clock.h>

#define delay(d)		DELAY(d)
#endif

#ifdef UHCI_DEBUG
#define DPRINTF(x)	if (uhcidebug) logprintf x
#define DPRINTFN(n,x)	if (uhcidebug>(n)) logprintf x
int uhcidebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define MS_TO_TICKS(ms) ((ms) * hz / 1000)

struct uhci_pipe {
	struct usbd_pipe pipe;
	uhci_intr_info_t *iinfo;
	int newtoggle;
	/* Info needed for different pipe kinds. */
	union {
		/* Control pipe */
		struct {
			uhci_soft_qh_t *sqh;
			usb_dma_t reqdma;
			usb_dma_t datadma;
			uhci_soft_td_t *setup, *stat;
			u_int length;
		} ctl;
		/* Interrupt pipe */
		struct {
			usb_dma_t datadma;
			int npoll;
			uhci_soft_qh_t **qhs;
		} intr;
		/* Bulk pipe */
		struct {
			uhci_soft_qh_t *sqh;
			usb_dma_t datadma;
			u_int length;
			int isread;
		} bulk;
		/* Iso pipe */
		struct iso {
			u_int bufsize;
			u_int nbuf;
			usb_dma_t *bufs;
			uhci_soft_td_t **stds;
		} iso;
	} u;
};

/* 
 * The uhci_intr_info free list can be global since they contain
 * no dma specific data.  The other free lists do.
 */
LIST_HEAD(, uhci_intr_info) uhci_ii_free;

void		uhci_busreset __P((uhci_softc_t *));
usbd_status	uhci_run __P((uhci_softc_t *, int run));
uhci_soft_td_t *uhci_alloc_std __P((uhci_softc_t *));
void		uhci_free_std __P((uhci_softc_t *, uhci_soft_td_t *));
uhci_soft_qh_t *uhci_alloc_sqh __P((uhci_softc_t *));
void		uhci_free_sqh __P((uhci_softc_t *, uhci_soft_qh_t *));
uhci_intr_info_t *uhci_alloc_intr_info __P((uhci_softc_t *));
void		uhci_free_intr_info __P((uhci_intr_info_t *ii));
#if 0
void		uhci_enter_ctl_q __P((uhci_softc_t *, uhci_soft_qh_t *,
				      uhci_intr_info_t *));
void		uhci_exit_ctl_q __P((uhci_softc_t *, uhci_soft_qh_t *));
#endif

void		uhci_free_std_chain __P((uhci_softc_t *, 
					 uhci_soft_td_t *, uhci_soft_td_t *));
usbd_status	uhci_alloc_std_chain __P((struct uhci_pipe *, uhci_softc_t *,
					  int, int, int, usb_dma_t *, 
					  uhci_soft_td_t **,
					  uhci_soft_td_t **));
void		uhci_timo __P((void *));
void		uhci_waitintr __P((uhci_softc_t *, usbd_request_handle));
void		uhci_check_intr __P((uhci_softc_t *, uhci_intr_info_t *));
void		uhci_ii_done __P((uhci_intr_info_t *, int));
void		uhci_timeout __P((void *));
void		uhci_wakeup_ctrl __P((void *, int, int, void *, int));
void		uhci_lock_frames __P((uhci_softc_t *));
void		uhci_unlock_frames __P((uhci_softc_t *));
void		uhci_add_ctrl __P((uhci_softc_t *, uhci_soft_qh_t *));
void		uhci_add_bulk __P((uhci_softc_t *, uhci_soft_qh_t *));
void		uhci_remove_ctrl __P((uhci_softc_t *, uhci_soft_qh_t *));
void		uhci_remove_bulk __P((uhci_softc_t *, uhci_soft_qh_t *));
int		uhci_str __P((usb_string_descriptor_t *, int, char *));

void		uhci_wakeup_cb __P((usbd_request_handle reqh));

usbd_status	uhci_device_ctrl_transfer __P((usbd_request_handle));
usbd_status	uhci_device_ctrl_start __P((usbd_request_handle));
void		uhci_device_ctrl_abort __P((usbd_request_handle));
void		uhci_device_ctrl_close __P((usbd_pipe_handle));
usbd_status	uhci_device_intr_transfer __P((usbd_request_handle));
usbd_status	uhci_device_intr_start __P((usbd_request_handle));
void		uhci_device_intr_abort __P((usbd_request_handle));
void		uhci_device_intr_close __P((usbd_pipe_handle));
usbd_status	uhci_device_bulk_transfer __P((usbd_request_handle));
usbd_status	uhci_device_bulk_start __P((usbd_request_handle));
void		uhci_device_bulk_abort __P((usbd_request_handle));
void		uhci_device_bulk_close __P((usbd_pipe_handle));
usbd_status	uhci_device_isoc_transfer __P((usbd_request_handle));
usbd_status	uhci_device_isoc_start __P((usbd_request_handle));
void		uhci_device_isoc_abort __P((usbd_request_handle));
void		uhci_device_isoc_close __P((usbd_pipe_handle));
usbd_status	uhci_device_isoc_setbuf __P((usbd_pipe_handle, u_int, u_int));

usbd_status	uhci_root_ctrl_transfer __P((usbd_request_handle));
usbd_status	uhci_root_ctrl_start __P((usbd_request_handle));
void		uhci_root_ctrl_abort __P((usbd_request_handle));
void		uhci_root_ctrl_close __P((usbd_pipe_handle));
usbd_status	uhci_root_intr_transfer __P((usbd_request_handle));
usbd_status	uhci_root_intr_start __P((usbd_request_handle));
void		uhci_root_intr_abort __P((usbd_request_handle));
void		uhci_root_intr_close __P((usbd_pipe_handle));

usbd_status	uhci_open __P((usbd_pipe_handle));
void		uhci_poll __P((struct usbd_bus *));

usbd_status	uhci_device_request __P((usbd_request_handle reqh));
void		uhci_ctrl_done __P((uhci_intr_info_t *ii));
void		uhci_bulk_done __P((uhci_intr_info_t *ii));

void		uhci_add_intr __P((uhci_softc_t *, int, uhci_soft_qh_t *));
void		uhci_remove_intr __P((uhci_softc_t *, int, uhci_soft_qh_t *));
usbd_status	uhci_device_setintr __P((uhci_softc_t *sc, 
					 struct uhci_pipe *pipe, int ival));
void		uhci_intr_done __P((uhci_intr_info_t *ii));
void		uhci_isoc_done __P((uhci_intr_info_t *ii));

#ifdef UHCI_DEBUG
static void	uhci_dumpregs __P((uhci_softc_t *));
void		uhci_dump_tds __P((uhci_soft_td_t *));
void		uhci_dump_qh __P((uhci_soft_qh_t *));
void		uhci_dump __P((void));
void		uhci_dump_td __P((uhci_soft_td_t *));
#endif

#if defined(__NetBSD__)
#define UWRITE2(sc, r, x) bus_space_write_2((sc)->iot, (sc)->ioh, (r), (x))
#define UWRITE4(sc, r, x) bus_space_write_4((sc)->iot, (sc)->ioh, (r), (x))
#define UREAD2(sc, r) bus_space_read_2((sc)->iot, (sc)->ioh, (r))
#define UREAD4(sc, r) bus_space_read_4((sc)->iot, (sc)->ioh, (r))
#elif defined(__FreeBSD__)
#define UWRITE2(sc, r, x) bus_space_write_2((sc)->iot, (sc)->ioh, (r), (x))
#define UWRITE4(sc, r, x) bus_space_write_4((sc)->iot, (sc)->ioh, (r), (x))
#define UREAD1(sc, r) bus_space_read_1((sc)->iot, (sc)->ioh, (r))
#define UREAD2(sc, r) bus_space_read_2((sc)->iot, (sc)->ioh, (r))
#define UREAD4(sc, r) bus_space_read_4((sc)->iot, (sc)->ioh, (r))
#endif

#define UHCICMD(sc, cmd) UWRITE2(sc, UHCI_CMD, cmd)
#define UHCISTS(sc) UREAD2(sc, UHCI_STS)

#define UHCI_RESET_TIMEOUT 100	/* reset timeout */

#define UHCI_CURFRAME(sc) (UREAD2(sc, UHCI_FRNUM) & UHCI_FRNUM_MASK)

#define UHCI_INTR_ENDPT 1

struct usbd_methods uhci_root_ctrl_methods = {	
	uhci_root_ctrl_transfer,
	uhci_root_ctrl_start,
	uhci_root_ctrl_abort,
	uhci_root_ctrl_close,
	0,
};

struct usbd_methods uhci_root_intr_methods = {	
	uhci_root_intr_transfer,
	uhci_root_intr_start,
	uhci_root_intr_abort,
	uhci_root_intr_close,
	0,
};

struct usbd_methods uhci_device_ctrl_methods = {
	uhci_device_ctrl_transfer,
	uhci_device_ctrl_start,
	uhci_device_ctrl_abort,
	uhci_device_ctrl_close,
	0,
};

struct usbd_methods uhci_device_intr_methods = {
	uhci_device_intr_transfer,
	uhci_device_intr_start,
	uhci_device_intr_abort,
	uhci_device_intr_close,
	0,
};

struct usbd_methods uhci_device_bulk_methods = {
	uhci_device_bulk_transfer,
	uhci_device_bulk_start,
	uhci_device_bulk_abort,
	uhci_device_bulk_close,
	0,
};

struct usbd_methods uhci_device_isoc_methods = {
	uhci_device_isoc_transfer,
	uhci_device_isoc_start,
	uhci_device_isoc_abort,
	uhci_device_isoc_close,
	uhci_device_isoc_setbuf,
};

void
uhci_busreset(sc)
	uhci_softc_t *sc;
{
	UHCICMD(sc, UHCI_CMD_GRESET);	/* global reset */
	usb_delay_ms(&sc->sc_bus, USB_BUS_RESET_DELAY); /* wait a little */
	UHCICMD(sc, 0);			/* do nothing */
}

usbd_status
uhci_init(sc)
	uhci_softc_t *sc;
{
	usbd_status r;
	int i, j;
	uhci_soft_qh_t *csqh, *bsqh, *sqh;
	uhci_soft_td_t *std;
	usb_dma_t dma;
	static int uhci_global_init_done = 0;

	DPRINTFN(1,("uhci_init: start\n"));

	if (!uhci_global_init_done) {
		uhci_global_init_done = 1;
		LIST_INIT(&uhci_ii_free);
	}

	uhci_run(sc, 0);			/* stop the controller */
#if defined(__NetBSD__)
	UWRITE2(sc, UHCI_INTR, 0);		/* disable interrupts */
#endif

	uhci_busreset(sc);
	
	/* Allocate and initialize real frame array. */
	r = usb_allocmem(sc->sc_dmatag, 
			 UHCI_FRAMELIST_COUNT * sizeof(uhci_physaddr_t),
			 UHCI_FRAMELIST_ALIGN, &dma);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);

	sc->sc_pframes = KERNADDR(&dma);
	UWRITE2(sc, UHCI_FRNUM, 0);		/* set frame number to 0 */
	UWRITE4(sc, UHCI_FLBASEADDR, DMAADDR(&dma)); /* set frame list */

	/* Allocate the dummy QH where bulk traffic will be queued. */
	bsqh = uhci_alloc_sqh(sc);
	if (!bsqh)
		return (USBD_NOMEM);
	bsqh->qh->qh_hlink = UHCI_PTR_T;	/* end of QH chain */
	bsqh->qh->qh_elink = UHCI_PTR_T;
	sc->sc_bulk_start = sc->sc_bulk_end = bsqh;

	/* Allocate the dummy QH where control traffic will be queued. */
	csqh = uhci_alloc_sqh(sc);
	if (!csqh)
		return (USBD_NOMEM);
	csqh->qh->hlink = bsqh;
	csqh->qh->qh_hlink = bsqh->physaddr | UHCI_PTR_Q;
	csqh->qh->qh_elink = UHCI_PTR_T;
	sc->sc_ctl_start = sc->sc_ctl_end = csqh;

	/* 
	 * Make all (virtual) frame list pointers point to the interrupt
	 * queue heads and the interrupt queue heads at the control
	 * queue head and point the physical frame list to the virtual.
	 */
	for(i = 0; i < UHCI_VFRAMELIST_COUNT; i++) {
		std = uhci_alloc_std(sc);
		sqh = uhci_alloc_sqh(sc);
		if (!std || !sqh)
			return (USBD_NOMEM);
		std->td->link.sqh = sqh;
		std->td->td_link = sqh->physaddr | UHCI_PTR_Q;
		std->td->td_status = UHCI_TD_IOS;	/* iso, inactive */
		std->td->td_token = 0;
		std->td->td_buffer = 0;
		sqh->qh->hlink = csqh;
		sqh->qh->qh_hlink = csqh->physaddr | UHCI_PTR_Q;
		sqh->qh->elink = 0;
		sqh->qh->qh_elink = UHCI_PTR_T;
		sc->sc_vframes[i].htd = std;
		sc->sc_vframes[i].etd = std;
		sc->sc_vframes[i].hqh = sqh;
		sc->sc_vframes[i].eqh = sqh;
		for (j = i; 
		     j < UHCI_FRAMELIST_COUNT; 
		     j += UHCI_VFRAMELIST_COUNT)
			sc->sc_pframes[j] = std->physaddr;
	}

	LIST_INIT(&sc->sc_intrhead);

	/* Set up the bus struct. */
	sc->sc_bus.open_pipe = uhci_open;
	sc->sc_bus.pipe_size = sizeof(struct uhci_pipe);
	sc->sc_bus.do_poll = uhci_poll;

	DPRINTFN(1,("uhci_init: enabling\n"));
	UWRITE2(sc, UHCI_INTR, UHCI_INTR_TOCRCIE | UHCI_INTR_RIE | 
		UHCI_INTR_IOCE | UHCI_INTR_SPIE);	/* enable interrupts */

	return (uhci_run(sc, 1));		/* and here we go... */
}

#ifdef UHCI_DEBUG
static void
uhci_dumpregs(sc)
	uhci_softc_t *sc;
{
	printf("%s: regs: cmd=%04x, sts=%04x, intr=%04x, frnum=%04x, "
	       "flbase=%08x, sof=%02x, portsc1=%04x, portsc2=%04x, ",
	       USBDEVNAME(sc->sc_bus.bdev),
	       UREAD2(sc, UHCI_CMD),
	       UREAD2(sc, UHCI_STS),
	       UREAD2(sc, UHCI_INTR),
	       UREAD2(sc, UHCI_FRNUM),
	       UREAD4(sc, UHCI_FLBASEADDR),
	       UREAD1(sc, UHCI_SOF),
	       UREAD2(sc, UHCI_PORTSC1),
	       UREAD2(sc, UHCI_PORTSC2));
}

int uhci_longtd = 1;

void
uhci_dump_td(p)
	uhci_soft_td_t *p;
{
	printf("TD(%p) at %08lx link=0x%08lx st=0x%08lx tok=0x%08lx buf=0x%08lx\n",
	       p, (long)p->physaddr,
	       (long)p->td->td_link,
	       (long)p->td->td_status,
	       (long)p->td->td_token,
	       (long)p->td->td_buffer);
	if (uhci_longtd)
		printf("  %b %b,errcnt=%d,actlen=%d pid=%02x,addr=%d,endpt=%d,"
		       "D=%d,maxlen=%d\n",
		       (int)p->td->td_link,
		       "\20\1T\2Q\3VF",
		       (int)p->td->td_status,
		       "\20\22BITSTUFF\23CRCTO\24NAK\25BABBLE\26DBUFFER\27"
		       "STALLED\30ACTIVE\31IOC\32ISO\33LS\36SPD",
		       UHCI_TD_GET_ERRCNT(p->td->td_status),
		       UHCI_TD_GET_ACTLEN(p->td->td_status),
		       UHCI_TD_GET_PID(p->td->td_token),
		       UHCI_TD_GET_DEVADDR(p->td->td_token),
		       UHCI_TD_GET_ENDPT(p->td->td_token),
		       UHCI_TD_GET_DT(p->td->td_token),
		       UHCI_TD_GET_MAXLEN(p->td->td_token));
}

void
uhci_dump_qh(p)
	uhci_soft_qh_t *p;
{
	printf("QH(%p) at %08x: hlink=%08x elink=%08x\n", p, (int)p->physaddr,
	       p->qh->qh_hlink, p->qh->qh_elink);
}


#if 0
void
uhci_dump()
{
	uhci_softc_t *sc = uhci;

	uhci_dumpregs(sc);
	printf("intrs=%d\n", sc->sc_intrs);
	printf("framelist[i].link = %08x\n", sc->sc_framelist[0].link);
	uhci_dump_qh(sc->sc_ctl_start->qh->hlink);
}
#endif

void
uhci_dump_tds(std)
	uhci_soft_td_t *std;
{
	uhci_soft_td_t *p;

	for(p = std; p; p = p->td->link.std)
		uhci_dump_td(p);
}
#endif

/*
 * This routine is executed periodically and simulates interrupts
 * from the root controller interrupt pipe for port status change.
 */
void
uhci_timo(addr)
	void *addr;
{
	usbd_request_handle reqh = addr;
	usbd_pipe_handle pipe = reqh->pipe;
	uhci_softc_t *sc = (uhci_softc_t *)pipe->device->bus;
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;
	int s;
	u_char *p;

	DPRINTFN(15, ("uhci_timo\n"));

	p = KERNADDR(&upipe->u.intr.datadma);
	p[0] = 0;
	if (UREAD2(sc, UHCI_PORTSC1) & (UHCI_PORTSC_CSC|UHCI_PORTSC_OCIC))
		p[0] |= 1<<1;
	if (UREAD2(sc, UHCI_PORTSC2) & (UHCI_PORTSC_CSC|UHCI_PORTSC_OCIC))
		p[0] |= 1<<2;
	s = splusb();
	if (p[0] != 0) {
		reqh->actlen = 1;
		reqh->status = USBD_NORMAL_COMPLETION;
		reqh->xfercb(reqh);
	}
	if (reqh->pipe->intrreqh == reqh) {
		usb_timeout(uhci_timo, reqh, sc->sc_ival, reqh->timo_handle);
	} else {
		usb_freemem(sc->sc_dmatag, &upipe->u.intr.datadma);
		usb_start_next(reqh->pipe);
	}
	splx(s);
}


void
uhci_lock_frames(sc)
	uhci_softc_t *sc;
{
	int s = splusb();
	while (sc->sc_vflock) {
		sc->sc_vflock |= UHCI_WANT_LOCK;
		tsleep(&sc->sc_vflock, PRIBIO, "uhcqhl", 0);
	}
	sc->sc_vflock = UHCI_HAS_LOCK;
	splx(s);
}

void
uhci_unlock_frames(sc)
	uhci_softc_t *sc;
{
	int s = splusb();
	sc->sc_vflock &= ~UHCI_HAS_LOCK;
	if (sc->sc_vflock & UHCI_WANT_LOCK)
		wakeup(&sc->sc_vflock);
	splx(s);
}

/*
 * Allocate an interrupt information struct.  A free list is kept
 * for fast allocation.
 */
uhci_intr_info_t *
uhci_alloc_intr_info(sc)
	uhci_softc_t *sc;
{
	uhci_intr_info_t *ii;

	ii = LIST_FIRST(&uhci_ii_free);
	if (ii)
		LIST_REMOVE(ii, list);
	else {
		ii = malloc(sizeof(uhci_intr_info_t), M_USBDEV, M_NOWAIT);
	}
	ii->sc = sc;
#if defined(__FreeBSD__)
	callout_handle_init(&ii->timeout_handle);
#endif
	return ii;
}

void
uhci_free_intr_info(ii)
	uhci_intr_info_t *ii;
{
	LIST_INSERT_HEAD(&uhci_ii_free, ii, list); /* and put on free list */
}

/* Add control QH, called at splusb(). */
void
uhci_add_ctrl(sc, sqh)
	uhci_softc_t *sc;
	uhci_soft_qh_t *sqh;
{
	uhci_qh_t *eqh;

	DPRINTFN(10, ("uhci_add_ctrl: sqh=%p\n", sqh));
	eqh = sc->sc_ctl_end->qh;
	sqh->qh->hlink     = eqh->hlink;
	sqh->qh->qh_hlink  = eqh->qh_hlink;
	eqh->hlink         = sqh;
	eqh->qh_hlink      = sqh->physaddr | UHCI_PTR_Q;
	sc->sc_ctl_end = sqh;
}

/* Remove control QH, called at splusb(). */
void
uhci_remove_ctrl(sc, sqh)
	uhci_softc_t *sc;
	uhci_soft_qh_t *sqh;
{
	uhci_soft_qh_t *pqh;

	DPRINTFN(10, ("uhci_remove_ctrl: sqh=%p\n", sqh));
	for (pqh = sc->sc_ctl_start; pqh->qh->hlink != sqh; pqh=pqh->qh->hlink)
#if defined(DIAGNOSTIC) || defined(UHCI_DEBUG)		
		if (pqh->qh->qh_hlink & UHCI_PTR_T) {
			printf("uhci_remove_ctrl: QH not found\n");
			return;
		}
#else
		;
#endif
	pqh->qh->hlink    = sqh->qh->hlink;
	pqh->qh->qh_hlink = sqh->qh->qh_hlink;
	if (sc->sc_ctl_end == sqh)
		sc->sc_ctl_end = pqh;
}

/* Add bulk QH, called at splusb(). */
void
uhci_add_bulk(sc, sqh)
	uhci_softc_t *sc;
	uhci_soft_qh_t *sqh;
{
	uhci_qh_t *eqh;

	DPRINTFN(10, ("uhci_add_bulk: sqh=%p\n", sqh));
	eqh = sc->sc_bulk_end->qh;
	sqh->qh->hlink     = eqh->hlink;
	sqh->qh->qh_hlink  = eqh->qh_hlink;
	eqh->hlink         = sqh;
	eqh->qh_hlink      = sqh->physaddr | UHCI_PTR_Q;
	sc->sc_bulk_end = sqh;
}

/* Remove bulk QH, called at splusb(). */
void
uhci_remove_bulk(sc, sqh)
	uhci_softc_t *sc;
	uhci_soft_qh_t *sqh;
{
	uhci_soft_qh_t *pqh;

	DPRINTFN(10, ("uhci_remove_bulk: sqh=%p\n", sqh));
	for (pqh = sc->sc_bulk_start; 
	     pqh->qh->hlink != sqh; 
	     pqh = pqh->qh->hlink)
#if defined(DIAGNOSTIC) || defined(UHCI_DEBUG)		
		if (pqh->qh->qh_hlink & UHCI_PTR_T) {
			printf("uhci_remove_bulk: QH not found\n");
			return;
		}
#else
		;
#endif
	pqh->qh->hlink    = sqh->qh->hlink;
	pqh->qh->qh_hlink = sqh->qh->qh_hlink;
	if (sc->sc_bulk_end == sqh)
		sc->sc_bulk_end = pqh;
}

int
uhci_intr(priv)
	void *priv;
{
	uhci_softc_t *sc = priv;
	int status;
	int ack = 0;
	uhci_intr_info_t *ii;

	sc->sc_intrs++;

#if defined(UHCI_DEBUG)
	if (uhcidebug > 9) {
		printf("%s: uhci_intr\n", USBDEVNAME(sc->sc_bus.bdev));
		uhci_dumpregs(sc);
	}
#endif

	status = UREAD2(sc, UHCI_STS);

	if (status & UHCI_STS_USBINT)
		ack |= UHCI_STS_USBINT;
	if (status & UHCI_STS_USBEI)
		ack |= UHCI_STS_USBEI;
	if (status & UHCI_STS_RD) {
		ack |= UHCI_STS_RD;
		printf("%s: resume detect\n", USBDEVNAME(sc->sc_bus.bdev));
	}
	if (status & UHCI_STS_HSE) {
		ack |= UHCI_STS_HSE;
		printf("%s: Host Controller Process Error\n", USBDEVNAME(sc->sc_bus.bdev));
	}
	if (status & UHCI_STS_HCPE) {
		ack |= UHCI_STS_HCPE;
		printf("%s: Host System Error\n", USBDEVNAME(sc->sc_bus.bdev));
	}
	if (status & UHCI_STS_HCH) {
		/* no acknowledge needed */
		printf("%s: controller halted\n", USBDEVNAME(sc->sc_bus.bdev));
	}

	if (ack)	/* acknowledge the ints */
		UWRITE2(sc, UHCI_STS, ack);
	else	/* nothing to acknowledge */
		return 0;

	/*
	 * Interrupts on UHCI really suck.  When the host controller
	 * interrupts because a transfer is completed there is no
	 * way of knowing which transfer it was.  You can scan down
	 * the TDs and QHs of the previous frame to limit the search,
	 * but that assumes that the interrupt was not delayed by more
	 * than 1 ms, which may not always be true (e.g. after debug
	 * output on a slow console).
	 * We scan all interrupt descriptors to see if any have
	 * completed.
	 */
	for (ii = LIST_FIRST(&sc->sc_intrhead); ii; ii = LIST_NEXT(ii, list))
		uhci_check_intr(sc, ii);

	DPRINTFN(10, ("uhci_intr: exit\n"));
	return 1;
}

/* Check for an interrupt. */
void
uhci_check_intr(sc, ii)
	uhci_softc_t *sc;
	uhci_intr_info_t *ii;
{
	struct uhci_pipe *upipe;
	uhci_soft_td_t *std;
	u_int32_t status;

	DPRINTFN(15, ("uhci_check_intr: ii=%p\n", ii));
#ifdef DIAGNOSTIC
	if (!ii) {
		printf("uhci_check_intr: no ii? %p\n", ii);
		return;
	}
	if (!ii->stdend) {
		printf("uhci_check_intr: ii->stdend==0\n");
		return;
	}
#endif

	if (!ii->stdstart)
		return;

	/* If the last TD is still active we need to check whether there
	 * is a an error somewhere in the middle, or whether there was a
	 * short packet (SPD and not ACTIVE).
	 */
	if (ii->stdend->td->td_status & UHCI_TD_ACTIVE) {
		for (std = ii->stdstart; std != ii->stdend; std = std->td->link.std){
			status = std->td->td_status;
			if ((status & UHCI_TD_STALLED) ||
			     (status & (UHCI_TD_SPD | UHCI_TD_ACTIVE)) == UHCI_TD_SPD)
				goto done;
		}
		return;
	}
 done:
	usb_untimeout(uhci_timeout, ii, ii->timeout_handle);
	upipe = (struct uhci_pipe *)ii->reqh->pipe;
	upipe->pipe.endpoint->toggle = upipe->newtoggle;
	uhci_ii_done(ii, 0);
}

void
uhci_ii_done(ii, timo)
	uhci_intr_info_t *ii;
	int timo;		/* timeout that triggered function call? */
{
	usbd_request_handle reqh = ii->reqh;
	uhci_soft_td_t *std;
	int actlen = 0;		/* accumulated actual length for queue */
	int err = 0;		/* error status of last inactive transfer */

	DPRINTFN(10, ("uhci_ii_done: ii=%p ready %d\n", ii, timo));

#ifdef DIAGNOSTIC
	{
		/* avoid finishing a transfer more than once */
		int s = splhigh();
		if (ii->isdone) {
			splx(s);
			printf("uhci_ii_done: is done!\n");
			return;
		}
		ii->isdone = 1;
		splx(s);
	}
#endif

	/* The transfer is done; compute actual length and status */
	/* XXX Is this correct for control xfers? */
	for (std = ii->stdstart; std; std = std->td->link.std) {
		if (std->td->td_status & UHCI_TD_ACTIVE)
			break;

		/* error status of last TD for error handling below */
		err = std->td->td_status & UHCI_TD_ERROR;

		if (UHCI_TD_GET_PID(std->td->td_token) != UHCI_TD_PID_SETUP)
			actlen += UHCI_TD_GET_ACTLEN(std->td->td_status);
	}

	DPRINTFN(10, ("uhci_ii_done: actlen=%d, err=0x%x\n", actlen, err));

	if (err != 0) {
		DPRINTFN(-1+((err & ~UHCI_TD_STALLED) != 0),
			 ("uhci_ii_done: error, addr=%d, endpt=0x%02x, "
			  "err=0x%b\n",
			  reqh->pipe->device->address,
			  reqh->pipe->endpoint->edesc->bEndpointAddress,
			  (int)err, 
			  "\20\22BITSTUFF\23CRCTO\24NAK\25BABBLE\26DBUFFER\27"
			  "STALLED\30ACTIVE"));

		if (err & ~UHCI_TD_STALLED) {
			/* more then STALLED, like +BABBLE or +CRC/TIMEOUT */
			reqh->status = USBD_IOERROR; /* more info XXX */
		} else {
			reqh->status = USBD_STALLED;
		}
	} else {
		reqh->status = USBD_NORMAL_COMPLETION;
	}

	reqh->actlen = actlen;

	if (timo) {
		/* We got a timeout.  Make sure transaction is not active. */
		for (std = ii->stdstart; std != 0; std = std->td->link.std)
			std->td->td_status &= ~UHCI_TD_ACTIVE;
		/* XXX should we wait 1 ms */
		reqh->status = USBD_TIMEOUT;
	}
	DPRINTFN(5, ("uhci_ii_done: calling handler ii=%p\n", ii));

	/* select the proper type termination of the transfer
	 * based on the transfer type for the queue
	 */
	switch (reqh->pipe->endpoint->edesc->bmAttributes & UE_XFERTYPE) {
	case UE_CONTROL:
		uhci_ctrl_done(ii);
		usb_start_next(reqh->pipe);
		break;
	case UE_ISOCHRONOUS:
		uhci_isoc_done(ii);
		usb_start_next(reqh->pipe);
		break;
	case UE_BULK:
		uhci_bulk_done(ii);
		usb_start_next(reqh->pipe);
		break;
	case UE_INTERRUPT:
		uhci_intr_done(ii);
		break;
	}

	/* And finally execute callback. */
	reqh->xfercb(reqh);
}

/*
 * Called when a request does not complete.
 */
void
uhci_timeout(addr)
	void *addr;
{
	uhci_intr_info_t *ii = addr;
	int s;

	DPRINTF(("uhci_timeout: ii=%p\n", ii));
	s = splusb();
	uhci_ii_done(ii, 1);
	splx(s);
}

/*
 * Wait here until controller claims to have an interrupt.
 * Then call uhci_intr and return.  Use timeout to avoid waiting
 * too long.
 * Only used during boot when interrupts are not enabled yet.
 */
void
uhci_waitintr(sc, reqh)
	uhci_softc_t *sc;
	usbd_request_handle reqh;
{
	int timo = reqh->timeout;
	int usecs;
	uhci_intr_info_t *ii;

	DPRINTFN(15,("uhci_waitintr: timeout = %ds\n", timo));

	reqh->status = USBD_IN_PROGRESS;
	for (usecs = timo * 1000000 / hz; usecs > 0; usecs -= 1000) {
		usb_delay_ms(&sc->sc_bus, 1);
		DPRINTFN(10,("uhci_waitintr: 0x%04x\n", UREAD2(sc, UHCI_STS)));
		if (UREAD2(sc, UHCI_STS) & UHCI_STS_USBINT) {
			uhci_intr(sc);
			if (reqh->status != USBD_IN_PROGRESS)
				return;
		}
	}

	/* Timeout */
	DPRINTF(("uhci_waitintr: timeout\n"));
	for (ii = LIST_FIRST(&sc->sc_intrhead);
	     ii && ii->reqh != reqh; 
	     ii = LIST_NEXT(ii, list))
		;
	if (ii)
		uhci_ii_done(ii, 1);
	else
		panic("uhci_waitintr: lost intr_info\n");
}

void
uhci_poll(bus)
	struct usbd_bus *bus;
{
	uhci_softc_t *sc = (uhci_softc_t *)bus;

	if (UREAD2(sc, UHCI_STS) & UHCI_STS_USBINT)
		uhci_intr(sc);
}

#if 0
void
uhci_reset(p)
	void *p;
{
	uhci_softc_t *sc = p;
	int n;

	UHCICMD(sc, UHCI_CMD_HCRESET);
	/* The reset bit goes low when the controller is done. */
	for (n = 0; n < UHCI_RESET_TIMEOUT && 
		    (UREAD2(sc, UHCI_CMD) & UHCI_CMD_HCRESET); n++)
		delay(100);
	if (n >= UHCI_RESET_TIMEOUT)
		printf("%s: controller did not reset\n", 
		       USBDEVNAME(sc->sc_bus.bdev));
}
#endif

usbd_status
uhci_run(sc, run)
	uhci_softc_t *sc;
	int run;
{
	int s, n, running;

	s = splusb();
	running = ((UREAD2(sc, UHCI_STS) & UHCI_STS_HCH) == 0);
	if (run == running) {
		splx(s);
		return (USBD_NORMAL_COMPLETION);
	}
	UWRITE2(sc, UHCI_CMD, run ? UHCI_CMD_RS : 0);
	for(n = 0; n < 10; n++) {
		running = ((UREAD2(sc, UHCI_STS) & UHCI_STS_HCH) == 0);
		/* return when we've entered the state we want */
		if (run == running) {
			splx(s);
			return (USBD_NORMAL_COMPLETION);
		}
		usb_delay_ms(&sc->sc_bus, 1);
	}
	splx(s);
	printf("%s: cannot %s\n", USBDEVNAME(sc->sc_bus.bdev),
	       run ? "start" : "stop");
	return (USBD_IOERROR);
}

/*
 * Memory management routines.
 *  uhci_alloc_std allocates TDs
 *  uhci_alloc_sqh allocates QHs
 * These two routines do their own free list management,
 * partly for speed, partly because allocating DMAable memory
 * has page size granularaity so much memory would be wasted if
 * only one TD/QH (32 bytes) was placed in each allocated chunk.
 */

uhci_soft_td_t *
uhci_alloc_std(sc)
	uhci_softc_t *sc;
{
	uhci_soft_td_t *std;
	usbd_status r;
	int i;
	usb_dma_t dma;

	if (!sc->sc_freetds) {
		DPRINTFN(2,("uhci_alloc_std: allocating chunk\n"));
		std = malloc(sizeof(uhci_soft_td_t) * UHCI_TD_CHUNK, 
			     M_USBDEV, M_NOWAIT);
		if (!std)
			return (0);
		r = usb_allocmem(sc->sc_dmatag, UHCI_TD_SIZE * UHCI_TD_CHUNK,
				 UHCI_TD_ALIGN, &dma);
		if (r != USBD_NORMAL_COMPLETION) {
			free(std, M_USBDEV);
			return (0);
		}
		for(i = 0; i < UHCI_TD_CHUNK; i++, std++) {
			std->physaddr = DMAADDR(&dma) + i * UHCI_TD_SIZE;
			std->td = (uhci_td_t *)
				((char *)KERNADDR(&dma) + i * UHCI_TD_SIZE);
			std->td->link.std = sc->sc_freetds;
			sc->sc_freetds = std;
		}
	}
	std = sc->sc_freetds;
	sc->sc_freetds = std->td->link.std;
	memset(std->td, 0, UHCI_TD_SIZE);
	return std;
}

void
uhci_free_std(sc, std)
	uhci_softc_t *sc;
	uhci_soft_td_t *std;
{
#ifdef DIAGNOSTIC
#define TD_IS_FREE 0x12345678
	if (std->td->td_token == TD_IS_FREE) {
		printf("uhci_free_std: freeing free TD %p\n", std);
		return;
	}
	std->td->td_token = TD_IS_FREE;
#endif
	std->td->link.std = sc->sc_freetds;
	sc->sc_freetds = std;
}

uhci_soft_qh_t *
uhci_alloc_sqh(sc)
	uhci_softc_t *sc;
{
	uhci_soft_qh_t *sqh;
	usbd_status r;
	int i, offs;
	usb_dma_t dma;

	if (!sc->sc_freeqhs) {
		DPRINTFN(2, ("uhci_alloc_sqh: allocating chunk\n"));
		sqh = malloc(sizeof(uhci_soft_qh_t) * UHCI_QH_CHUNK, 
			     M_USBDEV, M_NOWAIT);
		if (!sqh)
			return 0;
		r = usb_allocmem(sc->sc_dmatag, UHCI_QH_SIZE * UHCI_QH_CHUNK,
				 UHCI_QH_ALIGN, &dma);
		if (r != USBD_NORMAL_COMPLETION) {
			free(sqh, M_USBDEV);
			return 0;
		}
		for(i = 0; i < UHCI_QH_CHUNK; i++, sqh++) {
			offs = i * UHCI_QH_SIZE;
			sqh->physaddr = DMAADDR(&dma) + offs;
			sqh->qh = (uhci_qh_t *)
					((char *)KERNADDR(&dma) + offs);
			sqh->qh->hlink = sc->sc_freeqhs;
			sc->sc_freeqhs = sqh;
		}
	}
	sqh = sc->sc_freeqhs;
	sc->sc_freeqhs = sqh->qh->hlink;
	memset(sqh->qh, 0, UHCI_QH_SIZE);
	return (sqh);
}

void
uhci_free_sqh(sc, sqh)
	uhci_softc_t *sc;
	uhci_soft_qh_t *sqh;
{
	sqh->qh->hlink = sc->sc_freeqhs;
	sc->sc_freeqhs = sqh;
}

#if 0
/* 
 * Enter a list of transfers onto a control queue.
 * Called at splusb() 
 */
void
uhci_enter_ctl_q(sc, sqh, ii)
	uhci_softc_t *sc;
	uhci_soft_qh_t *sqh;
	uhci_intr_info_t *ii;
{
	DPRINTFN(5, ("uhci_enter_ctl_q: sqh=%p\n", sqh));

}
#endif

void
uhci_free_std_chain(sc, std, stdend)
	uhci_softc_t *sc;
	uhci_soft_td_t *std;
	uhci_soft_td_t *stdend;
{
	uhci_soft_td_t *p;

	for (; std != stdend; std = p) {
		p = std->td->link.std;
		uhci_free_std(sc, std);
	}
}

usbd_status
uhci_alloc_std_chain(upipe, sc, len, rd, spd, dma, sp, ep)
	struct uhci_pipe *upipe;
	uhci_softc_t *sc;
	int len, rd, spd;
	usb_dma_t *dma;
	uhci_soft_td_t **sp, **ep;
{
	uhci_soft_td_t *p, *lastp;
	uhci_physaddr_t lastlink;
	int i, ntd, l, tog, maxp;
	u_int32_t status;
	int addr = upipe->pipe.device->address;
	int endpt = upipe->pipe.endpoint->edesc->bEndpointAddress;

	DPRINTFN(15, ("uhci_alloc_std_chain: addr=%d endpt=%d len=%d ls=%d "
		      "spd=%d\n", addr, endpt, len, 
		      upipe->pipe.device->lowspeed, spd));
	if (len == 0) {
		*sp = *ep = 0;
		DPRINTFN(-1,("uhci_alloc_std_chain: len=0\n"));
		return (USBD_NORMAL_COMPLETION);
	}
	maxp = UGETW(upipe->pipe.endpoint->edesc->wMaxPacketSize);
	if (maxp == 0) {
		printf("uhci_alloc_std_chain: maxp=0\n");
		return (USBD_INVAL);
	}
	ntd = (len + maxp - 1) / maxp;
	tog = upipe->pipe.endpoint->toggle;
	if (ntd % 2 == 0)
		tog ^= 1;
	upipe->newtoggle = tog ^ 1;
	lastp = 0;
	lastlink = UHCI_PTR_T;
	ntd--;
	status = UHCI_TD_SET_ERRCNT(3) | UHCI_TD_ACTIVE;
	if (upipe->pipe.device->lowspeed)
		status |= UHCI_TD_LOWSPEED;
	if (spd)
		status |= UHCI_TD_SPD;
	for (i = ntd; i >= 0; i--) {
		p = uhci_alloc_std(sc);
		if (!p) {
			uhci_free_std_chain(sc, lastp, 0);
			return (USBD_NOMEM);
		}
		p->td->link.std = lastp;
		p->td->td_link = lastlink;
		lastp = p;
		lastlink = p->physaddr;
		p->td->td_status = status;
		if (i == ntd) {
			/* last TD */
			l = len % maxp;
			if (l == 0) l = maxp;
			*ep = p;
		} else
			l = maxp;
		p->td->td_token = 
		    rd ? UHCI_TD_IN (l, endpt, addr, tog) :
			 UHCI_TD_OUT(l, endpt, addr, tog);
		p->td->td_buffer = DMAADDR(dma) + i * maxp;
		tog ^= 1;
	}
	*sp = lastp;
	/*upipe->pipe.endpoint->toggle = tog;*/
	DPRINTFN(10, ("uhci_alloc_std_chain: oldtog=%d newtog=%d\n", 
		      upipe->pipe.endpoint->toggle, upipe->newtoggle));
	return (USBD_NORMAL_COMPLETION);
}

usbd_status
uhci_device_bulk_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status r;

	s = splusb();
	r = usb_insert_transfer(reqh);
	splx(s);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	else
		return (uhci_device_bulk_start(reqh));
}

usbd_status
uhci_device_bulk_start(reqh)
	usbd_request_handle reqh;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
	usbd_device_handle dev = upipe->pipe.device;
	uhci_softc_t *sc = (uhci_softc_t *)dev->bus;
	uhci_intr_info_t *ii = upipe->iinfo;
	uhci_soft_td_t *xfer, *xferend;
	uhci_soft_qh_t *sqh;
	usb_dma_t *dmap;
	usbd_status r;
	int len, isread;
	int s;

	DPRINTFN(3, ("uhci_device_bulk_transfer: reqh=%p buf=%p len=%d "
		     "flags=%d\n",
		     reqh, reqh->buffer, reqh->length, reqh->flags));

	if (reqh->isreq)
		panic("uhci_device_bulk_transfer: a request\n");

	len = reqh->length;
	dmap = &upipe->u.bulk.datadma;
	isread = reqh->pipe->endpoint->edesc->bEndpointAddress & UE_IN;
	sqh = upipe->u.bulk.sqh;

	upipe->u.bulk.isread = isread;
	upipe->u.bulk.length = len;

	r = usb_allocmem(sc->sc_dmatag, len, 0, dmap);
	if (r != USBD_NORMAL_COMPLETION)
		goto ret1;
	r = uhci_alloc_std_chain(upipe, sc, len, isread, 
				 reqh->flags & USBD_SHORT_XFER_OK,
				 dmap, &xfer, &xferend);
	if (r != USBD_NORMAL_COMPLETION)
		goto ret2;
	xferend->td->td_status |= UHCI_TD_IOC;

	if (!isread && len != 0)
		memcpy(KERNADDR(dmap), reqh->buffer, len);

#ifdef UHCI_DEBUG
	if (uhcidebug > 10) {
		printf("uhci_device_bulk_transfer: xfer(1)\n");
		uhci_dump_tds(xfer);
	}
#endif

	/* Set up interrupt info. */
	ii->reqh = reqh;
	ii->stdstart = xfer;
	ii->stdend = xferend;
#if defined(__FreeBSD__)
	callout_handle_init(&ii->timeout_handle);
#endif
#ifdef DIAGNOSTIC
	ii->isdone = 0;
#endif

	sqh->qh->elink = xfer;
	sqh->qh->qh_elink = xfer->physaddr;
	sqh->intr_info = ii;

	s = splusb();
	uhci_add_bulk(sc, sqh);
	LIST_INSERT_HEAD(&sc->sc_intrhead, ii, list);

	if (reqh->timeout && !sc->sc_bus.use_polling) {
		usb_timeout(uhci_timeout, ii,
                            MS_TO_TICKS(reqh->timeout), ii->timeout_handle);
	}
	splx(s);

#ifdef UHCI_DEBUG
	if (uhcidebug > 10) {
		printf("uhci_device_bulk_transfer: xfer(2)\n");
		uhci_dump_tds(xfer);
	}
#endif

	return (USBD_IN_PROGRESS);

 ret2:
	if (len != 0)
		usb_freemem(sc->sc_dmatag, dmap);
 ret1:
	return (r);
}

/* Abort a device bulk request. */
void
uhci_device_bulk_abort(reqh)
	usbd_request_handle reqh;
{
	/* XXX inactivate */
	usb_delay_ms(reqh->pipe->device->bus, 1);/* make sure it is done */
	/* XXX call done */
}

/* Close a device bulk pipe. */
void
uhci_device_bulk_close(pipe)
	usbd_pipe_handle pipe;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;
	usbd_device_handle dev = upipe->pipe.device;
	uhci_softc_t *sc = (uhci_softc_t *)dev->bus;

	uhci_free_sqh(sc, upipe->u.bulk.sqh);
	uhci_free_intr_info(upipe->iinfo);
	/* XXX free other resources */
}

usbd_status
uhci_device_ctrl_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status r;

	s = splusb();
	r = usb_insert_transfer(reqh);
	splx(s);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	else
		return (uhci_device_ctrl_start(reqh));
}

usbd_status
uhci_device_ctrl_start(reqh)
	usbd_request_handle reqh;
{
	uhci_softc_t *sc = (uhci_softc_t *)reqh->pipe->device->bus;
	usbd_status r;

	if (!reqh->isreq)
		panic("uhci_device_ctrl_transfer: not a request\n");

	r = uhci_device_request(reqh);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);

	if (sc->sc_bus.use_polling)
		uhci_waitintr(sc, reqh);
	return (USBD_IN_PROGRESS);
}

usbd_status
uhci_device_intr_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status r;

	s = splusb();
	r = usb_insert_transfer(reqh);
	splx(s);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	else
		return (uhci_device_intr_start(reqh));
}

usbd_status
uhci_device_intr_start(reqh)
	usbd_request_handle reqh;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
	usbd_device_handle dev = upipe->pipe.device;
	uhci_softc_t *sc = (uhci_softc_t *)dev->bus;
	uhci_intr_info_t *ii = upipe->iinfo;
	uhci_soft_td_t *xfer, *xferend;
	uhci_soft_qh_t *sqh;
	usb_dma_t *dmap;
	usbd_status r;
	int len, i;
	int s;

	DPRINTFN(3, ("uhci_device_intr_transfer: reqh=%p buf=%p len=%d "
		     "flags=%d\n",
		     reqh, reqh->buffer, reqh->length, reqh->flags));

	if (reqh->isreq)
		panic("uhci_device_intr_transfer: a request\n");

	len = reqh->length;
	dmap = &upipe->u.intr.datadma;
	if (len == 0)
		return (USBD_INVAL); /* XXX should it be? */

	r = usb_allocmem(sc->sc_dmatag, len, 0, dmap);
	if (r != USBD_NORMAL_COMPLETION)
		goto ret1;
	r = uhci_alloc_std_chain(upipe, sc, len, 1,
 				 reqh->flags & USBD_SHORT_XFER_OK,
				 dmap, &xfer, &xferend);
	if (r != USBD_NORMAL_COMPLETION)
		goto ret2;
	xferend->td->td_status |= UHCI_TD_IOC;

#ifdef UHCI_DEBUG
	if (uhcidebug > 10) {
		printf("uhci_device_intr_transfer: xfer(1)\n");
		uhci_dump_tds(xfer);
		uhci_dump_qh(upipe->u.intr.qhs[0]);
	}
#endif

	s = splusb();
	/* Set up interrupt info. */
	ii->reqh = reqh;
	ii->stdstart = xfer;
	ii->stdend = xferend;
#ifdef DIAGNOSTIC
	ii->isdone = 0;
#endif

	DPRINTFN(10,("uhci_device_intr_transfer: qhs[0]=%p\n", 
		     upipe->u.intr.qhs[0]));
	for (i = 0; i < upipe->u.intr.npoll; i++) {
		sqh = upipe->u.intr.qhs[i];
		sqh->qh->elink = xfer;
		sqh->qh->qh_elink = xfer->physaddr;
	}
	splx(s);

#ifdef UHCI_DEBUG
	if (uhcidebug > 10) {
		printf("uhci_device_intr_transfer: xfer(2)\n");
		uhci_dump_tds(xfer);
		uhci_dump_qh(upipe->u.intr.qhs[0]);
	}
#endif

	return (USBD_IN_PROGRESS);

 ret2:
	if (len != 0)
		usb_freemem(sc->sc_dmatag, dmap);
 ret1:
	return (r);
}

/* Abort a device control request. */
void
uhci_device_ctrl_abort(reqh)
	usbd_request_handle reqh;
{
	/* XXX inactivate */
	usb_delay_ms(reqh->pipe->device->bus, 1); /* make sure it is done */
	/* XXX call done */
}

/* Close a device control pipe. */
void
uhci_device_ctrl_close(pipe)
	usbd_pipe_handle pipe;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;

	uhci_free_intr_info(upipe->iinfo);
	/* XXX free other resources */
}

/* Abort a device interrupt request. */
void
uhci_device_intr_abort(reqh)
	usbd_request_handle reqh;
{
	DPRINTFN(1, ("uhci_device_intr_abort: reqh=%p\n", reqh));
	if (reqh->pipe->intrreqh == reqh) {
		DPRINTF(("uhci_device_intr_abort: remove\n"));
		reqh->pipe->intrreqh = 0;
		/* make sure it is done */
		usb_delay_ms(reqh->pipe->device->bus, 2);
	}
}

/* Close a device interrupt pipe. */
void
uhci_device_intr_close(pipe)
	usbd_pipe_handle pipe;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;
	uhci_softc_t *sc = (uhci_softc_t *)pipe->device->bus;
	int i, s, npoll;

	upipe->iinfo->stdstart = 0;		/* inactive */

	/* Unlink descriptors from controller data structures. */
	npoll = upipe->u.intr.npoll;
	uhci_lock_frames(sc);
	for (i = 0; i < npoll; i++)
		uhci_remove_intr(sc, upipe->u.intr.qhs[i]->pos, 
				 upipe->u.intr.qhs[i]);
	uhci_unlock_frames(sc);

	/* 
	 * We now have to wait for any activity on the physical
	 * descriptors to stop.
	 */
	usb_delay_ms(&sc->sc_bus, 2);

	for(i = 0; i < npoll; i++)
		uhci_free_sqh(sc, upipe->u.intr.qhs[i]);
	free(upipe->u.intr.qhs, M_USB);

	s = splusb();
	LIST_REMOVE(upipe->iinfo, list);	/* remove from active list */
	splx(s);
	uhci_free_intr_info(upipe->iinfo);

	/* XXX free other resources */
}

usbd_status
uhci_device_request(reqh)
	usbd_request_handle reqh;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
	usb_device_request_t *req = &reqh->request;
	usbd_device_handle dev = upipe->pipe.device;
	uhci_softc_t *sc = (uhci_softc_t *)dev->bus;
	int addr = dev->address;
	int endpt = upipe->pipe.endpoint->edesc->bEndpointAddress;
	uhci_intr_info_t *ii = upipe->iinfo;
	uhci_soft_td_t *setup, *xfer, *stat, *next, *xferend;
	uhci_soft_qh_t *sqh;
	usb_dma_t *dmap;
	int len;
	u_int32_t ls;
	usbd_status r;
	int isread;
	int s;

	DPRINTFN(3,("uhci_device_control type=0x%02x, request=0x%02x, "
		    "wValue=0x%04x, wIndex=0x%04x len=%d, addr=%d, endpt=%d\n",
		    req->bmRequestType, req->bRequest, UGETW(req->wValue),
		    UGETW(req->wIndex), UGETW(req->wLength),
		    addr, endpt));

	ls = dev->lowspeed ? UHCI_TD_LOWSPEED : 0;
	isread = req->bmRequestType & UT_READ;
	len = UGETW(req->wLength);

	setup = upipe->u.ctl.setup;
	stat = upipe->u.ctl.stat;
	sqh = upipe->u.ctl.sqh;
	dmap = &upipe->u.ctl.datadma;

	/* Set up data transaction */
	if (len != 0) {
		r = usb_allocmem(sc->sc_dmatag, len, 0, dmap);
		if (r != USBD_NORMAL_COMPLETION)
			goto ret1;
		upipe->pipe.endpoint->toggle = 1;
		r = uhci_alloc_std_chain(upipe, sc, len, isread, 
					 reqh->flags & USBD_SHORT_XFER_OK,
					 dmap, &xfer, &xferend);
		if (r != USBD_NORMAL_COMPLETION)
			goto ret2;
		next = xfer;
		xferend->td->link.std = stat;
		xferend->td->td_link = stat->physaddr;
	} else {
		next = stat;
	}
	upipe->u.ctl.length = len;

	memcpy(KERNADDR(&upipe->u.ctl.reqdma), req, sizeof *req);
	if (!isread && len != 0)
		memcpy(KERNADDR(dmap), reqh->buffer, len);

	setup->td->link.std = next;
	setup->td->td_link = next->physaddr;
	setup->td->td_status = UHCI_TD_SET_ERRCNT(3) | ls | UHCI_TD_ACTIVE;
	setup->td->td_token = UHCI_TD_SETUP(sizeof *req, endpt, addr);
	setup->td->td_buffer = DMAADDR(&upipe->u.ctl.reqdma);

	stat->td->link.std = 0;
	stat->td->td_link = UHCI_PTR_T;
	stat->td->td_status = UHCI_TD_SET_ERRCNT(3) | ls | 
		UHCI_TD_ACTIVE | UHCI_TD_IOC;
	stat->td->td_token = 
		isread ? UHCI_TD_OUT(0, endpt, addr, 1) :
		         UHCI_TD_IN (0, endpt, addr, 1);
	stat->td->td_buffer = 0;

#ifdef UHCI_DEBUG
	if (uhcidebug > 20) {
		printf("uhci_device_request: setup\n");
		uhci_dump_td(setup);
		printf("uhci_device_request: stat\n");
		uhci_dump_td(stat);
	}
#endif

	/* Set up interrupt info. */
	ii->reqh = reqh;
	ii->stdstart = setup;
	ii->stdend = stat;
#if defined(__FreeBSD__)
	callout_handle_init(&ii->timeout_handle);
#endif
#ifdef DIAGNOSTIC
	ii->isdone = 0;
#endif

	sqh->qh->elink = setup;
	sqh->qh->qh_elink = setup->physaddr;
	sqh->intr_info = ii;

	s = splusb();
	uhci_add_ctrl(sc, sqh);
	LIST_INSERT_HEAD(&sc->sc_intrhead, ii, list);
#ifdef UHCI_DEBUG
	if (uhcidebug > 12) {
		uhci_soft_td_t *std;
		uhci_soft_qh_t *xqh;
		uhci_soft_qh_t *sxqh;
		int maxqh = 0;
		uhci_physaddr_t link;
		printf("uhci_enter_ctl_q: follow from [0]\n");
		for (std = sc->sc_vframes[0].htd, link = 0;
		     (link & UHCI_PTR_Q) == 0;
		     std = std->td->link.std) {
			link = std->td->td_link;
			uhci_dump_td(std);
		}
		for (sxqh = xqh = (uhci_soft_qh_t *)std;
		     xqh;
		     xqh = (maxqh++ == 5 || xqh->qh->hlink==sxqh || 
                            xqh->qh->hlink==xqh ? NULL : xqh->qh->hlink)) {
			uhci_dump_qh(xqh);
			uhci_dump_qh(sxqh);
		}
		printf("Enqueued QH:\n");
		uhci_dump_qh(sqh);
		uhci_dump_tds(sqh->qh->elink);
	}
#endif
	if (reqh->timeout && !sc->sc_bus.use_polling) {
		usb_timeout(uhci_timeout, ii,
                            MS_TO_TICKS(reqh->timeout), ii->timeout_handle);
	}
	splx(s);

	return (USBD_NORMAL_COMPLETION);

 ret2:
	if (len != 0)
		usb_freemem(sc->sc_dmatag, dmap);
 ret1:
	return (r);
}

usbd_status
uhci_device_isoc_transfer(reqh)
	usbd_request_handle reqh;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
#ifdef UHCI_DEBUG
	usbd_device_handle dev = upipe->pipe.device;
	uhci_softc_t *sc = (uhci_softc_t *)dev->bus;
#endif

	DPRINTFN(1,("uhci_device_isoc_transfer: sc=%p\n", sc));
	if (upipe->u.iso.bufsize == 0)
		return (USBD_INVAL);

	/* XXX copy data */
	return (USBD_XXX);
}

usbd_status
uhci_device_isoc_start(reqh)
	usbd_request_handle reqh;
{
	return (USBD_XXX);
}

void
uhci_device_isoc_abort(reqh)
	usbd_request_handle reqh;
{
	/* XXX Can't abort a single request. */
}

void
uhci_device_isoc_close(pipe)
	usbd_pipe_handle pipe;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;
	usbd_device_handle dev = upipe->pipe.device;
	uhci_softc_t *sc = (uhci_softc_t *)dev->bus;
	struct iso *iso;
	int i;

	/*
	 * Make sure all TDs are marked as inactive.
	 * Wait for completion.
	 * Unschedule.
	 * Deallocate.
	 */
	iso = &upipe->u.iso;

	for (i = 0; i < UHCI_VFRAMELIST_COUNT; i++)
		iso->stds[i]->td->td_status &= ~UHCI_TD_ACTIVE;
	usb_delay_ms(&sc->sc_bus, 2); /* wait for completion */

	uhci_lock_frames(sc);
	for (i = 0; i < UHCI_VFRAMELIST_COUNT; i++) {
		uhci_soft_td_t *std, *vstd;

		std = iso->stds[i];
		for (vstd = sc->sc_vframes[i % UHCI_VFRAMELIST_COUNT].htd;
		     vstd && vstd->td->link.std != std;
		     vstd = vstd->td->link.std)
			;
		if (!vstd) {
			/*panic*/
			printf("uhci_device_isoc_close: %p not found\n", std);
			uhci_unlock_frames(sc);
			return;
		}
		vstd->td->link = std->td->link;
		vstd->td->td_link = std->td->td_link;
		uhci_free_std(sc, std);
	}
	uhci_unlock_frames(sc);

	for (i = 0; i < iso->nbuf; i++)
		usb_freemem(sc->sc_dmatag, &iso->bufs[i]);
	free(iso->stds, M_USB);
	free(iso->bufs, M_USB);

	/* XXX what else? */
}

usbd_status
uhci_device_isoc_setbuf(pipe, bufsize, nbuf)
	usbd_pipe_handle pipe;
	u_int bufsize;
	u_int nbuf;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;
	usbd_device_handle dev = upipe->pipe.device;
	uhci_softc_t *sc = (uhci_softc_t *)dev->bus;
	int addr = upipe->pipe.device->address;
	int endpt = upipe->pipe.endpoint->edesc->bEndpointAddress;
	int rd = upipe->pipe.endpoint->edesc->bEndpointAddress & UE_IN;
	struct iso *iso;
	int i;
	usbd_status r;

	/* 
	 * For simplicity the number of buffers must fit nicely in the frame
	 * list.
	 */
	if (UHCI_VFRAMELIST_COUNT % nbuf != 0)
		return (USBD_INVAL);

	iso = &upipe->u.iso;
	iso->bufsize = bufsize;
	iso->nbuf = nbuf;

	/* Allocate memory for buffers. */
	iso->bufs = malloc(nbuf * sizeof(usb_dma_t), M_USB, M_WAITOK);
	iso->stds = malloc(UHCI_VFRAMELIST_COUNT * sizeof (uhci_soft_td_t *),
			   M_USB, M_WAITOK);

	for (i = 0; i < nbuf; i++) {
		r = usb_allocmem(sc->sc_dmatag, bufsize, 0, &iso->bufs[i]);
		if (r != USBD_NORMAL_COMPLETION) {
			nbuf = i;
			goto bad1;
		}
	}

	/* Allocate the TDs. */
	for (i = 0; i < UHCI_VFRAMELIST_COUNT; i++) {
		iso->stds[i] = uhci_alloc_std(sc);
		if (iso->stds[i] == 0)
			goto bad2;
	}

	/* XXX check schedule */

	/* XXX interrupts */

	/* Insert TDs into schedule, all marked inactive. */
	uhci_lock_frames(sc);
	for (i = 0; i < UHCI_VFRAMELIST_COUNT; i++) {
		uhci_soft_td_t *std, *vstd;

		std = iso->stds[i];
		std->td->td_status = UHCI_TD_IOS;	/* iso, inactive */
		std->td->td_token =
		    rd ? UHCI_TD_IN (0, endpt, addr, 0) :
			 UHCI_TD_OUT(0, endpt, addr, 0);
		std->td->td_buffer = DMAADDR(&iso->bufs[i % nbuf]);

		vstd = sc->sc_vframes[i % UHCI_VFRAMELIST_COUNT].htd;
		std->td->link = vstd->td->link;
		std->td->td_link = vstd->td->td_link;
		vstd->td->link.std = std;
		vstd->td->td_link = std->physaddr;
	}
	uhci_unlock_frames(sc);

	return (USBD_NORMAL_COMPLETION);

 bad2:
	while (--i >= 0)
		uhci_free_std(sc, iso->stds[i]);
 bad1:
	for (i = 0; i < nbuf; i++)
		usb_freemem(sc->sc_dmatag, &iso->bufs[i]);
	free(iso->stds, M_USB);
	free(iso->bufs, M_USB);
	return (USBD_NOMEM);
}

void
uhci_isoc_done(ii)
	uhci_intr_info_t *ii;
{
}

void
uhci_intr_done(ii)
	uhci_intr_info_t *ii;
{
	uhci_softc_t *sc = ii->sc;
	usbd_request_handle reqh = ii->reqh;
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
	usb_dma_t *dma;
	uhci_soft_qh_t *sqh;
	int i, npoll;

	DPRINTFN(5, ("uhci_intr_done: length=%d\n", reqh->actlen));

	dma = &upipe->u.intr.datadma;
	memcpy(reqh->buffer, KERNADDR(dma), reqh->actlen);
	npoll = upipe->u.intr.npoll;
	for(i = 0; i < npoll; i++) {
		sqh = upipe->u.intr.qhs[i];
		sqh->qh->elink = 0;
		sqh->qh->qh_elink = UHCI_PTR_T;
	}
	uhci_free_std_chain(sc, ii->stdstart, 0);

	/* XXX Wasteful. */
	if (reqh->pipe->intrreqh == reqh
	    && reqh->status == USBD_NORMAL_COMPLETION) {
		uhci_soft_td_t *xfer, *xferend;

		/* This alloc cannot fail since we freed the chain above. */
		uhci_alloc_std_chain(upipe, sc, reqh->length, 1,
				     reqh->flags & USBD_SHORT_XFER_OK,
				     dma, &xfer, &xferend);
		xferend->td->td_status |= UHCI_TD_IOC;

#ifdef UHCI_DEBUG
		if (uhcidebug > 10) {
			printf("uhci_device_intr_done: xfer(1)\n");
			uhci_dump_tds(xfer);
			uhci_dump_qh(upipe->u.intr.qhs[0]);
		}
#endif

		ii->stdstart = xfer;
		ii->stdend = xferend;
#ifdef DIAGNOSTIC
		ii->isdone = 0;
#endif
		for (i = 0; i < npoll; i++) {
			sqh = upipe->u.intr.qhs[i];
			sqh->qh->elink = xfer;
			sqh->qh->qh_elink = xfer->physaddr;
		}
	} else {
		usb_freemem(sc->sc_dmatag, dma);
		ii->stdstart = 0;	/* mark as inactive */
	}
}

/* Deallocate request data structures */
void
uhci_ctrl_done(ii)
	uhci_intr_info_t *ii;
{
	uhci_softc_t *sc = ii->sc;
	usbd_request_handle reqh = ii->reqh;
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
	u_int len = upipe->u.ctl.length;
	usb_dma_t *dma;
	uhci_td_t *htd = ii->stdstart->td;

#ifdef DIAGNOSTIC
	if (!reqh->isreq)
		panic("uhci_ctrl_done: not a request\n");
#endif

	LIST_REMOVE(ii, list);	/* remove from active list */

	uhci_remove_ctrl(sc, upipe->u.ctl.sqh);

	if (len != 0) {
		dma = &upipe->u.ctl.datadma;
		if (reqh->request.bmRequestType & UT_READ)
			memcpy(reqh->buffer, KERNADDR(dma), len);
		uhci_free_std_chain(sc, htd->link.std, ii->stdend);
		usb_freemem(sc->sc_dmatag, dma);
	}
	DPRINTFN(5, ("uhci_ctrl_done: length=%d\n", reqh->actlen));
}

/* Deallocate request data structures */
void
uhci_bulk_done(ii)
	uhci_intr_info_t *ii;
{
	uhci_softc_t *sc = ii->sc;
	usbd_request_handle reqh = ii->reqh;
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
	u_int len = upipe->u.bulk.length;
	usb_dma_t *dma;
	uhci_td_t *htd = ii->stdstart->td;

	LIST_REMOVE(ii, list);	/* remove from active list */

	uhci_remove_bulk(sc, upipe->u.bulk.sqh);

	if (len != 0) {
		dma = &upipe->u.bulk.datadma;
		if (upipe->u.bulk.isread && len != 0)
			memcpy(reqh->buffer, KERNADDR(dma), len);
		uhci_free_std_chain(sc, htd->link.std, 0);
		usb_freemem(sc->sc_dmatag, dma);
	}
	DPRINTFN(4, ("uhci_bulk_done: length=%d\n", reqh->actlen));
	/* XXX compute new toggle */
}

/* Add interrupt QH, called with vflock. */
void
uhci_add_intr(sc, n, sqh)
	uhci_softc_t *sc;
	int n;
	uhci_soft_qh_t *sqh;
{
	struct uhci_vframe *vf = &sc->sc_vframes[n];
	uhci_qh_t *eqh;

	DPRINTFN(4, ("uhci_add_intr: n=%d sqh=%p\n", n, sqh));
	eqh = vf->eqh->qh;
	sqh->qh->hlink     = eqh->hlink;
	sqh->qh->qh_hlink  = eqh->qh_hlink;
	eqh->hlink         = sqh;
	eqh->qh_hlink      = sqh->physaddr | UHCI_PTR_Q;
	vf->eqh = sqh;
	vf->bandwidth++;
}

/* Remove interrupt QH, called with vflock. */
void
uhci_remove_intr(sc, n, sqh)
	uhci_softc_t *sc;
	int n;
	uhci_soft_qh_t *sqh;
{
	struct uhci_vframe *vf = &sc->sc_vframes[n];
	uhci_soft_qh_t *pqh;

	DPRINTFN(4, ("uhci_remove_intr: n=%d sqh=%p\n", n, sqh));

	for (pqh = vf->hqh; pqh->qh->hlink != sqh; pqh = pqh->qh->hlink)
#if defined(DIAGNOSTIC) || defined(UHCI_DEBUG)		
		if (pqh->qh->qh_hlink & UHCI_PTR_T) {
			printf("uhci_remove_intr: QH not found\n");
			return;
		}
#else
		;
#endif
	pqh->qh->hlink    = sqh->qh->hlink;
	pqh->qh->qh_hlink = sqh->qh->qh_hlink;
	if (vf->eqh == sqh)
		vf->eqh = pqh;
	vf->bandwidth--;
}

usbd_status
uhci_device_setintr(sc, upipe, ival)
	uhci_softc_t *sc;
	struct uhci_pipe *upipe;
	int ival;
{
	uhci_soft_qh_t *sqh;
	int i, npoll, s;
	u_int bestbw, bw, bestoffs, offs;

	DPRINTFN(2, ("uhci_setintr: pipe=%p\n", upipe));
	if (ival == 0) {
		printf("uhci_setintr: 0 interval\n");
		return (USBD_INVAL);
	}

	if (ival > UHCI_VFRAMELIST_COUNT)
		ival = UHCI_VFRAMELIST_COUNT;
	npoll = (UHCI_VFRAMELIST_COUNT + ival - 1) / ival;
	DPRINTFN(2, ("uhci_setintr: ival=%d npoll=%d\n", ival, npoll));

	upipe->u.intr.npoll = npoll;
	upipe->u.intr.qhs = 
		malloc(npoll * sizeof(uhci_soft_qh_t *), M_USB, M_WAITOK);

	/* 
	 * Figure out which offset in the schedule that has most
	 * bandwidth left over.
	 */
#define MOD(i) ((i) & (UHCI_VFRAMELIST_COUNT-1))
	for (bestoffs = offs = 0, bestbw = ~0; offs < ival; offs++) {
		for (bw = i = 0; i < npoll; i++)
			bw += sc->sc_vframes[MOD(i * ival + offs)].bandwidth;
		if (bw < bestbw) {
			bestbw = bw;
			bestoffs = offs;
		}
	}
	DPRINTFN(1, ("uhci_setintr: bw=%d offs=%d\n", bestbw, bestoffs));

	upipe->iinfo->stdstart = 0;
	for(i = 0; i < npoll; i++) {
		upipe->u.intr.qhs[i] = sqh = uhci_alloc_sqh(sc);
		sqh->qh->elink = 0;
		sqh->qh->qh_elink = UHCI_PTR_T;
		sqh->pos = MOD(i * ival + bestoffs);
		sqh->intr_info = upipe->iinfo;
	}
#undef MOD

	s = splusb();
	LIST_INSERT_HEAD(&sc->sc_intrhead, upipe->iinfo, list);
	splx(s);

	uhci_lock_frames(sc);
	/* Enter QHs into the controller data structures. */
	for(i = 0; i < npoll; i++)
		uhci_add_intr(sc, upipe->u.intr.qhs[i]->pos, 
			      upipe->u.intr.qhs[i]);
	uhci_unlock_frames(sc);

	DPRINTFN(5, ("uhci_setintr: returns %p\n", upipe));
	return (USBD_NORMAL_COMPLETION);
}

/* Open a new pipe. */
usbd_status
uhci_open(pipe)
	usbd_pipe_handle pipe;
{
	uhci_softc_t *sc = (uhci_softc_t *)pipe->device->bus;
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	usbd_status r;

	DPRINTFN(1, ("uhci_open: pipe=%p, addr=%d, endpt=%d (%d)\n",
		     pipe, pipe->device->address, 
		     ed->bEndpointAddress, sc->sc_addr));
	if (pipe->device->address == sc->sc_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &uhci_root_ctrl_methods;
			break;
		case UE_IN | UHCI_INTR_ENDPT:
			pipe->methods = &uhci_root_intr_methods;
			break;
		default:
			return (USBD_INVAL);
		}
	} else {
		upipe->iinfo = uhci_alloc_intr_info(sc);
		if (upipe->iinfo == 0)
			return (USBD_NOMEM);
		switch (ed->bmAttributes & UE_XFERTYPE) {
		case UE_CONTROL:
			pipe->methods = &uhci_device_ctrl_methods;
			upipe->u.ctl.sqh = uhci_alloc_sqh(sc);
			if (upipe->u.ctl.sqh == 0)
				goto bad;
			upipe->u.ctl.setup = uhci_alloc_std(sc);
			if (upipe->u.ctl.setup == 0) {
				uhci_free_sqh(sc, upipe->u.ctl.sqh);
				goto bad;
			}
			upipe->u.ctl.stat = uhci_alloc_std(sc);
			if (upipe->u.ctl.stat == 0) {
				uhci_free_sqh(sc, upipe->u.ctl.sqh);
				uhci_free_std(sc, upipe->u.ctl.setup);
				goto bad;
			}
			r = usb_allocmem(sc->sc_dmatag, 
					 sizeof(usb_device_request_t), 
					 0, &upipe->u.ctl.reqdma);
			if (r != USBD_NORMAL_COMPLETION) {
				uhci_free_sqh(sc, upipe->u.ctl.sqh);
				uhci_free_std(sc, upipe->u.ctl.setup);
				uhci_free_std(sc, upipe->u.ctl.stat);
				goto bad;
			}
			break;
		case UE_INTERRUPT:
			pipe->methods = &uhci_device_intr_methods;
			return (uhci_device_setintr(sc, upipe, ed->bInterval));
		case UE_ISOCHRONOUS:
			pipe->methods = &uhci_device_isoc_methods;
			upipe->u.iso.nbuf = 0;
			return (USBD_NORMAL_COMPLETION);
		case UE_BULK:
			pipe->methods = &uhci_device_bulk_methods;
			upipe->u.bulk.sqh = uhci_alloc_sqh(sc);
			if (upipe->u.bulk.sqh == 0)
				goto bad;
			break;
		}
	}
	return (USBD_NORMAL_COMPLETION);

 bad:
	uhci_free_intr_info(upipe->iinfo);
	return (USBD_NOMEM);
}

/*
 * Data structures and routines to emulate the root hub.
 */
usb_device_descriptor_t uhci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x00, 0x01},		/* USB version */
	UCLASS_HUB,		/* class */
	USUBCLASS_HUB,		/* subclass */
	0,			/* protocol */
	64,			/* max packet */
	{0},{0},{0x00,0x01},	/* device id */
	1,2,0,			/* string indicies */
	1			/* # of configurations */
};

usb_config_descriptor_t uhci_confd = {
	USB_CONFIG_DESCRIPTOR_SIZE,
	UDESC_CONFIG,
	{USB_CONFIG_DESCRIPTOR_SIZE +
	 USB_INTERFACE_DESCRIPTOR_SIZE +
	 USB_ENDPOINT_DESCRIPTOR_SIZE},
	1,
	1,
	0,
	UC_SELF_POWERED,
	0			/* max power */
};

usb_interface_descriptor_t uhci_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,
	0,
	1,
	UCLASS_HUB,
	USUBCLASS_HUB,
	0,
	0
};

usb_endpoint_descriptor_t uhci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_IN | UHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{8},
	255
};

usb_hub_descriptor_t uhci_hubd_piix = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	2,
	{ UHD_PWR_NO_SWITCH | UHD_OC_INDIVIDUAL, 0 },
	50,			/* power on to power good */
	0,
	{ 0x00 },		/* both ports are removable */
};

int
uhci_str(p, l, s)
	usb_string_descriptor_t *p;
	int l;
	char *s;
{
	int i;

	if (l == 0)
		return (0);
	p->bLength = 2 * strlen(s) + 2;
	if (l == 1)
		return (1);
	p->bDescriptorType = UDESC_STRING;
	l -= 2;
	for (i = 0; s[i] && l > 1; i++, l -= 2)
		USETW2(p->bString[i], 0, s[i]);
	return (2*i+2);
}

/*
 * Simulate a hardware hub by handling all the necessary requests.
 */
usbd_status
uhci_root_ctrl_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status r;

	s = splusb();
	r = usb_insert_transfer(reqh);
	splx(s);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	else
		return (uhci_root_ctrl_start(reqh));
}

usbd_status
uhci_root_ctrl_start(reqh)
	usbd_request_handle reqh;
{
	uhci_softc_t *sc = (uhci_softc_t *)reqh->pipe->device->bus;
	usb_device_request_t *req;
	void *buf;
	int port, x;
	int len, value, index, status, change, l, totlen = 0;
	usb_port_status_t ps;
	usbd_status r;

	if (!reqh->isreq)
		panic("uhci_root_ctrl_transfer: not a request\n");
	req = &reqh->request;
	buf = reqh->buffer;

	DPRINTFN(10,("uhci_root_ctrl_control type=0x%02x request=%02x\n", 
		    req->bmRequestType, req->bRequest));

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);
#define C(x,y) ((x) | ((y) << 8))
	switch(C(req->bRequest, req->bmRequestType)) {
	case C(UR_CLEAR_FEATURE, UT_WRITE_DEVICE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_CLEAR_FEATURE, UT_WRITE_ENDPOINT):
		/* 
		 * DEVICE_REMOTE_WAKEUP and ENDPOINT_HALT are no-ops
		 * for the integrated root hub.
		 */
		break;
	case C(UR_GET_CONFIG, UT_READ_DEVICE):
		if (len > 0) {
			*(u_int8_t *)buf = sc->sc_conf;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(2,("uhci_root_ctrl_control wValue=0x%04x\n", value));
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				r = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			memcpy(buf, &uhci_devd, l);
			break;
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				r = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &uhci_confd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &uhci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &uhci_endpd, l);
			break;
		case UDESC_STRING:
			if (len == 0)
				break;
			*(u_int8_t *)buf = 0;
			totlen = 1;
			switch (value & 0xff) {
			case 1: /* Vendor */
				totlen = uhci_str(buf, len, sc->sc_vendor);
				break;
			case 2: /* Product */
				totlen = uhci_str(buf, len, "UHCI root hub");
				break;
			}
			break;
		default:
			r = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE):
		if (len > 0) {
			*(u_int8_t *)buf = 0;
			totlen = 1;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_DEVICE):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus,UDS_SELF_POWERED);
			totlen = 2;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_ENDPOINT):
		if (len > 1) {
			USETW(((usb_status_t *)buf)->wStatus, 0);
			totlen = 2;
		}
		break;
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		if (value >= USB_MAX_DEVICES) {
			r = USBD_IOERROR;
			goto ret;
		}
		sc->sc_addr = value;
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			r = USBD_IOERROR;
			goto ret;
		}
		sc->sc_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		r = USBD_IOERROR;
		goto ret;
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		break;
	case C(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		break;
	/* Hub requests */
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
		DPRINTFN(3, ("uhci_root_ctrl_control: UR_CLEAR_PORT_FEATURE "
			     "port=%d feature=%d\n",
			     index, value));
		if (index == 1)
			port = UHCI_PORTSC1;
		else if (index == 2)
			port = UHCI_PORTSC2;
		else {
			r = USBD_IOERROR;
			goto ret;
		}
		switch(value) {
		case UHF_PORT_ENABLE:
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x & ~UHCI_PORTSC_PE);
			break;
		case UHF_PORT_SUSPEND:
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x & ~UHCI_PORTSC_SUSP);
			break;
		case UHF_PORT_RESET:
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x & ~UHCI_PORTSC_PR);
			break;
		case UHF_C_PORT_CONNECTION:
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x | UHCI_PORTSC_CSC);
			break;
		case UHF_C_PORT_ENABLE:
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x | UHCI_PORTSC_POEDC);
			break;
		case UHF_C_PORT_OVER_CURRENT:
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x | UHCI_PORTSC_OCIC);
			break;
		case UHF_C_PORT_RESET:
			sc->sc_isreset = 0;
			r = USBD_NORMAL_COMPLETION;
			goto ret;
		case UHF_PORT_CONNECTION:
		case UHF_PORT_OVER_CURRENT:
		case UHF_PORT_POWER:
		case UHF_PORT_LOW_SPEED:
		case UHF_C_PORT_SUSPEND:
		default:
			r = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_GET_BUS_STATE, UT_READ_CLASS_OTHER):
		if (index == 1)
			port = UHCI_PORTSC1;
		else if (index == 2)
			port = UHCI_PORTSC2;
		else {
			r = USBD_IOERROR;
			goto ret;
		}
		if (len > 0) {
			*(u_int8_t *)buf = 
				(UREAD2(sc, port) & UHCI_PORTSC_LS) >>
				UHCI_PORTSC_LS_SHIFT;
			totlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if (value != 0) {
			r = USBD_IOERROR;
			goto ret;
		}
		l = min(len, USB_HUB_DESCRIPTOR_SIZE);
		totlen = l;
		memcpy(buf, &uhci_hubd_piix, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		if (len != 4) {
			r = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len);
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		if (index == 1)
			port = UHCI_PORTSC1;
		else if (index == 2)
			port = UHCI_PORTSC2;
		else {
			r = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			r = USBD_IOERROR;
			goto ret;
		}
		x = UREAD2(sc, port);
		status = change = 0;
		if (x & UHCI_PORTSC_CCS  )
			status |= UPS_CURRENT_CONNECT_STATUS;
		if (x & UHCI_PORTSC_CSC  ) 
			change |= UPS_C_CONNECT_STATUS;
		if (x & UHCI_PORTSC_PE   ) 
			status |= UPS_PORT_ENABLED;
		if (x & UHCI_PORTSC_POEDC) 
			change |= UPS_C_PORT_ENABLED;
		if (x & UHCI_PORTSC_OCI  ) 
			status |= UPS_OVERCURRENT_INDICATOR;
		if (x & UHCI_PORTSC_OCIC ) 
			change |= UPS_C_OVERCURRENT_INDICATOR;
		if (x & UHCI_PORTSC_SUSP ) 
			status |= UPS_SUSPEND;
		if (x & UHCI_PORTSC_LSDA ) 
			status |= UPS_LOW_SPEED;
		status |= UPS_PORT_POWER;
		if (sc->sc_isreset)
			change |= UPS_C_PORT_RESET;
		USETW(ps.wPortStatus, status);
		USETW(ps.wPortChange, change);
		l = min(len, sizeof ps);
		memcpy(buf, &ps, l);
		totlen = l;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_CLASS_DEVICE):
		r = USBD_IOERROR;
		goto ret;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
		if (index == 1)
			port = UHCI_PORTSC1;
		else if (index == 2)
			port = UHCI_PORTSC2;
		else {
			r = USBD_IOERROR;
			goto ret;
		}
		switch(value) {
		case UHF_PORT_ENABLE:
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x | UHCI_PORTSC_PE);
			break;
		case UHF_PORT_SUSPEND:
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x | UHCI_PORTSC_SUSP);
			break;
		case UHF_PORT_RESET:
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x | UHCI_PORTSC_PR);
			usb_delay_ms(&sc->sc_bus, 10);
			UWRITE2(sc, port, x & ~UHCI_PORTSC_PR);
			delay(100);
			x = UREAD2(sc, port);
			UWRITE2(sc, port, x  | UHCI_PORTSC_PE);
			delay(100);
			DPRINTFN(3,("uhci port %d reset, status = 0x%04x\n",
				    index, UREAD2(sc, port)));
			sc->sc_isreset = 1;
			break;
		case UHF_C_PORT_CONNECTION:
		case UHF_C_PORT_ENABLE:
		case UHF_C_PORT_OVER_CURRENT:
		case UHF_PORT_CONNECTION:
		case UHF_PORT_OVER_CURRENT:
		case UHF_PORT_POWER:
		case UHF_PORT_LOW_SPEED:
		case UHF_C_PORT_SUSPEND:
		case UHF_C_PORT_RESET:
		default:
			r = USBD_IOERROR;
			goto ret;
		}
		break;
	default:
		r = USBD_IOERROR;
		goto ret;
	}
	reqh->actlen = totlen;
	r = USBD_NORMAL_COMPLETION;
 ret:
	reqh->status = r;
	reqh->xfercb(reqh);
	usb_start_next(reqh->pipe);
	return (USBD_IN_PROGRESS);
}

/* Abort a root control request. */
void
uhci_root_ctrl_abort(reqh)
	usbd_request_handle reqh;
{
	/* Nothing to do, all transfers are syncronous. */
}

/* Close the root pipe. */
void
uhci_root_ctrl_close(pipe)
	usbd_pipe_handle pipe;
{
	usb_untimeout(uhci_timo, pipe->intrreqh, pipe->intrreqh->timo_handle);
	DPRINTF(("uhci_root_ctrl_close\n"));
}

/* Abort a root interrupt request. */
void
uhci_root_intr_abort(reqh)
	usbd_request_handle reqh;
{
	usb_untimeout(uhci_timo, reqh, reqh->timo_handle);
}

usbd_status
uhci_root_intr_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status r;

	s = splusb();
	r = usb_insert_transfer(reqh);
	splx(s);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	else
		return (uhci_root_intr_start(reqh));
}

/* Start a transfer on the root interrupt pipe */
usbd_status
uhci_root_intr_start(reqh)
	usbd_request_handle reqh;
{
	usbd_pipe_handle pipe = reqh->pipe;
	uhci_softc_t *sc = (uhci_softc_t *)pipe->device->bus;
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;
	usb_dma_t *dmap;
	usbd_status r;
	int len;

	DPRINTFN(3, ("uhci_root_intr_transfer: reqh=%p buf=%p len=%d "
		     "flags=%d\n",
		     reqh, reqh->buffer, reqh->length, reqh->flags));

	len = reqh->length;
	dmap = &upipe->u.intr.datadma;
	if (len == 0)
		return (USBD_INVAL); /* XXX should it be? */

	r = usb_allocmem(sc->sc_dmatag, len, 0, dmap);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);

	sc->sc_ival = MS_TO_TICKS(reqh->pipe->endpoint->edesc->bInterval);
	usb_timeout(uhci_timo, reqh, sc->sc_ival, reqh->timo_handle);
	return (USBD_IN_PROGRESS);
}

/* Close the root interrupt pipe. */
void
uhci_root_intr_close(pipe)
	usbd_pipe_handle pipe;
{
	usb_untimeout(uhci_timo, pipe->intrreqh, pipe->intrreqh->timo_handle);
	DPRINTF(("uhci_root_intr_close\n"));
}
