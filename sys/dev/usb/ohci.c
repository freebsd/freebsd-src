/*	$NetBSD: ohci.c,v 1.52 1999/10/13 08:10:55 augustss Exp $	*/
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
 * USB Open Host Controller driver.
 *
 * OHCI spec: ftp://ftp.compaq.com/pub/supportinformation/papers/hcir1_0a.exe
 * USB spec: http://www.usb.org/developers/data/usb11.pdf
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#if defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/select.h>
#elif defined(__FreeBSD__)
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#if defined(DIAGNOSTIC) && defined(__i386__)
#include <machine/cpu.h>
#endif
#endif
#include <sys/proc.h>
#include <sys/queue.h>

#include <machine/bus.h>
#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_mem.h>
#include <dev/usb/usb_quirks.h>

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>

#if defined(__FreeBSD__)
#include <machine/clock.h>

#define delay(d)                DELAY(d)
#endif

#if defined(__OpenBSD__)
struct cfdriver ohci_cd = {
	NULL, "ohci", DV_DULL
};
#endif

#ifdef OHCI_DEBUG
#define DPRINTF(x)	if (ohcidebug) logprintf x
#define DPRINTFN(n,x)	if (ohcidebug>(n)) logprintf x
int ohcidebug = 1;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/*
 * The OHCI controller is little endian, so on big endian machines
 * the data strored in memory needs to be swapped.
 */
#if BYTE_ORDER == BIG_ENDIAN
#define LE(x) (bswap32(x))
#else
#define LE(x) (x)
#endif

struct ohci_pipe;

static ohci_soft_ed_t	*ohci_alloc_sed __P((ohci_softc_t *));
static void		ohci_free_sed __P((ohci_softc_t *, ohci_soft_ed_t *));

static ohci_soft_td_t	*ohci_alloc_std __P((ohci_softc_t *));
static void		ohci_free_std __P((ohci_softc_t *, ohci_soft_td_t *));

#if 0
static void		ohci_free_std_chain __P((ohci_softc_t *, 
			    ohci_soft_td_t *, ohci_soft_td_t *));
#endif
static usbd_status	ohci_alloc_std_chain __P((struct ohci_pipe *,
			    ohci_softc_t *, int, int, int, usb_dma_t *, 
			    ohci_soft_td_t *, ohci_soft_td_t **));

static void		ohci_power __P((int, void *));
static usbd_status	ohci_open __P((usbd_pipe_handle));
static void		ohci_poll __P((struct usbd_bus *));
static void		ohci_waitintr __P((ohci_softc_t *,
			    usbd_xfer_handle));
static void		ohci_rhsc __P((ohci_softc_t *, usbd_xfer_handle));
static void		ohci_process_done __P((ohci_softc_t *,
			    ohci_physaddr_t));

static usbd_status	ohci_device_request __P((usbd_xfer_handle xfer));
static void		ohci_add_ed __P((ohci_soft_ed_t *, ohci_soft_ed_t *));
static void		ohci_rem_ed __P((ohci_soft_ed_t *, ohci_soft_ed_t *));
static void		ohci_hash_add_td __P((ohci_softc_t *,
			    ohci_soft_td_t *));
static void		ohci_hash_rem_td __P((ohci_softc_t *,
			    ohci_soft_td_t *));
static ohci_soft_td_t	*ohci_hash_find_td __P((ohci_softc_t *,
			    ohci_physaddr_t));

static usbd_status	ohci_allocm __P((struct usbd_bus *, usb_dma_t *,
			    u_int32_t));
static void		ohci_freem __P((struct usbd_bus *, usb_dma_t *));

static usbd_status	ohci_root_ctrl_transfer __P((usbd_xfer_handle));
static usbd_status	ohci_root_ctrl_start __P((usbd_xfer_handle));
static void		ohci_root_ctrl_abort __P((usbd_xfer_handle));
static void		ohci_root_ctrl_close __P((usbd_pipe_handle));

static usbd_status	ohci_root_intr_transfer __P((usbd_xfer_handle));
static usbd_status	ohci_root_intr_start __P((usbd_xfer_handle));
static void		ohci_root_intr_abort __P((usbd_xfer_handle));
static void		ohci_root_intr_close __P((usbd_pipe_handle));
static void		ohci_root_intr_done  __P((usbd_xfer_handle));

static usbd_status	ohci_device_ctrl_transfer __P((usbd_xfer_handle));
static usbd_status	ohci_device_ctrl_start __P((usbd_xfer_handle));
static void		ohci_device_ctrl_abort __P((usbd_xfer_handle));
static void		ohci_device_ctrl_close __P((usbd_pipe_handle));
static void		ohci_device_ctrl_done  __P((usbd_xfer_handle));

static usbd_status	ohci_device_bulk_transfer __P((usbd_xfer_handle));
static usbd_status	ohci_device_bulk_start __P((usbd_xfer_handle));
static void		ohci_device_bulk_abort __P((usbd_xfer_handle));
static void		ohci_device_bulk_close __P((usbd_pipe_handle));
static void		ohci_device_bulk_done  __P((usbd_xfer_handle));

static usbd_status	ohci_device_intr_transfer __P((usbd_xfer_handle));
static usbd_status	ohci_device_intr_start __P((usbd_xfer_handle));
static void		ohci_device_intr_abort __P((usbd_xfer_handle));
static void		ohci_device_intr_close __P((usbd_pipe_handle));
static void		ohci_device_intr_done  __P((usbd_xfer_handle));

#if 0
static usbd_status	ohci_device_isoc_transfer __P((usbd_xfer_handle));
static usbd_status	ohci_device_isoc_start __P((usbd_xfer_handle));
static void		ohci_device_isoc_abort __P((usbd_xfer_handle));
static void		ohci_device_isoc_close __P((usbd_pipe_handle));
static void		ohci_device_isoc_done  __P((usbd_xfer_handle));
#endif

static usbd_status	ohci_device_setintr __P((ohci_softc_t *sc, 
			    struct ohci_pipe *pipe, int ival));

static int		ohci_str __P((usb_string_descriptor_t *, int, char *));

static void		ohci_timeout __P((void *));
static void		ohci_rhsc_able __P((ohci_softc_t *, int));

static void		ohci_close_pipe __P((usbd_pipe_handle pipe, 
			    ohci_soft_ed_t *head));
static void		ohci_abort_req __P((usbd_xfer_handle xfer,
			    usbd_status status));
static void		ohci_abort_req_end __P((void *));

static void		ohci_device_clear_toggle __P((usbd_pipe_handle pipe));
static void		ohci_noop __P((usbd_pipe_handle pipe));

#ifdef OHCI_DEBUG
static void		ohci_dumpregs __P((ohci_softc_t *));
static void		ohci_dump_tds __P((ohci_soft_td_t *));
static void		ohci_dump_td __P((ohci_soft_td_t *));
static void		ohci_dump_ed __P((ohci_soft_ed_t *));
#endif

#define OWRITE4(sc, r, x) bus_space_write_4((sc)->iot, (sc)->ioh, (r), (x))
#define OREAD4(sc, r) bus_space_read_4((sc)->iot, (sc)->ioh, (r))
#define OREAD2(sc, r) bus_space_read_2((sc)->iot, (sc)->ioh, (r))

/* Reverse the bits in a value 0 .. 31 */
static u_int8_t revbits[OHCI_NO_INTRS] = 
  { 0x00, 0x10, 0x08, 0x18, 0x04, 0x14, 0x0c, 0x1c,
    0x02, 0x12, 0x0a, 0x1a, 0x06, 0x16, 0x0e, 0x1e,
    0x01, 0x11, 0x09, 0x19, 0x05, 0x15, 0x0d, 0x1d,
    0x03, 0x13, 0x0b, 0x1b, 0x07, 0x17, 0x0f, 0x1f };

struct ohci_pipe {
	struct usbd_pipe pipe;
	ohci_soft_ed_t *sed;
	ohci_soft_td_t *tail;
	/* Info needed for different pipe kinds. */
	union {
		/* Control pipe */
		struct {
			usb_dma_t reqdma;
			u_int length;
			ohci_soft_td_t *setup, *data, *stat;
		} ctl;
		/* Interrupt pipe */
		struct {
			int nslots;
			int pos;
		} intr;
		/* Bulk pipe */
		struct {
			u_int length;
			int isread;
		} bulk;
		/* Iso pipe */
		struct iso {
			int xxxxx;
		} iso;
	} u;
};

#define OHCI_INTR_ENDPT 1

static struct usbd_bus_methods ohci_bus_methods = {
	ohci_open,
	ohci_poll,
	ohci_allocm,
	ohci_freem,
};

static struct usbd_pipe_methods ohci_root_ctrl_methods = {	
	ohci_root_ctrl_transfer,
	ohci_root_ctrl_start,
	ohci_root_ctrl_abort,
	ohci_root_ctrl_close,
	ohci_noop,
	0,
};

static struct usbd_pipe_methods ohci_root_intr_methods = {	
	ohci_root_intr_transfer,
	ohci_root_intr_start,
	ohci_root_intr_abort,
	ohci_root_intr_close,
	ohci_noop,
	ohci_root_intr_done,
};

static struct usbd_pipe_methods ohci_device_ctrl_methods = {	
	ohci_device_ctrl_transfer,
	ohci_device_ctrl_start,
	ohci_device_ctrl_abort,
	ohci_device_ctrl_close,
	ohci_noop,
	ohci_device_ctrl_done,
};

static struct usbd_pipe_methods ohci_device_intr_methods = {	
	ohci_device_intr_transfer,
	ohci_device_intr_start,
	ohci_device_intr_abort,
	ohci_device_intr_close,
	ohci_device_clear_toggle,
	ohci_device_intr_done,
};

static struct usbd_pipe_methods ohci_device_bulk_methods = {	
	ohci_device_bulk_transfer,
	ohci_device_bulk_start,
	ohci_device_bulk_abort,
	ohci_device_bulk_close,
	ohci_device_clear_toggle,
	ohci_device_bulk_done,
};

#if 0
static struct usbd_pipe_methods ohci_device_isoc_methods = {
	ohci_device_isoc_transfer,
	ohci_device_isoc_start,
	ohci_device_isoc_abort,
	ohci_device_isoc_close,
	ohci_noop,
	ohci_device_isoc_done,
};
#endif

#if defined(__NetBSD__) || defined(__OpenBSD__)
int
ohci_activate(self, act)
	device_ptr_t self;
	enum devact act;
{
	struct ohci_softc *sc = (struct ohci_softc *)self;
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		return (EOPNOTSUPP);
		break;

	case DVACT_DEACTIVATE:
		if (sc->sc_child != NULL)
			rv = config_deactivate(sc->sc_child);
		break;
	}
	return (rv);
}

int
ohci_detach(sc, flags)
	struct ohci_softc *sc;
	int flags;
{
	int rv = 0;

	if (sc->sc_child != NULL)
		rv = config_detach(sc->sc_child, flags);
	
	if (rv != 0)
		return (rv);

	powerhook_disestablish(sc->sc_powerhook);
	/* free data structures XXX */

	return (rv);
}
#endif

ohci_soft_ed_t *
ohci_alloc_sed(sc)
	ohci_softc_t *sc;
{
	ohci_soft_ed_t *sed;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;

	if (sc->sc_freeeds == NULL) {
		DPRINTFN(2, ("ohci_alloc_sed: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, OHCI_SED_SIZE * OHCI_SED_CHUNK,
			  OHCI_ED_ALIGN, &dma);
		if (err)
			return (0);
		for(i = 0; i < OHCI_SED_CHUNK; i++) {
			offs = i * OHCI_SED_SIZE;
			sed = (ohci_soft_ed_t *)((char *)KERNADDR(&dma) +offs);
			sed->physaddr = DMAADDR(&dma) + offs;
			sed->next = sc->sc_freeeds;
			sc->sc_freeeds = sed;
		}
	}
	sed = sc->sc_freeeds;
	sc->sc_freeeds = sed->next;
	memset(&sed->ed, 0, sizeof(ohci_ed_t));
	sed->next = 0;
	return (sed);
}

void
ohci_free_sed(sc, sed)
	ohci_softc_t *sc;
	ohci_soft_ed_t *sed;
{
	sed->next = sc->sc_freeeds;
	sc->sc_freeeds = sed;
}

ohci_soft_td_t *
ohci_alloc_std(sc)
	ohci_softc_t *sc;
{
	ohci_soft_td_t *std;
	usbd_status err;
	int i, offs;
	usb_dma_t dma;

	if (sc->sc_freetds == NULL) {
		DPRINTFN(2, ("ohci_alloc_std: allocating chunk\n"));
		err = usb_allocmem(&sc->sc_bus, OHCI_STD_SIZE * OHCI_STD_CHUNK,
			  OHCI_TD_ALIGN, &dma);
		if (err)
			return (0);
		for(i = 0; i < OHCI_STD_CHUNK; i++) {
			offs = i * OHCI_STD_SIZE;
			std = (ohci_soft_td_t *)((char *)KERNADDR(&dma) +offs);
			std->physaddr = DMAADDR(&dma) + offs;
			std->nexttd = sc->sc_freetds;
			sc->sc_freetds = std;
		}
	}
	std = sc->sc_freetds;
	sc->sc_freetds = std->nexttd;
	memset(&std->td, 0, sizeof(ohci_td_t));
	std->nexttd = 0;
	return (std);
}

void
ohci_free_std(sc, std)
	ohci_softc_t *sc;
	ohci_soft_td_t *std;
{
	std->nexttd = sc->sc_freetds;
	sc->sc_freetds = std;
}

usbd_status
ohci_alloc_std_chain(upipe, sc, len, rd, shortok, dma, sp, ep)
	struct ohci_pipe *upipe;
	ohci_softc_t *sc;
	int len, rd, shortok;
	usb_dma_t *dma;
	ohci_soft_td_t *sp, **ep;
{
	ohci_soft_td_t *next, *cur;
	ohci_physaddr_t dataphys, dataphysend;
	u_int32_t intr;
	int curlen;

	DPRINTFN(len < 4096,("ohci_alloc_std_chain: start len=%d\n", len));
	cur = sp;
	dataphys = DMAADDR(dma);
	dataphysend = OHCI_PAGE(dataphys + len - 1);
	for (;;) {
		next = ohci_alloc_std(sc);
		if (next == 0) {
			/* XXX free chain */
			return (USBD_NOMEM);
		}

		/* The OHCI hardware can handle at most one page crossing. */
		if (OHCI_PAGE(dataphys) == dataphysend ||
		    OHCI_PAGE(dataphys) + OHCI_PAGE_SIZE == dataphysend) {
			/* we can handle it in this TD */
			curlen = len;
		} else {
			/* must use multiple TDs, fill as much as possible. */
			curlen = 2 * OHCI_PAGE_SIZE - 
				 (dataphys & (OHCI_PAGE_SIZE-1));
		}
		DPRINTFN(4,("ohci_alloc_std_chain: dataphys=0x%08x "
			    "dataphysend=0x%08x len=%d curlen=%d\n",
			    dataphys, dataphysend,
			    len, curlen));
		len -= curlen;

		intr = len == 0 ? OHCI_TD_SET_DI(1) : OHCI_TD_NOINTR;
		cur->td.td_flags = LE(
			(rd ? OHCI_TD_IN : OHCI_TD_OUT) | OHCI_TD_NOCC |
			intr | OHCI_TD_TOGGLE_CARRY |
			(shortok ? OHCI_TD_R : 0));
		cur->td.td_cbp = LE(dataphys);
		cur->nexttd = next;
		cur->td.td_nexttd = LE(next->physaddr);
		cur->td.td_be = LE(dataphys + curlen - 1);
		cur->len = curlen;
		cur->flags = OHCI_ADD_LEN;
		DPRINTFN(10,("ohci_alloc_std_chain: cbp=0x%08x be=0x%08x\n",
			    dataphys, dataphys + curlen - 1));
		if (len == 0)
			break;
		DPRINTFN(10,("ohci_alloc_std_chain: extend chain\n"));
		dataphys += curlen;
		cur = next;
	}
	cur->flags = OHCI_CALL_DONE | OHCI_ADD_LEN;
	*ep = next;

	return (USBD_NORMAL_COMPLETION);
}

#if 0
static void
ohci_free_std_chain(sc, std, stdend)
	ohci_softc_t *sc;
	ohci_soft_td_t *std;
	ohci_soft_td_t *stdend;
{
	ohci_soft_td_t *p;

	for (; std != stdend; std = p) {
		p = std->nexttd;
		ohci_free_std(sc, std);
	}
}
#endif

usbd_status
ohci_init(sc)
	ohci_softc_t *sc;
{
	ohci_soft_ed_t *sed, *psed;
	usbd_status err;
	int rev;
	int i;
	u_int32_t s, ctl, ival, hcr, fm, per;

	DPRINTF(("ohci_init: start\n"));
	rev = OREAD4(sc, OHCI_REVISION);
#if defined(__OpenBSD__)
	printf(",");
#else
	printf("%s", USBDEVNAME(sc->sc_bus.bdev));
#endif
	printf(" OHCI version %d.%d%s\n", OHCI_REV_HI(rev), OHCI_REV_LO(rev),
	       OHCI_REV_LEGACY(rev) ? ", legacy support" : "");

	if (OHCI_REV_HI(rev) != 1 || OHCI_REV_LO(rev) != 0) {
		printf("%s: unsupported OHCI revision\n", 
		       USBDEVNAME(sc->sc_bus.bdev));
		return (USBD_INVAL);
	}

	for (i = 0; i < OHCI_HASH_SIZE; i++)
		LIST_INIT(&sc->sc_hash_tds[i]);

	/* Allocate the HCCA area. */
	err = usb_allocmem(&sc->sc_bus, OHCI_HCCA_SIZE, 
		  OHCI_HCCA_ALIGN, &sc->sc_hccadma);
	if (err)
		return (err);
	sc->sc_hcca = (struct ohci_hcca *)KERNADDR(&sc->sc_hccadma);
	memset(sc->sc_hcca, 0, OHCI_HCCA_SIZE);

	sc->sc_eintrs = OHCI_NORMAL_INTRS;

	sc->sc_ctrl_head = ohci_alloc_sed(sc);
	if (sc->sc_ctrl_head == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	sc->sc_ctrl_head->ed.ed_flags |= LE(OHCI_ED_SKIP);

	sc->sc_bulk_head = ohci_alloc_sed(sc);
	if (sc->sc_bulk_head == NULL) {
		err = USBD_NOMEM;
		goto bad2;
	}
	sc->sc_bulk_head->ed.ed_flags |= LE(OHCI_ED_SKIP);

	/* Allocate all the dummy EDs that make up the interrupt tree. */
	for (i = 0; i < OHCI_NO_EDS; i++) {
		sed = ohci_alloc_sed(sc);
		if (sed == NULL) {
			while (--i >= 0)
				ohci_free_sed(sc, sc->sc_eds[i]);
			err = USBD_NOMEM;
			goto bad3;
		}
		/* All ED fields are set to 0. */
		sc->sc_eds[i] = sed;
		sed->ed.ed_flags |= LE(OHCI_ED_SKIP);
		if (i != 0) {
			psed = sc->sc_eds[(i-1) / 2];
			sed->next = psed;
			sed->ed.ed_nexted = LE(psed->physaddr);
		}
	}
	/* 
	 * Fill HCCA interrupt table.  The bit reversal is to get
	 * the tree set up properly to spread the interrupts.
	 */
	for (i = 0; i < OHCI_NO_INTRS; i++)
		sc->sc_hcca->hcca_interrupt_table[revbits[i]] = 
			LE(sc->sc_eds[OHCI_NO_EDS-OHCI_NO_INTRS+i]->physaddr);

	/* Determine in what context we are running. */
	ctl = OREAD4(sc, OHCI_CONTROL);
	if (ctl & OHCI_IR) {
		/* SMM active, request change */
		DPRINTF(("ohci_init: SMM active, request owner change\n"));
		s = OREAD4(sc, OHCI_COMMAND_STATUS);
		OWRITE4(sc, OHCI_COMMAND_STATUS, s | OHCI_OCR);
		for (i = 0; i < 100 && (ctl & OHCI_IR); i++) {
			usb_delay_ms(&sc->sc_bus, 1);
			ctl = OREAD4(sc, OHCI_CONTROL);
		}
		if ((ctl & OHCI_IR) == 0) {
			printf("%s: SMM does not respond, resetting\n",
			       USBDEVNAME(sc->sc_bus.bdev));
			OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
			goto reset;
		}
	} else if ((ctl & OHCI_HCFS_MASK) != OHCI_HCFS_RESET) {
		/* BIOS started controller. */
		DPRINTF(("ohci_init: BIOS active\n"));
		if ((ctl & OHCI_HCFS_MASK) != OHCI_HCFS_OPERATIONAL) {
			OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_OPERATIONAL);
			usb_delay_ms(&sc->sc_bus, USB_RESUME_DELAY);
		}
	} else {
		DPRINTF(("ohci_init: cold started\n"));
	reset:
		/* Controller was cold started. */
		usb_delay_ms(&sc->sc_bus, USB_BUS_RESET_DELAY);
	}

	/*
	 * This reset should not be necessary according to the OHCI spec, but
	 * without it some controllers do not start.
	 */
	DPRINTF(("%s: resetting\n", USBDEVNAME(sc->sc_bus.bdev)));
	OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
	usb_delay_ms(&sc->sc_bus, USB_BUS_RESET_DELAY);

	/* We now own the host controller and the bus has been reset. */
	ival = OHCI_GET_IVAL(OREAD4(sc, OHCI_FM_INTERVAL));

	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_HCR); /* Reset HC */
	/* Nominal time for a reset is 10 us. */
	for (i = 0; i < 10; i++) {
		delay(10);
		hcr = OREAD4(sc, OHCI_COMMAND_STATUS) & OHCI_HCR;
		if (!hcr)
			break;
	}
	if (hcr) {
		printf("%s: reset timeout\n", USBDEVNAME(sc->sc_bus.bdev));
		err = USBD_IOERROR;
		goto bad3;
	}
#ifdef OHCI_DEBUG
	if (ohcidebug > 15)
		ohci_dumpregs(sc);
#endif

	/* The controller is now in suspend state, we have 2ms to finish. */

	/* Set up HC registers. */
	OWRITE4(sc, OHCI_HCCA, DMAADDR(&sc->sc_hccadma));
	OWRITE4(sc, OHCI_CONTROL_HEAD_ED, sc->sc_ctrl_head->physaddr);
	OWRITE4(sc, OHCI_BULK_HEAD_ED, sc->sc_bulk_head->physaddr);
	/* disable all interrupts and then switch on all desired interrupts */
	OWRITE4(sc, OHCI_INTERRUPT_DISABLE, OHCI_ALL_INTRS);
	OWRITE4(sc, OHCI_INTERRUPT_ENABLE, sc->sc_eintrs | OHCI_MIE);
	/* switch on desired functional features */
	ctl = OREAD4(sc, OHCI_CONTROL);
	ctl &= ~(OHCI_CBSR_MASK | OHCI_LES | OHCI_HCFS_MASK | OHCI_IR);
	ctl |= OHCI_PLE | OHCI_IE | OHCI_CLE | OHCI_BLE |
		OHCI_RATIO_1_4 | OHCI_HCFS_OPERATIONAL;
	/* And finally start it! */
	OWRITE4(sc, OHCI_CONTROL, ctl);

	/*
	 * The controller is now OPERATIONAL.  Set a some final
	 * registers that should be set earlier, but that the
	 * controller ignores when in the SUSPEND state.
	 */
	fm = (OREAD4(sc, OHCI_FM_INTERVAL) & OHCI_FIT) ^ OHCI_FIT;
	fm |= OHCI_FSMPS(ival) | ival;
	OWRITE4(sc, OHCI_FM_INTERVAL, fm);
	per = OHCI_PERIODIC(ival); /* 90% periodic */
	OWRITE4(sc, OHCI_PERIODIC_START, per);

	OWRITE4(sc, OHCI_RH_STATUS, OHCI_LPSC);	/* Enable port power */

	sc->sc_noport = OHCI_GET_NDP(OREAD4(sc, OHCI_RH_DESCRIPTOR_A));

#ifdef OHCI_DEBUG
	if (ohcidebug > 5)
		ohci_dumpregs(sc);
#endif
	
	/* Set up the bus struct. */
	sc->sc_bus.methods = &ohci_bus_methods;
	sc->sc_bus.pipe_size = sizeof(struct ohci_pipe);

	sc->sc_powerhook = powerhook_establish(ohci_power, sc);

	return (USBD_NORMAL_COMPLETION);

 bad3:
	ohci_free_sed(sc, sc->sc_ctrl_head);
 bad2:
	ohci_free_sed(sc, sc->sc_bulk_head);
 bad1:
	usb_freemem(&sc->sc_bus, &sc->sc_hccadma);
	return (err);
}

usbd_status
ohci_allocm(bus, dma, size)
	struct usbd_bus *bus;
	usb_dma_t *dma;
	u_int32_t size;
{
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct ohci_softc *sc = (struct ohci_softc *)bus;
#endif

	return (usb_allocmem(&sc->sc_bus, size, 0, dma));
}

void
ohci_freem(bus, dma)
	struct usbd_bus *bus;
	usb_dma_t *dma;
{
#if defined(__NetBSD__) || defined(__OpenBSD__)
	struct ohci_softc *sc = (struct ohci_softc *)bus;
#endif

	usb_freemem(&sc->sc_bus, dma);
}

#if defined(__NetBSD__)
void
ohci_power(why, v)
	int why;
	void *v;
{
#ifdef OHCI_DEBUG
	ohci_softc_t *sc = v;

	DPRINTF(("ohci_power: sc=%p, why=%d\n", sc, why));
	/* XXX should suspend/resume */
	ohci_dumpregs(sc);
#endif
}
#endif /* defined(__NetBSD__) */

#ifdef OHCI_DEBUG
void
ohci_dumpregs(sc)
	ohci_softc_t *sc;
{
	DPRINTF(("ohci_dumpregs: rev=0x%08x control=0x%08x command=0x%08x\n",
		 OREAD4(sc, OHCI_REVISION),
		 OREAD4(sc, OHCI_CONTROL),
		 OREAD4(sc, OHCI_COMMAND_STATUS)));
	DPRINTF(("               intrstat=0x%08x intre=0x%08x intrd=0x%08x\n",
		 OREAD4(sc, OHCI_INTERRUPT_STATUS),
		 OREAD4(sc, OHCI_INTERRUPT_ENABLE),
		 OREAD4(sc, OHCI_INTERRUPT_DISABLE)));
	DPRINTF(("               hcca=0x%08x percur=0x%08x ctrlhd=0x%08x\n",
		 OREAD4(sc, OHCI_HCCA),
		 OREAD4(sc, OHCI_PERIOD_CURRENT_ED),
		 OREAD4(sc, OHCI_CONTROL_HEAD_ED)));
	DPRINTF(("               ctrlcur=0x%08x bulkhd=0x%08x bulkcur=0x%08x\n",
		 OREAD4(sc, OHCI_CONTROL_CURRENT_ED),
		 OREAD4(sc, OHCI_BULK_HEAD_ED),
		 OREAD4(sc, OHCI_BULK_CURRENT_ED)));
	DPRINTF(("               done=0x%08x fmival=0x%08x fmrem=0x%08x\n",
		 OREAD4(sc, OHCI_DONE_HEAD),
		 OREAD4(sc, OHCI_FM_INTERVAL),
		 OREAD4(sc, OHCI_FM_REMAINING)));
	DPRINTF(("               fmnum=0x%08x perst=0x%08x lsthrs=0x%08x\n",
		 OREAD4(sc, OHCI_FM_NUMBER),
		 OREAD4(sc, OHCI_PERIODIC_START),
		 OREAD4(sc, OHCI_LS_THRESHOLD)));
	DPRINTF(("               desca=0x%08x descb=0x%08x stat=0x%08x\n",
		 OREAD4(sc, OHCI_RH_DESCRIPTOR_A),
		 OREAD4(sc, OHCI_RH_DESCRIPTOR_B),
		 OREAD4(sc, OHCI_RH_STATUS)));
	DPRINTF(("               port1=0x%08x port2=0x%08x\n",
		 OREAD4(sc, OHCI_RH_PORT_STATUS(1)),
		 OREAD4(sc, OHCI_RH_PORT_STATUS(2))));
	DPRINTF(("         HCCA: frame_number=0x%04x done_head=0x%08x\n",
		 LE(sc->sc_hcca->hcca_frame_number),
		 LE(sc->sc_hcca->hcca_done_head)));
}
#endif

static int ohci_intr1 __P((ohci_softc_t *));

int
ohci_intr(p)
	void *p;
{
	ohci_softc_t *sc = p;

	/* If we get an interrupt while polling, then just ignore it. */
	if (sc->sc_bus.use_polling)
		return (0);

	return (ohci_intr1(sc));
}

static int
ohci_intr1(sc)
	ohci_softc_t *sc;
{
	u_int32_t intrs, eintrs;
	ohci_physaddr_t done;

	/* In case the interrupt occurs before initialization has completed. */
	if (sc == NULL || sc->sc_hcca == NULL) {
#ifdef DIAGNOSTIC
		printf("ohci_intr: sc->sc_hcca == NULL\n");
#endif
		return (0);
	}

        intrs = 0;
	done = LE(sc->sc_hcca->hcca_done_head);
	if (done != 0) {
		sc->sc_hcca->hcca_done_head = 0;
		if (done & ~OHCI_DONE_INTRS)
			intrs = OHCI_WDH;
		if (done & OHCI_DONE_INTRS)
			intrs |= OREAD4(sc, OHCI_INTERRUPT_STATUS);
	} else
		intrs = OREAD4(sc, OHCI_INTERRUPT_STATUS);

	if (!intrs)
		return (0);

	intrs &= ~OHCI_MIE;
	OWRITE4(sc, OHCI_INTERRUPT_STATUS, intrs); /* Acknowledge */
	eintrs = intrs & sc->sc_eintrs;
	if (!eintrs)
		return (0);

	sc->sc_bus.intr_context++;
	sc->sc_bus.no_intrs++;
	DPRINTFN(7, ("ohci_intr: sc=%p intrs=%x(%x) eintr=%x\n", 
		     sc, (u_int)intrs, OREAD4(sc, OHCI_INTERRUPT_STATUS),
		     (u_int)eintrs));

	if (eintrs & OHCI_SO) {
		printf("%s: scheduling overrun\n",USBDEVNAME(sc->sc_bus.bdev));
		/* XXX do what */
		intrs &= ~OHCI_SO;
	}
	if (eintrs & OHCI_WDH) {
		ohci_process_done(sc, done &~ OHCI_DONE_INTRS);
		intrs &= ~OHCI_WDH;
	}
	if (eintrs & OHCI_RD) {
		printf("%s: resume detect\n", USBDEVNAME(sc->sc_bus.bdev));
		/* XXX process resume detect */
	}
	if (eintrs & OHCI_UE) {
		printf("%s: unrecoverable error, controller halted\n",
		       USBDEVNAME(sc->sc_bus.bdev));
		OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
		/* XXX what else */
	}
	if (eintrs & OHCI_RHSC) {
		ohci_rhsc(sc, sc->sc_intrxfer);
		intrs &= ~OHCI_RHSC;

		/* 
		 * Disable RHSC interrupt for now, because it will be
		 * on until the port has been reset.
		 */
		ohci_rhsc_able(sc, 0);
	}

	sc->sc_bus.intr_context--;

	/* Block unprocessed interrupts. XXX */
	OWRITE4(sc, OHCI_INTERRUPT_DISABLE, intrs);
	sc->sc_eintrs &= ~intrs;

	return (1);
}

void
ohci_rhsc_able(sc, on)
	ohci_softc_t *sc;
	int on;
{
	DPRINTFN(4, ("ohci_rhsc_able: on=%d\n", on));
	if (on) {
		sc->sc_eintrs |= OHCI_RHSC;
		OWRITE4(sc, OHCI_INTERRUPT_ENABLE, OHCI_RHSC);
	} else {
		sc->sc_eintrs &= ~OHCI_RHSC;
		OWRITE4(sc, OHCI_INTERRUPT_DISABLE, OHCI_RHSC);
	}
}

#ifdef OHCI_DEBUG
char *ohci_cc_strs[] = {
	"NO_ERROR",
	"CRC",
	"BIT_STUFFING",
	"DATA_TOGGLE_MISMATCH",
	"STALL",
	"DEVICE_NOT_RESPONDING",
	"PID_CHECK_FAILURE",
	"UNEXPECTED_PID",
	"DATA_OVERRUN",
	"DATA_UNDERRUN",
	"BUFFER_OVERRUN",
	"BUFFER_UNDERRUN",
	"NOT_ACCESSED",
};
#endif

void
ohci_process_done(sc, done)
	ohci_softc_t *sc;
	ohci_physaddr_t done;
{
	ohci_soft_td_t *std, *sdone, *stdnext;
	usbd_xfer_handle xfer;
	int len, cc;

	DPRINTFN(10,("ohci_process_done: done=0x%08lx\n", (u_long)done));

	/* Reverse the done list. */
	for (sdone = 0; done; done = LE(std->td.td_nexttd)) {
		std = ohci_hash_find_td(sc, done);
		std->dnext = sdone;
		sdone = std;
	}

#ifdef OHCI_DEBUG
	if (ohcidebug > 10) {
		DPRINTF(("ohci_process_done: TD done:\n"));
		ohci_dump_tds(sdone);
	}
#endif

	for (std = sdone; std; std = stdnext) {
		xfer = std->xfer;
		stdnext = std->dnext;
		DPRINTFN(10, ("ohci_process_done: std=%p xfer=%p hcpriv=%p\n",
				std, xfer, xfer->hcpriv));
		cc = OHCI_TD_GET_CC(LE(std->td.td_flags));
		usb_untimeout(ohci_timeout, xfer, xfer->timo_handle);
		if (xfer->status == USBD_CANCELLED ||
		    xfer->status == USBD_TIMEOUT) {
			DPRINTF(("ohci_process_done: cancel/timeout %p\n",
				 xfer));
			/* Handled by abort routine. */
		} else if (cc == OHCI_CC_NO_ERROR) {
			len = std->len;
			if (std->td.td_cbp != 0)
				len -= LE(std->td.td_be) -
				       LE(std->td.td_cbp) + 1;
			if (std->flags & OHCI_ADD_LEN)
				xfer->actlen += len;
			if (std->flags & OHCI_CALL_DONE) {
				xfer->status = USBD_NORMAL_COMPLETION;
				usb_transfer_complete(xfer);
			}
			ohci_hash_rem_td(sc, std);
			ohci_free_std(sc, std);
		} else {
			/*
			 * Endpoint is halted.  First unlink all the TDs
			 * belonging to the failed transfer, and then restart
			 * the endpoint.
			 */
			ohci_soft_td_t *p, *n;
			struct ohci_pipe *opipe = 
				(struct ohci_pipe *)xfer->pipe;

			DPRINTF(("ohci_process_done: error cc=%d (%s)\n",
			  OHCI_TD_GET_CC(LE(std->td.td_flags)),
			  ohci_cc_strs[OHCI_TD_GET_CC(LE(std->td.td_flags))]));

			/* remove TDs */
			for (p = std; p->xfer == xfer; p = n) {
				n = p->nexttd;
				ohci_hash_rem_td(sc, p);
				ohci_free_std(sc, p);
			}

			/* clear halt */
			opipe->sed->ed.ed_headp = LE(p->physaddr);
			OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_CLF);

			if (cc == OHCI_CC_STALL)
				xfer->status = USBD_STALLED;
			else
				xfer->status = USBD_IOERROR;
			usb_transfer_complete(xfer);
		}
	}
}

void
ohci_device_ctrl_done(xfer)
	usbd_xfer_handle xfer;
{
	DPRINTFN(10,("ohci_ctrl_done: xfer=%p\n", xfer));

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		panic("ohci_ctrl_done: not a request\n");
	}
#endif
	xfer->hcpriv = NULL;
}

void
ohci_device_intr_done(xfer)
	usbd_xfer_handle xfer;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)opipe->pipe.device->bus;
	ohci_soft_ed_t *sed = opipe->sed;
	ohci_soft_td_t *data, *tail;


	DPRINTFN(10,("ohci_intr_done: xfer=%p, actlen=%d\n", 
		     xfer, xfer->actlen));

	xfer->hcpriv = NULL;

	if (xfer->pipe->repeat) {
		data = opipe->tail;
		tail = ohci_alloc_std(sc); /* XXX should reuse TD */
		if (tail == NULL) {
			xfer->status = USBD_NOMEM;
			return;
		}
		tail->xfer = NULL;
		
		data->td.td_flags = LE(
			OHCI_TD_IN | OHCI_TD_NOCC | 
			OHCI_TD_SET_DI(1) | OHCI_TD_TOGGLE_CARRY);
		if (xfer->flags & USBD_SHORT_XFER_OK)
			data->td.td_flags |= LE(OHCI_TD_R);
		data->td.td_cbp = LE(DMAADDR(&xfer->dmabuf));
		data->nexttd = tail;
		data->td.td_nexttd = LE(tail->physaddr);
		data->td.td_be = LE(LE(data->td.td_cbp) + xfer->length - 1);
		data->len = xfer->length;
		data->xfer = xfer;
		data->flags = OHCI_CALL_DONE | OHCI_ADD_LEN;
		xfer->hcpriv = data;
		xfer->actlen = 0;

		ohci_hash_add_td(sc, data);
		sed->ed.ed_tailp = LE(tail->physaddr);
		opipe->tail = tail;
	}
}

void
ohci_device_bulk_done(xfer)
	usbd_xfer_handle xfer;
{
	DPRINTFN(10,("ohci_bulk_done: xfer=%p, actlen=%d\n", 
		     xfer, xfer->actlen));

	xfer->hcpriv = NULL;
}

void
ohci_rhsc(sc, xfer)
	ohci_softc_t *sc;
	usbd_xfer_handle xfer;
{
	usbd_pipe_handle pipe;
	struct ohci_pipe *opipe;
	u_char *p;
	int i, m;
	int hstatus;

	hstatus = OREAD4(sc, OHCI_RH_STATUS);
	DPRINTF(("ohci_rhsc: sc=%p xfer=%p hstatus=0x%08x\n", 
		 sc, xfer, hstatus));

	if (xfer == NULL) {
		/* Just ignore the change. */
		return;
	}

	pipe = xfer->pipe;
	opipe = (struct ohci_pipe *)pipe;

	p = KERNADDR(&xfer->dmabuf);
	m = min(sc->sc_noport, xfer->length * 8 - 1);
	memset(p, 0, xfer->length);
	for (i = 1; i <= m; i++) {
		if (OREAD4(sc, OHCI_RH_PORT_STATUS(i)) >> 16)
			p[i/8] |= 1 << (i%8);
	}
	DPRINTF(("ohci_rhsc: change=0x%02x\n", *p));
	xfer->actlen = xfer->length;
	xfer->status = USBD_NORMAL_COMPLETION;

	usb_transfer_complete(xfer);
}

void
ohci_root_intr_done(xfer)
	usbd_xfer_handle xfer;
{
	xfer->hcpriv = NULL;
}

/*
 * Wait here until controller claims to have an interrupt.
 * Then call ohci_intr and return.  Use timeout to avoid waiting
 * too long.
 */
void
ohci_waitintr(sc, xfer)
	ohci_softc_t *sc;
	usbd_xfer_handle xfer;
{
	int timo = xfer->timeout;
	int usecs;
	u_int32_t intrs;

	xfer->status = USBD_IN_PROGRESS;
	for (usecs = timo * 1000000 / hz; usecs > 0; usecs -= 1000) {
		usb_delay_ms(&sc->sc_bus, 1);
		intrs = OREAD4(sc, OHCI_INTERRUPT_STATUS) & sc->sc_eintrs;
		DPRINTFN(15,("ohci_waitintr: 0x%04x\n", intrs));
#ifdef OHCI_DEBUG
		if (ohcidebug > 15)
			ohci_dumpregs(sc);
#endif
		if (intrs) {
			ohci_intr1(sc);
			if (xfer->status != USBD_IN_PROGRESS)
				return;
		}
	}

	/* Timeout */
	DPRINTF(("ohci_waitintr: timeout\n"));
	xfer->status = USBD_TIMEOUT;
	usb_transfer_complete(xfer);
	/* XXX should free TD */
}

void
ohci_poll(bus)
	struct usbd_bus *bus;
{
	ohci_softc_t *sc = (ohci_softc_t *)bus;

	if (OREAD4(sc, OHCI_INTERRUPT_STATUS) & sc->sc_eintrs)
		ohci_intr1(sc);
}

usbd_status
ohci_device_request(xfer)
	usbd_xfer_handle xfer;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usb_device_request_t *req = &xfer->request;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	int addr = dev->address;
	ohci_soft_td_t *setup, *data = 0, *stat, *next, *tail;
	ohci_soft_ed_t *sed;
	int isread;
	int len;
	usbd_status err;
	int s;

	isread = req->bmRequestType & UT_READ;
	len = UGETW(req->wLength);

	DPRINTFN(3,("ohci_device_control type=0x%02x, request=0x%02x, "
		    "wValue=0x%04x, wIndex=0x%04x len=%d, addr=%d, endpt=%d\n",
		    req->bmRequestType, req->bRequest, UGETW(req->wValue),
		    UGETW(req->wIndex), len, addr, 
		    opipe->pipe.endpoint->edesc->bEndpointAddress));

	setup = opipe->tail;
	stat = ohci_alloc_std(sc);
	if (stat == NULL) {
		err = USBD_NOMEM;
		goto bad1;
	}
	tail = ohci_alloc_std(sc);
	if (tail == NULL) {
		err = USBD_NOMEM;
		goto bad2;
	}
	tail->xfer = NULL;

	sed = opipe->sed;
	opipe->u.ctl.length = len;

	/* Update device address and length since they may have changed. */
	/* XXX This only needs to be done once, but it's too early in open. */
	sed->ed.ed_flags = LE(
	 (LE(sed->ed.ed_flags) & ~(OHCI_ED_ADDRMASK | OHCI_ED_MAXPMASK)) |
	 OHCI_ED_SET_FA(addr) |
	 OHCI_ED_SET_MAXP(UGETW(opipe->pipe.endpoint->edesc->wMaxPacketSize)));

	/* Set up data transaction */
	if (len != 0) {
		data = ohci_alloc_std(sc);
		if (data == NULL) {
			err = USBD_NOMEM;
			goto bad3;
		}
		data->td.td_flags = LE(
			(isread ? OHCI_TD_IN : OHCI_TD_OUT) | OHCI_TD_NOCC |
			OHCI_TD_TOGGLE_1 | OHCI_TD_NOINTR |
			(xfer->flags & USBD_SHORT_XFER_OK ? OHCI_TD_R : 0));
		data->td.td_cbp = LE(DMAADDR(&xfer->dmabuf));
		data->nexttd = stat;
		data->td.td_nexttd = LE(stat->physaddr);
		data->td.td_be = LE(LE(data->td.td_cbp) + len - 1);
		data->len = len;
		data->xfer = xfer;
		data->flags = OHCI_ADD_LEN;

		next = data;
		stat->flags = OHCI_CALL_DONE;
	} else {
		next = stat;
		/* XXX ADD_LEN? */
		stat->flags = OHCI_CALL_DONE | OHCI_ADD_LEN;
	}

	memcpy(KERNADDR(&opipe->u.ctl.reqdma), req, sizeof *req);

	setup->td.td_flags = LE(OHCI_TD_SETUP | OHCI_TD_NOCC |
				 OHCI_TD_TOGGLE_0 | OHCI_TD_NOINTR);
	setup->td.td_cbp = LE(DMAADDR(&opipe->u.ctl.reqdma));
	setup->nexttd = next;
	setup->td.td_nexttd = LE(next->physaddr);
	setup->td.td_be = LE(LE(setup->td.td_cbp) + sizeof *req - 1);
	setup->len = 0;		/* XXX The number of byte we count */
	setup->xfer = xfer;
	setup->flags = 0;
	xfer->hcpriv = setup;

	stat->td.td_flags = LE(
		(isread ? OHCI_TD_OUT : OHCI_TD_IN) | OHCI_TD_NOCC |
		OHCI_TD_TOGGLE_1 | OHCI_TD_SET_DI(1));
	stat->td.td_cbp = 0;
	stat->nexttd = tail;
	stat->td.td_nexttd = LE(tail->physaddr);
	stat->td.td_be = 0;
	stat->len = 0;
	stat->xfer = xfer;

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		DPRINTF(("ohci_device_request:\n"));
		ohci_dump_ed(sed);
		ohci_dump_tds(setup);
	}
#endif

	/* Insert ED in schedule */
	s = splusb();
	ohci_hash_add_td(sc, setup);
	if (len != 0)
		ohci_hash_add_td(sc, data);
	ohci_hash_add_td(sc, stat);
	sed->ed.ed_tailp = LE(tail->physaddr);
	opipe->tail = tail;
	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_CLF);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
                usb_timeout(ohci_timeout, xfer,
			    MS_TO_TICKS(xfer->timeout), xfer->timo_handle);
	}
	splx(s);

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		usb_delay_ms(&sc->sc_bus, 5);
		DPRINTF(("ohci_device_request: status=%x\n",
			 OREAD4(sc, OHCI_COMMAND_STATUS)));
		ohci_dump_ed(sed);
		ohci_dump_tds(setup);
	}
#endif

	return (USBD_NORMAL_COMPLETION);

 bad3:
	ohci_free_std(sc, tail);
 bad2:
	ohci_free_std(sc, stat);
 bad1:
	return (err);
}

/*
 * Add an ED to the schedule.  Called at splusb().
 */
void
ohci_add_ed(sed, head)
	ohci_soft_ed_t *sed; 
	ohci_soft_ed_t *head; 
{
	SPLUSBCHECK;
	sed->next = head->next;
	sed->ed.ed_nexted = head->ed.ed_nexted;
	head->next = sed;
	head->ed.ed_nexted = LE(sed->physaddr);
}

/*
 * Remove an ED from the schedule.  Called at splusb().
 */
void
ohci_rem_ed(sed, head)
	ohci_soft_ed_t *sed; 
	ohci_soft_ed_t *head; 
{
	ohci_soft_ed_t *p; 

	SPLUSBCHECK;

	/* XXX */
	for (p = head; p == NULL && p->next != sed; p = p->next)
		;
	if (p == NULL)
		panic("ohci_rem_ed: ED not found\n");
	p->next = sed->next;
	p->ed.ed_nexted = sed->ed.ed_nexted;
}

/*
 * When a transfer is completed the TD is added to the done queue by
 * the host controller.  This queue is the processed by software.
 * Unfortunately the queue contains the physical address of the TD
 * and we have no simple way to translate this back to a kernel address.
 * To make the translation possible (and fast) we use a hash table of
 * TDs currently in the schedule.  The physical address is used as the
 * hash value.
 */

#define HASH(a) (((a) >> 4) % OHCI_HASH_SIZE)
/* Called at splusb() */
void
ohci_hash_add_td(sc, std)
	ohci_softc_t *sc;
	ohci_soft_td_t *std;
{
	int h = HASH(std->physaddr);

	SPLUSBCHECK;

	LIST_INSERT_HEAD(&sc->sc_hash_tds[h], std, hnext);
}

/* Called at splusb() */
void
ohci_hash_rem_td(sc, std)
	ohci_softc_t *sc;
	ohci_soft_td_t *std;
{
	SPLUSBCHECK;

	LIST_REMOVE(std, hnext);
}

ohci_soft_td_t *
ohci_hash_find_td(sc, a)
	ohci_softc_t *sc;
	ohci_physaddr_t a;
{
	int h = HASH(a);
	ohci_soft_td_t *std;

	for (std = LIST_FIRST(&sc->sc_hash_tds[h]); 
	     std != NULL; 
	     std = LIST_NEXT(std, hnext))
		if (std->physaddr == a)
			return (std);
	panic("ohci_hash_find_td: addr 0x%08lx not found\n", (u_long)a);
}

void
ohci_timeout(addr)
	void *addr;
{
	usbd_xfer_handle xfer = addr;
	int s;

	DPRINTF(("ohci_timeout: xfer=%p\n", xfer));

	s = splusb();
	xfer->device->bus->intr_context++;
	ohci_abort_req(xfer, USBD_TIMEOUT);
	xfer->device->bus->intr_context--;
	splx(s);
}

#ifdef OHCI_DEBUG
void
ohci_dump_tds(std)
	ohci_soft_td_t *std;
{
	for (; std; std = std->nexttd)
		ohci_dump_td(std);
}

void
ohci_dump_td(std)
	ohci_soft_td_t *std;
{
	DPRINTF(("TD(%p) at %08lx: %b delay=%d ec=%d cc=%d\ncbp=0x%08lx "
		 "nexttd=0x%08lx be=0x%08lx\n", 
		 std, (u_long)std->physaddr,
		 (int)LE(std->td.td_flags),
		 "\20\23R\24OUT\25IN\31TOG1\32SETTOGGLE",
		 OHCI_TD_GET_DI(LE(std->td.td_flags)),
		 OHCI_TD_GET_EC(LE(std->td.td_flags)),
		 OHCI_TD_GET_CC(LE(std->td.td_flags)),
		 (u_long)LE(std->td.td_cbp),
		 (u_long)LE(std->td.td_nexttd), (u_long)LE(std->td.td_be)));
}

void
ohci_dump_ed(sed)
	ohci_soft_ed_t *sed;
{
	DPRINTF(("ED(%p) at %08lx: addr=%d endpt=%d maxp=%d %b\ntailp=0x%08lx "
		 "headp=%b nexted=0x%08lx\n",
		 sed, (u_long)sed->physaddr, 
		 OHCI_ED_GET_FA(LE(sed->ed.ed_flags)),
		 OHCI_ED_GET_EN(LE(sed->ed.ed_flags)),
		 OHCI_ED_GET_MAXP(LE(sed->ed.ed_flags)),
		 (int)LE(sed->ed.ed_flags),
		 "\20\14OUT\15IN\16LOWSPEED\17SKIP\20ISO",
		 (u_long)LE(sed->ed.ed_tailp),
		 (u_long)LE(sed->ed.ed_headp),
		 "\20\1HALT\2CARRY",
		 (u_long)LE(sed->ed.ed_nexted)));
}
#endif

usbd_status
ohci_open(pipe)
	usbd_pipe_handle pipe;
{
	usbd_device_handle dev = pipe->device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	usb_endpoint_descriptor_t *ed = pipe->endpoint->edesc;
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	u_int8_t addr = dev->address;
	ohci_soft_ed_t *sed;
	ohci_soft_td_t *std;
	usbd_status err;
	int s;

	DPRINTFN(1, ("ohci_open: pipe=%p, addr=%d, endpt=%d (%d)\n",
		     pipe, addr, ed->bEndpointAddress, sc->sc_addr));
	if (addr == sc->sc_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &ohci_root_ctrl_methods;
			break;
		case UE_DIR_IN | OHCI_INTR_ENDPT:
			pipe->methods = &ohci_root_intr_methods;
			break;
		default:
			return (USBD_INVAL);
		}
	} else {
		sed = ohci_alloc_sed(sc);
		if (sed == NULL)
			goto bad0;
	        std = ohci_alloc_std(sc);
		if (std == NULL)
			goto bad1;
		opipe->sed = sed;
		opipe->tail = std;
		sed->ed.ed_flags = LE(
			OHCI_ED_SET_FA(addr) | 
			OHCI_ED_SET_EN(ed->bEndpointAddress) |
			OHCI_ED_DIR_TD | 
			(dev->lowspeed ? OHCI_ED_SPEED : 0) | 
			((ed->bmAttributes & UE_XFERTYPE) == UE_ISOCHRONOUS ?
			 OHCI_ED_FORMAT_ISO : OHCI_ED_FORMAT_GEN) |
			OHCI_ED_SET_MAXP(UGETW(ed->wMaxPacketSize)));
		sed->ed.ed_headp = sed->ed.ed_tailp = LE(std->physaddr);

		switch (ed->bmAttributes & UE_XFERTYPE) {
		case UE_CONTROL:
			pipe->methods = &ohci_device_ctrl_methods;
			err = usb_allocmem(&sc->sc_bus, 
				  sizeof(usb_device_request_t), 
				  0, &opipe->u.ctl.reqdma);
			if (err)
				goto bad;
			s = splusb();
			ohci_add_ed(sed, sc->sc_ctrl_head);
			splx(s);
			break;
		case UE_INTERRUPT:
			pipe->methods = &ohci_device_intr_methods;
			return (ohci_device_setintr(sc, opipe, ed->bInterval));
		case UE_ISOCHRONOUS:
			printf("ohci_open: open iso unimplemented\n");
			return (USBD_INVAL);
		case UE_BULK:
			pipe->methods = &ohci_device_bulk_methods;
			s = splusb();
			ohci_add_ed(sed, sc->sc_bulk_head);
			splx(s);
			break;
		}
	}
	return (USBD_NORMAL_COMPLETION);

 bad:
	ohci_free_std(sc, std);
 bad1:
	ohci_free_sed(sc, sed);
 bad0:
	return (USBD_NOMEM);
	
}

/*
 * Close a reqular pipe.
 * Assumes that there are no pending transactions.
 */
void
ohci_close_pipe(pipe, head)
	usbd_pipe_handle pipe;
	ohci_soft_ed_t *head;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	ohci_soft_ed_t *sed = opipe->sed;
	int s;

	s = splusb();
#ifdef DIAGNOSTIC
	sed->ed.ed_flags |= LE(OHCI_ED_SKIP);
	if ((sed->ed.ed_tailp & LE(OHCI_TAILMASK)) != 
	    (sed->ed.ed_headp & LE(OHCI_TAILMASK))) {
		ohci_physaddr_t td = sed->ed.ed_headp;
		ohci_soft_td_t *std;
		for (std = LIST_FIRST(&sc->sc_hash_tds[HASH(td)]); 
		     std != NULL; 
		     std = LIST_NEXT(std, hnext))
		    if (std->physaddr == td)
			break;
		printf("ohci_close_pipe: pipe not empty sed=%p hd=0x%x "
		       "tl=0x%x pipe=%p, std=%p\n", sed,
		       (int)LE(sed->ed.ed_headp), (int)LE(sed->ed.ed_tailp),
		       pipe, std);
		usb_delay_ms(&sc->sc_bus, 2);
		if ((sed->ed.ed_tailp & LE(OHCI_TAILMASK)) != 
		    (sed->ed.ed_headp & LE(OHCI_TAILMASK)))
			printf("ohci_close_pipe: pipe still not empty\n");
	}
#endif
	ohci_rem_ed(sed, head);
	splx(s);
	ohci_free_std(sc, opipe->tail);
	ohci_free_sed(sc, opipe->sed);
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
ohci_abort_req(xfer, status)
	usbd_xfer_handle xfer;
	usbd_status status;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_soft_ed_t *sed;

	DPRINTF(("ohci_abort_req: xfer=%p pipe=%p\n", xfer, opipe));

	xfer->status = status;

	usb_untimeout(ohci_timeout, xfer, xfer->timo_handle);

	sed = opipe->sed;
	DPRINTFN(1,("ohci_abort_req: stop ed=%p\n", sed));
	sed->ed.ed_flags |= LE(OHCI_ED_SKIP); /* force hardware skip */

	if (xfer->device->bus->intr_context) {
		/* We have no process context, so we can't use tsleep(). */
		timeout(ohci_abort_req_end, xfer, hz / USB_FRAMES_PER_SECOND);
	} else {
#if defined(DIAGNOSTIC) && defined(__i386__)
		KASSERT(intr_nesting_level == 0,
	        	("ohci_abort_req in interrupt context"));
#endif
		usb_delay_ms(opipe->pipe.device->bus, 1);
		ohci_abort_req_end(xfer);
	}
}

void
ohci_abort_req_end(v)
	void *v;
{
	usbd_xfer_handle xfer = v;
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)opipe->pipe.device->bus;
	ohci_soft_ed_t *sed;
	ohci_soft_td_t *p, *n;
	int s;

	s = splusb();

	p = xfer->hcpriv;
#ifdef DIAGNOSTIC
	if (p == NULL) {
		printf("ohci_abort_req: hcpriv==0\n");
		return;
	}
#endif
	for (; p->xfer == xfer; p = n) {
		n = p->nexttd;
		ohci_hash_rem_td(sc, p);
		ohci_free_std(sc, p);
	}

	sed = opipe->sed;
	DPRINTFN(2,("ohci_abort_req: set hd=%x, tl=%x\n",
		    (int)LE(p->physaddr), (int)LE(sed->ed.ed_tailp)));
	sed->ed.ed_headp = p->physaddr; /* unlink TDs */
	sed->ed.ed_flags &= LE(~OHCI_ED_SKIP); /* remove hardware skip */

	usb_transfer_complete(xfer);

	splx(s);
}

/*
 * Data structures and routines to emulate the root hub.
 */
static usb_device_descriptor_t ohci_devd = {
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

static usb_config_descriptor_t ohci_confd = {
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

static usb_interface_descriptor_t ohci_ifcd = {
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

static usb_endpoint_descriptor_t ohci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_DIR_IN | OHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{8, 0},			/* max packet */
	255
};

static usb_hub_descriptor_t ohci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	0,
	{0,0},
	0,
	0,
	{0},
};

static int
ohci_str(p, l, s)
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
static usbd_status
ohci_root_ctrl_transfer(xfer)
	usbd_xfer_handle xfer;
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_root_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_root_ctrl_start(xfer)
	usbd_xfer_handle xfer;
{
	ohci_softc_t *sc = (ohci_softc_t *)xfer->pipe->device->bus;
	usb_device_request_t *req;
	void *buf = NULL;
	int port, i;
	int s, len, value, index, l, totlen = 0;
	usb_port_status_t ps;
	usb_hub_descriptor_t hubd;
	usbd_status err;
	u_int32_t v;

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST))
		/* XXX panic */
		return (USBD_INVAL);
#endif
	req = &xfer->request;

	DPRINTFN(4,("ohci_root_ctrl_control type=0x%02x request=%02x\n", 
		    req->bmRequestType, req->bRequest));

	len = UGETW(req->wLength);
	value = UGETW(req->wValue);
	index = UGETW(req->wIndex);

	if (len != 0)
		buf = KERNADDR(&xfer->dmabuf);

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
		DPRINTFN(8,("ohci_root_ctrl_control wValue=0x%04x\n", value));
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			USETW(ohci_devd.idVendor, sc->sc_id_vendor);
			memcpy(buf, &ohci_devd, l);
			break;
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				err = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_CONFIG_DESCRIPTOR_SIZE);
			memcpy(buf, &ohci_confd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_INTERFACE_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ohci_ifcd, l);
			buf = (char *)buf + l;
			len -= l;
			l = min(len, USB_ENDPOINT_DESCRIPTOR_SIZE);
			totlen += l;
			memcpy(buf, &ohci_endpd, l);
			break;
		case UDESC_STRING:
			if (len == 0)
				break;
			*(u_int8_t *)buf = 0;
			totlen = 1;
			switch (value & 0xff) {
			case 1: /* Vendor */
				totlen = ohci_str(buf, len, sc->sc_vendor);
				break;
			case 2: /* Product */
				totlen = ohci_str(buf, len, "OHCI root hub");
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
		DPRINTFN(8, ("ohci_root_ctrl_control: UR_CLEAR_PORT_FEATURE "
			     "port=%d feature=%d\n",
			     index, value));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		port = OHCI_RH_PORT_STATUS(index);
		switch(value) {
		case UHF_PORT_ENABLE:
			OWRITE4(sc, port, UPS_CURRENT_CONNECT_STATUS);
			break;
		case UHF_PORT_SUSPEND:
			OWRITE4(sc, port, UPS_OVERCURRENT_INDICATOR);
			break;
		case UHF_PORT_POWER:
			OWRITE4(sc, port, UPS_LOW_SPEED);
			break;
		case UHF_C_PORT_CONNECTION:
			OWRITE4(sc, port, UPS_C_CONNECT_STATUS << 16);
			break;
		case UHF_C_PORT_ENABLE:
			OWRITE4(sc, port, UPS_C_PORT_ENABLED << 16);
			break;
		case UHF_C_PORT_SUSPEND:
			OWRITE4(sc, port, UPS_C_SUSPEND << 16);
			break;
		case UHF_C_PORT_OVER_CURRENT:
			OWRITE4(sc, port, UPS_C_OVERCURRENT_INDICATOR << 16);
			break;
		case UHF_C_PORT_RESET:
			OWRITE4(sc, port, UPS_C_PORT_RESET << 16);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
		switch(value) {
		case UHF_C_PORT_CONNECTION:
		case UHF_C_PORT_ENABLE:
		case UHF_C_PORT_SUSPEND:
		case UHF_C_PORT_OVER_CURRENT:
		case UHF_C_PORT_RESET:
			/* Enable RHSC interrupt if condition is cleared. */
			if ((OREAD4(sc, port) >> 16) == 0)
				ohci_rhsc_able(sc, 1);
			break;
		default:
			break;
		}
		break;
	case C(UR_GET_DESCRIPTOR, UT_READ_CLASS_DEVICE):
		if (value != 0) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = OREAD4(sc, OHCI_RH_DESCRIPTOR_A);
		hubd = ohci_hubd;
		hubd.bNbrPorts = sc->sc_noport;
		USETW(hubd.wHubCharacteristics,
		      (v & OHCI_NPS ? UHD_PWR_NO_SWITCH : 
		       v & OHCI_PSM ? UHD_PWR_GANGED : UHD_PWR_INDIVIDUAL)
		      /* XXX overcurrent */
		      );
		hubd.bPwrOn2PwrGood = OHCI_GET_POTPGT(v);
		v = OREAD4(sc, OHCI_RH_DESCRIPTOR_B);
		for (i = 0, l = sc->sc_noport; l > 0; i++, l -= 8, v >>= 8) 
			hubd.DeviceRemovable[i++] = (u_int8_t)v;
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
		DPRINTFN(8,("ohci_root_ctrl_transfer: get port status i=%d\n",
			    index));
		if (index < 1 || index > sc->sc_noport) {
			err = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			err = USBD_IOERROR;
			goto ret;
		}
		v = OREAD4(sc, OHCI_RH_PORT_STATUS(index));
		DPRINTFN(8,("ohci_root_ctrl_transfer: port status=0x%04x\n",
			    v));
		USETW(ps.wPortStatus, v);
		USETW(ps.wPortChange, v >> 16);
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
		port = OHCI_RH_PORT_STATUS(index);
		switch(value) {
		case UHF_PORT_ENABLE:
			OWRITE4(sc, port, UPS_PORT_ENABLED);
			break;
		case UHF_PORT_SUSPEND:
			OWRITE4(sc, port, UPS_SUSPEND);
			break;
		case UHF_PORT_RESET:
			DPRINTFN(5,("ohci_root_ctrl_transfer: reset port %d\n",
				    index));
			OWRITE4(sc, port, UPS_RESET);
			for (i = 0; i < 10; i++) {
				usb_delay_ms(&sc->sc_bus, 10);
				if ((OREAD4(sc, port) & UPS_RESET) == 0)
					break;
			}
			DPRINTFN(8,("ohci port %d reset, status = 0x%04x\n",
				    index, OREAD4(sc, port)));
			break;
		case UHF_PORT_POWER:
			DPRINTFN(2,("ohci_root_ctrl_transfer: set port power "
				    "%d\n", index));
			OWRITE4(sc, port, UPS_PORT_POWER);
			break;
		default:
			err = USBD_IOERROR;
			goto ret;
		}
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
	usb_transfer_complete(xfer);
	splx(s);
	return (USBD_IN_PROGRESS);
}

/* Abort a root control request. */
static void
ohci_root_ctrl_abort(xfer)
	usbd_xfer_handle xfer;
{
	/* Nothing to do, all transfers are synchronous. */
}

/* Close the root pipe. */
static void
ohci_root_ctrl_close(pipe)
	usbd_pipe_handle pipe;
{
	DPRINTF(("ohci_root_ctrl_close\n"));
	/* Nothing to do. */
}

static usbd_status
ohci_root_intr_transfer(xfer)
	usbd_xfer_handle xfer;
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_root_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_root_intr_start(xfer)
	usbd_xfer_handle xfer;
{
	usbd_pipe_handle pipe = xfer->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;

	sc->sc_intrxfer = xfer;

	return (USBD_IN_PROGRESS);
}

/* Abort a root interrupt request. */
static void
ohci_root_intr_abort(xfer)
	usbd_xfer_handle xfer;
{
	int s;

	if (xfer->pipe->intrxfer == xfer) {
		DPRINTF(("ohci_root_intr_abort: remove\n"));
		xfer->pipe->intrxfer = NULL;
	}
	xfer->status = USBD_CANCELLED;
	s = splusb();
	usb_transfer_complete(xfer);
	splx(s);
}

/* Close the root pipe. */
static void
ohci_root_intr_close(pipe)
	usbd_pipe_handle pipe;
{
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	
	DPRINTF(("ohci_root_intr_close\n"));

	sc->sc_intrxfer = NULL;
}

/************************/

static usbd_status
ohci_device_ctrl_transfer(xfer)
	usbd_xfer_handle xfer;
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_device_ctrl_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_device_ctrl_start(xfer)
	usbd_xfer_handle xfer;
{
	ohci_softc_t *sc = (ohci_softc_t *)xfer->pipe->device->bus;
	usbd_status err;

#ifdef DIAGNOSTIC
	if (!(xfer->rqflags & URQ_REQUEST)) {
		/* XXX panic */
		printf("ohci_device_ctrl_transfer: not a request\n");
		return (USBD_INVAL);
	}
#endif

	err = ohci_device_request(xfer);
	if (err)
		return (err);

	if (sc->sc_bus.use_polling)
		ohci_waitintr(sc, xfer);
	return (USBD_IN_PROGRESS);
}

/* Abort a device control request. */
static void
ohci_device_ctrl_abort(xfer)
	usbd_xfer_handle xfer;
{
	DPRINTF(("ohci_device_ctrl_abort: xfer=%p\n", xfer));
	ohci_abort_req(xfer, USBD_CANCELLED);
}

/* Close a device control pipe. */
static void
ohci_device_ctrl_close(pipe)
	usbd_pipe_handle pipe;
{
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;

	DPRINTF(("ohci_device_ctrl_close: pipe=%p\n", pipe));
	ohci_close_pipe(pipe, sc->sc_ctrl_head);
}

/************************/

static void
ohci_device_clear_toggle(pipe)
	usbd_pipe_handle pipe;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;

	opipe->sed->ed.ed_tailp &= LE(~OHCI_TOGGLECARRY);
}

static void
ohci_noop(pipe)
	usbd_pipe_handle pipe;
{
}

static usbd_status
ohci_device_bulk_transfer(xfer)
	usbd_xfer_handle xfer;
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_device_bulk_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_device_bulk_start(xfer)
	usbd_xfer_handle xfer;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	int addr = dev->address;
	ohci_soft_td_t *data, *tail, *tdp;
	ohci_soft_ed_t *sed;
	int s, len, isread, endpt;
	usbd_status err;

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST) {
		/* XXX panic */
		printf("ohci_device_bulk_start: a request\n");
		return (USBD_INVAL);
	}
#endif

	len = xfer->length;
	endpt = xfer->pipe->endpoint->edesc->bEndpointAddress;
	isread = UE_GET_DIR(endpt) == UE_DIR_IN;
	sed = opipe->sed;

	DPRINTFN(4,("ohci_device_bulk_start: xfer=%p len=%d isread=%d "
		    "flags=%d endpt=%d\n", xfer, len, isread, xfer->flags,
		    endpt));

	opipe->u.bulk.isread = isread;
	opipe->u.bulk.length = len;

	/* Update device address */
	sed->ed.ed_flags = LE(
		(LE(sed->ed.ed_flags) & ~OHCI_ED_ADDRMASK) |
		OHCI_ED_SET_FA(addr));

	/* Allocate a chain of new TDs (including a new tail). */
	data = opipe->tail;
	err = ohci_alloc_std_chain(opipe, sc, len, isread, 
		  xfer->flags & USBD_SHORT_XFER_OK,
		  &xfer->dmabuf, data, &tail);
	if (err)
		return (err);

	tail->xfer = NULL;
	xfer->hcpriv = data;

	DPRINTFN(4,("ohci_device_bulk_start: ed_flags=0x%08x td_flags=0x%08x "
		    "td_cbp=0x%08x td_be=0x%08x\n",
		    (int)LE(sed->ed.ed_flags), (int)LE(data->td.td_flags),
		    (int)LE(data->td.td_cbp), (int)LE(data->td.td_be)));

#ifdef OHCI_DEBUG
	if (ohcidebug > 4) {
		ohci_dump_ed(sed);
		ohci_dump_tds(data);
	}
#endif

	/* Insert ED in schedule */
	s = splusb();
	for (tdp = data; tdp != tail; tdp = tdp->nexttd) {
		tdp->xfer = xfer;
		ohci_hash_add_td(sc, tdp);
	}
	sed->ed.ed_tailp = LE(tail->physaddr);
	opipe->tail = tail;
	sed->ed.ed_flags &= LE(~OHCI_ED_SKIP);
	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_BLF);
	if (xfer->timeout && !sc->sc_bus.use_polling) {
                usb_timeout(ohci_timeout, xfer,
			    MS_TO_TICKS(xfer->timeout), xfer->timo_handle);
	}

#if 0
/* This goes wrong if we are too slow. */
	if (ohcidebug > 5) {
		usb_delay_ms(&sc->sc_bus, 5);
		DPRINTF(("ohci_device_intr_transfer: status=%x\n",
			 OREAD4(sc, OHCI_COMMAND_STATUS)));
		ohci_dump_ed(sed);
		ohci_dump_tds(data);
	}
#endif

	splx(s);

	return (USBD_IN_PROGRESS);
}

static void
ohci_device_bulk_abort(xfer)
	usbd_xfer_handle xfer;
{
	DPRINTF(("ohci_device_bulk_abort: xfer=%p\n", xfer));
	ohci_abort_req(xfer, USBD_CANCELLED);
}

/* 
 * Close a device bulk pipe.
 */
static void
ohci_device_bulk_close(pipe)
	usbd_pipe_handle pipe;
{
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;

	DPRINTF(("ohci_device_bulk_close: pipe=%p\n", pipe));
	ohci_close_pipe(pipe, sc->sc_bulk_head);
}

/************************/

static usbd_status
ohci_device_intr_transfer(xfer)
	usbd_xfer_handle xfer;
{
	usbd_status err;

	/* Insert last in queue. */
	err = usb_insert_transfer(xfer);
	if (err)
		return (err);

	/* Pipe isn't running, start first */
	return (ohci_device_intr_start(SIMPLEQ_FIRST(&xfer->pipe->queue)));
}

static usbd_status
ohci_device_intr_start(xfer)
	usbd_xfer_handle xfer;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)xfer->pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	ohci_soft_ed_t *sed = opipe->sed;
	ohci_soft_td_t *data, *tail;
	int len;
	int s;

	DPRINTFN(3, ("ohci_device_intr_transfer: xfer=%p len=%d "
		     "flags=%d priv=%p\n",
		     xfer, xfer->length, xfer->flags, xfer->priv));

#ifdef DIAGNOSTIC
	if (xfer->rqflags & URQ_REQUEST)
		panic("ohci_device_intr_transfer: a request\n");
#endif

	len = xfer->length;

	data = opipe->tail;
	tail = ohci_alloc_std(sc);
	if (tail == NULL)
		return (USBD_NOMEM);
	tail->xfer = NULL;

	data->td.td_flags = LE(
		OHCI_TD_IN | OHCI_TD_NOCC | 
		OHCI_TD_SET_DI(1) | OHCI_TD_TOGGLE_CARRY);
	if (xfer->flags & USBD_SHORT_XFER_OK)
		data->td.td_flags |= LE(OHCI_TD_R);
	data->td.td_cbp = LE(DMAADDR(&xfer->dmabuf));
	data->nexttd = tail;
	data->td.td_nexttd = LE(tail->physaddr);
	data->td.td_be = LE(LE(data->td.td_cbp) + len - 1);
	data->len = len;
	data->xfer = xfer;
	data->flags = OHCI_CALL_DONE | OHCI_ADD_LEN;
	xfer->hcpriv = data;

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		DPRINTF(("ohci_device_intr_transfer:\n"));
		ohci_dump_ed(sed);
		ohci_dump_tds(data);
	}
#endif

	/* Insert ED in schedule */
	s = splusb();
	ohci_hash_add_td(sc, data);
	sed->ed.ed_tailp = LE(tail->physaddr);
	opipe->tail = tail;
	sed->ed.ed_flags &= LE(~OHCI_ED_SKIP);

#if 0
/*
 * This goes horribly wrong, printing thousands of descriptors,
 * because false references are followed due to the fact that the
 * TD is gone.
 */
	if (ohcidebug > 5) {
		usb_delay_ms(&sc->sc_bus, 5);
		DPRINTF(("ohci_device_intr_transfer: status=%x\n",
			 OREAD4(sc, OHCI_COMMAND_STATUS)));
		ohci_dump_ed(sed);
		ohci_dump_tds(data);
	}
#endif
	splx(s);

	return (USBD_IN_PROGRESS);
}

/* Abort a device control request. */
static void
ohci_device_intr_abort(xfer)
	usbd_xfer_handle xfer;
{
	if (xfer->pipe->intrxfer == xfer) {
		DPRINTF(("ohci_device_intr_abort: remove\n"));
		xfer->pipe->intrxfer = NULL;
	}
	ohci_abort_req(xfer, USBD_CANCELLED);
}

/* Close a device interrupt pipe. */
static void
ohci_device_intr_close(pipe)
	usbd_pipe_handle pipe;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	int nslots = opipe->u.intr.nslots;
	int pos = opipe->u.intr.pos;
	int j;
	ohci_soft_ed_t *p, *sed = opipe->sed;
	int s;

	DPRINTFN(1,("ohci_device_intr_close: pipe=%p nslots=%d pos=%d\n",
		    pipe, nslots, pos));
	s = splusb();
	sed->ed.ed_flags |= LE(OHCI_ED_SKIP);
	if ((sed->ed.ed_tailp & LE(OHCI_TAILMASK)) != 
	    (sed->ed.ed_headp & LE(OHCI_TAILMASK)))
		usb_delay_ms(&sc->sc_bus, 2);

	for (p = sc->sc_eds[pos]; p && p->next != sed; p = p->next)
		;
#ifdef DIAGNOSTIC
	if (p == NULL)
		panic("ohci_device_intr_close: ED not found\n");
#endif
	p->next = sed->next;
	p->ed.ed_nexted = sed->ed.ed_nexted;
	splx(s);

	for (j = 0; j < nslots; j++)
		--sc->sc_bws[(pos * nslots + j) % OHCI_NO_INTRS];

	ohci_free_std(sc, opipe->tail);
	ohci_free_sed(sc, opipe->sed);
}

static usbd_status
ohci_device_setintr(sc, opipe, ival)
	ohci_softc_t *sc;
	struct ohci_pipe *opipe;
	int ival;
{
	int i, j, s, best;
	u_int npoll, slow, shigh, nslots;
	u_int bestbw, bw;
	ohci_soft_ed_t *hsed, *sed = opipe->sed;

	DPRINTFN(2, ("ohci_setintr: pipe=%p\n", opipe));
	if (ival == 0) {
		printf("ohci_setintr: 0 interval\n");
		return (USBD_INVAL);
	}

	npoll = OHCI_NO_INTRS;
	while (npoll > ival)
		npoll /= 2;
	DPRINTFN(2, ("ohci_setintr: ival=%d npoll=%d\n", ival, npoll));

	/*
	 * We now know which level in the tree the ED must go into.
	 * Figure out which slot has most bandwidth left over.
	 * Slots to examine:
	 * npoll
	 * 1	0
	 * 2	1 2
	 * 4	3 4 5 6
	 * 8	7 8 9 10 11 12 13 14
	 * N    (N-1) .. (N-1+N-1)
	 */
	slow = npoll-1;
	shigh = slow + npoll;
	nslots = OHCI_NO_INTRS / npoll;
	for (best = i = slow, bestbw = ~0; i < shigh; i++) {
		bw = 0;
		for (j = 0; j < nslots; j++)
			bw += sc->sc_bws[(i * nslots + j) % OHCI_NO_INTRS];
		if (bw < bestbw) {
			best = i;
			bestbw = bw;
		}
	}
	DPRINTFN(2, ("ohci_setintr: best=%d(%d..%d) bestbw=%d\n", 
		     best, slow, shigh, bestbw));

	s = splusb();
	hsed = sc->sc_eds[best];
	sed->next = hsed->next;
	sed->ed.ed_nexted = hsed->ed.ed_nexted;
	hsed->next = sed;
	hsed->ed.ed_nexted = LE(sed->physaddr);
	splx(s);

	for (j = 0; j < nslots; j++)
		++sc->sc_bws[(best * nslots + j) % OHCI_NO_INTRS];
	opipe->u.intr.nslots = nslots;
	opipe->u.intr.pos = best;

	DPRINTFN(5, ("ohci_setintr: returns %p\n", opipe));
	return (USBD_NORMAL_COMPLETION);
}
