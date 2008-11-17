/*	$NetBSD: ehci.c,v 1.91 2005/02/27 00:27:51 perry Exp $ */

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Lennart Augustsson (lennart@augustsson.net) and by Charles M. Hannum.
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
 * USB Enhanced Host Controller Driver, a.k.a. USB 2.0 controller.
 *
 * The EHCI 1.0 spec can be found at
 * http://developer.intel.com/technology/usb/download/ehci-r10.pdf
 * and the USB 2.0 spec at
 * http://www.usb.org/developers/docs/usb_20.zip
 *
 */

/*
 * TODO:
 * 1) The EHCI driver lacks support for isochronous transfers, so
 *    devices using them don't work.
 *
 * 2) Interrupt transfer scheduling does not manage the time available
 *    in each frame, so it is possible for the transfers to overrun
 *    the end of the frame.
 *
 * 3) Command failures are not recovered correctly.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/lockmgr.h>
#if defined(DIAGNOSTIC) && defined(__i386__) && defined(__FreeBSD__)
#include <machine/cpu.h>
#endif
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ehcireg.h>
#include <dev/usb/ehcivar.h>

#define delay(d)                DELAY(d)

#ifdef USB_DEBUG
#define EHCI_DEBUG USB_DEBUG
#define DPRINTF(x)	do { if (ehcidebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (ehcidebug>(n)) printf x; } while (0)
int ehcidebug = 0;
SYSCTL_NODE(_hw_usb, OID_AUTO, ehci, CTLFLAG_RW, 0, "USB ehci");
SYSCTL_INT(_hw_usb_ehci, OID_AUTO, debug, CTLFLAG_RW,
	   &ehcidebug, 0, "ehci debug level");
#define bitmask_snprintf(q,f,b,l) snprintf((b), (l), "%b", (q), (f))
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct ehci_pipe {
	struct usbd_pipe pipe;

	ehci_soft_qh_t *sqh;
	union {
		ehci_soft_qtd_t *qtd;
		/* ehci_soft_itd_t *itd; */
	} tail;
	union {
		/* Control pipe */
		struct {
			usb_dma_t reqdma;
			u_int length;
			/*ehci_soft_qtd_t *setup, *data, *stat;*/
		} ctl;
		/* Interrupt pipe */
		struct {
			u_int length;
		} intr;
		/* Bulk pipe */
		struct {
			u_int length;
		} bulk;
		/* Iso pipe */
		struct {
			u_int next_frame;
			u_int cur_xfers;
		} isoc;
	} u;
};

static usbd_status	ehci_open(usbd_pipe_handle);
static void		ehci_poll(struct usbd_bus *);
static void		ehci_softintr(void *);
static int		ehci_intr1(ehci_softc_t *);
static void		ehci_waitintr(ehci_softc_t *, usbd_xfer_handle);
static void		ehci_check_intr(ehci_softc_t *, struct ehci_xfer *);
static void		ehci_check_qh_intr(ehci_softc_t *, struct ehci_xfer *);
static void		ehci_check_itd_intr(ehci_softc_t *, struct ehci_xfer *);
static void		ehci_idone(struct ehci_xfer *);
static void		ehci_timeout(void *);
static void		ehci_timeout_task(void *);
static void		ehci_intrlist_timeout(void *);

static usbd_status	ehci_allocm(struct usbd_bus *, usb_dma_t *, u_int32_t);
static void		ehci_freem(struct usbd_bus *, usb_dma_t *);

static usbd_xfer_handle	ehci_allocx(struct usbd_bus *);
static void		ehci_freex(struct usbd_bus *, usbd_xfer_handle);

static usbd_status	ehci_root_ctrl_transfer(usbd_xfer_handle);
static usbd_status	ehci_root_ctrl_start(usbd_xfer_handle);
static void		ehci_root_ctrl_abort(usbd_xfer_handle);
static void		ehci_root_ctrl_close(usbd_pipe_handle);
static void		ehci_root_ctrl_done(usbd_xfer_handle);

static usbd_status	ehci_root_intr_transfer(usbd_xfer_handle);
static usbd_status	ehci_root_intr_start(usbd_xfer_handle);
static void		ehci_root_intr_abort(usbd_xfer_handle);
static void		ehci_root_intr_close(usbd_pipe_handle);
static void		ehci_root_intr_done(usbd_xfer_handle);

static usbd_status	ehci_device_ctrl_transfer(usbd_xfer_handle);
static usbd_status	ehci_device_ctrl_start(usbd_xfer_handle);
static void		ehci_device_ctrl_abort(usbd_xfer_handle);
static void		ehci_device_ctrl_close(usbd_pipe_handle);
static void		ehci_device_ctrl_done(usbd_xfer_handle);

static usbd_status	ehci_device_bulk_transfer(usbd_xfer_handle);
static usbd_status	ehci_device_bulk_start(usbd_xfer_handle);
static void		ehci_device_bulk_abort(usbd_xfer_handle);
static void		ehci_device_bulk_close(usbd_pipe_handle);
static void		ehci_device_bulk_done(usbd_xfer_handle);

static usbd_status	ehci_device_intr_transfer(usbd_xfer_handle);
static usbd_status	ehci_device_intr_start(usbd_xfer_handle);
static void		ehci_device_intr_abort(usbd_xfer_handle);
static void		ehci_device_intr_close(usbd_pipe_handle);
static void		ehci_device_intr_done(usbd_xfer_handle);

static usbd_status	ehci_device_isoc_transfer(usbd_xfer_handle);
static usbd_status	ehci_device_isoc_start(usbd_xfer_handle);
static void		ehci_device_isoc_abort(usbd_xfer_handle);
static void		ehci_device_isoc_close(usbd_pipe_handle);
static void		ehci_device_isoc_done(usbd_xfer_handle);

static void		ehci_device_clear_toggle(usbd_pipe_handle pipe);
static void		ehci_noop(usbd_pipe_handle pipe);

static int		ehci_str(usb_string_descriptor_t *, int, char *);
static void		ehci_pcd(ehci_softc_t *, usbd_xfer_handle);
static void		ehci_disown(ehci_softc_t *, int, int);

static ehci_soft_qh_t  *ehci_alloc_sqh(ehci_softc_t *);
static void		ehci_free_sqh(ehci_softc_t *, ehci_soft_qh_t *);

static ehci_soft_qtd_t  *ehci_alloc_sqtd(ehci_softc_t *);
static void		ehci_free_sqtd(ehci_softc_t *, ehci_soft_qtd_t *);
static usbd_status	ehci_alloc_sqtd_chain(struct ehci_pipe *,
			    ehci_softc_t *, int, int, usbd_xfer_handle,
			    ehci_soft_qtd_t *, ehci_soft_qtd_t *,
			    ehci_soft_qtd_t **, ehci_soft_qtd_t **);
static void		ehci_free_sqtd_chain(ehci_softc_t *, ehci_soft_qh_t *,
			    ehci_soft_qtd_t *, ehci_soft_qtd_t *);

static ehci_soft_itd_t	*ehci_alloc_itd(ehci_softc_t *);
static void		ehci_free_itd(ehci_softc_t *, ehci_soft_itd_t *);
static void 		ehci_rem_free_itd_chain(ehci_softc_t *, 
			    struct ehci_xfer *);
static void 		ehci_abort_isoc_xfer(usbd_xfer_handle, usbd_status);

static usbd_status	ehci_device_request(usbd_xfer_handle xfer);

static usbd_status	ehci_device_setintr(ehci_softc_t *, ehci_soft_qh_t *,
			    int ival);

static void		ehci_add_qh(ehci_soft_qh_t *, ehci_soft_qh_t *);
static void		ehci_rem_qh(ehci_softc_t *, ehci_soft_qh_t *,
				    ehci_soft_qh_t *);
static void		ehci_activate_qh(ehci_soft_qh_t *, ehci_soft_qtd_t *);
static void		ehci_sync_hc(ehci_softc_t *);

static void		ehci_close_pipe(usbd_pipe_handle, ehci_soft_qh_t *);
static void		ehci_abort_xfer(usbd_xfer_handle, usbd_status);

#ifdef EHCI_DEBUG
static void		ehci_dump_regs(ehci_softc_t *);
void			ehci_dump(void);
static ehci_softc_t 	*theehci;
static void		ehci_dump_link(ehci_link_t, int);
static void		ehci_dump_sqtds(ehci_soft_qtd_t *);
static void		ehci_dump_sqtd(ehci_soft_qtd_t *);
static void		ehci_dump_qtd(ehci_qtd_t *);
static void		ehci_dump_sqh(ehci_soft_qh_t *);
#ifdef notyet
static void		ehci_dump_sitd(struct ehci_soft_itd *);
static void		ehci_dump_itd(struct ehci_soft_itd *);
#endif
#ifdef DIAGNOSTIC
static void		ehci_dump_exfer(struct ehci_xfer *);
#endif
#endif

#define EHCI_NULL htole32(EHCI_LINK_TERMINATE)

#define EHCI_INTR_ENDPT 1

#define ehci_add_intr_list(sc, ex) \
	LIST_INSERT_HEAD(&(sc)->sc_intrhead, (ex), inext);
#define ehci_del_intr_list(ex) \
	do { \
		LIST_REMOVE((ex), inext); \
		(ex)->inext.le_prev = NULL; \
	} while (0)
#define ehci_active_intr_list(ex) ((ex)->inext.le_prev != NULL)

static struct usbd_bus_methods ehci_bus_methods = {
	ehci_open,
	ehci_softintr,
	ehci_poll,
	ehci_allocm,
	ehci_freem,
	ehci_allocx,
	ehci_freex,
};

static struct usbd_pipe_methods ehci_root_ctrl_methods = {
	ehci_root_ctrl_transfer,
	ehci_root_ctrl_start,
	ehci_root_ctrl_abort,
	ehci_root_ctrl_close,
	ehci_noop,
	ehci_root_ctrl_done,
};

static struct usbd_pipe_methods ehci_root_intr_methods = {
	ehci_root_intr_transfer,
	ehci_root_intr_start,
	ehci_root_intr_abort,
	ehci_root_intr_close,
	ehci_noop,
	ehci_root_intr_done,
};

static struct usbd_pipe_methods ehci_device_ctrl_methods = {
	ehci_device_ctrl_transfer,
	ehci_device_ctrl_start,
	ehci_device_ctrl_abort,
	ehci_device_ctrl_close,
	ehci_noop,
	ehci_device_ctrl_done,
};

static struct usbd_pipe_methods ehci_device_intr_methods = {
	ehci_device_intr_transfer,
	ehci_device_intr_start,
	ehci_device_intr_abort,
	ehci_device_intr_close,
	ehci_device_clear_toggle,
	ehci_device_intr_done,
};

static struct usbd_pipe_methods ehci_device_bulk_methods = {
	ehci_device_bulk_transfer,
	ehci_device_bulk_start,
	ehci_device_bulk_abort,
	ehci_device_bulk_close,
	ehci_device_clear_toggle,
	ehci_device_bulk_done,
};

static struct usbd_pipe_methods ehci_device_isoc_methods = {
	ehci_device_isoc_transfer,
	ehci_device_isoc_start,
	ehci_device_isoc_abort,
	ehci_device_isoc_close,
	ehci_noop,
	ehci_device_isoc_done,
};

static usbd_status
ehci_hcreset(ehci_softc_t *sc)
{
	u_int32_t hcr;
	u_int i;

	EOWRITE4(sc, EHCI_USBCMD, 0);	/* Halt controller */
	for (i = 0; i < 100; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
		if (hcr)
			break;
	}
	if (!hcr)
		/*
                 * Fall through and try reset anyway even though
                 * Table 2-9 in the EHCI spec says this will result
                 * in undefined behavior.
                 */
		printf("%s: stop timeout\n",
		       device_get_nameunit(sc->sc_bus.bdev));

	EOWRITE4(sc, EHCI_USBCMD, EHCI_CMD_HCRESET);
	for (i = 0; i < 100; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		hcr = EOREAD4(sc, EHCI_USBCMD) & EHCI_CMD_HCRESET;
		if (!hcr) {
			if (sc->sc_flags & EHCI_SCFLG_SETMODE)
				EOWRITE4(sc,  0x68, 0x3);

			return (USBD_NORMAL_COMPLETION);
		}
	}
	printf("%s: reset timeout\n", device_get_nameunit(sc->sc_bus.bdev));
	return (USBD_IOERROR);
}

usbd_status
ehci_init(ehci_softc_t *sc)
{
	u_int32_t version, sparams, cparams, hcr;
	u_int i;
	usbd_status err;
	ehci_soft_qh_t *sqh;
	u_int ncomp;
	int lev;

	DPRINTF(("ehci_init: start\n"));
#ifdef EHCI_DEBUG
	theehci = sc;
#endif

	sc->sc_offs = EREAD1(sc, EHCI_CAPLENGTH);

	version = EREAD2(sc, EHCI_HCIVERSION);
	printf("%s: EHCI version %x.%x\n", device_get_nameunit(sc->sc_bus.bdev),
	       version >> 8, version & 0xff);

	sparams = EREAD4(sc, EHCI_HCSPARAMS);
	DPRINTF(("ehci_init: sparams=0x%x\n", sparams));
	sc->sc_npcomp = EHCI_HCS_N_PCC(sparams);
	ncomp = EHCI_HCS_N_CC(sparams);
	if (ncomp != sc->sc_ncomp) {
		printf("%s: wrong number of companions (%d != %d)\n",
		       device_get_nameunit(sc->sc_bus.bdev),
		       ncomp, sc->sc_ncomp);
		if (ncomp < sc->sc_ncomp)
			sc->sc_ncomp = ncomp;
	}
	if (sc->sc_ncomp > 0) {
		printf("%s: companion controller%s, %d port%s each:",
		    device_get_nameunit(sc->sc_bus.bdev), sc->sc_ncomp!=1 ? "s" : "",
		    EHCI_HCS_N_PCC(sparams),
		    EHCI_HCS_N_PCC(sparams)!=1 ? "s" : "");
		for (i = 0; i < sc->sc_ncomp; i++)
			printf(" %s", device_get_nameunit(sc->sc_comps[i]->bdev));
		printf("\n");
	}
	sc->sc_noport = EHCI_HCS_N_PORTS(sparams);
	cparams = EREAD4(sc, EHCI_HCCPARAMS);
	DPRINTF(("ehci_init: cparams=0x%x\n", cparams));

	if (EHCI_HCC_64BIT(cparams)) {
		/* MUST clear segment register if 64 bit capable. */
		EWRITE4(sc, EHCI_CTRLDSSEGMENT, 0);
	}

	sc->sc_bus.usbrev = USBREV_2_0;

	/* Reset the controller */
	DPRINTF(("%s: resetting\n", device_get_nameunit(sc->sc_bus.bdev)));
	err = ehci_hcreset(sc);
	if (err != USBD_NORMAL_COMPLETION)
		return (err);

	/* frame list size at default, read back what we got and use that */
	switch (EHCI_CMD_FLS(EOREAD4(sc, EHCI_USBCMD))) {
	case 0: sc->sc_flsize = 1024; break;
	case 1: sc->sc_flsize = 512; break;
	case 2: sc->sc_flsize = 256; break;
	case 3: return (USBD_IOERROR);
	}
	err = usb_allocmem(&sc->sc_bus, sc->sc_flsize * sizeof(ehci_link_t),
	    EHCI_FLALIGN_ALIGN, &sc->sc_fldma);
	if (err)
		return (err);
	DPRINTF(("%s: flsize=%d\n", device_get_nameunit(sc->sc_bus.bdev),sc->sc_flsize));
	sc->sc_flist = KERNADDR(&sc->sc_fldma, 0);

	for (i = 0; i < sc->sc_flsize; i++) {
		sc->sc_flist[i] = EHCI_NULL;
	}

	EOWRITE4(sc, EHCI_PERIODICLISTBASE, DMAADDR(&sc->sc_fldma, 0));

	sc->sc_softitds = malloc(sc->sc_flsize * sizeof(ehci_soft_itd_t *),
	    M_USB, M_NOWAIT | M_ZERO);
	if (sc->sc_softitds == NULL)
		return (ENOMEM);
	LIST_INIT(&sc->sc_freeitds);

	/* Set up the bus struct. */
	sc->sc_bus.methods = &ehci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct ehci_pipe);

#if defined(__NetBSD__) || defined(__OpenBSD__)
	sc->sc_powerhook = powerhook_establish(ehci_power, sc);
	sc->sc_shutdownhook = shutdownhook_establish(ehci_shutdown, sc);
#endif

	sc->sc_eintrs = EHCI_NORMAL_INTRS;

	/*
	 * Allocate the interrupt dummy QHs. These are arranged to give
	 * poll intervals that are powers of 2 times 1ms.
	 */
	for (i = 0; i < EHCI_INTRQHS; i++) {
		sqh = ehci_alloc_sqh(sc);
		if (sqh == NULL) {
			err = USBD_NOMEM;
			goto bad1;
		}
		sc->sc_islots[i].sqh = sqh;
	}
	lev = 0;
	for (i = 0; i < EHCI_INTRQHS; i++) {
		if (i == EHCI_IQHIDX(lev + 1, 0))
			lev++;
		sqh = sc->sc_islots[i].sqh;
		if (i == 0) {
			/* The last (1ms) QH terminates. */
			sqh->qh.qh_link = EHCI_NULL;
			sqh->next = NULL;
		} else {
			/* Otherwise the next QH has half the poll interval */
			sqh->next =
			    sc->sc_islots[EHCI_IQHIDX(lev - 1, i + 1)].sqh;
			sqh->qh.qh_link = htole32(sqh->next->physaddr |
			    EHCI_LINK_QH);
		}
		sqh->qh.qh_endp = htole32(EHCI_QH_SET_EPS(EHCI_QH_SPEED_HIGH));
		sqh->qh.qh_endphub = htole32(EHCI_QH_SET_MULT(1));
		sqh->qh.qh_curqtd = EHCI_NULL;
		sqh->qh.qh_qtd.qtd_next = EHCI_NULL;
		sqh->qh.qh_qtd.qtd_altnext = EHCI_NULL;
		sqh->qh.qh_qtd.qtd_status = htole32(EHCI_QTD_HALTED);
	}
	/* Point the frame list at the last level (128ms). */
	for (i = 0; i < sc->sc_flsize; i++) {
		sc->sc_flist[i] = htole32(EHCI_LINK_QH |
		    sc->sc_islots[EHCI_IQHIDX(EHCI_IPOLLRATES - 1,
		    i)].sqh->physaddr);
	}

	/* Allocate dummy QH that starts the async list. */
	sqh = ehci_alloc_sqh(sc);
	if (sqh == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	/* Fill the QH */
	sqh->qh.qh_endp =
	    htole32(EHCI_QH_SET_EPS(EHCI_QH_SPEED_HIGH) | EHCI_QH_HRECL);
	sqh->qh.qh_link =
	    htole32(sqh->physaddr | EHCI_LINK_QH);
	sqh->qh.qh_curqtd = EHCI_NULL;
	sqh->prev = sqh; /*It's a circular list.. */
	sqh->next = sqh;
	/* Fill the overlay qTD */
	sqh->qh.qh_qtd.qtd_next = EHCI_NULL;
	sqh->qh.qh_qtd.qtd_altnext = EHCI_NULL;
	sqh->qh.qh_qtd.qtd_status = htole32(0);
#ifdef EHCI_DEBUG
	if (ehcidebug) {
		ehci_dump_sqh(sqh);
	}
#endif

	/* Point to async list */
	sc->sc_async_head = sqh;
	EOWRITE4(sc, EHCI_ASYNCLISTADDR, sqh->physaddr | EHCI_LINK_QH);

	callout_init(&sc->sc_tmo_intrlist, 0);

	lockinit(&sc->sc_doorbell_lock, PZERO, "ehcidb", 0, 0);

	/* Enable interrupts */
	EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);

	/* Turn on controller */
	EOWRITE4(sc, EHCI_USBCMD,
		 EHCI_CMD_ITC_2 | /* 2 microframes interrupt delay */
		 (EOREAD4(sc, EHCI_USBCMD) & EHCI_CMD_FLS_M) |
		 EHCI_CMD_ASE |
		 EHCI_CMD_PSE |
		 EHCI_CMD_RS);

	/* Take over port ownership */
	EOWRITE4(sc, EHCI_CONFIGFLAG, EHCI_CONF_CF);

	for (i = 0; i < 100; i++) {
		usb_delay_ms(&sc->sc_bus, 1);
		hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
		if (!hcr)
			break;
	}
	if (hcr) {
		printf("%s: run timeout\n", device_get_nameunit(sc->sc_bus.bdev));
		return (USBD_IOERROR);
	}

	return (USBD_NORMAL_COMPLETION);

#if 0
 bad2:
	ehci_free_sqh(sc, sc->sc_async_head);
#endif
 bad1:
	usb_freemem(&sc->sc_bus, &sc->sc_fldma);
	return (err);
}

int
ehci_intr(void *v)
{
	ehci_softc_t *sc = v;

	if (sc == NULL || sc->sc_dying)
		return (0);

	/* If we get an interrupt while polling, then just ignore it. */
	if (sc->sc_bus.use_polling) {
		u_int32_t intrs = EHCI_STS_INTRS(EOREAD4(sc, EHCI_USBSTS));

		if (intrs)
			EOWRITE4(sc, EHCI_USBSTS, intrs); /* Acknowledge */
#ifdef DIAGNOSTIC
		DPRINTFN(16, ("ehci_intr: ignored interrupt while polling\n"));
#endif
		return (0);
	}

	return (ehci_intr1(sc));
}

static int
ehci_intr1(ehci_softc_t *sc)
{
	u_int32_t intrs, eintrs;

	DPRINTFN(20,("ehci_intr1: enter\n"));

	/* In case the interrupt occurs before initialization has completed. */
	if (sc == NULL) {
#ifdef DIAGNOSTIC
		printf("ehci_intr1: sc == NULL\n");
#endif
		return (0);
	}

	intrs = EHCI_STS_INTRS(EOREAD4(sc, EHCI_USBSTS));
	if (!intrs)
		return (0);

	eintrs = intrs & sc->sc_eintrs;
	DPRINTFN(7, ("ehci_intr1: sc=%p intrs=0x%x(0x%x) eintrs=0x%x\n",
		     sc, (u_int)intrs, EOREAD4(sc, EHCI_USBSTS),
		     (u_int)eintrs));
	if (!eintrs)
		return (0);

	EOWRITE4(sc, EHCI_USBSTS, intrs); /* Acknowledge */
	sc->sc_bus.intr_context++;
	sc->sc_bus.no_intrs++;
	if (eintrs & EHCI_STS_IAA) {
		DPRINTF(("ehci_intr1: door bell\n"));
		wakeup(&sc->sc_async_head);
		eintrs &= ~EHCI_STS_IAA;
	}
	if (eintrs & (EHCI_STS_INT | EHCI_STS_ERRINT)) {
		DPRINTFN(5,("ehci_intr1: %s %s\n",
			    eintrs & EHCI_STS_INT ? "INT" : "",
			    eintrs & EHCI_STS_ERRINT ? "ERRINT" : ""));
		usb_schedsoftintr(&sc->sc_bus);
		eintrs &= ~(EHCI_STS_INT | EHCI_STS_ERRINT);
	}
	if (eintrs & EHCI_STS_HSE) {
		printf("%s: unrecoverable error, controller halted\n",
		       device_get_nameunit(sc->sc_bus.bdev));
		/* XXX what else */
	}
	if (eintrs & EHCI_STS_PCD) {
		ehci_pcd(sc, sc->sc_intrxfer);
		eintrs &= ~EHCI_STS_PCD;
	}

	sc->sc_bus.intr_context--;

	if (eintrs != 0) {
		/* Block unprocessed interrupts. */
		sc->sc_eintrs &= ~eintrs;
		EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);
		printf("%s: blocking intrs 0x%x\n",
		       device_get_nameunit(sc->sc_bus.bdev), eintrs);
	}

	return (1);
}

/*
 * XXX write back xfer data for architectures with a write-back
 *     data cache; this is a hack because usb is mis-architected
 *     in blindly mixing bus_dma w/ PIO.
 */
static __inline void
hacksync(usbd_xfer_handle xfer)
{
	bus_dma_tag_t tag;
	struct usb_dma_mapping *dmap;

	if (xfer->length == 0)
		return;
	tag = xfer->pipe->device->bus->buffer_dmatag;
	dmap = &xfer->dmamap;
	bus_dmamap_sync(tag, dmap->map, BUS_DMASYNC_PREWRITE);
}

void
ehci_pcd(ehci_softc_t *sc, usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe;
	u_char *p;
	int i, m;

	if (xfer == NULL) {
		/* Just ignore the change. */
		return;
	}

	pipe = xfer->pipe;

	p = xfer->buffer;
	m = min(sc->sc_noport, xfer->length * 8 - 1);
	memset(p, 0, xfer->length);
	for (i = 1; i <= m; i++) {
		/* Pick out CHANGE bits from the status reg. */
		if (EOREAD4(sc, EHCI_PORTSC(i)) & EHCI_PS_CLEAR)
			p[i/8] |= 1 << (i%8);
	}
	DPRINTF(("ehci_pcd: change=0x%02x\n", *p));
	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;

	hacksync(xfer);	/* XXX to compensate for usb_transfer_complete */
	usb_transfer_complete(xfer);
}

void
ehci_softintr(void *v)
{
	ehci_softc_t *sc = v;
	struct ehci_xfer *ex, *nextex;

	DPRINTFN(10,("%s: ehci_softintr (%d)\n", device_get_nameunit(sc->sc_bus.bdev),
		     sc->sc_bus.intr_context));

	sc->sc_bus.intr_context++;

	/*
	 * The only explanation I can think of for why EHCI is as brain dead
	 * as UHCI interrupt-wise is that Intel was involved in both.
	 * An interrupt just tells us that something is done, we have no
	 * clue what, so we need to scan through all active transfers. :-(
	 */
	for (ex = LIST_FIRST(&sc->sc_intrhead); ex; ex = nextex) {
		nextex = LIST_NEXT(ex, inext);
		ehci_check_intr(sc, ex);
	}

	/* Schedule a callout to catch any dropped transactions. */
	if ((sc->sc_flags & EHCI_SCFLG_LOSTINTRBUG) &&
	    !LIST_EMPTY(&sc->sc_intrhead))
		callout_reset(&sc->sc_tmo_intrlist, hz / 5,
		    ehci_intrlist_timeout, sc);

#ifdef USB_USE_SOFTINTR
	if (sc->sc_softwake) {
		sc->sc_softwake = 0;
		wakeup(&sc->sc_softwake);
	}
#endif /* USB_USE_SOFTINTR */

	sc->sc_bus.intr_context--;
}

/* Check for an interrupt. */
void
ehci_check_intr(ehci_softc_t *sc, struct ehci_xfer *ex)
{
	int attr;

	DPRINTFN(/*15*/2, ("ehci_check_intr: ex=%p\n", ex));

	attr = ex->xfer.pipe->endpoint->edesc->bmAttributes;
	if (UE_GET_XFERTYPE(attr) == UE_ISOCHRONOUS)
		ehci_check_itd_intr(sc, ex);
	else
		ehci_check_qh_intr(sc, ex);
}

void
ehci_check_qh_intr(ehci_softc_t *sc, struct ehci_xfer *ex)
{
	ehci_soft_qtd_t *sqtd, *lsqtd;
	u_int32_t status;

	if (ex->sqtdstart == NULL) {
		printf("ehci_check_qh_intr: not valid sqtd\n");
		return;
	}
	lsqtd = ex->sqtdend;
#ifdef DIAGNOSTIC
	if (lsqtd == NULL) {
		printf("ehci_check_qh_intr: lsqtd==0\n");
		return;
	}
#endif
	/*
	 * If the last TD is still active we need to check whether there
	 * is a an error somewhere in the middle, or whether there was a
	 * short packet (SPD and not ACTIVE).
	 */
	if (le32toh(lsqtd->qtd.qtd_status) & EHCI_QTD_ACTIVE) {
		DPRINTFN(12, ("ehci_check_intr: active ex=%p\n", ex));
		for (sqtd = ex->sqtdstart; sqtd != lsqtd; sqtd=sqtd->nextqtd) {
			status = le32toh(sqtd->qtd.qtd_status);
			/* If there's an active QTD the xfer isn't done. */
			if (status & EHCI_QTD_ACTIVE)
				break;
			/* Any kind of error makes the xfer done. */
			if (status & EHCI_QTD_HALTED)
				goto done;
			/* We want short packets, and it is short: it's done */
			if (EHCI_QTD_GET_BYTES(status) != 0)
				goto done;
		}
		DPRINTFN(12, ("ehci_check_intr: ex=%p std=%p still active\n",
			      ex, ex->sqtdstart));
		return;
	}
 done:
	DPRINTFN(12, ("ehci_check_intr: ex=%p done\n", ex));
	callout_stop(&ex->xfer.timeout_handle);
	usb_rem_task(ex->xfer.pipe->device, &ex->abort_task);
	ehci_idone(ex);
}

void
ehci_check_itd_intr(ehci_softc_t *sc, struct ehci_xfer *ex)
{
	ehci_soft_itd_t *itd;
	int i;

	if (ex->itdstart == NULL) {
		printf("ehci_check_itd_intr: not valid itd\n");
		return;
	}

	itd = ex->itdend;
#ifdef DIAGNOSTIC
	if (itd == NULL) {
		printf("ehci_check_itd_intr: itdend == 0\n");
		return;
	}
#endif

	/*
	 * Step 1, check no active transfers in last itd, meaning we're finished
	 */
	for (i = 0; i < 8; i++) {
		if (le32toh(itd->itd.itd_ctl[i]) & EHCI_ITD_ACTIVE)
			break;
	}

	if (i == 8) {
		goto done;	/* All 8 descriptors inactive, it's done */
	}

	/*
	 * Step 2, check for errors in status bits, throughout chain...
	 */

	DPRINTFN(12, ("ehci_check_itd_intr: active ex=%p\n", ex));

	for (itd = ex->itdstart; itd != ex->itdend; itd = itd->xfer_next) {
		for (i = 0; i < 8; i++) {
			if (le32toh(itd->itd.itd_ctl[i]) & (EHCI_ITD_BUF_ERR |
			    EHCI_ITD_BABBLE | EHCI_ITD_ERROR))
				break;
		}
		if (i != 8) { /* Error in one of the itds */
			goto done;
		}
	} /* itd search loop */

	DPRINTFN(12, ("ehci_check_itd_intr: ex %p itd %p still active\n", ex,
	    ex->itdstart));
	return;
done:
	DPRINTFN(12, ("ehci_check_itd_intr: ex=%p done\n", ex));
	callout_stop(&ex->xfer.timeout_handle);
	usb_rem_task(ex->xfer.pipe->device, &ex->abort_task);
	ehci_idone(ex);
}

void
ehci_idone(struct ehci_xfer *ex)
{
	usbd_xfer_handle xfer = &ex->xfer;
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	ehci_soft_qtd_t *sqtd, *lsqtd;
	u_int32_t status = 0, nstatus = 0;
	ehci_physaddr_t nextphys, altnextphys;
	int actlen, cerr;

	DPRINTFN(/*12*/2, ("ehci_idone: ex=%p\n", ex));
#ifdef DIAGNOSTIC
	{
		int s = splhigh();
		if (ex->isdone) {
			splx(s);
#ifdef EHCI_DEBUG
			printf("ehci_idone: ex is done!\n   ");
			ehci_dump_exfer(ex);
#else
			printf("ehci_idone: ex=%p is done!\n", ex);
#endif
			return;
		}
		ex->isdone = 1;
		splx(s);
	}
#endif

	if (xfer->status == USBD_CANCELLED ||
	    xfer->status == USBD_TIMEOUT) {
		DPRINTF(("ehci_idone: aborted xfer=%p\n", xfer));
		return;
	}

#ifdef EHCI_DEBUG
	DPRINTFN(/*10*/2, ("ehci_idone: xfer=%p, pipe=%p ready\n", xfer, epipe));
	if (ehcidebug > 10)
		ehci_dump_sqtds(ex->sqtdstart);
#endif

	/*
	 * Make sure that the QH overlay qTD does not reference any
	 * of the qTDs we are about to free. This is probably only
	 * necessary if the transfer is marked as HALTED.
	 */
	nextphys = EHCI_LINK_ADDR(le32toh(epipe->sqh->qh.qh_qtd.qtd_next));
	altnextphys =
	    EHCI_LINK_ADDR(le32toh(epipe->sqh->qh.qh_qtd.qtd_altnext));
	for (sqtd = ex->sqtdstart; sqtd != ex->sqtdend->nextqtd;
	     sqtd = sqtd->nextqtd) {
		if (sqtd->physaddr == nextphys) {
			epipe->sqh->qh.qh_qtd.qtd_next =
			    htole32(ex->sqtdend->nextqtd->physaddr);
			DPRINTFN(4, ("ehci_idone: updated overlay next ptr\n"));

		}
		if (sqtd->physaddr == altnextphys) {
			DPRINTFN(4,
			    ("ehci_idone: updated overlay altnext ptr\n"));
			epipe->sqh->qh.qh_qtd.qtd_altnext =
			    htole32(ex->sqtdend->nextqtd->physaddr);
		}
	}

	/* The transfer is done, compute actual length and status. */
	if (UE_GET_XFERTYPE(xfer->pipe->endpoint->edesc->bmAttributes)
	    == UE_ISOCHRONOUS) {
		/* Isoc transfer */
		struct ehci_soft_itd *itd;
		int i, nframes, len, uframes;

		nframes = 0;
		actlen = 0;

		switch (xfer->pipe->endpoint->edesc->bInterval) {
		case 0:
			panic("ehci: isoc xfer suddenly has 0 bInterval, "
			    "invalid\n");
		case 1: 
			uframes = 1;
			break;
		case 2: 
			uframes = 2; 
			break;
		case 3: 
			uframes = 4; 
			break;
		default: 
			uframes = 8; 
			break;
		}

		for (itd = ex->itdstart; itd != NULL; itd = itd->xfer_next) {
			for (i = 0; i < 8; i += uframes) {
				/* XXX - driver didn't fill in the frame full
				 *   of uframes. This leads to scheduling
				 *   inefficiencies, but working around
				 *   this doubles complexity of tracking
				 *   an xfer.
				 */
				if (nframes >= xfer->nframes)
					break;

				status = le32toh(itd->itd.itd_ctl[i]);
				len = EHCI_ITD_GET_LEN(status);
				xfer->frlengths[nframes++] = len;
				actlen += len;
			}
			if (nframes >= xfer->nframes)
				break;
		}
		xfer->actlen = actlen;
		xfer->status = USBD_NORMAL_COMPLETION;

		goto end;
	}

	/* Continue processing xfers using queue heads */

	lsqtd = ex->sqtdend;
	actlen = 0;
	for (sqtd = ex->sqtdstart; sqtd != lsqtd->nextqtd; 
	    sqtd =sqtd->nextqtd) {
		nstatus = le32toh(sqtd->qtd.qtd_status);
		if (nstatus & EHCI_QTD_ACTIVE)
			break;

		status = nstatus;
		/* halt is ok if descriptor is last, and complete */
		if (sqtd == lsqtd && EHCI_QTD_GET_BYTES(status) == 0)
			status &= ~EHCI_QTD_HALTED;
		if (EHCI_QTD_GET_PID(status) !=	EHCI_QTD_PID_SETUP)
			actlen += sqtd->len - EHCI_QTD_GET_BYTES(status);
	}

	cerr = EHCI_QTD_GET_CERR(status);
	DPRINTFN(/*10*/2, ("ehci_idone: len=%d, actlen=%d, cerr=%d, "
	    "status=0x%x\n", xfer->length, actlen, cerr, status));
	xfer->actlen = actlen;
	if ((status & EHCI_QTD_HALTED) != 0) {
#ifdef EHCI_DEBUG
		char sbuf[128];

		bitmask_snprintf((u_int32_t)status,
		    "\20\7HALTED\6BUFERR\5BABBLE\4XACTERR"
		    "\3MISSED\2SPLIT\1PING", sbuf, sizeof(sbuf));

		DPRINTFN(2,
			 ("ehci_idone: error, addr=%d, endpt=0x%02x, "
			  "status 0x%s\n",
			  xfer->pipe->device->address,
			  xfer->pipe->endpoint->edesc->bEndpointAddress,
			  sbuf));
		if (ehcidebug > 2) {
			ehci_dump_sqh(epipe->sqh);
			ehci_dump_sqtds(ex->sqtdstart);
		}
#endif
		if ((status & EHCI_QTD_BABBLE) == 0 && cerr > 0)
			xfer->status = USBD_STALLED;
		else
			xfer->status = USBD_IOERROR; /* more info XXX */
	} else {
		xfer->status = USBD_NORMAL_COMPLETION;
	}
end:
	/* XXX transfer_complete memcpys out transfer data (for in endpoints)
	 * during this call, before methods->done is called: dma sync required
	 * beforehand?
	 */
	usb_transfer_complete(xfer);
	DPRINTFN(/*12*/2, ("ehci_idone: ex=%p done\n", ex));
}

/*
 * Wait here until controller claims to have an interrupt.
 * Then call ehci_intr and return.  Use timeout to avoid waiting
 * too long.
 */
void
ehci_waitintr(ehci_softc_t *sc, usbd_xfer_handle xfer)
{
	int timo = xfer->timeout;
	int usecs;
	u_int32_t intrs;

	xfer->status = USBD_IN_PROGRESS;
	for (usecs = timo * 1000000 / hz; usecs > 0; usecs -= 1000) {
		usb_delay_ms(&sc->sc_bus, 1);
		if (sc->sc_dying)
			break;
		intrs = EHCI_STS_INTRS(EOREAD4(sc, EHCI_USBSTS)) &
			sc->sc_eintrs;
		DPRINTFN(15,("ehci_waitintr: 0x%04x\n", intrs));
#ifdef EHCI_DEBUG
		if (ehcidebug > 15)
			ehci_dump_regs(sc);
#endif
		if (intrs) {
			ehci_intr1(sc);
			if (xfer->status != USBD_IN_PROGRESS)
				return;
		}
	}

	/* Timeout */
	DPRINTF(("ehci_waitintr: timeout\n"));
	xfer->status = USBD_TIMEOUT;
	usb_transfer_complete(xfer);
	/* XXX should free TD */
}

void
ehci_poll(struct usbd_bus *bus)
{
	ehci_softc_t *sc = (ehci_softc_t *)bus;
#ifdef EHCI_DEBUG
	static int last;
	int new;
	new = EHCI_STS_INTRS(EOREAD4(sc, EHCI_USBSTS));
	if (new != last) {
		DPRINTFN(10,("ehci_poll: intrs=0x%04x\n", new));
		last = new;
	}
#endif

	if (EOREAD4(sc, EHCI_USBSTS) & sc->sc_eintrs)
		ehci_intr1(sc);
}

int
ehci_detach(struct ehci_softc *sc, int flags)
{
	int rv = 0;

	sc->sc_dying = 1;

	EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);
	(void) ehci_hcreset(sc);
	callout_stop(&sc->sc_tmo_intrlist);

#if defined(__NetBSD__) || defined(__OpenBSD__)
	if (sc->sc_powerhook != NULL)
		powerhook_disestablish(sc->sc_powerhook);
	if (sc->sc_shutdownhook != NULL)
		shutdownhook_disestablish(sc->sc_shutdownhook);
#endif
	usb_delay_ms(&sc->sc_bus, 300); /* XXX let stray task complete */

	usb_freemem(&sc->sc_bus, &sc->sc_fldma);
	/* XXX free other data structures XXX */

	return (rv);
}

/*
 * Handle suspend/resume.
 *
 * We need to switch to polling mode here, because this routine is
 * called from an interrupt context.  This is all right since we
 * are almost suspended anyway.
 */
void
ehci_power(int why, void *v)
{
	ehci_softc_t *sc = v;
	u_int32_t cmd, hcr;
	int s, i;

#ifdef EHCI_DEBUG
	DPRINTF(("ehci_power: sc=%p, why=%d\n", sc, why));
	if (ehcidebug > 0)
		ehci_dump_regs(sc);
#endif

	s = splhardusb();
	switch (why) {
	case PWR_SUSPEND:
	case PWR_STANDBY:
		sc->sc_bus.use_polling++;

		for (i = 1; i <= sc->sc_noport; i++) {
			cmd = EOREAD4(sc, EHCI_PORTSC(i));
			if ((cmd & EHCI_PS_PO) == 0 &&
			    (cmd & EHCI_PS_PE) == EHCI_PS_PE)
				EOWRITE4(sc, EHCI_PORTSC(i),
				    cmd | EHCI_PS_SUSP);
		}

		sc->sc_cmd = EOREAD4(sc, EHCI_USBCMD);

		cmd = sc->sc_cmd & ~(EHCI_CMD_ASE | EHCI_CMD_PSE);
		EOWRITE4(sc, EHCI_USBCMD, cmd);

		for (i = 0; i < 100; i++) {
			hcr = EOREAD4(sc, EHCI_USBSTS) &
			    (EHCI_STS_ASS | EHCI_STS_PSS);
			if (hcr == 0)
				break;

			usb_delay_ms(&sc->sc_bus, 1);
		}
		if (hcr != 0) {
			printf("%s: reset timeout\n",
			    device_get_nameunit(sc->sc_bus.bdev));
		}

		cmd &= ~EHCI_CMD_RS;
		EOWRITE4(sc, EHCI_USBCMD, cmd);

		for (i = 0; i < 100; i++) {
			hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
			if (hcr == EHCI_STS_HCH)
				break;

			usb_delay_ms(&sc->sc_bus, 1);
		}
		if (hcr != EHCI_STS_HCH) {
			printf("%s: config timeout\n",
			    device_get_nameunit(sc->sc_bus.bdev));
		}

		sc->sc_bus.use_polling--;
		break;

	case PWR_RESUME:
		sc->sc_bus.use_polling++;

		/* restore things in case the bios sucks */
		EOWRITE4(sc, EHCI_CTRLDSSEGMENT, 0);
		EOWRITE4(sc, EHCI_PERIODICLISTBASE, DMAADDR(&sc->sc_fldma, 0));
		EOWRITE4(sc, EHCI_ASYNCLISTADDR,
		    sc->sc_async_head->physaddr | EHCI_LINK_QH);
		EOWRITE4(sc, EHCI_USBINTR, sc->sc_eintrs);

		hcr = 0;
		for (i = 1; i <= sc->sc_noport; i++) {
			cmd = EOREAD4(sc, EHCI_PORTSC(i));
			if ((cmd & EHCI_PS_PO) == 0 &&
			    (cmd & EHCI_PS_SUSP) == EHCI_PS_SUSP) {
				EOWRITE4(sc, EHCI_PORTSC(i),
				    cmd | EHCI_PS_FPR);
				hcr = 1;
			}
		}

		if (hcr) {
			usb_delay_ms(&sc->sc_bus, USB_RESUME_WAIT);

			for (i = 1; i <= sc->sc_noport; i++) {
				cmd = EOREAD4(sc, EHCI_PORTSC(i));
				if ((cmd & EHCI_PS_PO) == 0 &&
				    (cmd & EHCI_PS_SUSP) == EHCI_PS_SUSP)
					EOWRITE4(sc, EHCI_PORTSC(i),
					    cmd & ~EHCI_PS_FPR);
			}
		}

		EOWRITE4(sc, EHCI_USBCMD, sc->sc_cmd);

		for (i = 0; i < 100; i++) {
			hcr = EOREAD4(sc, EHCI_USBSTS) & EHCI_STS_HCH;
			if (hcr != EHCI_STS_HCH)
				break;

			usb_delay_ms(&sc->sc_bus, 1);
		}
		if (hcr == EHCI_STS_HCH) {
			printf("%s: config timeout\n",
			    device_get_nameunit(sc->sc_bus.bdev));
		}

		usb_delay_ms(&sc->sc_bus, USB_RESUME_WAIT);

		sc->sc_bus.use_polling--;
		break;
	case PWR_SOFTSUSPEND:
	case PWR_SOFTSTANDBY:
	case PWR_SOFTRESUME:
		break;
	}
	splx(s);

#ifdef EHCI_DEBUG
	DPRINTF(("ehci_power: sc=%p\n", sc));
	if (ehcidebug > 0)
		ehci_dump_regs(sc);
#endif
}

/*
 * Shut down the controller when the system is going down.
 */
void
ehci_shutdown(void *v)
{
	ehci_softc_t *sc = v;

	DPRINTF(("ehci_shutdown: stopping the HC\n"));
	(void) ehci_hcreset(sc);
}

usbd_status
ehci_allocm(struct usbd_bus *bus, usb_dma_t *dma, u_int32_t size)
{
	usbd_status err;

	err = usb_allocmem(bus, size, 0, dma);
#ifdef EHCI_DEBUG
	if (err)
		printf("ehci_allocm: usb_allocmem()=%d\n", err);
#endif
	return (err);
}

void
ehci_freem(struct usbd_bus *bus, usb_dma_t *dma)
{
	usb_freemem(bus, dma);
}

usbd_xfer_handle
ehci_allocx(struct usbd_bus *bus)
{
	struct ehci_softc *sc = (struct ehci_softc *)bus;
	usbd_xfer_handle xfer;

	xfer = STAILQ_FIRST(&sc->sc_free_xfers);
	if (xfer != NULL) {
		STAILQ_REMOVE_HEAD(&sc->sc_free_xfers, next);
#ifdef DIAGNOSTIC
		if (xfer->busy_free != XFER_FREE) {
			printf("ehci_allocx: xfer=%p not free, 0x%08x\n", xfer,
			       xfer->busy_free);
		}
#endif
	} else {
		xfer = malloc(sizeof(struct ehci_xfer), M_USB, M_NOWAIT);
	}
	if (xfer != NULL) {
		memset(xfer, 0, sizeof(struct ehci_xfer));
		usb_init_task(&EXFER(xfer)->abort_task, ehci_timeout_task,
		    xfer);
		EXFER(xfer)->ehci_xfer_flags = 0;
#ifdef DIAGNOSTIC
		EXFER(xfer)->isdone = 1;
		xfer->busy_free = XFER_BUSY;
#endif
	}
	return (xfer);
}

void
ehci_freex(struct usbd_bus *bus, usbd_xfer_handle xfer)
{
	struct ehci_softc *sc = (struct ehci_softc *)bus;

#ifdef DIAGNOSTIC
	if (xfer->busy_free != XFER_BUSY) {
		printf("ehci_freex: xfer=%p not busy, 0x%08x\n", xfer,
		       xfer->busy_free);
		return;
	}
	xfer->busy_free = XFER_FREE;
	if (!EXFER(xfer)->isdone) {
		printf("ehci_freex: !isdone\n");
		return;
	}
#endif
	STAILQ_INSERT_HEAD(&sc->sc_free_xfers, xfer, next);
}

static void
ehci_device_clear_toggle(usbd_pipe_handle pipe)
{
	struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;

	DPRINTF(("ehci_device_clear_toggle: epipe=%p status=0x%x\n",
		 epipe, epipe->sqh->qh.qh_qtd.qtd_status));
#ifdef USB_DEBUG
	if (ehcidebug)
		usbd_dump_pipe(pipe);
#endif
	KASSERT((epipe->sqh->qh.qh_qtd.qtd_status &
	    htole32(EHCI_QTD_ACTIVE)) == 0,
	    ("ehci_device_clear_toggle: queue active"));
	epipe->sqh->qh.qh_qtd.qtd_status &= htole32(~EHCI_QTD_TOGGLE_MASK);
}

static void
ehci_noop(usbd_pipe_handle pipe)
{
}

#ifdef EHCI_DEBUG
void
ehci_dump_regs(ehci_softc_t *sc)
{
	int i;
	printf("cmd=0x%08x, sts=0x%08x, ien=0x%08x\n",
	       EOREAD4(sc, EHCI_USBCMD),
	       EOREAD4(sc, EHCI_USBSTS),
	       EOREAD4(sc, EHCI_USBINTR));
	printf("frindex=0x%08x ctrdsegm=0x%08x periodic=0x%08x async=0x%08x\n",
	       EOREAD4(sc, EHCI_FRINDEX),
	       EOREAD4(sc, EHCI_CTRLDSSEGMENT),
	       EOREAD4(sc, EHCI_PERIODICLISTBASE),
	       EOREAD4(sc, EHCI_ASYNCLISTADDR));
	for (i = 1; i <= sc->sc_noport; i++)
		printf("port %d status=0x%08x\n", i,
		       EOREAD4(sc, EHCI_PORTSC(i)));
}

/*
 * Unused function - this is meant to be called from a kernel
 * debugger.
 */
void
ehci_dump()
{
	ehci_dump_regs(theehci);
}

void
ehci_dump_link(ehci_link_t link, int type)
{
	link = le32toh(link);
	printf("0x%08x", link);
	if (link & EHCI_LINK_TERMINATE)
		printf("<T>");
	else {
		printf("<");
		if (type) {
			switch (EHCI_LINK_TYPE(link)) {
			case EHCI_LINK_ITD: printf("ITD"); break;
			case EHCI_LINK_QH: printf("QH"); break;
			case EHCI_LINK_SITD: printf("SITD"); break;
			case EHCI_LINK_FSTN: printf("FSTN"); break;
			}
		}
		printf(">");
	}
}

void
ehci_dump_sqtds(ehci_soft_qtd_t *sqtd)
{
	int i;
	u_int32_t stop;

	stop = 0;
	for (i = 0; sqtd && i < 20 && !stop; sqtd = sqtd->nextqtd, i++) {
		ehci_dump_sqtd(sqtd);
		stop = sqtd->qtd.qtd_next & htole32(EHCI_LINK_TERMINATE);
	}
	if (sqtd)
		printf("dump aborted, too many TDs\n");
}

void
ehci_dump_sqtd(ehci_soft_qtd_t *sqtd)
{
	printf("QTD(%p) at 0x%08x:\n", sqtd, sqtd->physaddr);
	ehci_dump_qtd(&sqtd->qtd);
}

void
ehci_dump_qtd(ehci_qtd_t *qtd)
{
	u_int32_t s;
	char sbuf[128];

	printf("  next="); ehci_dump_link(qtd->qtd_next, 0);
	printf(" altnext="); ehci_dump_link(qtd->qtd_altnext, 0);
	printf("\n");
	s = le32toh(qtd->qtd_status);
	bitmask_snprintf(EHCI_QTD_GET_STATUS(s),
			 "\20\10ACTIVE\7HALTED\6BUFERR\5BABBLE\4XACTERR"
			 "\3MISSED\2SPLIT\1PING", sbuf, sizeof(sbuf));
	printf("  status=0x%08x: toggle=%d bytes=0x%x ioc=%d c_page=0x%x\n",
	       s, EHCI_QTD_GET_TOGGLE(s), EHCI_QTD_GET_BYTES(s),
	       EHCI_QTD_GET_IOC(s), EHCI_QTD_GET_C_PAGE(s));
	printf("    cerr=%d pid=%d stat=0x%s\n", EHCI_QTD_GET_CERR(s),
	       EHCI_QTD_GET_PID(s), sbuf);
	for (s = 0; s < 5; s++)
		printf("  buffer[%d]=0x%08x\n", s, le32toh(qtd->qtd_buffer[s]));
}

void
ehci_dump_sqh(ehci_soft_qh_t *sqh)
{
	ehci_qh_t *qh = &sqh->qh;
	u_int32_t endp, endphub;

	printf("QH(%p) at 0x%08x:\n", sqh, sqh->physaddr);
	printf("  sqtd=%p inactivesqtd=%p\n", sqh->sqtd, sqh->inactivesqtd);
	printf("  link="); ehci_dump_link(qh->qh_link, 1); printf("\n");
	endp = le32toh(qh->qh_endp);
	printf("  endp=0x%08x\n", endp);
	printf("    addr=0x%02x inact=%d endpt=%d eps=%d dtc=%d hrecl=%d\n",
	       EHCI_QH_GET_ADDR(endp), EHCI_QH_GET_INACT(endp),
	       EHCI_QH_GET_ENDPT(endp),  EHCI_QH_GET_EPS(endp),
	       EHCI_QH_GET_DTC(endp), EHCI_QH_GET_HRECL(endp));
	printf("    mpl=0x%x ctl=%d nrl=%d\n",
	       EHCI_QH_GET_MPL(endp), EHCI_QH_GET_CTL(endp),
	       EHCI_QH_GET_NRL(endp));
	endphub = le32toh(qh->qh_endphub);
	printf("  endphub=0x%08x\n", endphub);
	printf("    smask=0x%02x cmask=0x%02x huba=0x%02x port=%d mult=%d\n",
	       EHCI_QH_GET_SMASK(endphub), EHCI_QH_GET_CMASK(endphub),
	       EHCI_QH_GET_HUBA(endphub), EHCI_QH_GET_PORT(endphub),
	       EHCI_QH_GET_MULT(endphub));
	printf("  curqtd="); ehci_dump_link(qh->qh_curqtd, 0); printf("\n");
	printf("Overlay qTD:\n");
	ehci_dump_qtd(&qh->qh_qtd);
}

#ifdef notyet
void
ehci_dump_itd(struct ehci_soft_itd *itd)
{
	ehci_isoc_trans_t t;
	ehci_isoc_bufr_ptr_t b, b2, b3;
	int i;

	printf("ITD: next phys=%X\n", itd->itd.itd_next);

	for (i = 0; i < 8;i++) {
		t = le32toh(itd->itd.itd_ctl[i]);
		printf("ITDctl %d: stat=%X len=%X ioc=%X pg=%X offs=%X\n", i,
		    EHCI_ITD_GET_STATUS(t), EHCI_ITD_GET_LEN(t),
		    EHCI_ITD_GET_IOC(t), EHCI_ITD_GET_PG(t),
		    EHCI_ITD_GET_OFFS(t));
	}
	printf("ITDbufr: ");
	for (i = 0; i < 7; i++)
		printf("%X,", EHCI_ITD_GET_BPTR(le32toh(itd->itd.itd_bufr[i])));

	b = le32toh(itd->itd.itd_bufr[0]);
	b2 = le32toh(itd->itd.itd_bufr[1]);
	b3 = le32toh(itd->itd.itd_bufr[2]);
	printf("\nep=%X daddr=%X dir=%d maxpkt=%X multi=%X\n",
	    EHCI_ITD_GET_EP(b), EHCI_ITD_GET_DADDR(b), EHCI_ITD_GET_DIR(b2),
	    EHCI_ITD_GET_MAXPKT(b2), EHCI_ITD_GET_MULTI(b3));
}

void
ehci_dump_sitd(struct ehci_soft_itd *itd)
{
	printf("SITD %p next=%p prev=%p xfernext=%p physaddr=%X slot=%d\n",
	    itd, itd->u.frame_list.next, itd->u.frame_list.prev,
	    itd->xfer_next, itd->physaddr, itd->slot);
}
#endif

#ifdef DIAGNOSTIC
void
ehci_dump_exfer(struct ehci_xfer *ex)
{
	printf("ehci_dump_exfer: ex=%p sqtdstart=%p end=%p itdstart=%p "
	    "end=%p isdone=%d\n", ex, ex->sqtdstart, ex->sqtdend, ex->itdstart,
	    ex->itdend, ex->isdone);
}
#endif
#endif

usbd_status
ehci_open(usbd_pipe_handle pipe)
{
	usbd_device_handle dev = pipe->device;
	ehci_softc_t *sc = (ehci_softc_t *)dev->bus;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	u_int8_t addr = dev->address;
	u_int8_t xfertype = ed->bmAttributes & UE_XFERTYPE;
	struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;
	ehci_soft_qh_t *sqh;
	usbd_status err;
	int s;
	int ival, speed, naks;
	int hshubaddr, hshubport;

	DPRINTFN(1, ("ehci_open: pipe=%p, addr=%d, endpt=%d (%d)\n",
		     pipe, addr, ed->bEndpointAddress, sc->sc_addr));

	if (dev->myhsport) {
		hshubaddr = dev->myhsport->parent->address;
		hshubport = dev->myhsport->portno;
	} else {
		hshubaddr = 0;
		hshubport = 0;
	}

	if (sc->sc_dying)
		return (USBD_IOERROR);

	if (addr == sc->sc_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &ehci_root_ctrl_methods;
			break;
		case UE_DIR_IN | EHCI_INTR_ENDPT:
			pipe->methods = &ehci_root_intr_methods;
			break;
		default:
			DPRINTF(("ehci_open: bad bEndpointAddress 0x%02x\n",
			    ed->bEndpointAddress));
			return (USBD_INVAL);
		}
		return (USBD_NORMAL_COMPLETION);
	}

	/* XXX All this stuff is only valid for async. */
	switch (dev->speed) {
	case USB_SPEED_LOW:  speed = EHCI_QH_SPEED_LOW;  break;
	case USB_SPEED_FULL: speed = EHCI_QH_SPEED_FULL; break;
	case USB_SPEED_HIGH: speed = EHCI_QH_SPEED_HIGH; break;
	default: panic("ehci_open: bad device speed %d", dev->speed);
	}
	if (speed != EHCI_QH_SPEED_HIGH && xfertype == UE_ISOCHRONOUS) {
		printf("%s: *** Error: opening low/full speed isoc device on"
		    "ehci, this does not work yet. Feel free to implement\n",
		    device_get_nameunit(sc->sc_bus.bdev));
		DPRINTFN(1,("ehci_open: hshubaddr=%d hshubport=%d\n",
			    hshubaddr, hshubport));
		return USBD_INVAL;
	}

	naks = 8;		/* XXX */
	/* Allocate sqh for everything, save isoc xfers */
	if (xfertype != UE_ISOCHRONOUS) {
		sqh = ehci_alloc_sqh(sc);
		if (sqh == NULL)
			goto bad0;
		/* qh_link filled when the QH is added */
		sqh->qh.qh_endp = htole32(
		    EHCI_QH_SET_ADDR(addr) |
		    EHCI_QH_SET_ENDPT(UE_GET_ADDR(ed->bEndpointAddress)) |
		    EHCI_QH_SET_EPS(speed) |
		    (xfertype == UE_CONTROL ? EHCI_QH_DTC : 0) |
		    EHCI_QH_SET_MPL(UGETW(ed->wMaxPacketSize)) |
		    (speed != EHCI_QH_SPEED_HIGH && xfertype == UE_CONTROL ?
		    EHCI_QH_CTL : 0) |
		    EHCI_QH_SET_NRL(naks)
		    );
		sqh->qh.qh_endphub = htole32(
		    EHCI_QH_SET_MULT(1) |
		    EHCI_QH_SET_HUBA(hshubaddr) |
		    EHCI_QH_SET_PORT(hshubport) |
		    EHCI_QH_SET_CMASK(0x1c) |
		    EHCI_QH_SET_SMASK(xfertype == UE_INTERRUPT ? 0x01 : 0)
		    );
		sqh->qh.qh_curqtd = EHCI_NULL;
		/* The overlay qTD was already set up by ehci_alloc_sqh(). */
		sqh->qh.qh_qtd.qtd_status =
	    	    htole32(EHCI_QTD_SET_TOGGLE(pipe->endpoint->savedtoggle));
		epipe->sqh = sqh;
	} else {
		sqh = NULL;
	}

	switch (xfertype) {
	case UE_CONTROL:
		err = usb_allocmem(&sc->sc_bus, sizeof(usb_device_request_t),
				   0, &epipe->u.ctl.reqdma);
#ifdef EHCI_DEBUG
		if (err)
			printf("ehci_open: usb_allocmem()=%d\n", err);
#endif
		if (err)
			goto bad1;
		pipe->methods = &ehci_device_ctrl_methods;
		s = splusb();
		ehci_add_qh(sqh, sc->sc_async_head);
		splx(s);
		break;
	case UE_BULK:
		pipe->methods = &ehci_device_bulk_methods;
		s = splusb();
		ehci_add_qh(sqh, sc->sc_async_head);
		splx(s);
		break;
	case UE_INTERRUPT:
		pipe->methods = &ehci_device_intr_methods;
		ival = pipe->interval;
		if (ival == USBD_DEFAULT_INTERVAL)
			ival = ed->bInterval;
		return (ehci_device_setintr(sc, sqh, ival));
	case UE_ISOCHRONOUS:
		pipe->methods = &ehci_device_isoc_methods;
		if (ed->bInterval == 0 || ed->bInterval > 16) {
			printf("ehci: opening pipe with invalid bInterval\n");
			err = USBD_INVAL;
			goto bad1;
		}
		if (UGETW(ed->wMaxPacketSize) == 0) {
			printf("ehci: zero length endpoint open request\n");
			err = USBD_INVAL;
			goto bad1;
		}
		epipe->u.isoc.next_frame = 0;
		epipe->u.isoc.cur_xfers = 0;
		break;
	default:
		DPRINTF(("ehci: bad xfer type %d\n", xfertype));
		return (USBD_INVAL);
	}
	return (USBD_NORMAL_COMPLETION);

 bad1:
	if (sqh != NULL)
		ehci_free_sqh(sc, sqh);
	return (err);
 bad0:
	return (USBD_NOMEM);
}

/*
 * Add an ED to the schedule.  Called at splusb().
 * If in the async schedule, it will always have a next.
 * If in the intr schedule it may not.
 */
void
ehci_add_qh(ehci_soft_qh_t *sqh, ehci_soft_qh_t *head)
{
	SPLUSBCHECK;

	sqh->next = head->next;
	sqh->prev = head;
	sqh->qh.qh_link = head->qh.qh_link;
	head->next = sqh;
	if (sqh->next)
		sqh->next->prev = sqh;
	head->qh.qh_link = htole32(sqh->physaddr | EHCI_LINK_QH);

#ifdef EHCI_DEBUG
	if (ehcidebug > 5) {
		printf("ehci_add_qh:\n");
		ehci_dump_sqh(sqh);
	}
#endif
}

/*
 * Remove an ED from the schedule.  Called at splusb().
 * Will always have a 'next' if it's in the async list as it's circular.
 */
void
ehci_rem_qh(ehci_softc_t *sc, ehci_soft_qh_t *sqh, ehci_soft_qh_t *head)
{
	SPLUSBCHECK;
	/* XXX */
	sqh->prev->qh.qh_link = sqh->qh.qh_link;
	sqh->prev->next = sqh->next;
	if (sqh->next)
		sqh->next->prev = sqh->prev;
	ehci_sync_hc(sc);
}

/* Restart a QH following the addition of a qTD. */
void
ehci_activate_qh(ehci_soft_qh_t *sqh, ehci_soft_qtd_t *sqtd)
{
	KASSERT((sqtd->qtd.qtd_status & htole32(EHCI_QTD_ACTIVE)) == 0,
	    ("ehci_activate_qh: already active"));

	/*
	 * When a QH is idle, the overlay qTD should be marked as not
	 * halted and not active. This causes the host controller to
	 * retrieve the real qTD on each pass (rather than just examinig
	 * the overlay), so it will notice when we activate the qTD.
	 */
	if (sqtd == sqh->sqtd) {
		/* Check that the hardware is in the state we expect. */
		if (EHCI_LINK_ADDR(le32toh(sqh->qh.qh_qtd.qtd_next)) !=
		    sqtd->physaddr) {
#ifdef EHCI_DEBUG
			printf("ehci_activate_qh: unexpected next ptr\n");
			ehci_dump_sqh(sqh);
			ehci_dump_sqtds(sqh->sqtd);
#endif
			sqh->qh.qh_qtd.qtd_next = htole32(sqtd->physaddr);
			sqh->qh.qh_qtd.qtd_altnext = EHCI_NULL;
		}
		/* Ensure the flags are correct. */
		sqh->qh.qh_qtd.qtd_status &= htole32(EHCI_QTD_PINGSTATE |
		    EHCI_QTD_TOGGLE_MASK);
	}

	/* Now activate the qTD. */
	sqtd->qtd.qtd_status |= htole32(EHCI_QTD_ACTIVE);
}

/*
 * Ensure that the HC has released all references to the QH.  We do this
 * by asking for a Async Advance Doorbell interrupt and then we wait for
 * the interrupt.
 * To make this easier we first obtain exclusive use of the doorbell.
 */
void
ehci_sync_hc(ehci_softc_t *sc)
{
	int s, error;

	if (sc->sc_dying) {
		DPRINTFN(2,("ehci_sync_hc: dying\n"));
		return;
	}
	DPRINTFN(2,("ehci_sync_hc: enter\n"));
	/* get doorbell */
	lockmgr(&sc->sc_doorbell_lock, LK_EXCLUSIVE, NULL);
	s = splhardusb();
	/* ask for doorbell */
	EOWRITE4(sc, EHCI_USBCMD, EOREAD4(sc, EHCI_USBCMD) | EHCI_CMD_IAAD);
	DPRINTFN(1,("ehci_sync_hc: cmd=0x%08x sts=0x%08x\n",
		    EOREAD4(sc, EHCI_USBCMD), EOREAD4(sc, EHCI_USBSTS)));
	error = tsleep(&sc->sc_async_head, PZERO, "ehcidi", hz); /* bell wait */
	DPRINTFN(1,("ehci_sync_hc: cmd=0x%08x sts=0x%08x\n",
		    EOREAD4(sc, EHCI_USBCMD), EOREAD4(sc, EHCI_USBSTS)));
	splx(s);
	/* release doorbell */
	lockmgr(&sc->sc_doorbell_lock, LK_RELEASE, NULL);
#ifdef DIAGNOSTIC
	if (error)
		printf("ehci_sync_hc: tsleep() = %d\n", error);
#endif
	DPRINTFN(2,("ehci_sync_hc: exit\n"));
}

/*Call at splusb*/
void
ehci_rem_free_itd_chain(ehci_softc_t *sc, struct ehci_xfer *exfer)
{
	struct ehci_soft_itd *itd, *prev;

	prev = NULL;

	if (exfer->itdstart == NULL || exfer->itdend == NULL)
		panic("ehci isoc xfer being freed, but with no itd chain\n");

	for (itd = exfer->itdstart; itd != NULL; itd = itd->xfer_next) {
		prev = itd->u.frame_list.prev;
		/* Unlink itd from hardware chain, or frame array */
		if (prev == NULL) { /* We're at the table head */
			sc->sc_softitds[itd->slot] = itd->u.frame_list.next;
			sc->sc_flist[itd->slot] = itd->itd.itd_next;

			if (itd->u.frame_list.next != NULL)
				itd->u.frame_list.next->u.frame_list.prev = 
				    NULL;
		} else {
			/* XXX this part is untested... */
			prev->itd.itd_next = itd->itd.itd_next;
			prev->u.frame_list.next = itd->u.frame_list.next;
			if (itd->u.frame_list.next != NULL)
				itd->u.frame_list.next->u.frame_list.prev = 
				    prev;
		}
	}

	prev = NULL;
	for (itd = exfer->itdstart; itd != NULL; itd = itd->xfer_next) {
		if (prev != NULL)
			ehci_free_itd(sc, prev);
		prev = itd;
	}
	if (prev)
		ehci_free_itd(sc, prev);
	exfer->itdstart = NULL;
	exfer->itdend = NULL;
}

/***********/

/*
 * Data structures and routines to emulate the root hub.
 */
static usb_device_descriptor_t ehci_devd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE,		/* type */
	{0x00, 0x02},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_HSHUBSTT,	/* protocol */
	64,			/* max packet */
	{0},{0},{0x00,0x01},	/* device id */
	1,2,0,			/* string indicies */
	1			/* # of configurations */
};

static usb_device_qualifier_t ehci_odevd = {
	USB_DEVICE_DESCRIPTOR_SIZE,
	UDESC_DEVICE_QUALIFIER,	/* type */
	{0x00, 0x02},		/* USB version */
	UDCLASS_HUB,		/* class */
	UDSUBCLASS_HUB,		/* subclass */
	UDPROTO_FSHUB,		/* protocol */
	64,			/* max packet */
	1,			/* # of configurations */
	0
};

static usb_config_descriptor_t ehci_confd = {
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

static usb_interface_descriptor_t ehci_ifcd = {
	USB_INTERFACE_DESCRIPTOR_SIZE,
	UDESC_INTERFACE,
	0,
	0,
	1,
	UICLASS_HUB,
	UISUBCLASS_HUB,
	UIPROTO_HSHUBSTT,
	0
};

static usb_endpoint_descriptor_t ehci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | EHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{8, 0},			/* max packet */
	255
};

static usb_hub_descriptor_t ehci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	0,
	{0,0},
	0,
	0,
	{0},
};

static int
ehci_str(usb_string_descriptor_t *p, int l, char *s)
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
static usbd_status
ehci_root_ctrl_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ehci_root_ctrl_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ehci_root_ctrl_start(usbd_xfer_handle xfer)
{
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	usb_device_request_t *req;
	void *buf = NULL;
	int port, i;
	int s, len, value, index, l, totlen = 0;
	usb_port_status_t ps;
	usb_hub_descriptor_t hubd;
	usbd_status err;
	u_int32_t v;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST))
		/* XXX panic */
		return (USBD_INVAL);
#endif
	req = &xfer->request;

	DPRINTFN(4,("ehci_root_ctrl_start: type=0x%02x request=%02x\n",
		    req->bmRequestType, req->bRequest));

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	if (len != 0)
		buf = xfer->buffer;

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
		DPRINTFN(8,("ehci_root_ctrl_start: wValue=0x%04x\n", value));
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			USETW(ehci_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &ehci_devd, l);
			break;
		/*
		 * We can't really operate at another speed, but the spec says
		 * we need this descriptor.
		 */
		case UDESC_DEVICE_QUALIFIER:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			memcpy(buf, &ehci_odevd, l);
			break;
		/*
		 * We can't really operate at another speed, but the spec says
		 * we need this descriptor.
		 */
		case UDESC_OTHER_SPEED_CONFIGURATION:
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &ehci_confd, l);
			((usb_config_descriptor_t *)buf)->bDescriptorType =
				value >> 8;
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ehci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ehci_endpd, l);
			break;
		case UDESC_STRING:
			if (len == 0)
				break;
			*(u_int8_t *)buf = 0;
			totlen = 1;
			switch (value & 0xff) {
			case 0: /* Language table */
				totlen = ehci_str(buf, len, "\001");
				break;
			case 1: /* Vendor */
				totlen = ehci_str(buf, len, sc->sc_vendor);
				break;
			case 2: /* Product */
				totlen = ehci_str(buf, len, "EHCI root hub");
				break;
			}
			break;
		default:
			err = USBD_IOERROR;
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
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_addr = value;
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			err = USBD_IOERROR;
			goto ret;
		}
		sc->sc_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_INTERFACE, UT_WRITE_INTERFACE):
		break;
	case C(UR_SYNCH_FRAME, UT_WRITE_ENDPOINT):
		break;
	/* Hub requests */
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_CLEAR_FEATURE, UT_WRITE_CLASS_OTHER):
		DPRINTFN(8, ("ehci_root_ctrl_start: UR_CLEAR_PORT_FEATURE "
			     "port=%d feature=%d\n",
			     index, value));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = EHCI_PORTSC(index);
		v = EOREAD4(sc, port) &~ EHCI_PS_CLEAR;
		switch(value) {
		case UHF_PORT_ENABLE:
			EOWRITE4(sc, port, v &~ EHCI_PS_PE);
			break;
		case UHF_PORT_SUSPEND:
			EOWRITE4(sc, port, v &~ EHCI_PS_SUSP);
			break;
		case UHF_PORT_POWER:
			EOWRITE4(sc, port, v &~ EHCI_PS_PP);
			break;
		case UHF_PORT_TEST:
			DPRINTFN(2,("ehci_root_ctrl_start: clear port test "
				    "%d\n", index));
			break;
		case UHF_PORT_INDICATOR:
			DPRINTFN(2,("ehci_root_ctrl_start: clear port ind "
				    "%d\n", index));
			EOWRITE4(sc, port, v &~ EHCI_PS_PIC);
			break;
		case UHF_C_PORT_CONNECTION:
			EOWRITE4(sc, port, v | EHCI_PS_CSC);
			break;
		case UHF_C_PORT_ENABLE:
			EOWRITE4(sc, port, v | EHCI_PS_PEC);
			break;
		case UHF_C_PORT_SUSPEND:
			/* how? */
			break;
		case UHF_C_PORT_OVER_CURRENT:
			EOWRITE4(sc, port, v | EHCI_PS_OCC);
			break;
		case UHF_C_PORT_RESET:
			sc->sc_isreset = 0;
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if ((value & 0xff) != 0) {
			err = USBD_IOERROR;
			goto ret;
		}
		hubd = ehci_hubd;
		hubd.bNbrPorts = sc->sc_noport;
		v = EOREAD4(sc, EHCI_HCSPARAMS);
		USETW(hubd.wHubCharacteristics,
		    EHCI_HCS_PPC(v) ? UHD_PWR_INDIVIDUAL : UHD_PWR_NO_SWITCH |
		    EHCI_HCS_P_INDICATOR(EREAD4(sc, EHCI_HCSPARAMS))
		        ? UHD_PORT_IND : 0);
		hubd.bPwrOn2PwrGood = 200; /* XXX can't find out? */
		for (i = 0, l = sc->sc_noport; l > 0; i++, l -= 8, v >>= 8)
			hubd.DeviceRemovable[i++] = 0; /* XXX can't find out? */
		hubd.bDescLength = USB_HUB_DESCRIPTOR_SIZE + i;
		l = min(len, hubd.bDescLength);
		totlen = l;
		memcpy(buf, &hubd, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len); /* ? XXX */
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		DPRINTFN(8,("ehci_root_ctrl_start: get port status i=%d\n",
			    index));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = EOREAD4(sc, EHCI_PORTSC(index));
		DPRINTFN(8,("ehci_root_ctrl_start: port status=0x%04x\n",
			    v));

		i = UPS_HIGH_SPEED;

		if (sc->sc_flags & EHCI_SCFLG_FORCESPEED) {
			if ((v & 0xc000000) == 0x8000000)
				i = UPS_HIGH_SPEED;
			else if ((v & 0xc000000) == 0x4000000)
				i = UPS_LOW_SPEED;
			else
				i = 0;
		}

		if (v & EHCI_PS_CS)	i |= UPS_CURRENT_CONNECT_STATUS;
		if (v & EHCI_PS_PE)	i |= UPS_PORT_ENABLED;
		if (v & EHCI_PS_SUSP)	i |= UPS_SUSPEND;
		if (v & EHCI_PS_OCA)	i |= UPS_OVERCURRENT_INDICATOR;
		if (v & EHCI_PS_PR)	i |= UPS_RESET;
		if (v & EHCI_PS_PP)	i |= UPS_PORT_POWER;
		USETW(ps.wPortStatus, i);
		i = 0;
		if (v & EHCI_PS_CSC)	i |= UPS_C_CONNECT_STATUS;
		if (v & EHCI_PS_PEC)	i |= UPS_C_PORT_ENABLED;
		if (v & EHCI_PS_OCC)	i |= UPS_C_OVERCURRENT_INDICATOR;
		if (sc->sc_isreset)	i |= UPS_C_PORT_RESET;
		USETW(ps.wPortChange, i);
		l = min(len, sizeof ps);
		memcpy(buf, &ps, l);
		totlen = l;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_CLASS_DEVICE):
		err = USBD_IOERROR;
		goto ret;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = EHCI_PORTSC(index);
		v = EOREAD4(sc, port) &~ EHCI_PS_CLEAR;
		switch(value) {
		case UHF_PORT_ENABLE:
			EOWRITE4(sc, port, v | EHCI_PS_PE);
			break;
		case UHF_PORT_SUSPEND:
			EOWRITE4(sc, port, v | EHCI_PS_SUSP);
			break;
		case UHF_PORT_RESET:
			DPRINTFN(5,("ehci_root_ctrl_start: reset port %d\n",
				    index));
			if (EHCI_PS_IS_LOWSPEED(v)) {
				/* Low speed device, give up ownership. */
				ehci_disown(sc, index, 1);
				break;
			}
			/* Start reset sequence. */
			v &= ~ (EHCI_PS_PE | EHCI_PS_PR);
			EOWRITE4(sc, port, v | EHCI_PS_PR);
			/* Wait for reset to complete. */
			usb_delay_ms(&sc->sc_bus, USB_PORT_ROOT_RESET_DELAY);
			if (sc->sc_dying) {
				err = USBD_IOERROR;
				goto ret;
			}
			/* Terminate reset sequence. */
			if (sc->sc_flags & EHCI_SCFLG_NORESTERM)
				;
			else
				EOWRITE4(sc, port, v);

			/* Wait for HC to complete reset. */
			usb_delay_ms(&sc->sc_bus, EHCI_PORT_RESET_COMPLETE);
			if (sc->sc_dying) {
				err = USBD_IOERROR;
				goto ret;
			}
			v = EOREAD4(sc, port);
			DPRINTF(("ehci after reset, status=0x%08x\n", v));
			if (v & EHCI_PS_PR) {
				printf("%s: port reset timeout\n",
				       device_get_nameunit(sc->sc_bus.bdev));
				return (USBD_TIMEOUT);
			}
			if (!(v & EHCI_PS_PE)) {
				/* Not a high speed device, give up ownership.*/
				ehci_disown(sc, index, 0);
				break;
			}
			sc->sc_isreset = 1;
			DPRINTF(("ehci port %d reset, status = 0x%08x\n",
				 index, v));
			break;
		case UHF_PORT_POWER:
			DPRINTFN(2,("ehci_root_ctrl_start: set port power "
				    "%d\n", index));
			EOWRITE4(sc, port, v | EHCI_PS_PP);
			break;
		case UHF_PORT_TEST:
			DPRINTFN(2,("ehci_root_ctrl_start: set port test "
				    "%d\n", index));
			break;
		case UHF_PORT_INDICATOR:
			DPRINTFN(2,("ehci_root_ctrl_start: set port ind "
				    "%d\n", index));
			EOWRITE4(sc, port, v | EHCI_PS_PIC);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		break;
	case C(UR_CLEAR_TT_BUFFER, UT_WRITE_CLASS_OTHER):
	case C(UR_RESET_TT, UT_WRITE_CLASS_OTHER):
	case C(UR_GET_TT_STATE, UT_READ_CLASS_OTHER):
	case C(UR_STOP_TT, UT_WRITE_CLASS_OTHER):
		break;
	default:
		err = USBD_IOERROR;
		goto ret;
	}
	xfer->actlen = totlen;
	err = USBD_NORMAL_COMPLETION;
 ret:
	xfer->status = err;
	s = splusb();
	hacksync(xfer);	/* XXX to compensate for usb_transfer_complete */
	usb_transfer_complete(xfer);
	splx(s);
	return (USBD_IN_PROGRESS);
}

void
ehci_disown(ehci_softc_t *sc, int index, int lowspeed)
{
	int port;
	u_int32_t v;

	DPRINTF(("ehci_disown: index=%d lowspeed=%d\n", index, lowspeed));
#ifdef DIAGNOSTIC
	if (sc->sc_npcomp != 0) {
		int i = (index-1) / sc->sc_npcomp;
		if (i >= sc->sc_ncomp)
			printf("%s: strange port\n",
			       device_get_nameunit(sc->sc_bus.bdev));
		else
			printf("%s: handing over %s speed device on "
			       "port %d to %s\n",
			       device_get_nameunit(sc->sc_bus.bdev),
			       lowspeed ? "low" : "full",
			       index, device_get_nameunit(sc->sc_comps[i]->bdev));
	} else {
		printf("%s: npcomp == 0\n", device_get_nameunit(sc->sc_bus.bdev));
	}
#endif
	port = EHCI_PORTSC(index);
	v = EOREAD4(sc, port) &~ EHCI_PS_CLEAR;
	EOWRITE4(sc, port, v | EHCI_PS_PO);
}

/* Abort a root control request. */
static void
ehci_root_ctrl_abort(usbd_xfer_handle xfer)
{
	/* Nothing to do, all transfers are synchronous. */
}

/* Close the root pipe. */
static void
ehci_root_ctrl_close(usbd_pipe_handle pipe)
{
	DPRINTF(("ehci_root_ctrl_close\n"));
	/* Nothing to do. */
}

void
ehci_root_intr_done(usbd_xfer_handle xfer)
{
}

static usbd_status
ehci_root_intr_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ehci_root_intr_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ehci_root_intr_start(usbd_xfer_handle xfer)
{
	usbd_pipe_handle pipe = xfer->pipe;
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;

	if (sc->sc_dying)
		return (USBD_IOERROR);

	sc->sc_intrxfer = xfer;

	return (USBD_IN_PROGRESS);
}

/* Abort a root interrupt request. */
static void
ehci_root_intr_abort(usbd_xfer_handle xfer)
{
	int s;

	if (xfer->pipe->intrxfer == xfer) {
		DPRINTF(("ehci_root_intr_abort: remove\n"));
		xfer->pipe->intrxfer = NULL;
	}
	xfer->status = USBD_CANCELLED;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
}

/* Close the root pipe. */
static void
ehci_root_intr_close(usbd_pipe_handle pipe)
{
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;

	DPRINTF(("ehci_root_intr_close\n"));

	sc->sc_intrxfer = NULL;
}

void
ehci_root_ctrl_done(usbd_xfer_handle xfer)
{
}

/************************/

ehci_soft_qh_t *
ehci_alloc_sqh(ehci_softc_t *sc)
{
	ehci_soft_qh_t *sqh;
	ehci_soft_qtd_t *sqtd;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;

	if (sc->sc_freeqhs == NULL) {
		DPRINTFN(2, ("ehci_alloc_sqh: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, EHCI_SQH_SIZE * EHCI_SQH_CHUNK,
			  EHCI_PAGE_SIZE, &dma);
#ifdef EHCI_DEBUG
		if (err)
			printf("ehci_alloc_sqh: usb_allocmem()=%d\n", err);
#endif
		if (err)
			return (NULL);
		for(i = 0; i < EHCI_SQH_CHUNK; i++) {
			offs = i * EHCI_SQH_SIZE;
			sqh = KERNADDR(&dma, offs);
			sqh->physaddr = DMAADDR(&dma, offs);
			sqh->next = sc->sc_freeqhs;
			sc->sc_freeqhs = sqh;
		}
	}
	/* Allocate the initial inactive sqtd. */
	sqtd = ehci_alloc_sqtd(sc);
	if (sqtd == NULL)
		return (NULL);
	sqtd->qtd.qtd_status = htole32(0);
	sqtd->qtd.qtd_next = EHCI_NULL;
	sqtd->qtd.qtd_altnext = EHCI_NULL;

	sqh = sc->sc_freeqhs;
	sc->sc_freeqhs = sqh->next;

	/* The overlay QTD should begin zeroed. */
	sqh->qh.qh_qtd.qtd_next = htole32(sqtd->physaddr);
	sqh->qh.qh_qtd.qtd_altnext = EHCI_NULL;
	sqh->qh.qh_qtd.qtd_status = 0;
	for (i = 0; i < EHCI_QTD_NBUFFERS; i++) {
		sqh->qh.qh_qtd.qtd_buffer[i] = 0;
		sqh->qh.qh_qtd.qtd_buffer_hi[i] = 0;
	}
	sqh->next = NULL;
	sqh->prev = NULL;
	sqh->sqtd = sqtd;
	sqh->inactivesqtd = sqtd;
	return (sqh);
}

void
ehci_free_sqh(ehci_softc_t *sc, ehci_soft_qh_t *sqh)
{
	ehci_free_sqtd(sc, sqh->inactivesqtd);
	sqh->next = sc->sc_freeqhs;
	sc->sc_freeqhs = sqh;
}

ehci_soft_qtd_t *
ehci_alloc_sqtd(ehci_softc_t *sc)
{
	ehci_soft_qtd_t *sqtd;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;
	int s;

	if (sc->sc_freeqtds == NULL) {
		DPRINTFN(2, ("ehci_alloc_sqtd: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, EHCI_SQTD_SIZE*EHCI_SQTD_CHUNK,
			  EHCI_PAGE_SIZE, &dma);
#ifdef EHCI_DEBUG
		if (err)
			printf("ehci_alloc_sqtd: usb_allocmem()=%d\n", err);
#endif
		if (err)
			return (NULL);
		s = splusb();
		for(i = 0; i < EHCI_SQTD_CHUNK; i++) {
			offs = i * EHCI_SQTD_SIZE;
			sqtd = KERNADDR(&dma, offs);
			sqtd->physaddr = DMAADDR(&dma, offs);
			sqtd->nextqtd = sc->sc_freeqtds;
			sc->sc_freeqtds = sqtd;
		}
		splx(s);
	}

	s = splusb();
	sqtd = sc->sc_freeqtds;
	sc->sc_freeqtds = sqtd->nextqtd;
	sqtd->qtd.qtd_next = EHCI_NULL;
	sqtd->qtd.qtd_altnext = EHCI_NULL;
	sqtd->qtd.qtd_status = 0;
	for (i = 0; i < EHCI_QTD_NBUFFERS; i++) {
		sqtd->qtd.qtd_buffer[i] = 0;
		sqtd->qtd.qtd_buffer_hi[i] = 0;
	}
	sqtd->nextqtd = NULL;
	sqtd->xfer = NULL;
	splx(s);

	return (sqtd);
}

void
ehci_free_sqtd(ehci_softc_t *sc, ehci_soft_qtd_t *sqtd)
{
	int s;

	s = splusb();
	sqtd->nextqtd = sc->sc_freeqtds;
	sc->sc_freeqtds = sqtd;
	splx(s);
}

usbd_status
ehci_alloc_sqtd_chain(struct ehci_pipe *epipe, ehci_softc_t *sc,
     int alen, int rd, usbd_xfer_handle xfer, ehci_soft_qtd_t *start,
     ehci_soft_qtd_t *newinactive, ehci_soft_qtd_t **sp, ehci_soft_qtd_t **ep)
{
	ehci_soft_qtd_t *next, *cur;
	ehci_physaddr_t dataphys, nextphys;
	u_int32_t qtdstatus;
	int adj, len, curlen, mps, offset, pagelen, seg, segoff;
	int i, iscontrol, forceshort;
	struct usb_dma_mapping *dma = &xfer->dmamap;

	DPRINTFN(alen<4*4096,("ehci_alloc_sqtd_chain: start len=%d\n", alen));

	offset = 0;
	len = alen;
	iscontrol = (epipe->pipe.endpoint->edesc->bmAttributes & UE_XFERTYPE) ==
	    UE_CONTROL;
	qtdstatus = EHCI_QTD_ACTIVE |
	    EHCI_QTD_SET_PID(rd ? EHCI_QTD_PID_IN : EHCI_QTD_PID_OUT) |
	    EHCI_QTD_SET_CERR(3)
	    /* IOC set below */
	    /* BYTES set below */
	    ;
	mps = UGETW(epipe->pipe.endpoint->edesc->wMaxPacketSize);
	forceshort = ((xfer->flags & USBD_FORCE_SHORT_XFER) || len == 0) &&
	    len % mps == 0;
	/*
	 * The control transfer data stage always starts with a toggle of 1.
	 * For other transfers we let the hardware track the toggle state.
	 */
	if (iscontrol)
		qtdstatus |= EHCI_QTD_SET_TOGGLE(1);

	if (start != NULL) {
		/*
		 * If we are given a starting qTD, assume it is linked into
		 * an active QH so be careful not to mark it active.
		 */
		cur = start;
		*sp = cur;
		qtdstatus &= ~EHCI_QTD_ACTIVE;
	} else {
		cur = ehci_alloc_sqtd(sc);
		*sp = cur;
		if (cur == NULL)
			goto nomem;
	}
	seg = 0;
	segoff = 0;
	for (;;) {
		curlen = 0;

		/* The EHCI hardware can handle at most 5 pages. */
		for (i = 0; i < EHCI_QTD_NBUFFERS && curlen < len; i++) {
			KASSERT(seg < dma->nsegs,
			    ("ehci_alloc_sqtd_chain: overrun"));
			dataphys = dma->segs[seg].ds_addr + segoff;
			pagelen = dma->segs[seg].ds_len - segoff;
			if (pagelen > len - curlen)
				pagelen = len - curlen;
			if (pagelen > EHCI_PAGE_SIZE -
			    EHCI_PAGE_OFFSET(dataphys))
				pagelen = EHCI_PAGE_SIZE -
				    EHCI_PAGE_OFFSET(dataphys);
			segoff += pagelen;
			if (segoff >= dma->segs[seg].ds_len) {
				KASSERT(segoff == dma->segs[seg].ds_len,
				    ("ehci_alloc_sqtd_chain: overlap"));
				seg++;
				segoff = 0;
			}

			cur->qtd.qtd_buffer[i] = htole32(dataphys);
			cur->qtd.qtd_buffer_hi[i] = 0;
			curlen += pagelen;

			/*
			 * Must stop if there is any gap before or after
			 * the page boundary.
			 */
			if (EHCI_PAGE_OFFSET(dataphys + pagelen) != 0)
				break;
			if (seg < dma->nsegs && EHCI_PAGE_OFFSET(segoff +
			    dma->segs[seg].ds_addr) != 0)
				break;
		}
		/* Adjust down to a multiple of mps if not at the end. */
		if (curlen < len && curlen % mps != 0) {
			adj = curlen % mps;
			curlen -= adj;
			KASSERT(curlen > 0,
			    ("ehci_alloc_sqtd_chain: need to copy"));
			segoff -= adj;
			if (segoff < 0) {
				seg--;
				segoff += dma->segs[seg].ds_len;
			}
			KASSERT(seg >= 0 && segoff >= 0,
			    ("ehci_alloc_sqtd_chain: adjust to mps"));
		}

		len -= curlen;

		if (len != 0 || forceshort) {
			next = ehci_alloc_sqtd(sc);
			if (next == NULL)
				goto nomem;
			nextphys = htole32(next->physaddr);
		} else {
			next = NULL;
			nextphys = EHCI_NULL;
		}

		cur->nextqtd = next;
		cur->qtd.qtd_next = nextphys;
		/* Make sure to stop after a short transfer. */
		cur->qtd.qtd_altnext = htole32(newinactive->physaddr);
		cur->qtd.qtd_status =
		    htole32(qtdstatus | EHCI_QTD_SET_BYTES(curlen));
		cur->xfer = xfer;
		cur->len = curlen;
		DPRINTFN(10,("ehci_alloc_sqtd_chain: curlen=%d\n", curlen));
		if (iscontrol) {
			/*
			 * adjust the toggle based on the number of packets
			 * in this qtd
			 */
			if ((((curlen + mps - 1) / mps) & 1) || curlen == 0)
				qtdstatus ^= EHCI_QTD_TOGGLE_MASK;
		}
		qtdstatus |= EHCI_QTD_ACTIVE;
		if (len == 0) {
			if (!forceshort)
				break;
			forceshort = 0;
		}
		DPRINTFN(10,("ehci_alloc_sqtd_chain: extend chain\n"));
		offset += curlen;
		cur = next;
	}
	cur->qtd.qtd_status |= htole32(EHCI_QTD_IOC);
	*ep = cur;

	DPRINTFN(10,("ehci_alloc_sqtd_chain: return sqtd=%p sqtdend=%p\n",
		     *sp, *ep));

	return (USBD_NORMAL_COMPLETION);

 nomem:
	/* XXX free chain */
	DPRINTFN(-1,("ehci_alloc_sqtd_chain: no memory\n"));
	return (USBD_NOMEM);
}

/* Free the chain starting at sqtd and end at the qTD before sqtdend */
static void
ehci_free_sqtd_chain(ehci_softc_t *sc, ehci_soft_qh_t *sqh,
    ehci_soft_qtd_t *sqtd, ehci_soft_qtd_t *sqtdend)
{
	ehci_soft_qtd_t *p, **prevp;
	int i;

	DPRINTFN(10,("ehci_free_sqtd_chain: sqtd=%p sqtdend=%p\n",
		     sqtd, sqtdend));

	/* First unlink the chain from the QH's software qTD list. */
	prevp = &sqh->sqtd;
	for (p = sqh->sqtd; p != NULL; p = p->nextqtd) {
		if (p == sqtd) {
			*prevp = sqtdend;
			break;
		}
		prevp = &p->nextqtd;
	}
	KASSERT(p != NULL, ("ehci_free_sqtd_chain: chain not found"));
	for (i = 0; sqtd != sqtdend; sqtd = p, i++) {
		p = sqtd->nextqtd;
		ehci_free_sqtd(sc, sqtd);
	}
}

ehci_soft_itd_t *
ehci_alloc_itd(ehci_softc_t *sc)
{
	struct ehci_soft_itd *itd, *freeitd;
	usbd_status err;
	int i, s, offs, frindex, previndex;
	usb_dma_t dma;

	s = splusb();

	/* Find an itd that wasn't freed this frame or last frame. This can
	 * discard itds that were freed before frindex wrapped around
	 * XXX - can this lead to thrashing? Could fix by enabling wrap-around
	 *       interrupt and fiddling with list when that happens */
	frindex = (EOREAD4(sc, EHCI_FRINDEX) + 1) >> 3;
	previndex = (frindex != 0) ? frindex - 1 : sc->sc_flsize;

	freeitd = NULL;
	LIST_FOREACH(itd, &sc->sc_freeitds, u.free_list) {
		if (itd == NULL)
			break;
		if (itd->slot != frindex && itd->slot != previndex) {
			freeitd = itd;
			break;
		}
	}

	if (freeitd == NULL) {
		DPRINTFN(2, ("ehci_alloc_itd allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, EHCI_ITD_SIZE * EHCI_ITD_CHUNK,
		    EHCI_PAGE_SIZE, &dma);

		if (err) {
			DPRINTF(("ehci_alloc_itd, alloc returned %d\n", err));
			return NULL;
		}

		for (i = 0; i < EHCI_ITD_CHUNK; i++) {
			offs = i * EHCI_ITD_SIZE;
			itd = KERNADDR(&dma, offs);
			itd->physaddr = DMAADDR(&dma, offs);
	 		itd->dma = dma;
			itd->offs = offs;
			LIST_INSERT_HEAD(&sc->sc_freeitds, itd, u.free_list);
		}
		freeitd = LIST_FIRST(&sc->sc_freeitds);
	}

	itd = freeitd;
	LIST_REMOVE(itd, u.free_list);
	memset(&itd->itd, 0, sizeof(ehci_itd_t));
	itd->u.frame_list.next = NULL;
	itd->u.frame_list.prev = NULL;
	itd->xfer_next = NULL;
	itd->slot = 0;
	splx(s);

	return (itd);
}

void
ehci_free_itd(ehci_softc_t *sc, ehci_soft_itd_t *itd)
{
	int s;

	s = splusb();
	LIST_INSERT_AFTER(LIST_FIRST(&sc->sc_freeitds), itd, u.free_list);
	splx(s);
}

/****************/

/*
 * Close a reqular pipe.
 * Assumes that there are no pending transactions.
 */
void
ehci_close_pipe(usbd_pipe_handle pipe, ehci_soft_qh_t *head)
{
	struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;
	ehci_soft_qh_t *sqh = epipe->sqh;
	int s;

	s = splusb();
	ehci_rem_qh(sc, sqh, head);
	splx(s);
	pipe->endpoint->savedtoggle =
	    EHCI_QTD_GET_TOGGLE(le32toh(sqh->qh.qh_qtd.qtd_status));
	ehci_free_sqh(sc, epipe->sqh);
}

/*
 * Abort a device request.
 * If this routine is called at splusb() it guarantees that the request
 * will be removed from the hardware scheduling and that the callback
 * for it will be called with USBD_CANCELLED status.
 * It's impossible to guarantee that the requested transfer will not
 * have happened since the hardware runs concurrently.
 * If the transaction has already happened we rely on the ordinary
 * interrupt processing to process it.
 */
void
ehci_abort_xfer(usbd_xfer_handle xfer, usbd_status status)
{
#define exfer EXFER(xfer)
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	ehci_softc_t *sc = (ehci_softc_t *)epipe->pipe.device->bus;
	ehci_soft_qh_t *sqh = epipe->sqh;
	ehci_soft_qtd_t *sqtd, *snext;
	ehci_physaddr_t cur, us, next;
	int s;
	int hit, i;
	/* int count = 0; */
	ehci_soft_qh_t *psqh;

	DPRINTF(("ehci_abort_xfer: xfer=%p pipe=%p\n", xfer, epipe));

	if (sc->sc_dying) {
		/* If we're dying, just do the software part. */
		s = splusb();
		xfer->status = status;	/* make software ignore it */
		callout_stop(&xfer->timeout_handle);
		usb_rem_task(epipe->pipe.device, &exfer->abort_task);
		usb_transfer_complete(xfer);
		splx(s);
		return;
	}

	if (xfer->device->bus->intr_context)
		panic("ehci_abort_xfer: not in process context");

	/*
	 * If an abort is already in progress then just wait for it to
	 * complete and return.
	 */
	if (exfer->ehci_xfer_flags & EHCI_XFER_ABORTING) {
		DPRINTFN(2, ("ehci_abort_xfer: already aborting\n"));
		/* No need to wait if we're aborting from a timeout. */
		if (status == USBD_TIMEOUT)
			return;
		/* Override the status which might be USBD_TIMEOUT. */
		xfer->status = status;
		DPRINTFN(2, ("ehci_abort_xfer: waiting for abort to finish\n"));
		exfer->ehci_xfer_flags |= EHCI_XFER_ABORTWAIT;
		while (exfer->ehci_xfer_flags & EHCI_XFER_ABORTING)
			tsleep(&exfer->ehci_xfer_flags, PZERO, "ehciaw", 0);
		return;
	}

	/*
	 * Step 1: Make interrupt routine and timeouts ignore xfer.
	 */
	s = splusb();
	exfer->ehci_xfer_flags |= EHCI_XFER_ABORTING;
	xfer->status = status;	/* make software ignore it */
	callout_stop(&xfer->timeout_handle);
	usb_rem_task(epipe->pipe.device, &exfer->abort_task);
	splx(s);

	/*
	 * Step 2: Wait until we know hardware has finished any possible
	 * use of the xfer. We do this by removing the entire
	 * queue from the async schedule and waiting for the doorbell.
	 * Nothing else should be touching the queue now.
	 */
	psqh = sqh->prev;
	ehci_rem_qh(sc, sqh, psqh);

	/*
 	 * Step 3:  make sure the soft interrupt routine
	 * has run. This should remove any completed items off the queue.
	 * The hardware has no reference to completed items (TDs).
	 * It's safe to remove them at any time.
	 */
	s = splusb();
#ifdef USB_USE_SOFTINTR
	sc->sc_softwake = 1;
#endif /* USB_USE_SOFTINTR */
	usb_schedsoftintr(&sc->sc_bus);
#ifdef USB_USE_SOFTINTR
	tsleep(&sc->sc_softwake, PZERO, "ehciab", 0);
#endif /* USB_USE_SOFTINTR */

	/*
	 * Step 4: Remove any vestiges of the xfer from the hardware.
	 * The complication here is that the hardware may have executed
	 * into or even beyond the xfer we're trying to abort.
	 * So as we're scanning the TDs of this xfer we check if
	 * the hardware points to any of them.
	 *
	 * first we need to see if there are any transfers
	 * on this queue before the xfer we are aborting.. we need
	 * to update any pointers that point to us to point past
	 * the aborting xfer.  (If there is something past us).
	 * Hardware and software.
	 */
	cur = EHCI_LINK_ADDR(le32toh(sqh->qh.qh_curqtd));
	hit = 0;

	/* If they initially point here. */
	us = exfer->sqtdstart->physaddr;

	/* We will change them to point here */
	snext = exfer->sqtdend->nextqtd;
	next = htole32(snext->physaddr);

	/*
	 * Now loop through any qTDs before us and keep track of the pointer
	 * that points to us for the end.
	 */
	sqtd = sqh->sqtd;
	while (sqtd && sqtd != exfer->sqtdstart) {
		hit |= (cur == sqtd->physaddr);
		if (EHCI_LINK_ADDR(le32toh(sqtd->qtd.qtd_next)) == us)
			sqtd->qtd.qtd_next = next;
		if (EHCI_LINK_ADDR(le32toh(sqtd->qtd.qtd_altnext)) == us)
			sqtd->qtd.qtd_altnext = next;
		sqtd = sqtd->nextqtd;
	}

	/*
	 * If we already saw the active one then we are pretty much done.
	 * We've done all the relinking we need to do.
	 */
	if (!hit) {

		/*
		 * Now reinitialise the QH to point to the next qTD
		 * (if there is one). We only need to do this if
		 * it was previously pointing to us.
		 */
		for (sqtd = exfer->sqtdstart; ; sqtd = sqtd->nextqtd) {
			if (cur == sqtd->physaddr) {
				hit++;
			}
			if (sqtd == exfer->sqtdend)
				break;
		}
		sqtd = sqtd->nextqtd;
		/*
		 * Only need to alter the QH if it was pointing at a qTD
		 * that we are removing.
		 */
		if (hit) {
			sqh->qh.qh_qtd.qtd_next = htole32(snext->physaddr);
			sqh->qh.qh_qtd.qtd_altnext = EHCI_NULL;
			sqh->qh.qh_qtd.qtd_status &=
			    htole32(EHCI_QTD_TOGGLE_MASK);
			for (i = 0; i < EHCI_QTD_NBUFFERS; i++) {
				sqh->qh.qh_qtd.qtd_buffer[i] = 0;
				sqh->qh.qh_qtd.qtd_buffer_hi[i] = 0;
			}
		}
	}
	ehci_add_qh(sqh, psqh);
	/*
	 * Step 5: Execute callback.
	 */
#ifdef DIAGNOSTIC
	exfer->isdone = 1;
#endif
	/* Do the wakeup first to avoid touching the xfer after the callback. */
	exfer->ehci_xfer_flags &= ~EHCI_XFER_ABORTING;
	if (exfer->ehci_xfer_flags & EHCI_XFER_ABORTWAIT) {
		exfer->ehci_xfer_flags &= ~EHCI_XFER_ABORTWAIT;
		wakeup(&exfer->ehci_xfer_flags);
	}
	usb_transfer_complete(xfer);

	/* printf("%s: %d TDs aborted\n", __func__, count); */
	splx(s);
#undef exfer
}

void
ehci_timeout(void *addr)
{
	struct ehci_xfer *exfer = addr;
	struct ehci_pipe *epipe = (struct ehci_pipe *)exfer->xfer.pipe;
	ehci_softc_t *sc = (ehci_softc_t *)epipe->pipe.device->bus;

	DPRINTF(("ehci_timeout: exfer=%p\n", exfer));
#ifdef USB_DEBUG
	if (ehcidebug > 1)
		usbd_dump_pipe(exfer->xfer.pipe);
#endif

	if (sc->sc_dying) {
		ehci_abort_xfer(&exfer->xfer, USBD_TIMEOUT);
		return;
	}

	/* Execute the abort in a process context. */
	usb_add_task(exfer->xfer.pipe->device, &exfer->abort_task,
	    USB_TASKQ_HC);
}

void
ehci_abort_isoc_xfer(usbd_xfer_handle xfer, usbd_status status)
{
	ehci_isoc_trans_t trans_status;
	struct ehci_pipe *epipe;
	struct ehci_xfer *exfer;
	ehci_softc_t *sc;
	struct ehci_soft_itd *itd;
	int s, i;

	epipe = (struct ehci_pipe *) xfer->pipe;
	exfer = EXFER(xfer);
	sc = (ehci_softc_t *)epipe->pipe.device->bus;

	DPRINTF(("ehci_abort_isoc_xfer: xfer %p pipe %p\n", xfer, epipe));

	if (sc->sc_dying) {
		s = splusb();
		xfer->status = status;
		callout_stop(&xfer->timeout_handle);
		usb_rem_task(epipe->pipe.device, &exfer->abort_task);
		usb_transfer_complete(xfer);
		splx(s);
		return;
	}

	if (exfer->ehci_xfer_flags & EHCI_XFER_ABORTING) {
		DPRINTFN(2, ("ehci_abort_isoc_xfer: already aborting\n"));

#ifdef DIAGNOSTIC
		if (status == USBD_TIMEOUT)
			printf("ehci_abort_xfer: TIMEOUT while aborting\n");
#endif

		xfer->status = status;
		DPRINTFN(2, ("ehci_abort_xfer: waiting for abort to finish\n"));
		exfer->ehci_xfer_flags |= EHCI_XFER_ABORTWAIT;
		while (exfer->ehci_xfer_flags & EHCI_XFER_ABORTING)
			tsleep(&exfer->ehci_xfer_flags, PZERO, "ehciaw", 0);
		return;
	}
	exfer->ehci_xfer_flags |= EHCI_XFER_ABORTING;

	xfer->status = status;
	callout_stop(&xfer->timeout_handle);
	usb_rem_task(epipe->pipe.device, &exfer->abort_task);

	s = splusb();
	for (itd = exfer->itdstart; itd != NULL; itd = itd->xfer_next) {

		for (i = 0; i < 8; i++) {
			trans_status = le32toh(itd->itd.itd_ctl[i]);
			trans_status &= ~EHCI_ITD_ACTIVE;
			itd->itd.itd_ctl[i] = htole32(trans_status);
		}

	}
	splx(s);

        s = splusb();
#ifdef USB_USE_SOFTINTR
        sc->sc_softwake = 1;
#endif /* USB_USE_SOFTINTR */
        usb_schedsoftintr(&sc->sc_bus);
#ifdef USB_USE_SOFTINTR
        tsleep(&sc->sc_softwake, PZERO, "ehciab", 0);
#endif /* USB_USE_SOFTINTR */
        splx(s);

#ifdef DIAGNOSTIC
	exfer->isdone = 1;
#endif
	exfer->ehci_xfer_flags &= ~EHCI_XFER_ABORTING;
	if (exfer->ehci_xfer_flags & EHCI_XFER_ABORTWAIT) {
		exfer->ehci_xfer_flags &= ~EHCI_XFER_ABORTWAIT;
		wakeup(&exfer->ehci_xfer_flags);
	}
	usb_transfer_complete(xfer);
}

void
ehci_timeout_task(void *addr)
{
	usbd_xfer_handle xfer = addr;
	int s;

	DPRINTF(("ehci_timeout_task: xfer=%p\n", xfer));

	s = splusb();
	ehci_abort_xfer(xfer, USBD_TIMEOUT);
	splx(s);
}

/*
 * Some EHCI chips from VIA / ATI seem to trigger interrupts before writing
 * back the qTD status, or miss signalling occasionally under heavy load.
 * If the host machine is too fast, we can miss transaction completion - when
 * we scan the active list the transaction still seems to be active. This
 * generally exhibits itself as a umass stall that never recovers.
 *
 * We work around this behaviour by setting up this callback after any softintr
 * that completes with transactions still pending, giving us another chance to
 * check for completion after the writeback has taken place.
 */
void
ehci_intrlist_timeout(void *arg)
{
	ehci_softc_t *sc = arg;
	int s = splusb();

	DPRINTFN(3, ("ehci_intrlist_timeout\n"));
	usb_schedsoftintr(&sc->sc_bus);

	splx(s);
}

/************************/

static usbd_status
ehci_device_ctrl_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ehci_device_ctrl_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ehci_device_ctrl_start(usbd_xfer_handle xfer)
{
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	usbd_status err;

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		/* XXX panic */
		printf("ehci_device_ctrl_transfer: not a request\n");
		return (USBD_INVAL);
	}
#endif

	err = ehci_device_request(xfer);
	if (err)
		return (err);

	if (sc->sc_bus.use_polling)
		ehci_waitintr(sc, xfer);
	return (USBD_IN_PROGRESS);
}

void
ehci_device_ctrl_done(usbd_xfer_handle xfer)
{
	struct ehci_xfer *ex = EXFER(xfer);
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;

	DPRINTFN(10,("ehci_ctrl_done: xfer=%p\n", xfer));

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		panic("ehci_ctrl_done: not a request");
	}
#endif

	if (xfer->status != USBD_NOMEM && ehci_active_intr_list(ex)) {
		ehci_del_intr_list(ex);	/* remove from active list */
		ehci_free_sqtd_chain(sc, epipe->sqh, ex->sqtdstart,
		    ex->sqtdend->nextqtd);
	}

	DPRINTFN(5, ("ehci_ctrl_done: length=%d\n", xfer->actlen));
}

/* Abort a device control request. */
static void
ehci_device_ctrl_abort(usbd_xfer_handle xfer)
{
	DPRINTF(("ehci_device_ctrl_abort: xfer=%p\n", xfer));
	ehci_abort_xfer(xfer, USBD_CANCELLED);
}

/* Close a device control pipe. */
static void
ehci_device_ctrl_close(usbd_pipe_handle pipe)
{
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;
	/*struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;*/

	DPRINTF(("ehci_device_ctrl_close: pipe=%p\n", pipe));
	ehci_close_pipe(pipe, sc->sc_async_head);
}

usbd_status
ehci_device_request(usbd_xfer_handle xfer)
{
#define exfer EXFER(xfer)
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	usb_device_request_t *req = &xfer->request;
	usbd_device_handle dev = epipe->pipe.device;
	ehci_softc_t *sc = (ehci_softc_t *)dev->bus;
	ehci_soft_qtd_t *newinactive, *setup, *stat, *next;
	ehci_soft_qh_t *sqh;
	int isread;
	int len;
	usbd_status err;
	int s;

	isread = req->bmRequestType & UT_READ;
	len = UGETW(req->wLength);

	DPRINTFN(3,("ehci_device_request: type=0x%02x, request=0x%02x, "
		    "wValue=0x%04x, wIndex=0x%04x len=%d, addr=%d, endpt=%d\n",
		    req->bmRequestType, req->bRequest, UGETW(req->wValue),
		    UGETW(req->wIndex), len, dev->address,
		    epipe->pipe.endpoint->edesc->bEndpointAddress));

	newinactive = ehci_alloc_sqtd(sc);
	if (newinactive == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	newinactive->qtd.qtd_status = htole32(0);
	newinactive->qtd.qtd_next = EHCI_NULL;
	newinactive->qtd.qtd_altnext = EHCI_NULL;
	stat = ehci_alloc_sqtd(sc);
	if (stat == NULL) {
		err = USBD_NOMEM;
		goto bad2;
	}

	sqh = epipe->sqh;
	setup = sqh->inactivesqtd;
	sqh->inactivesqtd = newinactive;
	epipe->u.ctl.length = len;

	/* Set up data transaction */
	if (len != 0) {
		ehci_soft_qtd_t *end;

		err = ehci_alloc_sqtd_chain(epipe, sc, len, isread, xfer,
		    NULL, newinactive, &next, &end);
		if (err)
			goto bad3;
		end->qtd.qtd_status &= htole32(~EHCI_QTD_IOC);
		end->nextqtd = stat;
		end->qtd.qtd_next = htole32(stat->physaddr);
		end->qtd.qtd_altnext = htole32(newinactive->physaddr);
	} else {
		next = stat;
	}

	memcpy(KERNADDR(&epipe->u.ctl.reqdma, 0), req, sizeof *req);

	/* Clear toggle, and do not activate until complete */
	setup->qtd.qtd_status = htole32(
	    EHCI_QTD_SET_PID(EHCI_QTD_PID_SETUP) |
	    EHCI_QTD_SET_CERR(3) |
	    EHCI_QTD_SET_TOGGLE(0) |
	    EHCI_QTD_SET_BYTES(sizeof *req)
	    );
	setup->qtd.qtd_buffer[0] = htole32(DMAADDR(&epipe->u.ctl.reqdma, 0));
	setup->qtd.qtd_buffer_hi[0] = 0;
	setup->nextqtd = next;
	setup->qtd.qtd_next = htole32(next->physaddr);
	setup->qtd.qtd_altnext = htole32(newinactive->physaddr);
	setup->xfer = xfer;
	setup->len = sizeof *req;

	stat->qtd.qtd_status = htole32(
	    EHCI_QTD_ACTIVE |
	    EHCI_QTD_SET_PID(isread ? EHCI_QTD_PID_OUT : EHCI_QTD_PID_IN) |
	    EHCI_QTD_SET_CERR(3) |
	    EHCI_QTD_SET_TOGGLE(1) |
	    EHCI_QTD_IOC
	    );
	stat->qtd.qtd_buffer[0] = 0; /* XXX not needed? */
	stat->qtd.qtd_buffer_hi[0] = 0; /* XXX not needed? */
	stat->nextqtd = newinactive;
	stat->qtd.qtd_next = htole32(newinactive->physaddr);
	stat->qtd.qtd_altnext = htole32(newinactive->physaddr);
	stat->xfer = xfer;
	stat->len = 0;

#ifdef EHCI_DEBUG
	if (ehcidebug > 5) {
		DPRINTF(("ehci_device_request:\n"));
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(setup);
	}
#endif

	exfer->sqtdstart = setup;
	exfer->sqtdend = stat;
#ifdef DIAGNOSTIC
	if (!exfer->isdone) {
		printf("ehci_device_request: not done, exfer=%p\n", exfer);
	}
	exfer->isdone = 0;
#endif

	/* Activate the new qTD in the QH list. */
	s = splusb();
	ehci_activate_qh(sqh, setup);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
                callout_reset(&xfer->timeout_handle, MS_TO_TICKS(xfer->timeout),
		    ehci_timeout, xfer);
	}
	ehci_add_intr_list(sc, exfer);
	xfer->status = USBD_IN_PROGRESS;
	splx(s);

#ifdef EHCI_DEBUG
	if (ehcidebug > 10) {
		DPRINTF(("ehci_device_request: status=%x\n",
			 EOREAD4(sc, EHCI_USBSTS)));
		delay(10000);
		ehci_dump_regs(sc);
		ehci_dump_sqh(sc->sc_async_head);
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(setup);
	}
#endif

	return (USBD_NORMAL_COMPLETION);

 bad3:
	sqh->inactivesqtd = setup;
	ehci_free_sqtd(sc, stat);
 bad2:
	ehci_free_sqtd(sc, newinactive);
 bad1:
	DPRINTFN(-1,("ehci_device_request: no memory\n"));
	xfer->status = err;
	usb_transfer_complete(xfer);
	return (err);
#undef exfer
}

/************************/

static usbd_status
ehci_device_bulk_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ehci_device_bulk_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

usbd_status
ehci_device_bulk_start(usbd_xfer_handle xfer)
{
#define exfer EXFER(xfer)
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	usbd_device_handle dev = epipe->pipe.device;
	ehci_softc_t *sc = (ehci_softc_t *)dev->bus;
	ehci_soft_qtd_t *data, *dataend, *newinactive;
	ehci_soft_qh_t *sqh;
	usbd_status err;
	int len, isread, endpt;
	int s;

	DPRINTFN(2, ("ehci_device_bulk_start: xfer=%p len=%d flags=%d\n",
		     xfer, xfer->length, xfer->flags));

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST)
		panic("ehci_device_bulk_start: a request");
#endif

	len = xfer->length;
	endpt = epipe->pipe.endpoint->edesc->bEndpointAddress;
	isread = UE_GET_DIR(endpt) == UE_DIR_IN;
	sqh = epipe->sqh;

	epipe->u.bulk.length = len;

	newinactive = ehci_alloc_sqtd(sc);
	if (newinactive == NULL) {
		DPRINTFN(-1,("ehci_device_bulk_start: no sqtd memory\n"));
		err = USBD_NOMEM;
		xfer->status = err;
		usb_transfer_complete(xfer);
		return (err);
	}
	newinactive->qtd.qtd_status = htole32(0);
	newinactive->qtd.qtd_next = EHCI_NULL;
	newinactive->qtd.qtd_altnext = EHCI_NULL;
	err = ehci_alloc_sqtd_chain(epipe, sc, len, isread, xfer,
	    sqh->inactivesqtd, newinactive, &data, &dataend);
	if (err) {
		DPRINTFN(-1,("ehci_device_bulk_start: no memory\n"));
		ehci_free_sqtd(sc, newinactive);
		xfer->status = err;
		usb_transfer_complete(xfer);
		return (err);
	}
	dataend->nextqtd = newinactive;
	dataend->qtd.qtd_next = htole32(newinactive->physaddr);
	dataend->qtd.qtd_altnext = htole32(newinactive->physaddr);
	sqh->inactivesqtd = newinactive;

#ifdef EHCI_DEBUG
	if (ehcidebug > 5) {
		DPRINTF(("ehci_device_bulk_start: data(1)\n"));
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(data);
	}
#endif

	/* Set up interrupt info. */
	exfer->sqtdstart = data;
	exfer->sqtdend = dataend;
#ifdef DIAGNOSTIC
	if (!exfer->isdone) {
		printf("ehci_device_bulk_start: not done, ex=%p\n", exfer);
	}
	exfer->isdone = 0;
#endif

	s = splusb();
	ehci_activate_qh(sqh, data);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
		callout_reset(&xfer->timeout_handle, MS_TO_TICKS(xfer->timeout),
		    ehci_timeout, xfer);
	}
	ehci_add_intr_list(sc, exfer);
	xfer->status = USBD_IN_PROGRESS;
	splx(s);

#ifdef EHCI_DEBUG
	if (ehcidebug > 10) {
		DPRINTF(("ehci_device_bulk_start: data(2)\n"));
		delay(10000);
		DPRINTF(("ehci_device_bulk_start: data(3)\n"));
		ehci_dump_regs(sc);
#if 0
		printf("async_head:\n");
		ehci_dump_sqh(sc->sc_async_head);
#endif
		printf("sqh:\n");
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(data);
	}
#endif

	if (sc->sc_bus.use_polling)
		ehci_waitintr(sc, xfer);

	return (USBD_IN_PROGRESS);
#undef exfer
}

static void
ehci_device_bulk_abort(usbd_xfer_handle xfer)
{
	DPRINTF(("ehci_device_bulk_abort: xfer=%p\n", xfer));
	ehci_abort_xfer(xfer, USBD_CANCELLED);
}

/*
 * Close a device bulk pipe.
 */
static void
ehci_device_bulk_close(usbd_pipe_handle pipe)
{
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;

	DPRINTF(("ehci_device_bulk_close: pipe=%p\n", pipe));
	ehci_close_pipe(pipe, sc->sc_async_head);
}

void
ehci_device_bulk_done(usbd_xfer_handle xfer)
{
	struct ehci_xfer *ex = EXFER(xfer);
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;

	DPRINTFN(10,("ehci_bulk_done: xfer=%p, actlen=%d\n",
		     xfer, xfer->actlen));

	if (xfer->status != USBD_NOMEM && ehci_active_intr_list(ex)) {
		ehci_del_intr_list(ex);	/* remove from active list */
		ehci_free_sqtd_chain(sc, epipe->sqh, ex->sqtdstart,
		    ex->sqtdend->nextqtd);
	}

	DPRINTFN(5, ("ehci_bulk_done: length=%d\n", xfer->actlen));
}

/************************/

static usbd_status
ehci_device_setintr(ehci_softc_t *sc, ehci_soft_qh_t *sqh, int ival)
{
	struct ehci_soft_islot *isp;
	int islot, lev;

	/* Find a poll rate that is large enough. */
	for (lev = EHCI_IPOLLRATES - 1; lev > 0; lev--)
		if (EHCI_ILEV_IVAL(lev) <= ival)
			break;

	/* Pick an interrupt slot at the right level. */
	/* XXX could do better than picking at random. */
	islot = EHCI_IQHIDX(lev, arc4random());

	sqh->islot = islot;
	isp = &sc->sc_islots[islot];
	ehci_add_qh(sqh, isp->sqh);

	return (USBD_NORMAL_COMPLETION);
}

static usbd_status
ehci_device_intr_transfer(usbd_xfer_handle xfer)
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/*
	 * Pipe isn't running (otherwise err would be USBD_INPROG),
	 * so start it first.
	 */
	return (ehci_device_intr_start(STAILQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ehci_device_intr_start(usbd_xfer_handle xfer)
{
#define exfer EXFER(xfer)
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	usbd_device_handle dev = xfer->pipe->device;
	ehci_softc_t *sc = (ehci_softc_t *)dev->bus;
	ehci_soft_qtd_t *data, *dataend, *newinactive;
	ehci_soft_qh_t *sqh;
	usbd_status err;
	int len, isread, endpt;
	int s;

	DPRINTFN(2, ("ehci_device_intr_start: xfer=%p len=%d flags=%d\n",
	    xfer, xfer->length, xfer->flags));

	if (sc->sc_dying)
		return (USBD_IOERROR);

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST)
		panic("ehci_device_intr_start: a request");
#endif

	len = xfer->length;
	endpt = epipe->pipe.endpoint->edesc->bEndpointAddress;
	isread = UE_GET_DIR(endpt) == UE_DIR_IN;
	sqh = epipe->sqh;

	epipe->u.intr.length = len;

	newinactive = ehci_alloc_sqtd(sc);
	if (newinactive == NULL) {
		DPRINTFN(-1,("ehci_device_intr_start: no sqtd memory\n"));
		err = USBD_NOMEM;
		xfer->status = err;
		usb_transfer_complete(xfer);
		return (err);
	}
	newinactive->qtd.qtd_status = htole32(0);
	newinactive->qtd.qtd_next = EHCI_NULL;
	newinactive->qtd.qtd_altnext = EHCI_NULL;
	err = ehci_alloc_sqtd_chain(epipe, sc, len, isread, xfer,
	    sqh->inactivesqtd, newinactive, &data, &dataend);
	if (err) {
		DPRINTFN(-1, ("ehci_device_intr_start: no memory\n"));
		xfer->status = err;
		usb_transfer_complete(xfer);
		return (err);
	}
	dataend->nextqtd = newinactive;
	dataend->qtd.qtd_next = htole32(newinactive->physaddr);
	dataend->qtd.qtd_altnext = htole32(newinactive->physaddr);
	sqh->inactivesqtd = newinactive;

#ifdef EHCI_DEBUG
	if (ehcidebug > 5) {
		DPRINTF(("ehci_device_intr_start: data(1)\n"));
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(data);
	}
#endif

	/* Set up interrupt info. */
	exfer->sqtdstart = data;
	exfer->sqtdend = dataend;
#ifdef DIAGNOSTIC
	if (!exfer->isdone) {
		printf("ehci_device_intr_start: not done, ex=%p\n", exfer);
	}
	exfer->isdone = 0;
#endif

	s = splusb();
	ehci_activate_qh(sqh, data);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
		callout_reset(&xfer->timeout_handle, MS_TO_TICKS(xfer->timeout),
		    ehci_timeout, xfer);
	}
	ehci_add_intr_list(sc, exfer);
	xfer->status = USBD_IN_PROGRESS;
	splx(s);

#ifdef EHCI_DEBUG
	if (ehcidebug > 10) {
		DPRINTF(("ehci_device_intr_start: data(2)\n"));
		delay(10000);
		DPRINTF(("ehci_device_intr_start: data(3)\n"));
		ehci_dump_regs(sc);
		printf("sqh:\n");
		ehci_dump_sqh(sqh);
		ehci_dump_sqtds(data);
	}
#endif

	if (sc->sc_bus.use_polling)
		ehci_waitintr(sc, xfer);

	return (USBD_IN_PROGRESS);
#undef exfer
}

static void
ehci_device_intr_abort(usbd_xfer_handle xfer)
{
	DPRINTFN(1, ("ehci_device_intr_abort: xfer=%p\n", xfer));
	if (xfer->pipe->intrxfer == xfer) {
		DPRINTFN(1, ("ehci_device_intr_abort: remove\n"));
		xfer->pipe->intrxfer = NULL;
	}
	/* 
	 * XXX - abort_xfer uses ehci_sync_hc, which syncs via the advance
	 *       async doorbell. That's dependant on the async list, wheras
	 *       intr xfers are periodic, should not use this?
	 */
	ehci_abort_xfer(xfer, USBD_CANCELLED);
}

static void
ehci_device_intr_close(usbd_pipe_handle pipe)
{
	ehci_softc_t *sc = (ehci_softc_t *)pipe->device->bus;
	struct ehci_pipe *epipe = (struct ehci_pipe *)pipe;
	struct ehci_soft_islot *isp;

	isp = &sc->sc_islots[epipe->sqh->islot];
	ehci_close_pipe(pipe, isp->sqh);
}

static void
ehci_device_intr_done(usbd_xfer_handle xfer)
{
#define exfer EXFER(xfer)
	struct ehci_xfer *ex = EXFER(xfer);
	ehci_softc_t *sc = (ehci_softc_t *)xfer->pipe->device->bus;
	struct ehci_pipe *epipe = (struct ehci_pipe *)xfer->pipe;
	ehci_soft_qtd_t *data, *dataend, *newinactive;
	ehci_soft_qh_t *sqh;
	usbd_status err;
	int len, isread, endpt, s;

	DPRINTFN(10, ("ehci_device_intr_done: xfer=%p, actlen=%d\n",
	    xfer, xfer->actlen));

	sqh = epipe->sqh;
	if (xfer->pipe->repeat) {
		ehci_free_sqtd_chain(sc, sqh, ex->sqtdstart,
		    ex->sqtdend->nextqtd);

		len = epipe->u.intr.length;
		xfer->length = len;
		endpt = epipe->pipe.endpoint->edesc->bEndpointAddress;
		isread = UE_GET_DIR(endpt) == UE_DIR_IN;

		newinactive = ehci_alloc_sqtd(sc);
		if (newinactive == NULL) {
			DPRINTFN(-1,
			    ("ehci_device_intr_done: no sqtd memory\n"));
			err = USBD_NOMEM;
			xfer->status = err;
			return;
		}
		newinactive->qtd.qtd_status = htole32(0);
		newinactive->qtd.qtd_next = EHCI_NULL;
		newinactive->qtd.qtd_altnext = EHCI_NULL;
		err = ehci_alloc_sqtd_chain(epipe, sc, len, isread, xfer,
		    sqh->inactivesqtd, newinactive, &data, &dataend);
		if (err) {
			DPRINTFN(-1, ("ehci_device_intr_done: no memory\n"));
			xfer->status = err;
			return;
		}
		dataend->nextqtd = newinactive;
		dataend->qtd.qtd_next = htole32(newinactive->physaddr);
		dataend->qtd.qtd_altnext = htole32(newinactive->physaddr);
		sqh->inactivesqtd = newinactive;

		/* Set up interrupt info. */
		exfer->sqtdstart = data;
		exfer->sqtdend = dataend;
#ifdef DIAGNOSTIC
		if (!exfer->isdone) {
			printf("ehci_device_intr_done: not done, ex=%p\n",
			    exfer);
		}
		exfer->isdone = 0;
#endif

		s = splusb();
		ehci_activate_qh(sqh, data);
		if (xfer->timeout && !sc->sc_bus.use_polling) {
			callout_reset(&xfer->timeout_handle,
			    MS_TO_TICKS(xfer->timeout), ehci_timeout, xfer);
		}
		splx(s);

		xfer->status = USBD_IN_PROGRESS;
	} else if (xfer->status != USBD_NOMEM && ehci_active_intr_list(ex)) {
		ehci_del_intr_list(ex); /* remove from active list */
		ehci_free_sqtd_chain(sc, sqh, ex->sqtdstart,
		    ex->sqtdend->nextqtd);
	}
#undef exfer
}

/************************/

static usbd_status	
ehci_device_isoc_transfer(usbd_xfer_handle xfer) 
{
	usbd_status err;

	err = usb_insert_transfer(xfer);
	if (err && err != USBD_IN_PROGRESS)
		return (err);

	return (ehci_device_isoc_start(xfer));
}

static usbd_status	
ehci_device_isoc_start(usbd_xfer_handle xfer)
{
	struct ehci_pipe *epipe;
	usbd_device_handle dev;
	ehci_softc_t *sc;
	struct ehci_xfer *exfer;
	ehci_soft_itd_t *itd, *prev, *start, *stop;
	usb_dma_t *dma_buf;
	int i, j, k, frames, uframes, ufrperframe;
	int s, trans_count, offs, total_length;
	int frindex;

	start = NULL;
	prev = NULL;
	itd = NULL;
	trans_count = 0;
	total_length = 0;
	exfer = (struct ehci_xfer *) xfer;
	sc = (ehci_softc_t *)xfer->pipe->device->bus;
	dev = xfer->pipe->device;
	epipe = (struct ehci_pipe *)xfer->pipe;

	/*
	 * To allow continuous transfers, above we start all transfers
	 * immediately. However, we're still going to get usbd_start_next call
	 * this when another xfer completes. So, check if this is already
	 * in progress or not
	 */

	if (exfer->itdstart != NULL)
		return (USBD_IN_PROGRESS);

	DPRINTFN(2, ("ehci_device_isoc_start: xfer %p len %d flags %d\n",
	    xfer, xfer->length, xfer->flags));

	if (sc->sc_dying)
		return (USBD_IOERROR);

	/*
	 * To avoid complication, don't allow a request right now that'll span
	 * the entire frame table. To within 4 frames, to allow some leeway
	 * on either side of where the hc currently is.
	 */
	if ((1 << (epipe->pipe.endpoint->edesc->bInterval)) *
	    xfer->nframes >= (sc->sc_flsize - 4) * 8) {
		printf("ehci: isoc descriptor requested that spans the entire"
		    " frametable, too many frames\n");
		return (USBD_INVAL);
	}

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST)
		panic("ehci_device_isoc_start: request\n");

	if (!exfer->isdone)
		printf("ehci_device_isoc_start: not done, ex = %p\n", exfer);
	exfer->isdone = 0;
#endif

	/*
	 * Step 1: Allocate and initialize itds, how many do we need?
	 * One per transfer if interval >= 8 microframes, fewer if we use
	 * multiple microframes per frame.
	 */

	i = epipe->pipe.endpoint->edesc->bInterval;
	if (i > 16 || i == 0) {
		/* Spec page 271 says intervals > 16 are invalid */
		DPRINTF(("ehci_device_isoc_start: bInvertal %d invalid\n", i));
		return (USBD_INVAL);
	}

	switch (i) {
	case 1: 
		ufrperframe = 8;
		break;
	case 2: 
		ufrperframe = 4;
		break;
	case 3: 
		ufrperframe = 2;
		break;
	default: 
		ufrperframe = 1;
		break;
	}
	frames = (xfer->nframes + (ufrperframe - 1)) / ufrperframe;
	uframes = 8 / ufrperframe;

	if (frames == 0) {
		DPRINTF(("ehci_device_isoc_start: frames == 0\n"));
		return (USBD_INVAL);
	}

	dma_buf = xfer->buffer;
	offs = 0;

	for (i = 0; i < frames; i++) {
		int froffs = offs;
		itd = ehci_alloc_itd(sc);

		if (prev != NULL) {
			prev->itd.itd_next =
			    htole32(itd->physaddr | EHCI_LINK_ITD);
			prev->xfer_next = itd;
	    	} else {
			start = itd;
		}

		/*
		 * Step 1.5, initialize uframes
		 */
		for (j = 0; j < 8; j += uframes) {
			/* Calculate which page in the list this starts in */
			int addr = DMAADDR(dma_buf, froffs);
			addr = EHCI_PAGE_OFFSET(addr);
			addr += (offs - froffs);
			addr = EHCI_PAGE(addr);
			addr /= EHCI_PAGE_SIZE;

			/* This gets the initial offset into the first page,
			 * looks how far further along the current uframe
			 * offset is. Works out how many pages that is.
			 */

			itd->itd.itd_ctl[j] = htole32 ( EHCI_ITD_ACTIVE |
			    EHCI_ITD_SET_LEN(xfer->frlengths[trans_count]) | 
			    EHCI_ITD_SET_PG(addr) |
			    EHCI_ITD_SET_OFFS(EHCI_PAGE_OFFSET(DMAADDR(dma_buf,
				offs))));

			total_length += xfer->frlengths[trans_count];
			offs += xfer->frlengths[trans_count];
			trans_count++;

			if (trans_count >= xfer->nframes) { /*Set IOC*/
				itd->itd.itd_ctl[j] |= htole32(EHCI_ITD_IOC);
			}
		}	

		/* Step 1.75, set buffer pointers. To simplify matters, all
		 * pointers are filled out for the next 7 hardware pages in
		 * the dma block, so no need to worry what pages to cover
		 * and what to not.
		 */

		for (j=0; j < 7; j++) {
			/*
			 * Don't try to lookup a page that's past the end
			 * of buffer
			 */
			int page_offs = EHCI_PAGE(froffs + 
			    (EHCI_PAGE_SIZE * j));
			if (page_offs >= dma_buf->block->size)
				break;

			int page = DMAADDR(dma_buf, page_offs);
			page = EHCI_PAGE(page);
			itd->itd.itd_bufr[j] =
			    htole32(EHCI_ITD_SET_BPTR(page) | EHCI_LINK_ITD);
		}

		/*
		 * Other special values
		 */

		k = epipe->pipe.endpoint->edesc->bEndpointAddress;
		itd->itd.itd_bufr[0] |= htole32(
		    EHCI_ITD_SET_EP(UE_GET_ADDR(k)) |
		    EHCI_ITD_SET_DADDR(epipe->pipe.device->address));

		k = (UE_GET_DIR(epipe->pipe.endpoint->edesc->bEndpointAddress))
		    ? 1 : 0;
		j = UE_GET_SIZE(
		    UGETW(epipe->pipe.endpoint->edesc->wMaxPacketSize));
		itd->itd.itd_bufr[1] |= htole32(EHCI_ITD_SET_DIR(k) |
		    EHCI_ITD_SET_MAXPKT(UE_GET_SIZE(j)));

		/* FIXME: handle invalid trans */
		itd->itd.itd_bufr[2] |= 
		    htole32(EHCI_ITD_SET_MULTI(UE_GET_TRANS(j)+1));
		prev = itd;
	} /* End of frame */

	stop = itd;
	stop->xfer_next = NULL;
	exfer->isoc_len = total_length;

	/*
	 * Part 2: Transfer descriptors have now been set up, now they must
	 * be scheduled into the period frame list. Erk. Not wanting to
	 * complicate matters, transfer is denied if the transfer spans
	 * more than the period frame list.
	 */

	s = splusb();

	/* Start inserting frames */
	if (epipe->u.isoc.cur_xfers > 0) {
		frindex = epipe->u.isoc.next_frame;
	} else {
		frindex = EOREAD4(sc, EHCI_FRINDEX);
		frindex = frindex >> 3; /* Erase microframe index */
		frindex += 2;
	}

	if (frindex >= sc->sc_flsize)
		frindex &= (sc->sc_flsize - 1);

	/* Whats the frame interval? */
	i = (1 << epipe->pipe.endpoint->edesc->bInterval);
	if (i / 8 == 0)
		i = 1;
	else
		i /= 8;

	itd = start;
	for (j = 0; j < frames; j++) {
		if (itd == NULL)
			panic("ehci: unexpectedly ran out of isoc itds,"
			    "isoc_start\n");

		itd->itd.itd_next = sc->sc_flist[frindex];
		if (itd->itd.itd_next == 0)
			/* FIXME: frindex table gets initialized to NULL
			 * or EHCI_NULL? */
			itd->itd.itd_next = htole32(EHCI_NULL);

		sc->sc_flist[frindex] = htole32(EHCI_LINK_ITD | itd->physaddr);

		itd->u.frame_list.next = sc->sc_softitds[frindex];
		sc->sc_softitds[frindex] = itd;
		if (itd->u.frame_list.next != NULL)
			itd->u.frame_list.next->u.frame_list.prev = itd;
		itd->slot = frindex;
		itd->u.frame_list.prev = NULL;

		frindex += i;
		if (frindex >= sc->sc_flsize)
			frindex -= sc->sc_flsize;

		itd = itd->xfer_next;
	}

	epipe->u.isoc.cur_xfers++;
	epipe->u.isoc.next_frame = frindex;

	exfer->itdstart = start;
	exfer->itdend = stop;
	exfer->sqtdstart = NULL;
	exfer->sqtdstart = NULL;

	ehci_add_intr_list(sc, exfer);
	xfer->status = USBD_IN_PROGRESS;
	xfer->done = 0;
	splx(s);

	if (sc->sc_bus.use_polling) {
		printf("Starting ehci isoc xfer with polling. Bad idea?\n");
		ehci_waitintr(sc, xfer);
	}

	return (USBD_IN_PROGRESS);
}

static void		
ehci_device_isoc_abort(usbd_xfer_handle xfer)
{
	DPRINTFN(1, ("ehci_device_isoc_abort: xfer = %p\n", xfer));
	ehci_abort_isoc_xfer(xfer, USBD_CANCELLED);
}

static void		
ehci_device_isoc_close(usbd_pipe_handle pipe)
{
	printf("ehci_device_isoc_close: nothing in the pipe to free?\n");
}

static void		
ehci_device_isoc_done(usbd_xfer_handle xfer)
{
	struct ehci_xfer *exfer;
	ehci_softc_t *sc;
	struct ehci_pipe *epipe;
	int s;

	exfer = EXFER(xfer);
	sc = (ehci_softc_t *)xfer->pipe->device->bus;
	epipe = (struct ehci_pipe *) xfer->pipe;

	s = splusb();
	epipe->u.isoc.cur_xfers--;
	if (xfer->status != USBD_NOMEM && ehci_active_intr_list(exfer)) {
		ehci_del_intr_list(exfer);
		ehci_rem_free_itd_chain(sc, exfer);
	}
	splx(s);
}
