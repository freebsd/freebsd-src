/*	$NetBSD: ohci.c,v 1.27 1999/01/13 10:33:53 augustss Exp $	*/
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
 * OHCI spec: http://www.intel.com/design/usb/ohci11d.pdf
 * USB spec: http://www.teleport.com/cgi-bin/mailmerge.cgi/~usb/cgiform.tpl
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

#ifdef __FreeBSD__
#include <machine/bus_pio.h>
#include <machine/bus_memio.h>
#endif
#include <machine/bus.h>
#include <machine/endian.h>

#include <dev/usb/usb.h>
#include <dev/usb/usbdi.h>
#include <dev/usb/usbdivar.h>
#include <dev/usb/usb_quirks.h>
#include <dev/usb/usb_mem.h>

#include <dev/usb/ohcireg.h>
#include <dev/usb/ohcivar.h>

#if defined(__FreeBSD__)
#include <machine/clock.h>

#define delay(d)                DELAY(d)
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

ohci_soft_ed_t *ohci_alloc_sed __P((ohci_softc_t *));
void		ohci_free_sed __P((ohci_softc_t *, ohci_soft_ed_t *));

ohci_soft_td_t *ohci_alloc_std __P((ohci_softc_t *));
void		ohci_free_std __P((ohci_softc_t *, ohci_soft_td_t *));

usbd_status	ohci_open __P((usbd_pipe_handle));
void		ohci_poll __P((struct usbd_bus *));
void		ohci_waitintr __P((ohci_softc_t *, usbd_request_handle));
void		ohci_rhsc __P((ohci_softc_t *, usbd_request_handle));
void		ohci_process_done __P((ohci_softc_t *, ohci_physaddr_t));
void		ohci_ii_done __P((ohci_softc_t *, usbd_request_handle));
void		ohci_ctrl_done __P((ohci_softc_t *, usbd_request_handle));
void		ohci_intr_done __P((ohci_softc_t *, usbd_request_handle));
void		ohci_bulk_done __P((ohci_softc_t *, usbd_request_handle));

usbd_status	ohci_device_request __P((usbd_request_handle reqh));
void		ohci_add_ed __P((ohci_soft_ed_t *, ohci_soft_ed_t *));
void		ohci_rem_ed __P((ohci_soft_ed_t *, ohci_soft_ed_t *));
void		ohci_hash_add_td __P((ohci_softc_t *, ohci_soft_td_t *));
void		ohci_hash_rem_td __P((ohci_softc_t *, ohci_soft_td_t *));
ohci_soft_td_t *ohci_hash_find_td __P((ohci_softc_t *, ohci_physaddr_t));

usbd_status	ohci_root_ctrl_transfer __P((usbd_request_handle));
usbd_status	ohci_root_ctrl_start __P((usbd_request_handle));
void		ohci_root_ctrl_abort __P((usbd_request_handle));
void		ohci_root_ctrl_close __P((usbd_pipe_handle));

usbd_status	ohci_root_intr_transfer __P((usbd_request_handle));
usbd_status	ohci_root_intr_start __P((usbd_request_handle));
void		ohci_root_intr_abort __P((usbd_request_handle));
void		ohci_root_intr_close __P((usbd_pipe_handle));

usbd_status	ohci_device_ctrl_transfer __P((usbd_request_handle));
usbd_status	ohci_device_ctrl_start __P((usbd_request_handle));
void		ohci_device_ctrl_abort __P((usbd_request_handle));
void		ohci_device_ctrl_close __P((usbd_pipe_handle));

usbd_status	ohci_device_bulk_transfer __P((usbd_request_handle));
usbd_status	ohci_device_bulk_start __P((usbd_request_handle));
void		ohci_device_bulk_abort __P((usbd_request_handle));
void		ohci_device_bulk_close __P((usbd_pipe_handle));

usbd_status	ohci_device_intr_transfer __P((usbd_request_handle));
usbd_status	ohci_device_intr_start __P((usbd_request_handle));
void		ohci_device_intr_abort __P((usbd_request_handle));
void		ohci_device_intr_close __P((usbd_pipe_handle));
usbd_status	ohci_device_setintr __P((ohci_softc_t *sc, 
					 struct ohci_pipe *pipe, int ival));

int		ohci_str __P((usb_string_descriptor_t *, int, char *));

void		ohci_timeout __P((void *));
void		ohci_rhsc_able __P((ohci_softc_t *, int));

#ifdef OHCI_DEBUG
ohci_softc_t   *thesc;
void		ohci_dumpregs __P((ohci_softc_t *));
void		ohci_dump_tds __P((ohci_soft_td_t *));
void		ohci_dump_td __P((ohci_soft_td_t *));
void		ohci_dump_ed __P((ohci_soft_ed_t *));
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
			usb_dma_t datadma;
			usb_dma_t reqdma;
			u_int length;
			ohci_soft_td_t *setup, *xfer, *stat;
		} ctl;
		/* Interrupt pipe */
		struct {
			usb_dma_t datadma;
			int nslots;
			int pos;
		} intr;
		/* Bulk pipe */
		struct {
			usb_dma_t datadma;
			u_int length;
		} bulk;
	} u;
};

#define OHCI_INTR_ENDPT 1

struct usbd_methods ohci_root_ctrl_methods = {	
	ohci_root_ctrl_transfer,
	ohci_root_ctrl_start,
	ohci_root_ctrl_abort,
	ohci_root_ctrl_close,
	0,
};

struct usbd_methods ohci_root_intr_methods = {	
	ohci_root_intr_transfer,
	ohci_root_intr_start,
	ohci_root_intr_abort,
	ohci_root_intr_close,
	0,
};

struct usbd_methods ohci_device_ctrl_methods = {	
	ohci_device_ctrl_transfer,
	ohci_device_ctrl_start,
	ohci_device_ctrl_abort,
	ohci_device_ctrl_close,
	0,
};

struct usbd_methods ohci_device_intr_methods = {	
	ohci_device_intr_transfer,
	ohci_device_intr_start,
	ohci_device_intr_abort,
	ohci_device_intr_close,
};

struct usbd_methods ohci_device_bulk_methods = {	
	ohci_device_bulk_transfer,
	ohci_device_bulk_start,
	ohci_device_bulk_abort,
	ohci_device_bulk_close,
	0,
};

ohci_soft_ed_t *
ohci_alloc_sed(sc)
	ohci_softc_t *sc;
{
	ohci_soft_ed_t *sed;
	usbd_status r;
	int i, offs;
	usb_dma_t dma;

	if (!sc->sc_freeeds) {
		DPRINTFN(2, ("ohci_alloc_sed: allocating chunk\n"));
		sed = malloc(sizeof(ohci_soft_ed_t) * OHCI_ED_CHUNK, 
			     M_USBDEV, M_NOWAIT);
		if (!sed)
			return 0;
		r = usb_allocmem(sc->sc_dmatag, OHCI_ED_SIZE * OHCI_ED_CHUNK,
				 OHCI_ED_ALIGN, &dma);
		if (r != USBD_NORMAL_COMPLETION) {
			free(sed, M_USBDEV);
			return 0;
		}
		for(i = 0; i < OHCI_ED_CHUNK; i++, sed++) {
			offs = i * OHCI_ED_SIZE;
			sed->physaddr = DMAADDR(&dma) + offs;
			sed->ed = (ohci_ed_t *)
					((char *)KERNADDR(&dma) + offs);
			sed->next = sc->sc_freeeds;
			sc->sc_freeeds = sed;
		}
	}
	sed = sc->sc_freeeds;
	sc->sc_freeeds = sed->next;
	memset(sed->ed, 0, OHCI_ED_SIZE);
	sed->next = 0;
	return sed;
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
	usbd_status r;
	int i, offs;
	usb_dma_t dma;

	if (!sc->sc_freetds) {
		DPRINTFN(2, ("ohci_alloc_std: allocating chunk\n"));
		std = malloc(sizeof(ohci_soft_td_t) * OHCI_TD_CHUNK, 
			     M_USBDEV, M_NOWAIT);
		if (!std)
			return 0;
		r = usb_allocmem(sc->sc_dmatag, OHCI_TD_SIZE * OHCI_TD_CHUNK,
				 OHCI_TD_ALIGN, &dma);
		if (r != USBD_NORMAL_COMPLETION) {
			free(std, M_USBDEV);
			return 0;
		}
		for(i = 0; i < OHCI_TD_CHUNK; i++, std++) {
			offs = i * OHCI_TD_SIZE;
			std->physaddr = DMAADDR(&dma) + offs;
			std->td = (ohci_td_t *)
					((char *)KERNADDR(&dma) + offs);
			std->nexttd = sc->sc_freetds;
			sc->sc_freetds = std;
		}
	}
	std = sc->sc_freetds;
	sc->sc_freetds = std->nexttd;
	memset(std->td, 0, OHCI_TD_SIZE);
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
ohci_init(sc)
	ohci_softc_t *sc;
{
	ohci_soft_ed_t *sed, *psed;
	usbd_status r;
	int rev;
	int i;
	u_int32_t s, ctl, ival, hcr, fm, per;

	DPRINTF(("ohci_init: start\n"));
	rev = OREAD4(sc, OHCI_REVISION);
	printf("%s: OHCI version %d.%d%s\n", USBDEVNAME(sc->sc_bus.bdev),
	       OHCI_REV_HI(rev), OHCI_REV_LO(rev),
	       OHCI_REV_LEGACY(rev) ? ", legacy support" : "");
	if (OHCI_REV_HI(rev) != 1 || OHCI_REV_LO(rev) != 0) {
		printf("%s: unsupported OHCI revision\n", 
		       USBDEVNAME(sc->sc_bus.bdev));
		return (USBD_INVAL);
	}

	for (i = 0; i < OHCI_HASH_SIZE; i++)
		LIST_INIT(&sc->sc_hash_tds[i]);

	/* Allocate the HCCA area. */
	r = usb_allocmem(sc->sc_dmatag, OHCI_HCCA_SIZE, 
			 OHCI_HCCA_ALIGN, &sc->sc_hccadma);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	sc->sc_hcca = (struct ohci_hcca *)KERNADDR(&sc->sc_hccadma);
	memset(sc->sc_hcca, 0, OHCI_HCCA_SIZE);

	sc->sc_eintrs = OHCI_NORMAL_INTRS;

	sc->sc_ctrl_head = ohci_alloc_sed(sc);
	if (!sc->sc_ctrl_head) {
		r = USBD_NOMEM;
		goto bad1;
	}
	sc->sc_ctrl_head->ed->ed_flags |= LE(OHCI_ED_SKIP);
	sc->sc_bulk_head = ohci_alloc_sed(sc);
	if (!sc->sc_bulk_head) {
		r = USBD_NOMEM;
		goto bad2;
	}
	sc->sc_bulk_head->ed->ed_flags |= LE(OHCI_ED_SKIP);

	/* Allocate all the dummy EDs that make up the interrupt tree. */
	for (i = 0; i < OHCI_NO_EDS; i++) {
		sed = ohci_alloc_sed(sc);
		if (!sed) {
			while (--i >= 0)
				ohci_free_sed(sc, sc->sc_eds[i]);
			r = USBD_NOMEM;
			goto bad3;
		}
		/* All ED fields are set to 0. */
		sc->sc_eds[i] = sed;
		sed->ed->ed_flags |= LE(OHCI_ED_SKIP);
		if (i != 0) {
			psed = sc->sc_eds[(i-1) / 2];
			sed->next = psed;
			sed->ed->ed_nexted = LE(psed->physaddr);
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
			delay(1000);
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
			delay(USB_RESUME_DELAY * 1000);
		}
	} else {
		DPRINTF(("ohci_init: cold started\n"));
	reset:
		/* Controller was cold started. */
		delay(USB_BUS_RESET_DELAY * 1000);
	}

	/*
	 * This reset should not be necessary according to the OHCI spec, but
	 * without it some controllers do not start.
	 */
	DPRINTF(("%s: resetting\n", USBDEVNAME(sc->sc_bus.bdev)));
	OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
	delay(USB_BUS_RESET_DELAY * 1000);

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
		r = USBD_IOERROR;
		goto bad3;
	}
#ifdef OHCI_DEBUG
	thesc = sc;
	if (ohcidebug > 15)
		ohci_dumpregs(sc);
#endif

	/* The controller is now in suspend state, we have 2ms to finish. */

	/* Set up HC registers. */
	OWRITE4(sc, OHCI_HCCA, DMAADDR(&sc->sc_hccadma));
	OWRITE4(sc, OHCI_CONTROL_HEAD_ED, sc->sc_ctrl_head->physaddr);
	OWRITE4(sc, OHCI_BULK_HEAD_ED, sc->sc_bulk_head->physaddr);
	OWRITE4(sc, OHCI_INTERRUPT_DISABLE, OHCI_ALL_INTRS);
	OWRITE4(sc, OHCI_INTERRUPT_ENABLE, sc->sc_eintrs | OHCI_MIE);
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
	sc->sc_bus.open_pipe = ohci_open;
	sc->sc_bus.pipe_size = sizeof(struct ohci_pipe);
	sc->sc_bus.do_poll = ohci_poll;

	return (USBD_NORMAL_COMPLETION);

 bad3:
	ohci_free_sed(sc, sc->sc_ctrl_head);
 bad2:
	ohci_free_sed(sc, sc->sc_bulk_head);
 bad1:
	usb_freemem(sc->sc_dmatag, &sc->sc_hccadma);
	return (r);
}

#ifdef OHCI_DEBUG
void ohcidump(void);
void ohcidump(void) { ohci_dumpregs(thesc); }

void
ohci_dumpregs(sc)
	ohci_softc_t *sc;
{
	printf("ohci_dumpregs: rev=0x%08x control=0x%08x command=0x%08x\n",
	       OREAD4(sc, OHCI_REVISION),
	       OREAD4(sc, OHCI_CONTROL),
	       OREAD4(sc, OHCI_COMMAND_STATUS));
	printf("               intrstat=0x%08x intre=0x%08x intrd=0x%08x\n",
	       OREAD4(sc, OHCI_INTERRUPT_STATUS),
	       OREAD4(sc, OHCI_INTERRUPT_ENABLE),
	       OREAD4(sc, OHCI_INTERRUPT_DISABLE));
	printf("               hcca=0x%08x percur=0x%08x ctrlhd=0x%08x\n",
	       OREAD4(sc, OHCI_HCCA),
	       OREAD4(sc, OHCI_PERIOD_CURRENT_ED),
	       OREAD4(sc, OHCI_CONTROL_HEAD_ED));
	printf("               ctrlcur=0x%08x bulkhd=0x%08x bulkcur=0x%08x\n",
	       OREAD4(sc, OHCI_CONTROL_CURRENT_ED),
	       OREAD4(sc, OHCI_BULK_HEAD_ED),
	       OREAD4(sc, OHCI_BULK_CURRENT_ED));
	printf("               done=0x%08x fmival=0x%08x fmrem=0x%08x\n",
	       OREAD4(sc, OHCI_DONE_HEAD),
	       OREAD4(sc, OHCI_FM_INTERVAL),
	       OREAD4(sc, OHCI_FM_REMAINING));
	printf("               fmnum=0x%08x perst=0x%08x lsthrs=0x%08x\n",
	       OREAD4(sc, OHCI_FM_NUMBER),
	       OREAD4(sc, OHCI_PERIODIC_START),
	       OREAD4(sc, OHCI_LS_THRESHOLD));
	printf("               desca=0x%08x descb=0x%08x stat=0x%08x\n",
	       OREAD4(sc, OHCI_RH_DESCRIPTOR_A),
	       OREAD4(sc, OHCI_RH_DESCRIPTOR_B),
	       OREAD4(sc, OHCI_RH_STATUS));
	printf("               port1=0x%08x port2=0x%08x\n",
	       OREAD4(sc, OHCI_RH_PORT_STATUS(1)),
	       OREAD4(sc, OHCI_RH_PORT_STATUS(2)));
	printf("         HCCA: frame_number=0x%04x done_head=0x%08x\n",
	       LE(sc->sc_hcca->hcca_frame_number),
	       LE(sc->sc_hcca->hcca_done_head));
}
#endif

int
ohci_intr(p)
	void *p;
{
	ohci_softc_t *sc = p;
	u_int32_t intrs, eintrs;
	ohci_physaddr_t done;

	/* In case the interrupt occurs before initialization has completed. */
	if (sc == NULL || sc->sc_hcca == NULL) {	/* NWH added sc==0 */
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

	sc->sc_intrs++;
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
		/* XXX process resume detect */
	}
	if (eintrs & OHCI_UE) {
		printf("%s: unrecoverable error, controller halted\n",
		       USBDEVNAME(sc->sc_bus.bdev));
		OWRITE4(sc, OHCI_CONTROL, OHCI_HCFS_RESET);
		/* XXX what else */
	}
	if (eintrs & OHCI_RHSC) {
		ohci_rhsc(sc, sc->sc_intrreqh);
		intrs &= ~OHCI_RHSC;

		/* 
		 * Disable RHSC interrupt for now, because it will be
		 * on until the port has been reset.
		 */
		ohci_rhsc_able(sc, 0);
	}

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
	ohci_soft_td_t *std, *sdone;
	usbd_request_handle reqh;
	int len, cc;

	DPRINTFN(10,("ohci_process_done: done=0x%08lx\n", (u_long)done));

	/* Reverse the done list. */
	for (sdone = 0; done; done = LE(std->td->td_nexttd)) {
		std = ohci_hash_find_td(sc, done);
		std->dnext = sdone;
		sdone = std;
	}

#ifdef OHCI_DEBUG
	if (ohcidebug > 11) {
		printf("ohci_process_done: TD done:\n");
		ohci_dump_tds(sdone);
	}
#endif

	for (std = sdone; std; std = std->dnext) {
		reqh = std->reqh;
		DPRINTFN(10, ("ohci_process_done: std=%p reqh=%p hcpriv=%p\n",
				std, reqh, reqh->hcpriv));
		cc = OHCI_TD_GET_CC(LE(std->td->td_flags));
		if (cc == OHCI_CC_NO_ERROR) {
			if (std->td->td_cbp == 0)
				len = std->len;
			else
				len = LE(std->td->td_be) - 
				      LE(std->td->td_cbp) + 1;
			/* 
			 * Only do a callback on the last stage of a transfer.
			 * Others have hcpriv = 0.
			 */
			if ((reqh->pipe->endpoint->edesc->bmAttributes & 
			     UE_XFERTYPE) == UE_CONTROL) {
				/* For a control transfer the length is in
				 * the xfer stage */
				if (reqh->hcpriv == std) {
					reqh->status = USBD_NORMAL_COMPLETION;
					ohci_ii_done(sc, reqh);
				} else
					reqh->actlen = len;
			} else {
				if (reqh->hcpriv == std) {
					reqh->actlen = len;
					reqh->status = USBD_NORMAL_COMPLETION;
					ohci_ii_done(sc, reqh);
				}
			}
		} else {
			ohci_soft_td_t *p, *n;
			struct ohci_pipe *opipe = 
				(struct ohci_pipe *)reqh->pipe;
			DPRINTFN(-1,("ohci_process_done: error cc=%d (%s)\n",
			 OHCI_TD_GET_CC(LE(std->td->td_flags)),
			 ohci_cc_strs[OHCI_TD_GET_CC(LE(std->td->td_flags))]));
			/*
			 * Endpoint is halted.  First unlink all the TDs
			 * belonging to the failed transfer, and then restart
			 * the endpoint.
			 */
			for (p = std->nexttd; p->reqh == reqh; p = n) {
				n = p->nexttd;
				ohci_hash_rem_td(sc, p);
				ohci_free_std(sc, p);
			}
			/* clear halt */
			opipe->sed->ed->ed_headp = LE(p->physaddr);
			OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_CLF);
			
			if (cc == OHCI_CC_STALL)
				reqh->status = USBD_STALLED;
			else
				reqh->status = USBD_IOERROR;
			ohci_ii_done(sc, reqh);
		}
		ohci_hash_rem_td(sc, std);
		ohci_free_std(sc, std);
	}
}

void
ohci_ii_done(sc, reqh)
	ohci_softc_t *sc;
	usbd_request_handle reqh;
{
	switch (reqh->pipe->endpoint->edesc->bmAttributes & UE_XFERTYPE) {
	case UE_CONTROL:
		ohci_ctrl_done(sc, reqh);
		usb_start_next(reqh->pipe);
		break;
	case UE_INTERRUPT:
		ohci_intr_done(sc, reqh);
		break;
	case UE_BULK:
		ohci_bulk_done(sc, reqh);
		usb_start_next(reqh->pipe);
		break;
	case UE_ISOCHRONOUS:
		printf("ohci_process_done: ISO done?\n");
		usb_start_next(reqh->pipe);
		break;
	}

	/* And finally execute callback. */
	reqh->xfercb(reqh);
}

void
ohci_ctrl_done(sc, reqh)
	ohci_softc_t *sc;
	usbd_request_handle reqh;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)reqh->pipe;
	u_int len = opipe->u.ctl.length;
	usb_dma_t *dma;

	DPRINTFN(10,("ohci_ctrl_done: reqh=%p\n", reqh));

	if (!reqh->isreq) {
		panic("ohci_ctrl_done: not a request\n");
		return;
	}

	if (len != 0) {
		dma = &opipe->u.ctl.datadma;
		if (reqh->request.bmRequestType & UT_READ)
			memcpy(reqh->buffer, KERNADDR(dma), len);
		usb_freemem(sc->sc_dmatag, dma);
	}
	usb_untimeout(ohci_timeout, reqh, reqh->timeout_handle);
}

void
ohci_intr_done(sc, reqh)
	ohci_softc_t *sc;
	usbd_request_handle reqh;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)reqh->pipe;
	usb_dma_t *dma;
	ohci_soft_ed_t *sed = opipe->sed;
	ohci_soft_td_t *xfer, *tail;


	DPRINTFN(10,("ohci_intr_done: reqh=%p, actlen=%d\n", 
		     reqh, reqh->actlen));

	dma = &opipe->u.intr.datadma;
	memcpy(reqh->buffer, KERNADDR(dma), reqh->actlen);

	if (reqh->pipe->intrreqh == reqh) {
		xfer = opipe->tail;
		tail = ohci_alloc_std(sc); /* XXX should reuse TD */
		if (!tail) {
			reqh->status = USBD_NOMEM;
			return;
		}
		tail->reqh = 0;
		
		xfer->td->td_flags = LE(
			OHCI_TD_IN | OHCI_TD_NOCC | 
			OHCI_TD_SET_DI(1) | OHCI_TD_TOGGLE_CARRY);
		if (reqh->flags & USBD_SHORT_XFER_OK)
			xfer->td->td_flags |= LE(OHCI_TD_R);
		xfer->td->td_cbp = LE(DMAADDR(dma));
		xfer->nexttd = tail;
		xfer->td->td_nexttd = LE(tail->physaddr);
		xfer->td->td_be = LE(LE(xfer->td->td_cbp) + reqh->length - 1);
		xfer->len = reqh->length;
		xfer->reqh = reqh;

		reqh->hcpriv = xfer;

		ohci_hash_add_td(sc, xfer);
		sed->ed->ed_tailp = LE(tail->physaddr);
		opipe->tail = tail;
	} else {
		usb_freemem(sc->sc_dmatag, dma);
		usb_start_next(reqh->pipe);
	}
}

void
ohci_bulk_done(sc, reqh)
	ohci_softc_t *sc;
	usbd_request_handle reqh;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)reqh->pipe;
	usb_dma_t *dma;


	DPRINTFN(10,("ohci_bulk_done: reqh=%p, actlen=%d\n", 
		     reqh, reqh->actlen));

	dma = &opipe->u.bulk.datadma;
	if (reqh->request.bmRequestType & UT_READ)
		memcpy(reqh->buffer, KERNADDR(dma), reqh->actlen);
	usb_freemem(sc->sc_dmatag, dma);
	usb_untimeout(ohci_timeout, reqh, reqh->timeout_handle);
}

void
ohci_rhsc(sc, reqh)
	ohci_softc_t *sc;
	usbd_request_handle reqh;
{
	usbd_pipe_handle pipe;
	struct ohci_pipe *opipe;
	u_char *p;
	int i, m;
	int hstatus;

	hstatus = OREAD4(sc, OHCI_RH_STATUS);
	DPRINTF(("ohci_rhsc: sc=%p reqh=%p hstatus=0x%08x\n", 
		 sc, reqh, hstatus));

	if (reqh == 0) {
		/* Just ignore the change. */
		return;
	}

	pipe = reqh->pipe;
	opipe = (struct ohci_pipe *)pipe;

	p = KERNADDR(&opipe->u.intr.datadma);
	m = min(sc->sc_noport, reqh->length * 8 - 1);
	memset(p, 0, reqh->length);
	for (i = 1; i <= m; i++) {
		if (OREAD4(sc, OHCI_RH_PORT_STATUS(i)) >> 16)
			p[i/8] |= 1 << (i%8);
	}
	DPRINTF(("ohci_rhsc: change=0x%02x\n", *p));
	reqh->actlen = reqh->length;
	reqh->status = USBD_NORMAL_COMPLETION;
	reqh->xfercb(reqh);

	if (reqh->pipe->intrreqh != reqh) {
		sc->sc_intrreqh = 0;
		usb_freemem(sc->sc_dmatag, &opipe->u.intr.datadma);
		usb_start_next(reqh->pipe);
	}
}

/*
 * Wait here until controller claims to have an interrupt.
 * Then call ohci_intr and return.  Use timeout to avoid waiting
 * too long.
 */
void
ohci_waitintr(sc, reqh)
	ohci_softc_t *sc;
	usbd_request_handle reqh;
{
	int timo = reqh->timeout;
	int usecs;
	u_int32_t intrs;

	reqh->status = USBD_IN_PROGRESS;
	for (usecs = timo * 1000000 / hz; usecs > 0; usecs -= 1000) {
		usb_delay_ms(&sc->sc_bus, 1);
		intrs = OREAD4(sc, OHCI_INTERRUPT_STATUS) & sc->sc_eintrs;
		DPRINTFN(15,("ohci_waitintr: 0x%04x\n", intrs));
#ifdef OHCI_DEBUG
		if (ohcidebug > 15)
			ohci_dumpregs(sc);
#endif
		if (intrs) {
			ohci_intr(sc);
			if (reqh->status != USBD_IN_PROGRESS)
				return;
		}
	}

	/* Timeout */
	DPRINTF(("ohci_waitintr: timeout\n"));
	reqh->status = USBD_TIMEOUT;
	ohci_ii_done(sc, reqh);
	/* XXX should free TD */
}

void
ohci_poll(bus)
	struct usbd_bus *bus;
{
	ohci_softc_t *sc = (ohci_softc_t *)bus;

	if (OREAD4(sc, OHCI_INTERRUPT_STATUS) & sc->sc_eintrs)
		ohci_intr(sc);
}

usbd_status
ohci_device_request(reqh)
	usbd_request_handle reqh;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)reqh->pipe;
	usb_device_request_t *req = &reqh->request;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	int addr = dev->address;
	ohci_soft_td_t *setup, *xfer = 0, *stat, *next, *tail;
	ohci_soft_ed_t *sed;
	usb_dma_t *dmap;
	int isread;
	int len;
	usbd_status r;
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
	if (!stat) {
		r = USBD_NOMEM;
		goto bad1;
	}
	tail = ohci_alloc_std(sc);
	if (!tail) {
		r = USBD_NOMEM;
		goto bad2;
	}
	tail->reqh = 0;

	sed = opipe->sed;
	dmap = &opipe->u.ctl.datadma;
	opipe->u.ctl.length = len;

	/* Update device address and length since they may have changed. */
	/* XXX This only needs to be done once, but it's too early in open. */
	sed->ed->ed_flags = LE(
	 (LE(sed->ed->ed_flags) & ~(OHCI_ED_ADDRMASK | OHCI_ED_MAXPMASK)) |
	 OHCI_ED_SET_FA(addr) |
	 OHCI_ED_SET_MAXP(UGETW(opipe->pipe.endpoint->edesc->wMaxPacketSize)));

	/* Set up data transaction */
	if (len != 0) {
		xfer = ohci_alloc_std(sc);
		if (!xfer) {
			r = USBD_NOMEM;
			goto bad3;
		}
		r = usb_allocmem(sc->sc_dmatag, len, 0, dmap);
		if (r != USBD_NORMAL_COMPLETION)
			goto bad4;
		xfer->td->td_flags = LE(
			(isread ? OHCI_TD_IN : OHCI_TD_OUT) | OHCI_TD_NOCC |
			OHCI_TD_TOGGLE_1 | OHCI_TD_NOINTR |
			(reqh->flags & USBD_SHORT_XFER_OK ? OHCI_TD_R : 0));
		xfer->td->td_cbp = LE(DMAADDR(dmap));
		xfer->nexttd = stat;
		xfer->td->td_nexttd = LE(stat->physaddr);
		xfer->td->td_be = LE(LE(xfer->td->td_cbp) + len - 1);
		xfer->len = len;
		xfer->reqh = reqh;

		next = xfer;
	} else
		next = stat;

	memcpy(KERNADDR(&opipe->u.ctl.reqdma), req, sizeof *req);
	if (!isread && len != 0)
		memcpy(KERNADDR(dmap), reqh->buffer, len);

	setup->td->td_flags = LE(OHCI_TD_SETUP | OHCI_TD_NOCC |
				 OHCI_TD_TOGGLE_0 | OHCI_TD_NOINTR);
	setup->td->td_cbp = LE(DMAADDR(&opipe->u.ctl.reqdma));
	setup->nexttd = next;
	setup->td->td_nexttd = LE(next->physaddr);
	setup->td->td_be = LE(LE(setup->td->td_cbp) + sizeof *req - 1);
	setup->len = 0;		/* XXX The number of byte we count */
	setup->reqh = reqh;

	stat->td->td_flags = LE(
		(isread ? OHCI_TD_OUT : OHCI_TD_IN) | OHCI_TD_NOCC |
		OHCI_TD_TOGGLE_1 | OHCI_TD_SET_DI(1));
	stat->td->td_cbp = 0;
	stat->nexttd = tail;
	stat->td->td_nexttd = LE(tail->physaddr);
	stat->td->td_be = 0;
	stat->len = 0;
	stat->reqh = reqh;

	reqh->hcpriv = stat;

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		printf("ohci_device_request:\n");
		ohci_dump_ed(sed);
		ohci_dump_tds(setup);
	}
#endif

	/* Insert ED in schedule */
	s = splusb();
	ohci_hash_add_td(sc, setup);
	if (len != 0)
		ohci_hash_add_td(sc, xfer);
	ohci_hash_add_td(sc, stat);
	sed->ed->ed_tailp = LE(tail->physaddr);
	opipe->tail = tail;
	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_CLF);
	if (reqh->timeout && !sc->sc_bus.use_polling) {
                usb_timeout(ohci_timeout, reqh,
			    MS_TO_TICKS(reqh->timeout), reqh->timeout_handle);
	}
	splx(s);

#if 0
#ifdef OHCI_DEBUG
	if (ohcidebug > 15) {
		delay(5000);
		printf("ohci_device_request: status=%x\n",
		       OREAD4(sc, OHCI_COMMAND_STATUS));
		ohci_dump_ed(sed);
		ohci_dump_tds(setup);
	}
#endif
#endif

	return (USBD_NORMAL_COMPLETION);

 bad4:
	ohci_free_std(sc, xfer);
 bad3:
	ohci_free_std(sc, tail);
 bad2:
	ohci_free_std(sc, stat);
 bad1:
	return (r);
}

/*
 * Add an ED to the schedule.  Called at splusb().
 */
void
ohci_add_ed(sed, head)
	ohci_soft_ed_t *sed; 
	ohci_soft_ed_t *head; 
{
	sed->next = head->next;
	sed->ed->ed_nexted = head->ed->ed_nexted;
	head->next = sed;
	head->ed->ed_nexted = LE(sed->physaddr);
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

	/* XXX */
	for (p = head; p && p->next != sed; p = p->next)
		;
	if (!p)
		panic("ohci_rem_ed: ED not found\n");
	p->next = sed->next;
	p->ed->ed_nexted = sed->ed->ed_nexted;
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

	LIST_INSERT_HEAD(&sc->sc_hash_tds[h], std, hnext);
}

/* Called at splusb() */
void
ohci_hash_rem_td(sc, std)
	ohci_softc_t *sc;
	ohci_soft_td_t *std;
{
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
	     std != 0; 
	     std = LIST_NEXT(std, hnext))
		if (std->physaddr == a)
			return (std);
	panic("ohci_hash_find_td: addr 0x%08lx not found\n", (u_long)a);
}

void
ohci_timeout(addr)
	void *addr;
{
#ifdef OHCI_DEBUG
	usbd_request_handle *reqh = addr;
#endif

	DPRINTF(("ohci_timeout: reqh=%p\n", reqh));
#if 0
	int s;

	s = splusb();
	/* XXX need to inactivate TD before calling interrupt routine */
	ohci_XXX_done(reqh);
	splx(s);
#endif
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
	printf("TD(%p) at %08lx: %b delay=%d ec=%d cc=%d\ncbp=0x%08lx "
	       "nexttd=0x%08lx be=0x%08lx\n", 
	       std, (u_long)std->physaddr,
	       (int)LE(std->td->td_flags),
	       "\20\23R\24OUT\25IN\31TOG1\32SETTOGGLE",
	       OHCI_TD_GET_DI(LE(std->td->td_flags)),
	       OHCI_TD_GET_EC(LE(std->td->td_flags)),
	       OHCI_TD_GET_CC(LE(std->td->td_flags)),
	       (u_long)LE(std->td->td_cbp),
	       (u_long)LE(std->td->td_nexttd), (u_long)LE(std->td->td_be));
}

void
ohci_dump_ed(sed)
	ohci_soft_ed_t *sed;
{
	printf("ED(%p) at %08lx: addr=%d endpt=%d maxp=%d %b\ntailp=0x%08lx "
	       "headp=%b nexted=0x%08lx\n",
	       sed, (u_long)sed->physaddr, 
	       OHCI_ED_GET_FA(LE(sed->ed->ed_flags)),
	       OHCI_ED_GET_EN(LE(sed->ed->ed_flags)),
	       OHCI_ED_GET_MAXP(LE(sed->ed->ed_flags)),
	       (int)LE(sed->ed->ed_flags),
	       "\20\14OUT\15IN\16LOWSPEED\17SKIP\20ISO",
	       (u_long)LE(sed->ed->ed_tailp),
	       (int)LE(sed->ed->ed_headp), "\20\1HALT\2CARRY",
	       (u_long)LE(sed->ed->ed_nexted));
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
	usbd_status r;
	int s;

	DPRINTFN(1, ("ohci_open: pipe=%p, addr=%d, endpt=%d (%d)\n",
		     pipe, addr, ed->bEndpointAddress, sc->sc_addr));
	if (addr == sc->sc_addr) {
		switch (ed->bEndpointAddress) {
		case USB_CONTROL_ENDPOINT:
			pipe->methods = &ohci_root_ctrl_methods;
			break;
		case UE_IN | OHCI_INTR_ENDPT:
			pipe->methods = &ohci_root_intr_methods;
			break;
		default:
			return (USBD_INVAL);
		}
	} else {
		sed = ohci_alloc_sed(sc);
		if (sed == 0)
			goto bad0;
	        std = ohci_alloc_std(sc);
		if (std == 0)
			goto bad1;
		opipe->sed = sed;
		opipe->tail = std;
		sed->ed->ed_flags = LE(
			OHCI_ED_SET_FA(addr) | 
			OHCI_ED_SET_EN(ed->bEndpointAddress) |
			OHCI_ED_DIR_TD | 
			(dev->lowspeed ? OHCI_ED_SPEED : 0) | 
			((ed->bmAttributes & UE_XFERTYPE) == UE_ISOCHRONOUS ?
			 OHCI_ED_FORMAT_ISO : OHCI_ED_FORMAT_GEN) |
			OHCI_ED_SET_MAXP(UGETW(ed->wMaxPacketSize)));
		sed->ed->ed_headp = sed->ed->ed_tailp = LE(std->physaddr);

		switch (ed->bmAttributes & UE_XFERTYPE) {
		case UE_CONTROL:
			pipe->methods = &ohci_device_ctrl_methods;
			r = usb_allocmem(sc->sc_dmatag, 
					 sizeof(usb_device_request_t), 
					 0, &opipe->u.ctl.reqdma);
			if (r != USBD_NORMAL_COMPLETION)
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
			return (USBD_XXX);
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
 * Data structures and routines to emulate the root hub.
 */
usb_device_descriptor_t ohci_devd = {
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

usb_config_descriptor_t ohci_confd = {
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

usb_interface_descriptor_t ohci_ifcd = {
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

usb_endpoint_descriptor_t ohci_endpd = {
	USB_ENDPOINT_DESCRIPTOR_SIZE,
	UDESC_ENDPOINT,
	UE_IN | OHCI_INTR_ENDPT,
	UE_INTERRUPT,
	{8, 0},			/* max packet */
	255
};

usb_hub_descriptor_t ohci_hubd = {
	USB_HUB_DESCRIPTOR_SIZE,
	UDESC_HUB,
	0,
	{0,0},
	0,
	0,
	{0},
};

int
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
usbd_status
ohci_root_ctrl_transfer(reqh)
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
		return (ohci_root_ctrl_start(reqh));
}

usbd_status
ohci_root_ctrl_start(reqh)
	usbd_request_handle reqh;
{
	ohci_softc_t *sc = (ohci_softc_t *)reqh->pipe->device->bus;
	usb_device_request_t *req;
	void *buf;
	int port, i;
	int len, value, index, l, totlen = 0;
	usb_port_status_t ps;
	usb_hub_descriptor_t hubd;
	usbd_status r;
	u_int32_t v;

	if (!reqh->isreq)
		/* XXX panic */
		return (USBD_INVAL);
	req = &reqh->request;
	buf = reqh->buffer;

	DPRINTFN(4,("ohci_root_ctrl_control type=0x%02x request=%02x\n", 
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
		DPRINTFN(8,("ohci_root_ctrl_control wValue=0x%04x\n", value));
		switch(value >> 8) {
		case UDESC_DEVICE:
			if ((value & 0xff) != 0) {
				r = USBD_IOERROR;
				goto ret;
			}
			totlen = l = min(len, USB_DEVICE_DESCRIPTOR_SIZE);
			memcpy(buf, &ohci_devd, l);
			break;
		case UDESC_CONFIG:
			if ((value & 0xff) != 0) {
				r = USBD_IOERROR;
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
		DPRINTFN(8, ("ohci_root_ctrl_control: UR_CLEAR_PORT_FEATURE "
			     "port=%d feature=%d\n",
			     index, value));
		if (index < 1 || index > sc->sc_noport) {
			r = USBD_IOERROR;
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
			r = USBD_IOERROR;
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
			r = USBD_IOERROR;
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
			r = USBD_IOERROR;
			goto ret;
		}
		memset(buf, 0, len); /* ? XXX */
		totlen = len;
		break;
	case C(UR_GET_STATUS, UT_READ_CLASS_OTHER):
		DPRINTFN(8,("ohci_root_ctrl_transfer: get port status i=%d\n",
			    index));
		if (index < 1 || index > sc->sc_noport) {
			r = USBD_IOERROR;
			goto ret;
		}
		if (len != 4) {
			r = USBD_IOERROR;
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
		r = USBD_IOERROR;
		goto ret;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_DEVICE):
		break;
	case C(UR_SET_FEATURE, UT_WRITE_CLASS_OTHER):
		if (index < 1 || index > sc->sc_noport) {
			r = USBD_IOERROR;
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
ohci_root_ctrl_abort(reqh)
	usbd_request_handle reqh;
{
	/* Nothing to do, all transfers are synchronous. */
}

/* Close the root pipe. */
void
ohci_root_ctrl_close(pipe)
	usbd_pipe_handle pipe;
{
	DPRINTF(("ohci_root_ctrl_close\n"));
}

usbd_status
ohci_root_intr_transfer(reqh)
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
		return (ohci_root_intr_start(reqh));
}

usbd_status
ohci_root_intr_start(reqh)
	usbd_request_handle reqh;
{
	usbd_pipe_handle pipe = reqh->pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	struct ohci_pipe *upipe = (struct ohci_pipe *)pipe;
	usb_dma_t *dmap;
	usbd_status r;
	int len;

	len = reqh->length;
	dmap = &upipe->u.intr.datadma;
	if (len == 0)
		return (USBD_INVAL); /* XXX should it be? */

	r = usb_allocmem(sc->sc_dmatag, len, 0, dmap);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);
	sc->sc_intrreqh = reqh;

	return (USBD_IN_PROGRESS);
}

/* Abort a root interrupt request. */
void
ohci_root_intr_abort(reqh)
	usbd_request_handle reqh;
{
	/* No need to abort. */
}

/* Close the root pipe. */
void
ohci_root_intr_close(pipe)
	usbd_pipe_handle pipe;
{
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	sc->sc_intrreqh = 0;
	
	DPRINTF(("ohci_root_intr_close\n"));
}

/************************/

usbd_status
ohci_device_ctrl_transfer(reqh)
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
		return (ohci_device_ctrl_start(reqh));
}

usbd_status
ohci_device_ctrl_start(reqh)
	usbd_request_handle reqh;
{
	ohci_softc_t *sc = (ohci_softc_t *)reqh->pipe->device->bus;
	usbd_status r;

	if (!reqh->isreq) {
		/* XXX panic */
		printf("ohci_device_ctrl_transfer: not a request\n");
		return (USBD_INVAL);
	}

	r = ohci_device_request(reqh);
	if (r != USBD_NORMAL_COMPLETION)
		return (r);

	if (sc->sc_bus.use_polling)
		ohci_waitintr(sc, reqh);
	return (USBD_IN_PROGRESS);
}

/* Abort a device control request. */
void
ohci_device_ctrl_abort(reqh)
	usbd_request_handle reqh;
{
	/* XXX inactivate */
	usb_delay_ms(reqh->pipe->device->bus, 1); /* make sure it is donw */
	/* XXX call done */
}

/* Close a device control pipe. */
void
ohci_device_ctrl_close(pipe)
	usbd_pipe_handle pipe;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	ohci_softc_t *sc = (ohci_softc_t *)pipe->device->bus;
	ohci_soft_ed_t *sed = opipe->sed;
	int s;

	s = splusb();
	sed->ed->ed_flags |= LE(OHCI_ED_SKIP);
	if ((LE(sed->ed->ed_tailp) & OHCI_TAILMASK) != LE(sed->ed->ed_headp))
		usb_delay_ms(&sc->sc_bus, 2);
	ohci_rem_ed(sed, sc->sc_ctrl_head);
	splx(s);
	ohci_free_std(sc, opipe->tail);
	ohci_free_sed(sc, opipe->sed);
	/* XXX free other resources */
}

/************************/

usbd_status
ohci_device_bulk_transfer(reqh)
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
		return (ohci_device_bulk_start(reqh));
}

usbd_status
ohci_device_bulk_start(reqh)
	usbd_request_handle reqh;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)reqh->pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	int addr = dev->address;
	ohci_soft_td_t *xfer, *tail;
	ohci_soft_ed_t *sed;
	usb_dma_t *dmap;
	usbd_status r;
	int s, len, isread;

	if (reqh->isreq) {
		/* XXX panic */
		printf("ohci_device_bulk_transfer: a request\n");
		return (USBD_INVAL);
	}

	len = reqh->length;
	dmap = &opipe->u.bulk.datadma;
	isread = reqh->pipe->endpoint->edesc->bEndpointAddress & UE_IN;
	sed = opipe->sed;

	opipe->u.bulk.length = len;

	r = usb_allocmem(sc->sc_dmatag, len, 0, dmap);
	if (r != USBD_NORMAL_COMPLETION)
		goto ret1;

	tail = ohci_alloc_std(sc);
	if (!tail) {
		r = USBD_NOMEM;
		goto ret2;
	}
	tail->reqh = 0;

	/* Update device address */
	sed->ed->ed_flags = LE(
		(LE(sed->ed->ed_flags) & ~OHCI_ED_ADDRMASK) |
		OHCI_ED_SET_FA(addr));

	/* Set up data transaction */
	xfer = opipe->tail;
	xfer->td->td_flags = LE(
		(isread ? OHCI_TD_IN : OHCI_TD_OUT) | OHCI_TD_NOCC |
		OHCI_TD_SET_DI(1) | OHCI_TD_TOGGLE_CARRY |
		(reqh->flags & USBD_SHORT_XFER_OK ? OHCI_TD_R : 0));
	xfer->td->td_cbp = LE(DMAADDR(dmap));
	xfer->nexttd = tail;
	xfer->td->td_nexttd = LE(tail->physaddr);
	xfer->td->td_be = LE(LE(xfer->td->td_cbp) + len - 1);
	xfer->len = len;
	xfer->reqh = reqh;

	reqh->hcpriv = xfer;

	if (!isread)
		memcpy(KERNADDR(dmap), reqh->buffer, len);

	/* Insert ED in schedule */
	s = splusb();
	ohci_hash_add_td(sc, xfer);
	sed->ed->ed_tailp = LE(tail->physaddr);
	opipe->tail = tail;
	OWRITE4(sc, OHCI_COMMAND_STATUS, OHCI_BLF);
	if (reqh->timeout && !sc->sc_bus.use_polling) {
                usb_timeout(ohci_timeout, reqh,
			    MS_TO_TICKS(reqh->timeout), reqh->timeout_handle);
	}
	splx(s);

	return (USBD_IN_PROGRESS);

 ret2:
	usb_freemem(sc->sc_dmatag, dmap);
 ret1:
	return (r);
}

/* Abort a device bulk request. */
void
ohci_device_bulk_abort(reqh)
	usbd_request_handle reqh;
{
#if 0
	sed->ed->ed_flags |= LE(OHCI_ED_SKIP);
	if ((LE(sed->ed->ed_tailp) & OHCI_TAILMASK) != LE(sed->ed->ed_headp))
		usb_delay_ms(reqh->pipe->device->bus, 2);
#endif
	/* XXX inactivate */
	usb_delay_ms(reqh->pipe->device->bus, 1); /* make sure it is done */
	/* XXX call done */
}

/* Close a device bulk pipe. */
void
ohci_device_bulk_close(pipe)
	usbd_pipe_handle pipe;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	int s;

	s = splusb();
	ohci_rem_ed(opipe->sed, sc->sc_bulk_head);
	splx(s);
	ohci_free_std(sc, opipe->tail);
	ohci_free_sed(sc, opipe->sed);
	/* XXX free other resources */
}

/************************/

usbd_status
ohci_device_intr_transfer(reqh)
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
		return (ohci_device_intr_start(reqh));
}

usbd_status
ohci_device_intr_start(reqh)
	usbd_request_handle reqh;
{
	struct ohci_pipe *opipe = (struct ohci_pipe *)reqh->pipe;
	usbd_device_handle dev = opipe->pipe.device;
	ohci_softc_t *sc = (ohci_softc_t *)dev->bus;
	ohci_soft_ed_t *sed = opipe->sed;
	ohci_soft_td_t *xfer, *tail;
	usb_dma_t *dmap;
	usbd_status r;
	int len;
	int s;

	DPRINTFN(3, ("ohci_device_intr_transfer: reqh=%p buf=%p len=%d "
		     "flags=%d priv=%p\n",
		 reqh, reqh->buffer, reqh->length, reqh->flags, reqh->priv));

	if (reqh->isreq)
		panic("ohci_device_intr_transfer: a request\n");

	len = reqh->length;
	dmap = &opipe->u.intr.datadma;
	if (len == 0)
		return (USBD_INVAL); /* XXX should it be? */

	xfer = opipe->tail;
	tail = ohci_alloc_std(sc);
	if (!tail) {
		r = USBD_NOMEM;
		goto ret1;
	}
	tail->reqh = 0;

	r = usb_allocmem(sc->sc_dmatag, len, 0, dmap);
	if (r != USBD_NORMAL_COMPLETION)
		goto ret2;

	xfer->td->td_flags = LE(
		OHCI_TD_IN | OHCI_TD_NOCC | 
		OHCI_TD_SET_DI(1) | OHCI_TD_TOGGLE_CARRY);
	if (reqh->flags & USBD_SHORT_XFER_OK)
		xfer->td->td_flags |= LE(OHCI_TD_R);
	xfer->td->td_cbp = LE(DMAADDR(dmap));
	xfer->nexttd = tail;
	xfer->td->td_nexttd = LE(tail->physaddr);
	xfer->td->td_be = LE(LE(xfer->td->td_cbp) + len - 1);
	xfer->len = len;
	xfer->reqh = reqh;

	reqh->hcpriv = xfer;

#ifdef OHCI_DEBUG
	if (ohcidebug > 5) {
		printf("ohci_device_intr_transfer:\n");
		ohci_dump_ed(sed);
		ohci_dump_tds(xfer);
	}
#endif

	/* Insert ED in schedule */
	s = splusb();
	ohci_hash_add_td(sc, xfer);
	sed->ed->ed_tailp = LE(tail->physaddr);
	opipe->tail = tail;
#if 0
	if (reqh->timeout && !sc->sc_bus.use_polling) {
                usb_timeout(ohci_timeout, reqh,
			    MS_TO_TICKS(reqh->timeout), reqh->timeout_handle);
	}
#endif
	sed->ed->ed_flags &= LE(~OHCI_ED_SKIP);

#if 0
#ifdef OHCI_DEBUG
	if (ohcidebug > 15) {
		delay(5000);
		printf("ohci_device_intr_transfer: status=%x\n",
		       OREAD4(sc, OHCI_COMMAND_STATUS));
		ohci_dump_ed(sed);
		ohci_dump_tds(xfer);
	}
#endif
#endif
	splx(s);

	return (USBD_IN_PROGRESS);

 ret2:
	ohci_free_std(sc, xfer);
 ret1:
	return (r);
}

/* Abort a device control request. */
void
ohci_device_intr_abort(reqh)
	usbd_request_handle reqh;
{
	/* XXX inactivate */
	usb_delay_ms(reqh->pipe->device->bus, 1); /* make sure it is done */
	if (reqh->pipe->intrreqh == reqh) {
		DPRINTF(("ohci_device_intr_abort: remove\n"));
		reqh->pipe->intrreqh = 0;
		ohci_intr_done((ohci_softc_t *)reqh->pipe->device->bus, reqh);
	}
}

/* Close a device interrupt pipe. */
void
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
	sed->ed->ed_flags |= LE(OHCI_ED_SKIP);
	if ((sed->ed->ed_tailp & LE(OHCI_TAILMASK)) != sed->ed->ed_headp)
		usb_delay_ms(&sc->sc_bus, 2);

	for (p = sc->sc_eds[pos]; p && p->next != sed; p = p->next)
		;
	if (!p)
		panic("ohci_device_intr_close: ED not found\n");
	p->next = sed->next;
	p->ed->ed_nexted = sed->ed->ed_nexted;
	splx(s);

	for (j = 0; j < nslots; j++)
		--sc->sc_bws[pos * nslots + j];

	ohci_free_std(sc, opipe->tail);
	ohci_free_sed(sc, opipe->sed);
	/* XXX free other resources */
}

usbd_status
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
			bw += sc->sc_bws[i * nslots + j];
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
	sed->ed->ed_nexted = hsed->ed->ed_nexted;
	hsed->next = sed;
	hsed->ed->ed_nexted = LE(sed->physaddr);
	splx(s);

	for (j = 0; j < nslots; j++)
		++sc->sc_bws[best * nslots + j];
	opipe->u.intr.nslots = nslots;
	opipe->u.intr.pos = best;

	DPRINTFN(5, ("ohci_setintr: returns %p\n", opipe));
	return (USBD_NORMAL_COMPLETION);
}

