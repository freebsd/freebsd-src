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
	int nexttoggle;

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
 * no dma specific data. The other free lists do.
 */
LIST_HEAD(, uhci_intr_info) uhci_ii_free = LIST_HEAD_INITIALIZER(uhci_ii_free);

/* initialisation */
usbd_status	uhci_init_framelist	__P((uhci_softc_t *sc));

/* modification of the host controller's status */
void		uhci_busreset		__P((uhci_softc_t *sc));
usbd_status	uhci_run		__P((uhci_softc_t *sc, int run));

/* resource management */
uhci_soft_td_t *uhci_alloc_std		__P((uhci_softc_t *sc));
void		uhci_free_std		__P((uhci_softc_t *sc,
					uhci_soft_td_t *std));
usbd_status	uhci_alloc_std_chain	__P((uhci_softc_t *sc,
					struct uhci_pipe *upipe,
					int datalen, int isread, int spd,
					usb_dma_t *dma, 
					uhci_soft_td_t **std,
					uhci_soft_td_t **stdend));
void		uhci_free_std_chain	__P((uhci_softc_t *sc, 
					uhci_soft_td_t *std,
					uhci_soft_td_t *stdend));
uhci_soft_qh_t *uhci_alloc_sqh		__P((uhci_softc_t *sc));
void		uhci_free_sqh		__P((uhci_softc_t *sc,
					uhci_soft_qh_t *sqh));
uhci_intr_info_t *uhci_alloc_intr_info	__P((uhci_softc_t *sc));
void		uhci_free_intr_info	__P((uhci_intr_info_t *ii));

/* locking of the framelist */
void		uhci_lock_frames	__P((uhci_softc_t *sc));
void		uhci_unlock_frames	__P((uhci_softc_t *sc));

/* handling of interrupts */
void		uhci_poll		__P((struct usbd_bus *bus));
void		uhci_waitintr		__P((uhci_softc_t *sc,
					usbd_request_handle reqh));
void		uhci_timeout		__P((void *priv));

/* check the list of TDs for an interrupt */
void		uhci_check_intr		__P((uhci_softc_t *sc,
					uhci_intr_info_t *ii));

/* handle a completed request */
void		uhci_ii_done		__P((uhci_intr_info_t *ii, int timedout));
void		uhci_ctrl_done		__P((uhci_intr_info_t *ii));
void		uhci_bulk_done		__P((uhci_intr_info_t *ii));
void		uhci_intr_done		__P((uhci_intr_info_t *ii));
void		uhci_isoc_done		__P((uhci_intr_info_t *ii));

/* pipe methods for devices and root hub; the latter doesn't use iso or bulk */
usbd_status	uhci_open		__P((usbd_pipe_handle pipe));

usbd_status	uhci_device_request	__P((usbd_request_handle reqh));

usbd_status	uhci_device_ctrl_transfer	__P((usbd_request_handle reqh));
usbd_status	uhci_device_ctrl_start		__P((usbd_request_handle reqh));
void		uhci_device_ctrl_abort		__P((usbd_request_handle reqh));
void		uhci_device_ctrl_close		__P((usbd_pipe_handle pipe));

usbd_status	uhci_device_bulk_transfer	__P((usbd_request_handle reqh));
usbd_status	uhci_device_bulk_start		__P((usbd_request_handle reqh));
void		uhci_device_bulk_abort		__P((usbd_request_handle reqh));
void		uhci_device_bulk_close		__P((usbd_pipe_handle pipe));

usbd_status	uhci_device_intr_transfer	__P((usbd_request_handle reqh));
usbd_status	uhci_device_intr_start		__P((usbd_request_handle reqh));
void		uhci_device_intr_abort		__P((usbd_request_handle reqh));
void		uhci_device_intr_close		__P((usbd_pipe_handle pipe));
usbd_status	uhci_device_intr_interval	__P((uhci_softc_t *sc, 
						struct uhci_pipe *upipe,
						int ival));

usbd_status	uhci_device_isoc_transfer	__P((usbd_request_handle reqh));
usbd_status	uhci_device_isoc_start		__P((usbd_request_handle reqh));
void		uhci_device_isoc_abort		__P((usbd_request_handle reqh));
void		uhci_device_isoc_close		__P((usbd_pipe_handle pipe));
usbd_status	uhci_device_isoc_setbuf		__P((usbd_pipe_handle pipe,
						u_int bufsize, u_int nbuf));

usbd_status	uhci_root_ctrl_transfer		__P((usbd_request_handle reqh));
usbd_status	uhci_root_ctrl_start		__P((usbd_request_handle reqh));
void		uhci_root_ctrl_abort		__P((usbd_request_handle reqh));
void		uhci_root_ctrl_close		__P((usbd_pipe_handle pipe));

usbd_status	uhci_root_intr_transfer		__P((usbd_request_handle reqh));
usbd_status	uhci_root_intr_start		__P((usbd_request_handle reqh));
void		uhci_root_intr_abort		__P((usbd_request_handle reqh));
void		uhci_root_intr_close		__P((usbd_pipe_handle pipe));
void		uhci_root_intr_sim		__P((void *priv));


void		uhci_add_ctrl		__P((uhci_softc_t *sc, uhci_soft_qh_t *sqh));
void		uhci_remove_ctrl	__P((uhci_softc_t *sc, uhci_soft_qh_t *sqh));

void		uhci_add_bulk		__P((uhci_softc_t *sc, uhci_soft_qh_t *sqh));
void		uhci_remove_bulk	__P((uhci_softc_t *sc, uhci_soft_qh_t *sqh));

void		uhci_add_intr		__P((uhci_softc_t *, int pos, uhci_soft_qh_t *sqh));
void		uhci_remove_intr	__P((uhci_softc_t *, int pos, uhci_soft_qh_t *sqh));
/* isochroneous mode transfers not yet supported */

/* the simulated root hub */
usbd_status	uhci_roothub_ctrl_transfer	__P((uhci_softc_t *sc,
						usb_device_request_t *req,
						void *buf, int *actlen));
usbd_status	uhci_roothub_intr_transfer	__P((uhci_softc_t *sc,
						u_int8_t *buf, int buflen, int *actlen));
int 		uhci_roothub_string_descriptor	__P((usb_string_descriptor_t *sd,
						int datalen, char *string));

#ifdef UHCI_DEBUG
void		uhci_dumpregs __P((uhci_softc_t *));
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



usbd_status
uhci_init(uhci_softc_t *sc)
{
	usbd_status err;
	usb_dma_t dma;

	DPRINTFN(1,("uhci_init: start\n"));

	uhci_run(sc, 0);			/* stop the controller */
#if defined(__NetBSD__)
	UWRITE2(sc, UHCI_INTR, 0);		/* disable interrupts */
#elif defined(__FreeBSD__)
	/*
	 * FreeBSD does this in the probe of the chip. Otherwise we
	 * get spurious interrupts
	 */
#endif

	uhci_busreset(sc);
	
	/* Allocate and initialize real frame array. */
	err = usb_allocmem(sc->sc_dmatag, 
			 UHCI_FRAMELIST_COUNT * sizeof(uhci_physaddr_t),
			 UHCI_FRAMELIST_ALIGN, &dma);
	if (err)
		return(err);

	sc->sc_pframes = KERNADDR(&dma);
	sc->sc_flbase  = DMAADDR(&dma);

	err = uhci_init_framelist(sc);
	if (err) {
		usb_freemem(sc->sc_dmatag, &dma);
		return(err);
	}

	LIST_INIT(&sc->sc_intrhead);

	/* Set up the bus struct. */
	sc->sc_bus.open_pipe = uhci_open;
	sc->sc_bus.pipe_size = sizeof(struct uhci_pipe);
	sc->sc_bus.do_poll = uhci_poll;

	DPRINTFN(1,("uhci_init: enabling\n"));
	return uhci_reset(sc);
}

usbd_status
uhci_init_framelist(uhci_softc_t *sc)
{
	uhci_soft_qh_t *csqh, *bsqh, *sqh;
	uhci_soft_td_t *std;
	int i, j;

	/* see uhcivar.h for an explanation of the queuing used */

	/* Allocate the QH where bulk traffic will be queued. */
	bsqh = uhci_alloc_sqh(sc);
	if (!bsqh)
		return(USBD_NOMEM);
	bsqh->qh->qh_hlink = UHCI_PTR_T;	/* end of QH chain */
	bsqh->qh->qh_elink = UHCI_PTR_T;
	sc->sc_bulk_start = sc->sc_bulk_end = bsqh;

	/* Allocate the QH where control traffic will be queued. */
	csqh = uhci_alloc_sqh(sc);
	if (!csqh) {
		uhci_free_sqh(sc, bsqh);
		return(USBD_NOMEM);
	}
	csqh->qh->hlink = bsqh;			/* link to bulk QH */
	csqh->qh->qh_hlink = bsqh->physaddr | UHCI_PTR_Q;
	csqh->qh->qh_elink = UHCI_PTR_T;
	sc->sc_ctl_start = sc->sc_ctl_end = csqh;

	/* 
	 * Make all (virtual) frame list pointers point to the interrupt
	 * queue heads and the interrupt queue heads point to the control
	 * queue head. Insert the elements for the virtual frame list multiple
	 * times in the physical framelist
	 * (UHCI_FRAMELIST_COUNT/UHCI_VFRAMELIST_COUNT times).
	 */
	for(i = 0; i < UHCI_VFRAMELIST_COUNT; i++) {
		/* Allocate the iso TD and the interrupt QH */
		std = uhci_alloc_std(sc);
		sqh = uhci_alloc_sqh(sc);
		if (!std || !sqh) {
			/* not allocated -> free the lot we've done previously */
			if (std)
				uhci_free_std(sc, std);
			for (i--; i >= 0; i--) {
				std = sc->sc_vframes[i].htd;
				sqh = std->td->link.sqh;
				uhci_free_sqh(sc, sqh);
				uhci_free_std(sc, std);
			}
			uhci_free_sqh(sc, csqh);
			uhci_free_sqh(sc, bsqh);
			return(USBD_NOMEM);
		}

		/* QH for interrupt transfers */
		sqh->qh->hlink = csqh;			/* link to control QH */
		sqh->qh->qh_hlink = csqh->physaddr | UHCI_PTR_Q;
		sqh->qh->elink = NULL;
		sqh->qh->qh_elink = UHCI_PTR_T;

		/* dummy TD for isochroneous transfers */
		std->td->link.sqh = sqh;		/* link to inter. QH */
		std->td->td_link = sqh->physaddr | UHCI_PTR_Q;
		std->td->td_status = UHCI_TD_IOS;	/* iso, inactive */
		std->td->td_token = 0;
		std->td->td_buffer = NULL;

		/* enter the iso TD in the virtual frame list */
		sc->sc_vframes[i].htd = std;
		sc->sc_vframes[i].etd = std;
		sc->sc_vframes[i].hqh = sqh;
		sc->sc_vframes[i].eqh = sqh;

		/*
		 * copy the entry in the virtual frame list
		 * UHCI_FRAMELIST_COUNT/UHCI_VFRAMELIST_COUNT times
		 */

		for (j = i; 
		     j < UHCI_FRAMELIST_COUNT; 
		     j += UHCI_VFRAMELIST_COUNT)
			sc->sc_pframes[j] = std->physaddr;
	}

	return(USBD_NORMAL_COMPLETION);
}

void
uhci_busreset(sc)
	uhci_softc_t *sc;
{
	UHCICMD(sc, UHCI_CMD_GRESET);	/* global reset */
	usb_delay_ms(&sc->sc_bus, USB_BUS_RESET_DELAY); /* wait a little */
	UHCICMD(sc, 0);			/* do nothing */
}

usbd_status
uhci_reset(sc)
	uhci_softc_t *sc;
{
	int n;

	/* Reset the host controller */

	UHCICMD(sc, UHCI_CMD_HCRESET);
	/* The reset bit goes low when the controller is done. */
	for (n = 0; n < UHCI_RESET_TIMEOUT && 
		    (UREAD2(sc, UHCI_CMD) & UHCI_CMD_HCRESET); n++)
		delay(100);
	if (n >= UHCI_RESET_TIMEOUT)
		printf("%s: controller did not reset\n", 
		       USBDEVNAME(sc->sc_bus.bdev));
	UWRITE2(sc, UHCI_FRNUM, 0);			/* set frame number to 0 */
	UWRITE4(sc, UHCI_FLBASEADDR, sc->sc_flbase);	/* set frame list address */
	UWRITE2(sc, UHCI_INTR, UHCI_INTR_TOCRCIE | UHCI_INTR_RIE | 
		UHCI_INTR_IOCE | UHCI_INTR_SPIE);	/* enable interrupts */

	return(uhci_run(sc, 1));		/* and here we go... */
}

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
		return(USBD_NORMAL_COMPLETION);
	}
	UWRITE2(sc, UHCI_CMD, run ? UHCI_CMD_RS : 0);
	for(n = 0; n < 10; n++) {
		running = ((UREAD2(sc, UHCI_STS) & UHCI_STS_HCH) == 0);
		/* return when we've entered the state we want */
		if (run == running) {
			splx(s);
			return(USBD_NORMAL_COMPLETION);
		}
		usb_delay_ms(&sc->sc_bus, 1);
	}
	splx(s);
	printf("%s: cannot %s\n", USBDEVNAME(sc->sc_bus.bdev),
	       run ? "start" : "stop");
	return(USBD_IOERROR);
}


/*
 * check whether the host controller has flagged an
 * interrupt.
 */
void
uhci_poll(bus)
	struct usbd_bus *bus;
{
	uhci_softc_t *sc = (uhci_softc_t *)bus;

	if (UREAD2(sc, UHCI_STS) & UHCI_STS_USBINT)
		uhci_intr(sc);
}

/*
 * Wait here until controller claims to have an interrupt.
 * Then call uhci_intr and return.  Use timeout to avoid waiting
 * too long.
 * Only used during boot when interrupts are not enabled yet.
 * XXX this function is not re-entrant *
 */
void
uhci_waitintr(sc, reqh)
	uhci_softc_t *sc;
	usbd_request_handle reqh;
{
	int timeout = reqh->timeout;
	int usecs;
	uhci_intr_info_t *ii;

	DPRINTFN(15,("uhci_waitintr: timeout = %ds\n", timeout));

	/* XXX NWH setting status here might give race condition */
	reqh->status = USBD_IN_PROGRESS;

	for (usecs = timeout * 1000000 / hz; usecs > 0; usecs -= 1000) {
		uhci_poll(&sc->sc_bus);
		if (reqh->status != USBD_IN_PROGRESS)
			return;

		usb_delay_ms(&sc->sc_bus, 1);
	}

	/* Timeout */

	DPRINTF(("uhci_waitintr: timeout\n"));

	/* Find the intr info in the queue */
	for (ii = LIST_FIRST(&sc->sc_intrhead);
	     ii && ii->reqh != reqh; 
	     ii = LIST_NEXT(ii, list))
		/* noop */ ;

	if (ii)
		uhci_ii_done(ii, 1);
	else
		/* this can only happen if there are 2 or more tasks
		 * polling or interrupts are enabled. This is not
		 * possible during boot.
		 * In that case the request has been handled already.
		 * If it does happen this should be non-fatal.
		 */
#ifdef UHCI_DEBUG
		panic("lost intr_info\n");
#else
		printf("lost intr_info\n");
#endif
}


/*
 * Called when a request does not complete.
 */
void
uhci_timeout(priv)
	void *priv;
{
	uhci_intr_info_t *ii = priv;

	uhci_ii_done(ii, 1);
}

/*
 * Handle interrupt from the host controller. We search the list of TDs
 * for completed ones and call uhci_ii_done for those.
 */
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
	if (uhcidebug > 15) {
		DPRINTF(("%s: uhci_intr\n", USBDEVNAME(sc->sc_bus.bdev)));
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
		printf("%s: resume detect\n",
			USBDEVNAME(sc->sc_bus.bdev));
	}
	if (status & UHCI_STS_HSE) {
		ack |= UHCI_STS_HSE;
		printf("%s: host controller process error\n",
			USBDEVNAME(sc->sc_bus.bdev));
	}
	if (status & UHCI_STS_HCPE) {
		ack |= UHCI_STS_HCPE;
		printf("%s: host system error\n",
			USBDEVNAME(sc->sc_bus.bdev));
	}
	if (status & UHCI_STS_HCH) {
		/* no acknowledge needed */
		printf("%s: host controller halted\n",
			USBDEVNAME(sc->sc_bus.bdev));
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


/*
 * Check the list of TDs for completeness.
 * If there is an error in the middle of the list of TDs or
 * a short packet, retire the list and call uhci_ii_done for the ii
 */
void
uhci_check_intr(sc, ii)
	uhci_softc_t *sc;
	uhci_intr_info_t *ii;
{
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

	if (ii->stdend->td->td_status & UHCI_TD_ACTIVE) {
		/*
		 * If the last TD is still active we need to check whether there
		 * is a an error somewhere in the middle or whether there was a
		 * short packet (SPD and not ACTIVE).
		 */

		for (std = ii->stdstart;
		     std != ii->stdend;
		     std = std->td->link.std) {
			status = std->td->td_status;
			if ((status & UHCI_TD_STALLED) ||
			     (status&(UHCI_TD_SPD|UHCI_TD_ACTIVE))==UHCI_TD_SPD)
				goto done;
		}
		return;
	}

done:
	usb_untimeout(uhci_timeout, ii, ii->timeout_handle);
	uhci_ii_done(ii, 0);
}


void
uhci_ii_done(ii, timedout)
	uhci_intr_info_t *ii;
	int timedout;		/* timeout that triggered function call? */
{
	usbd_request_handle reqh = ii->reqh;
	uhci_soft_td_t *std;
				/* error status of last inactive transfer */
	usbd_status err = USBD_NORMAL_COMPLETION;
	int actlen = 0;		/* accumulated actual length for queue */
	int s;

#ifdef DIAGNOSTIC
	{
		/* avoid finishing a transfer more than once */
		int s = splusb();
		if (ii->isdone) {
			splx(s);
			printf("uhci_ii_done: is done!\n");
			return;
		}
		ii->isdone = 1;
		splx(s);
	}
#endif

	/*
	 * The transfer is done; compute actual length and status
	 * XXX Is this correct for control transfers? Should not
	 * only the data stage be calculated?
	 */
	for (std = ii->stdstart; std; std = std->td->link.std) {
		if (std->td->td_status & UHCI_TD_ACTIVE)
			break;

		/* error status of last TD for error handling below */
		err = std->td->td_status & UHCI_TD_ERROR;

		if (UHCI_TD_GET_PID(std->td->td_token) != UHCI_TD_PID_SETUP)
			actlen += UHCI_TD_GET_ACTLEN(std->td->td_status);
	}

	DPRINTFN(10, ("uhci_ii_done: ii=%p%s, actlen=%d err=0x%x\n",
			ii, timedout? " timed out":"", actlen, err));
#ifdef UHCI_DEBUG
	if (uhcidebug > 10 && (err || timedout))
		uhci_dump_tds(ii->stdstart);
#endif

	if (err) {
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

	if (timedout) {
		s = splusb();
		/* We got a timeout.  Make sure transaction is not active. */
		for (std = ii->stdstart; std != 0; std = std->td->link.std)
			std->td->td_status &= ~UHCI_TD_ACTIVE;
		splx(s);

		/* XXX should we wait 1 ms */
		reqh->status = USBD_TIMEOUT;
	}

	/* select the proper type termination of the transfer
	 * based on the transfer type for the queue
	 */
	switch (reqh->pipe->endpoint->edesc->bmAttributes & UE_XFERTYPE) {
	case UE_CONTROL:
		uhci_ctrl_done(ii);
		usb_start_next(reqh->pipe);
		break;
	case UE_BULK:
		uhci_bulk_done(ii);
		usb_start_next(reqh->pipe);
		break;
	case UE_INTERRUPT:
		uhci_intr_done(ii);
		break;
	case UE_ISOCHRONOUS:
		uhci_isoc_done(ii);
		usb_start_next(reqh->pipe);
		break;
	}

	/* And finally execute callback. */
	reqh->xfercb(reqh);
}

/* Deallocate request data structures */
void
uhci_ctrl_done(ii)
	uhci_intr_info_t *ii;
{
	uhci_softc_t *sc = ii->sc;
	usbd_request_handle reqh = ii->reqh;
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
	u_int datalen = upipe->u.ctl.length;
	usb_dma_t *dma;

#ifdef DIAGNOSTIC
	if (!reqh->isreq)
		panic("uhci_ctrl_done: not a request\n");
#endif

	LIST_REMOVE(ii, list);	/* remove from active list */

	uhci_remove_ctrl(sc, upipe->u.ctl.sqh);

	if (datalen != 0) {
		/* there was a data stage */
		dma = &upipe->u.ctl.datadma;
		if (reqh->request.bmRequestType & UT_READ)
			memcpy(reqh->buffer, KERNADDR(dma), datalen);

		/*
		 * when freeing the chain skip the first (setup) and last
		 * (status) TD.
		 */
		uhci_free_std_chain(sc, ii->stdstart->td->link.std, ii->stdend);
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
	uhci_soft_td_t *std;
	u_int datalen = upipe->u.bulk.length;
	usb_dma_t *dma;

	LIST_REMOVE(ii, list);	/* remove from active list */

	uhci_remove_bulk(sc, upipe->u.bulk.sqh);

	/* find the toggle for the last transfer and invert it */
	for (std = ii->stdstart; std; std = std->td->link.std) {
		if (std->td->td_status & UHCI_TD_ACTIVE)
			break;

		upipe->nexttoggle = UHCI_TD_GET_DT(std->td->td_token);
	}
	upipe->nexttoggle ^= 1;

	/* copy the data from dma memory to userland storage */
	dma = &upipe->u.bulk.datadma;
	if (upipe->u.bulk.isread)
		memcpy(reqh->buffer, KERNADDR(dma), datalen);

	/* free the whole chain of TDs */
	uhci_free_std_chain(sc, ii->stdstart, 0);
	usb_freemem(sc->sc_dmatag, dma);
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
	uhci_free_std_chain(sc, ii->stdstart, NULL);

	/* XXX Wasteful. */
	if (reqh->pipe->intrreqh == reqh
	    && reqh->status == USBD_NORMAL_COMPLETION) {
		uhci_soft_td_t *std, *stdend;

		/* This alloc cannot fail since we freed the chain above. */
		upipe->pipe.endpoint->toggle = upipe->nexttoggle;
		uhci_alloc_std_chain(sc, upipe, reqh->length, 1,
				     reqh->flags & USBD_SHORT_XFER_OK,
				     dma, &std, &stdend);
		stdend->td->td_status |= UHCI_TD_IOC;

#ifdef UHCI_DEBUG
		if (uhcidebug > 10) {
			DPRINTF(("uhci_device_intr_done: xfer\n"));
			uhci_dump_tds(std);
			uhci_dump_qh(upipe->u.intr.qhs[0]);
		}
#endif

		ii->stdstart = std;
		ii->stdend = stdend;
#ifdef DIAGNOSTIC
		ii->isdone = 0;
#endif
		for (i = 0; i < npoll; i++) {
			sqh = upipe->u.intr.qhs[i];
			sqh->qh->elink = std;
			sqh->qh->qh_elink = std->physaddr;
		}
	} else {
		usb_freemem(sc->sc_dmatag, dma);
		ii->stdstart = NULL;	/* mark as inactive */
	}
}

void
uhci_isoc_done(ii)
	uhci_intr_info_t *ii;
{
}

/*
 * Memory management routines.
 *  uhci_alloc_std allocates TDs
 *  uhci_alloc_std_chain allocates a chain of TDs
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
	usbd_status err;
	int i;
	usb_dma_t dma;

	if (!sc->sc_freetds) {
		DPRINTFN(2,("uhci_alloc_std: allocating chunk\n"));
		std = malloc(sizeof(uhci_soft_td_t) * UHCI_TD_CHUNK, 
			     M_USBDEV, M_NOWAIT);
		if (!std)
			return(0);
		err = usb_allocmem(sc->sc_dmatag, UHCI_TD_SIZE * UHCI_TD_CHUNK,
				 UHCI_TD_ALIGN, &dma);
		if (err != USBD_NORMAL_COMPLETION) {
			free(std, M_USBDEV);
			return(0);
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
	if (!std)
#ifdef UHCI_DEBUG
		panic("invalid TD to be freed, std=%p", std);
#else
		return;
#endif

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

/* allocates and prepares a chain of TDs */
usbd_status
uhci_alloc_std_chain(sc, upipe, datalen, isread, spd, dma, rstd, rstdend)
	uhci_softc_t *sc;
	struct uhci_pipe *upipe;
	int datalen;
	int isread, spd;
	usb_dma_t *dma;
	uhci_soft_td_t **rstd, **rstdend;
{
	uhci_soft_td_t *std = NULL;		/* soft TD we are working on */
	uhci_soft_td_t *stdprev = NULL;		/* std from prev iteration */
	uhci_physaddr_t linkprev = UHCI_PTR_T;	/* links real TDs together */
	int i;					/* index over TDs */
	int ntd;				/* number of TDs */
	int l;					/* len of data in current std */
	int tog;				/* current data toggle */
	int maxpacketsize;
	u_int32_t status;			/* pre computed status for TD */
	int addr = upipe->pipe.device->address;	/* shortcuts */
	int endpt = upipe->pipe.endpoint->edesc->bEndpointAddress;

	maxpacketsize = UGETW(upipe->pipe.endpoint->edesc->wMaxPacketSize);
	if (maxpacketsize == 0) {
		printf("uhci_alloc_std_chain: maxpacketsize = 0\n");
		return(USBD_INVAL);
	}

	ntd = (datalen + maxpacketsize - 1) / maxpacketsize - 1;
	tog = upipe->pipe.endpoint->toggle;
	if (ntd % 2 == 1)
		/* toggle for last TD, list of TDs is initialised backwards */
		tog ^= 1;

	/*
	 * save the toggle for the first TD of the next transfer so we can
	 * simply copy the value in transfers that transfer all the TDs.  Bulk
	 * with n out of m TDs transferrred have to recompute though.
	 */
	upipe->nexttoggle = tog ^ 1;

	DPRINTFN(10, ("uhci_alloc_std_chain: addr=%d endpt=%d datalen=%d "
			"toggle=%d, nexttoggle=%d, %s%s%s\n",
			addr, endpt, datalen, 
			upipe->pipe.endpoint->toggle, upipe->nexttoggle,
			isread? "read":"write",
			upipe->pipe.device->lowspeed? ", lowspeed":"",
			spd? ", short packet":""));

	if (datalen == 0) {
		*rstd = *rstdend = 0;
		return(USBD_NORMAL_COMPLETION);
	}

	status = UHCI_TD_SET_ERRCNT(3) | UHCI_TD_ACTIVE;
	if (upipe->pipe.device->lowspeed)
		status |= UHCI_TD_LOWSPEED;
	if (spd)
		status |= UHCI_TD_SPD;

	/*
	 * create a list of std's, backwards. stdprev contains the std
	 * from the previous iteration.
	 */
	for (i = ntd; i >= 0; i--) {
		std = uhci_alloc_std(sc);
		if (!std) {
			uhci_free_std_chain(sc, stdprev, NULL);
			return(USBD_NOMEM);
		}
		
		std->td->link.std = stdprev;
		stdprev = std;
		std->td->td_link = linkprev;
		linkprev = std->physaddr;
		std->td->td_status = status;
		if (i == ntd) {			/* compute length of TD */
			/* last TD is 0 > l >= maxPacketSize */
			l = datalen % maxpacketsize;
			if (l == 0)
				l = maxpacketsize;

			*rstdend = std;		/* end of list of TDs */
		} else
			/* all other TDs should be max size */
			l = maxpacketsize;

		std->td->td_token = 
		    isread ? UHCI_TD_IN(l, endpt, addr, tog) :
			 UHCI_TD_OUT(l, endpt, addr, tog);
		std->td->td_buffer = DMAADDR(dma) + i * maxpacketsize;

		tog ^= 1;
	}
	*rstd = stdprev;

	return(USBD_NORMAL_COMPLETION);
}

void
uhci_free_std_chain(sc, std, stdend)
	uhci_softc_t *sc;
	uhci_soft_td_t *std;
	uhci_soft_td_t *stdend;
{
	uhci_soft_td_t *std_link;	/* temp store of next pointer */

	/* removes the chain up to (but excluding) the element stdend */

	if (!std)
#ifdef UHCI_DEBUG
		panic("invalid TD chain to be freed, std=%p", std);
#else
		return;
#endif

	for (; std != stdend; std = std_link) {
		std_link = std->td->link.std;
		uhci_free_std(sc, std);
	}
}

uhci_soft_qh_t *
uhci_alloc_sqh(sc)
	uhci_softc_t *sc;
{
	uhci_soft_qh_t *sqh;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;

	if (!sc->sc_freeqhs) {
		DPRINTFN(2, ("uhci_alloc_sqh: allocating chunk\n"));
		sqh = malloc(sizeof(uhci_soft_qh_t) * UHCI_QH_CHUNK, 
			     M_USBDEV, M_NOWAIT);
		if (!sqh)
			return NULL;
		err = usb_allocmem(sc->sc_dmatag, UHCI_QH_SIZE * UHCI_QH_CHUNK,
				 UHCI_QH_ALIGN, &dma);
		if (err != USBD_NORMAL_COMPLETION) {
			free(sqh, M_USBDEV);
			return NULL;
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
	return(sqh);
}

void
uhci_free_sqh(sc, sqh)
	uhci_softc_t *sc;
	uhci_soft_qh_t *sqh;
{
	if (!sqh)		/* safety net */
#ifdef UHCI_DEBUG
		panic("invalid QH to be freed, sqh=%p", sqh);
#else
		return;
#endif

	sqh->qh->hlink = sc->sc_freeqhs;
	sc->sc_freeqhs = sqh;
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
	if (!ii)
#ifdef UHCI_DEBUG
		panic("invalid intr info to be freed, ii=%p", ii);
#else
		return;
#endif

	LIST_INSERT_HEAD(&uhci_ii_free, ii, list); /* and put on free list */
}


/*
 * request and release lock on the frames list
 */

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


struct usbd_methods uhci_device_ctrl_methods = {
	uhci_device_ctrl_transfer,
	uhci_device_ctrl_start,
	uhci_device_ctrl_abort,
	uhci_device_ctrl_close,
	0,
};

struct usbd_methods uhci_device_bulk_methods = {
	uhci_device_bulk_transfer,
	uhci_device_bulk_start,
	uhci_device_bulk_abort,
	uhci_device_bulk_close,
	0,
};

struct usbd_methods uhci_device_intr_methods = {
	uhci_device_intr_transfer,
	uhci_device_intr_start,
	uhci_device_intr_abort,
	uhci_device_intr_close,
	0,
};

struct usbd_methods uhci_device_isoc_methods = {
	uhci_device_isoc_transfer,
	uhci_device_isoc_start,
	uhci_device_isoc_abort,
	uhci_device_isoc_close,
	uhci_device_isoc_setbuf,
};

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


usbd_status
uhci_open(pipe)
	usbd_pipe_handle pipe;
{
	uhci_softc_t *sc = (uhci_softc_t *)pipe->device->bus;
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	usbd_status err;

	DPRINTFN(1, ("uhci_open: pipe=%p, addr=%d, endpt=%d (%d)\n",
		     pipe, pipe->device->address, 
		     ed->bEndpointAddress, sc->sc_addr));

	if (pipe->device->address == sc->sc_addr) {
		/* root hub */
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &uhci_root_ctrl_methods;
			break;
		case UE_IN | UHCI_INTR_ENDPT:
			pipe->methods = &uhci_root_intr_methods;
			break;
		default:
			return(USBD_INVAL);
		}
		
	} else {
		upipe->iinfo = uhci_alloc_intr_info(sc);
		if (upipe->iinfo == 0)
			return(USBD_NOMEM);

		upipe->nexttoggle = 0;

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
			err = usb_allocmem(sc->sc_dmatag, 
					 sizeof(usb_device_request_t), 
					 0, &upipe->u.ctl.reqdma);
			if (err != USBD_NORMAL_COMPLETION) {
				uhci_free_sqh(sc, upipe->u.ctl.sqh);
				uhci_free_std(sc, upipe->u.ctl.setup);
				uhci_free_std(sc, upipe->u.ctl.stat);
				goto bad;
			}
			break;
		case UE_INTERRUPT:
			pipe->methods = &uhci_device_intr_methods;
			return(uhci_device_intr_interval(sc, upipe, ed->bInterval));
		case UE_ISOCHRONOUS:
			pipe->methods = &uhci_device_isoc_methods;
			upipe->u.iso.nbuf = 0;
			return(USBD_NORMAL_COMPLETION);
		case UE_BULK:
			pipe->methods = &uhci_device_bulk_methods;
			upipe->u.bulk.sqh = uhci_alloc_sqh(sc);
			if (upipe->u.bulk.sqh == 0)
				goto bad;
			break;
		}
	}
	return(USBD_NORMAL_COMPLETION);

 bad:
	uhci_free_intr_info(upipe->iinfo);
	return(USBD_NOMEM);
}


/* Control transfers are slightly more complicated as they consist of three
 * phases. This subroutine creates the three phases and schedules the chain
 */

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
	uhci_soft_td_t *std, *stdend;
	uhci_soft_td_t *setup, *stat, *next;
	uhci_soft_qh_t *sqh;
	usb_dma_t *dmap;
	int datalen;
	u_int32_t ls;
	usbd_status err;
	int isread;
	int s;

	DPRINTFN(3,("uhci_device_request: bmRequestType=0x%02x, bRequest=0x%02x, "
		    "wValue=0x%04x, wIndex=0x%04x, wLength=%d, addr=%d, endpt=%d\n",
		    req->bmRequestType, req->bRequest, UGETW(req->wValue),
		    UGETW(req->wIndex), UGETW(req->wLength),
		    addr, endpt));

	ls = dev->lowspeed ? UHCI_TD_LOWSPEED : 0;
	isread = req->bmRequestType & UT_READ;
	datalen = UGETW(req->wLength);

	setup = upipe->u.ctl.setup;
	stat = upipe->u.ctl.stat;
	sqh = upipe->u.ctl.sqh;
	dmap = &upipe->u.ctl.datadma;

	if (datalen != 0) {
		/* initialise the data stage */

		err = usb_allocmem(sc->sc_dmatag, datalen, 0, dmap);
		if (err != USBD_NORMAL_COMPLETION)
			return(err);

		/*
		 * data toggle starts at 0 with control requests, so first
		 * data packet has toggle 1
		 */
		upipe->pipe.endpoint->toggle = 1;
		err = uhci_alloc_std_chain(sc, upipe, datalen, isread, 
					 reqh->flags & USBD_SHORT_XFER_OK,
					 dmap, &std, &stdend);
		if (err != USBD_NORMAL_COMPLETION) {
			usb_freemem(sc->sc_dmatag, dmap);
			return(err);
		}

		if (!isread)
			memcpy(KERNADDR(dmap), reqh->buffer, datalen);

		stdend->td->link.std = stat;
		stdend->td->td_link = stat->physaddr;

		next = std;
	} else {
		next = stat;
	}

	upipe->u.ctl.length = datalen;


	/*
	 * initialise the setup stage and link it to either the data stage
	 * or the status stage (in the case where there is no data stage)
	 */
	setup->td->link.std = next;
	setup->td->td_link = next->physaddr;
	setup->td->td_status = UHCI_TD_SET_ERRCNT(3) | ls | UHCI_TD_ACTIVE;
	setup->td->td_token = UHCI_TD_SETUP(sizeof *req, endpt, addr);
	setup->td->td_buffer = DMAADDR(&upipe->u.ctl.reqdma);
	memcpy(KERNADDR(&upipe->u.ctl.reqdma), req, sizeof *req);

	/* initialise the status stage */
	stat->td->link.std = 0;
	stat->td->td_link = UHCI_PTR_T;
	stat->td->td_status = UHCI_TD_SET_ERRCNT(3) | ls | 
		UHCI_TD_ACTIVE | UHCI_TD_IOC;
	stat->td->td_token = 
		isread ? UHCI_TD_OUT(0, endpt, addr, 1) :
		         UHCI_TD_IN (0, endpt, addr, 1);
	stat->td->td_buffer = 0;

	/* initialise interrupt info. */
	ii->reqh = reqh;
	ii->stdstart = setup;
	ii->stdend = stat;
#if defined(__FreeBSD__)
	callout_handle_init(&ii->timeout_handle);
#endif
#ifdef DIAGNOSTIC
	ii->isdone = 0;
#endif

#ifdef UHCI_DEBUG
	if (uhcidebug > 10)
		uhci_dump_tds(setup);
#endif

	sqh->qh->elink = setup;
	sqh->qh->qh_elink = setup->physaddr;
	sqh->intr_info = ii;

	s = splusb();
	uhci_add_ctrl(sc, sqh);
	LIST_INSERT_HEAD(&sc->sc_intrhead, ii, list);
	if (reqh->timeout && !sc->sc_bus.use_polling) {
		usb_timeout(uhci_timeout, ii,
                            MS_TO_TICKS(reqh->timeout), ii->timeout_handle);
	}
	splx(s);

	return(USBD_NORMAL_COMPLETION);
}


usbd_status
uhci_device_ctrl_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status err;

	s = splusb();
	err = usb_insert_transfer(reqh);
	splx(s);
	if (err != USBD_NORMAL_COMPLETION)
		return(err);
	else
		return(uhci_device_ctrl_start(reqh));
}

usbd_status
uhci_device_ctrl_start(reqh)
	usbd_request_handle reqh;
{
	uhci_softc_t *sc = (uhci_softc_t *)reqh->pipe->device->bus;
	usbd_status err;

	if (!reqh->isreq)
		panic("uhci_device_ctrl_start: not a request\n");

	err = uhci_device_request(reqh);
	if (err != USBD_NORMAL_COMPLETION)
		return(err);

	if (sc->sc_bus.use_polling)
		uhci_waitintr(sc, reqh);
	return(USBD_IN_PROGRESS);
}


void
uhci_device_ctrl_abort(reqh)
	usbd_request_handle reqh;
{
	/* XXX inactivate */
	usb_delay_ms(reqh->pipe->device->bus, 1); /* make sure it is done */
	/* XXX call done */
}

void
uhci_device_ctrl_close(pipe)
	usbd_pipe_handle pipe;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;

	uhci_free_intr_info(upipe->iinfo);
	/* XXX free other resources */
}

usbd_status
uhci_device_bulk_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status err;

	s = splusb();
	err = usb_insert_transfer(reqh);
	splx(s);
	if (err != USBD_NORMAL_COMPLETION)
		return(err);
	else
		return(uhci_device_bulk_start(reqh));
}

usbd_status
uhci_device_bulk_start(reqh)
	usbd_request_handle reqh;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
	usbd_device_handle dev = upipe->pipe.device;
	uhci_softc_t *sc = (uhci_softc_t *)dev->bus;
	uhci_intr_info_t *ii = upipe->iinfo;
	uhci_soft_td_t *std, *stdend;
	uhci_soft_qh_t *sqh;
	usb_dma_t *dmap;
	usbd_status err;
	int datalen, isread;
	int s;

	DPRINTFN(3, ("uhci_device_bulk_start: reqh=%p buf=%p datalen=%d "
		     "flags=%d\n",
		     reqh, reqh->buffer, reqh->length, reqh->flags));

	if (reqh->isreq)
		panic("uhci_device_bulk_start: a request\n");

	if (reqh->length == 0)
		return(USBD_INVAL);

	datalen = reqh->length;
	dmap = &upipe->u.bulk.datadma;
	isread = reqh->pipe->endpoint->edesc->bEndpointAddress & UE_IN;
	sqh = upipe->u.bulk.sqh;

	upipe->u.bulk.isread = isread;
	upipe->u.bulk.length = datalen;

	err = usb_allocmem(sc->sc_dmatag, datalen, 0, dmap);
	if (err != USBD_NORMAL_COMPLETION)
		return(err);

	upipe->pipe.endpoint->toggle = upipe->nexttoggle;
	err = uhci_alloc_std_chain(sc, upipe, datalen, isread, 
				 reqh->flags & USBD_SHORT_XFER_OK,
				 dmap, &std, &stdend);
	if (err != USBD_NORMAL_COMPLETION) {
		usb_freemem(sc->sc_dmatag, dmap);
		return(err);
	}

	stdend->td->td_status |= UHCI_TD_IOC;

	if (!isread)
		memcpy(KERNADDR(dmap), reqh->buffer, datalen);

#ifdef UHCI_DEBUG
	if (uhcidebug > 10) {
		DPRINTF(("uhci_device_bulk_start: xfer\n"));
		uhci_dump_tds(std);
	}
#endif

	/* Set up interrupt info. */
	ii->reqh = reqh;
	ii->stdstart = std;
	ii->stdend = stdend;
#if defined(__FreeBSD__)
	callout_handle_init(&ii->timeout_handle);
#endif
#ifdef DIAGNOSTIC
	ii->isdone = 0;
#endif

	sqh->qh->elink = std;
	sqh->qh->qh_elink = std->physaddr;
	sqh->intr_info = ii;

	s = splusb();
	uhci_add_bulk(sc, sqh);
	LIST_INSERT_HEAD(&sc->sc_intrhead, ii, list);

	if (reqh->timeout && !sc->sc_bus.use_polling) {
		usb_timeout(uhci_timeout, ii,
                            MS_TO_TICKS(reqh->timeout), ii->timeout_handle);
	}
	splx(s);

	return(USBD_IN_PROGRESS);
}

void
uhci_device_bulk_abort(reqh)
	usbd_request_handle reqh;
{
	/* XXX inactivate */
	usb_delay_ms(reqh->pipe->device->bus, 1);/* make sure it is done */
	/* XXX call done */
}

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
uhci_device_intr_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status err;

	s = splusb();
	err = usb_insert_transfer(reqh);
	splx(s);
	if (err != USBD_NORMAL_COMPLETION)
		return(err);
	else
		return(uhci_device_intr_start(reqh));
}

usbd_status
uhci_device_intr_start(reqh)
	usbd_request_handle reqh;
{
	struct uhci_pipe *upipe = (struct uhci_pipe *)reqh->pipe;
	usbd_device_handle dev = upipe->pipe.device;
	uhci_softc_t *sc = (uhci_softc_t *)dev->bus;
	uhci_intr_info_t *ii = upipe->iinfo;
	uhci_soft_td_t *std, *stdend;
	uhci_soft_qh_t *sqh;
	usb_dma_t *dmap;
	usbd_status err;
	int datalen, i;
	int s;

	DPRINTFN(3, ("uhci_device_intr_start: reqh=%p buf=%p datalen=%d "
		     "flags=%d\n",
		     reqh, reqh->buffer, reqh->length, reqh->flags));

	if (reqh->isreq)
		panic("uhci_device_intr_start: a request\n");

	datalen = reqh->length;
	dmap = &upipe->u.intr.datadma;
	if (datalen == 0)
		return(USBD_INVAL); /* XXX should it be? */

	err = usb_allocmem(sc->sc_dmatag, datalen, 0, dmap);
	if (err != USBD_NORMAL_COMPLETION)
		return(err);

	upipe->pipe.endpoint->toggle = upipe->nexttoggle;
	err = uhci_alloc_std_chain(sc, upipe, datalen, 1,
 				 reqh->flags & USBD_SHORT_XFER_OK,
				 dmap, &std, &stdend);
	if (err != USBD_NORMAL_COMPLETION) {
		if (datalen != 0)
			usb_freemem(sc->sc_dmatag, dmap);
		return err;
	}

	stdend->td->td_status |= UHCI_TD_IOC;

#ifdef UHCI_DEBUG
	if (uhcidebug > 10) {
		DPRINTF(("uhci_device_intr_start: xfer\n"));
		uhci_dump_tds(std);
		uhci_dump_qh(upipe->u.intr.qhs[0]);
	}
#endif

	s = splusb();
	/* Set up interrupt info. */
	ii->reqh = reqh;
	ii->stdstart = std;
	ii->stdend = stdend;
#ifdef DIAGNOSTIC
	ii->isdone = 0;
#endif

	DPRINTFN(10,("uhci_device_intr_start: qhs[0]=%p\n", 
		     upipe->u.intr.qhs[0]));
	for (i = 0; i < upipe->u.intr.npoll; i++) {
		sqh = upipe->u.intr.qhs[i];
		sqh->qh->elink = std;
		sqh->qh->qh_elink = std->physaddr;
	}
	splx(s);

	return(USBD_IN_PROGRESS);
}

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
		return(USBD_INVAL);

	/* XXX copy data */
	return(USBD_XXX);
}

usbd_status
uhci_device_isoc_start(reqh)
	usbd_request_handle reqh;
{
	return(USBD_XXX);
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
			DPRINTF(("uhci_device_isoc_close: %p not found\n",std));
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
	int isread = upipe->pipe.endpoint->edesc->bEndpointAddress & UE_IN;
	struct iso *iso;
	int i;
	usbd_status err;

	/* 
	 * For simplicity the number of buffers must fit nicely in the frame
	 * list.
	 */
	if (UHCI_VFRAMELIST_COUNT % nbuf != 0)
		return(USBD_INVAL);

	iso = &upipe->u.iso;
	iso->bufsize = bufsize;
	iso->nbuf = nbuf;

	/* Allocate memory for buffers. */
	iso->bufs = malloc(nbuf * sizeof(usb_dma_t), M_USB, M_WAITOK);
	iso->stds = malloc(UHCI_VFRAMELIST_COUNT * sizeof (uhci_soft_td_t *),
			   M_USB, M_WAITOK);

	for (i = 0; i < nbuf; i++) {
		err = usb_allocmem(sc->sc_dmatag, bufsize, 0, &iso->bufs[i]);
		if (err != USBD_NORMAL_COMPLETION) {
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
		    isread ? UHCI_TD_IN (0, endpt, addr, 0) :
			 UHCI_TD_OUT(0, endpt, addr, 0);
		std->td->td_buffer = DMAADDR(&iso->bufs[i % nbuf]);

		vstd = sc->sc_vframes[i % UHCI_VFRAMELIST_COUNT].htd;
		std->td->link = vstd->td->link;
		std->td->td_link = vstd->td->td_link;
		vstd->td->link.std = std;
		vstd->td->td_link = std->physaddr;
	}
	uhci_unlock_frames(sc);

	return(USBD_NORMAL_COMPLETION);

 bad2:
	while (--i >= 0)
		uhci_free_std(sc, iso->stds[i]);
 bad1:
	for (i = 0; i < nbuf; i++)
		usb_freemem(sc->sc_dmatag, &iso->bufs[i]);
	free(iso->stds, M_USB);
	free(iso->bufs, M_USB);
	return(USBD_NOMEM);
}

/* Set interval for interrupt transfer */
usbd_status
uhci_device_intr_interval(sc, upipe, ival)
	uhci_softc_t *sc;
	struct uhci_pipe *upipe;
	int ival;
{
	uhci_soft_qh_t *sqh;
	int i, npoll, s;
	u_int bestbw, bw, bestoffs, offs;

	DPRINTFN(2, ("uhci_setintr: pipe=%p\n", upipe));
	if (ival == 0) {
		DPRINTF(("uhci_setintr: 0 interval\n"));
		return(USBD_INVAL);
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
	return(USBD_NORMAL_COMPLETION);
}


usbd_status
uhci_root_ctrl_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status err;

	s = splusb();
	err = usb_insert_transfer(reqh);
	splx(s);
	if (err != USBD_NORMAL_COMPLETION)
		return(err);
	else
		return(uhci_root_ctrl_start(reqh));
}

usbd_status
uhci_root_ctrl_start(usbd_request_handle reqh)
{
	uhci_softc_t *sc = (uhci_softc_t *)reqh->pipe->device->bus;

	if (!reqh->isreq)
		panic("uhci_root_ctrl_transfer: not a request\n");

	reqh->status = uhci_roothub_ctrl_transfer(sc,
				&reqh->request, reqh->buffer, &reqh->actlen);

	reqh->xfercb(reqh);
	usb_start_next(reqh->pipe);

	return(USBD_IN_PROGRESS);
}

void
uhci_root_ctrl_abort(reqh)
	usbd_request_handle reqh;
{
	/* Nothing to do, all transfers are syncronous. */
}

void
uhci_root_ctrl_close(pipe)
	usbd_pipe_handle pipe;
{
	usb_untimeout(uhci_root_intr_sim, pipe->intrreqh, pipe->intrreqh->timeout_handle);
	DPRINTF(("uhci_root_ctrl_close\n"));
}


usbd_status
uhci_root_intr_transfer(reqh)
	usbd_request_handle reqh;
{
	int s;
	usbd_status err;

	s = splusb();
	err = usb_insert_transfer(reqh);
	splx(s);
	if (err != USBD_NORMAL_COMPLETION)
		return(err);
	else
		return(uhci_root_intr_start(reqh));
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
	usbd_status err;
	int datalen;

	DPRINTFN(3, ("uhci_root_intr_transfer: reqh=%p buf=%p datalen=%d "
		     "flags=%d\n",
		     reqh, reqh->buffer, reqh->length, reqh->flags));

	datalen = reqh->length;
	dmap = &upipe->u.intr.datadma;
	if (datalen == 0)
		return(USBD_INVAL); /* XXX should it be? */

	err = usb_allocmem(sc->sc_dmatag, datalen, 0, dmap);
	if (err != USBD_NORMAL_COMPLETION)
		return(err);

	sc->sc_ival = MS_TO_TICKS(reqh->pipe->endpoint->edesc->bInterval);
	usb_timeout(uhci_root_intr_sim, reqh, sc->sc_ival, reqh->timeout_handle);
	return(USBD_IN_PROGRESS);
}

/* Abort a root interrupt request. */
void
uhci_root_intr_abort(reqh)
	usbd_request_handle reqh;
{
	usb_untimeout(uhci_root_intr_sim, reqh, reqh->timeout_handle);
}

/* Close the root interrupt pipe. */
void
uhci_root_intr_close(pipe)
	usbd_pipe_handle pipe;
{
	usb_untimeout(uhci_root_intr_sim, pipe->intrreqh, pipe->intrreqh->timeout_handle);
}

/*
 * This routine is executed periodically and simulates interrupts
 * from the root controller interrupt pipe for port status change.
 */
void
uhci_root_intr_sim(priv)
	void *priv;
{
	usbd_request_handle reqh = priv;
	usbd_pipe_handle pipe = reqh->pipe;
	uhci_softc_t *sc = (uhci_softc_t *)pipe->device->bus;
	struct uhci_pipe *upipe = (struct uhci_pipe *)pipe;
	int s;
	int actlen;
	u_int8_t *buf;
	int err;

	buf = KERNADDR(&upipe->u.intr.datadma);

	err = uhci_roothub_intr_transfer(sc, buf, reqh->length, &actlen);
	s = splusb();
	if (err) {
		reqh->status = err;
	} else {
		reqh->actlen = actlen;
		reqh->status = USBD_NORMAL_COMPLETION;
		reqh->xfercb(reqh);
	}

	if (reqh->pipe->intrreqh == reqh) {
		usb_timeout(uhci_root_intr_sim, reqh, sc->sc_ival, reqh->timeout_handle);
	} else {
		usb_freemem(sc->sc_dmatag, &upipe->u.intr.datadma);
		usb_start_next(reqh->pipe);
	}
	splx(s);
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
			DPRINTF(("uhci_remove_ctrl: QH not found\n"));
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
#if defined(DIAGNOSTIC)
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


/* Add interrupt QH, called with vflock. */
void
uhci_add_intr(sc, pos, sqh)
	uhci_softc_t *sc;
	int pos;
	uhci_soft_qh_t *sqh;
{
	struct uhci_vframe *vf = &sc->sc_vframes[pos];
	uhci_qh_t *eqh;

	DPRINTFN(4, ("uhci_add_intr: pos=%d sqh=%p\n", pos, sqh));
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
uhci_remove_intr(sc, pos, sqh)
	uhci_softc_t *sc;
	int pos;
	uhci_soft_qh_t *sqh;
{
	struct uhci_vframe *vf = &sc->sc_vframes[pos];
	uhci_soft_qh_t *pqh;

	DPRINTFN(4, ("uhci_remove_intr: pos=%d sqh=%p\n", pos, sqh));

	for (pqh = vf->hqh; pqh->qh->hlink != sqh; pqh = pqh->qh->hlink)
#if defined(DIAGNOSTIC)
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


/*
 * The simulated root hub
 */

/* Data structures */

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

/*
 * creates the UNICODE-ified string descriptor for the root hub
 * returns the length copied
 */

int
uhci_roothub_string_descriptor(sd, datalen, string)
	usb_string_descriptor_t *sd;
	int datalen;
	char *string;
{
	int i;

	if (datalen == 0)
		return(0);
	sd->bLength = 2 * strlen(string) + 2;
	if (datalen == 1)
		return(1);
	sd->bDescriptorType = UDESC_STRING;
	datalen -= 2;
	for (i = 0; string[i] && datalen > 1; i++, datalen -= 2)
		USETW2(sd->bString[i], 0, string[i]);
	return(2*i+2);
}

/* function handling all requests for the root hub */

usbd_status
uhci_roothub_ctrl_transfer(sc, req, buf, actlen)
	uhci_softc_t *sc;
	usb_device_request_t *req;
	void *buf;
	int *actlen;
{
	int port;			/* port number */
	int x;				/* temp storage for read register */
	int datalen, value, index;	/* values in request */
	int l;				/* temp storage for length to be copied */

	*actlen = 0;

	DPRINTFN(12,("uhci_root_ctrl_control type=0x%02x request=%02x\n", 
		    req->bmRequestType, req->bRequest));

	datalen = UGETW(req->wLength);
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
		if (datalen > 0) {
			*(u_int8_t *)buf = sc->sc_conf;
			*actlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_DEVICE):
		DPRINTFN(2,("uhci_root_ctrl_control wValue=0x%04x\n", value));
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				return USBD_IOERROR;
			}
			*actlen = l = min(datalen, USB_DEVICE_DESCRIPTOR_SIZE);
			memcpy(buf, &uhci_devd, l);
			break;
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				return USBD_IOERROR;
			}
			*actlen = l = min(datalen, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &uhci_confd, l);
			buf = (char *)buf + l;
			datalen -= l;
			l = min(datalen, USB_INTERFACE_DESCRIPTOR_SIZE);
			*actlen += l;
			memcpy(buf, &uhci_ifcd, l);
			buf = (char *)buf + l;
			datalen -= l;
			l = min(datalen, USB_ENDPOINT_DESCRIPTOR_SIZE);
			*actlen += l;
			memcpy(buf, &uhci_endpd, l);
			break;
		case UDESC_STRING:
			if (datalen == 0)
				break;
			*(u_int8_t *)buf = 0;
			*actlen = 1;
			switch (value & 0xff) {
			case 1: /* Vendor */
				*actlen = uhci_roothub_string_descriptor(buf, datalen, sc->sc_vendor);
				break;
			case 2: /* Product */
				*actlen = uhci_roothub_string_descriptor(buf, datalen, "UHCI root hub");
				break;
			}
			break;
		default:
			return USBD_IOERROR;
		}
		break;
	case C(UR_GET_INTERFACE, UT_READ_INTERFACE):
		if (datalen > 0) {
			*(u_int8_t *)buf = 0;
			*actlen = 1;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_DEVICE):
		if (datalen > 1) {
			USETW(((usb_status_t *)buf)->wStatus,UDS_SELF_POWERED);
			*actlen = 2;
		}
		break;
	case C(UR_GET_STATUS, UT_READ_INTERFACE):
	case C(UR_GET_STATUS, UT_READ_ENDPOINT):
		if (datalen > 1) {
			USETW(((usb_status_t *)buf)->wStatus, 0);
			*actlen = 2;
		}
		break;
	case C(UR_SET_ADDRESS, UT_WRITE_DEVICE):
		if (value >= USB_MAX_DEVICES) {
			return USBD_IOERROR;
		}
		sc->sc_addr = value;
		break;
	case C(UR_SET_CONFIG, UT_WRITE_DEVICE):
		if (value != 0 && value != 1) {
			return USBD_IOERROR;
		}
		sc->sc_conf = value;
		break;
	case C(UR_SET_DESCRIPTOR, UT_WRITE_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_DEVICE):
	case C(UR_SET_FEATURE, UT_WRITE_INTERFACE):
	case C(UR_SET_FEATURE, UT_WRITE_ENDPOINT):
		return USBD_IOERROR;
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
			return USBD_IOERROR;
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
			return USBD_NORMAL_COMPLETION;
		case UHF_PORT_CONNECTION:
		case UHF_PORT_OVER_CURRENT:
		case UHF_PORT_POWER:
		case UHF_PORT_LOW_SPEED:
		case UHF_C_PORT_SUSPEND:
		default:
			return USBD_IOERROR;
		}
		break;
	case C(UR_GET_BUS_STATE, UT_READ_CLASS_OTHER):
		if (index == 1)
			port = UHCI_PORTSC1;
		else if (index == 2)
			port = UHCI_PORTSC2;
		else {
			return USBD_IOERROR;
		}
		if (datalen > 0) {
			*(u_int8_t *)buf = 
				(UREAD2(sc, port) & UHCI_PORTSC_LS) >>
				UHCI_PORTSC_LS_SHIFT;
			*actlen = 1;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if (value != 0) {
			return USBD_IOERROR;
		}
		l = min(datalen, USB_HUB_DESCRIPTOR_SIZE);
		*actlen = l;
		memcpy(buf, &uhci_hubd_piix, l);
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_DEVICE):
		if (datalen != 4) {
			return USBD_IOERROR;
		}
		memset(buf, 0, datalen);
		*actlen = datalen;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
	{
		int status, change;
		usb_port_status_t ps;

		if (index == 1)
			port = UHCI_PORTSC1;
		else if (index == 2)
			port = UHCI_PORTSC2;
		else {
			return USBD_IOERROR;
		}
		if (datalen != 4) {
			return USBD_IOERROR;
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
		l = min(datalen, sizeof ps);
		memcpy(buf, &ps, l);
		*actlen = l;
		break;
	}
	case C(UR_SET_DESCRIPTOR, UT_WRITE_CLASS_DEVICE):
		return USBD_IOERROR;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
		if (index == 1)
			port = UHCI_PORTSC1;
		else if (index == 2)
			port = UHCI_PORTSC2;
		else {
			return USBD_IOERROR;
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
			return USBD_IOERROR;
		}
		break;
	default:
		return USBD_IOERROR;
	}
	
	return USBD_NORMAL_COMPLETION;
}

usbd_status
uhci_roothub_intr_transfer(uhci_softc_t *sc,
			u_int8_t *buf, int buflen, int *actlen)
{
	if (buflen < 1) {
		DPRINTF(("%s: buffer too small, %d < 1\n",
			USBDEVNAME(sc->sc_bus.bdev), buflen));
		return USBD_IOERROR;
	}

	buf[0] = 0;
	if (UREAD2(sc, UHCI_STS) & (UHCI_STS_RD))
		buf[0] |= 1<<0;
	if (UREAD2(sc, UHCI_PORTSC1) & (UHCI_PORTSC_CSC|UHCI_PORTSC_OCIC))
		buf[0] |= 1<<1;
	if (UREAD2(sc, UHCI_PORTSC2) & (UHCI_PORTSC_CSC|UHCI_PORTSC_OCIC))
		buf[0] |= 1<<2;

	if (buf[0] != 0)
		*actlen = 1;
	else
		actlen = 0;

	return USBD_NORMAL_COMPLETION;
}


/*
 * debugging functions
 */

#ifdef UHCI_DEBUG
void
uhci_dumpregs(sc)
	uhci_softc_t *sc;
{
	DPRINTF(("%s: regs: cmd=%04x, sts=%04x, intr=%04x, frnum=%04x, "
	       "flbase=%08x, sof=%02x, portsc1=%04x, portsc2=%04x\n",
	       USBDEVNAME(sc->sc_bus.bdev),
	       UREAD2(sc, UHCI_CMD),
	       UREAD2(sc, UHCI_STS),
	       UREAD2(sc, UHCI_INTR),
	       UREAD2(sc, UHCI_FRNUM),
	       UREAD4(sc, UHCI_FLBASEADDR),
	       UREAD1(sc, UHCI_SOF),
	       UREAD2(sc, UHCI_PORTSC1),
	       UREAD2(sc, UHCI_PORTSC2)));
}

int uhci_longtd = 1;

void
uhci_dump_td(p)
	uhci_soft_td_t *p;
{
	DPRINTF(("TD(%p) at %08lx link=0x%08lx st=0x%08lx tok=0x%08lx "
		"buf=0x%08lx\n",
	       p, (long)p->physaddr,
	       (long)p->td->td_link,
	       (long)p->td->td_status,
	       (long)p->td->td_token,
	       (long)p->td->td_buffer));
	if (uhci_longtd)
		DPRINTF((" %b %b,errcnt=%d,actlen=%d pid=%02x,addr=%d,endpt=%d,"
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
		       UHCI_TD_GET_MAXLEN(p->td->td_token)));
}

void
uhci_dump_qh(p)
	uhci_soft_qh_t *p;
{
	DPRINTF(("QH(%p) at %08x: hlink=%08x elink=%08x\n", p, (int)p->physaddr,
		p->qh->qh_hlink, p->qh->qh_elink));
}


void
uhci_dump_tds(std)
	uhci_soft_td_t *std;
{
	uhci_soft_td_t *p;

	for(p = std; p; p = p->td->link.std)
		uhci_dump_td(p);
}
#endif
