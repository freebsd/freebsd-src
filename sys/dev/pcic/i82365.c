/*	$NetBSD: i82365.c,v 1.25 1999/10/15 06:07:27 haya Exp $	*/
/* $FreeBSD$ */

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <machine/clock.h>

#include <sys/wait.h>
#include <sys/unistd.h>
#include <sys/kthread.h>

/* We shouldn't need to include the following, but sadly we do for now */
/* XXX */
#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include <dev/pcic/i82365reg.h>
#include <dev/pcic/i82365var.h>

#include "card_if.h"

#define PCICDEBUG

#ifdef PCICDEBUG
int	pcic_debug = 1;
#define	DPRINTF(arg) if (pcic_debug) printf arg; else ;
#define DEVPRINTF(arg) if (pcic_debug) device_printf arg; else ;
#else
#define	DPRINTF(arg)
#define	DEVPRINTF(arg)
#endif

#define VERBOSE(arg) if (bootverbose) printf arg; else ;

#define DETACH_FORCE	0x1

#define	PCIC_VENDOR_UNKNOWN		0
#define	PCIC_VENDOR_I82365SLR0		1
#define	PCIC_VENDOR_I82365SLR1		2
#define	PCIC_VENDOR_CIRRUS_PD6710	3
#define	PCIC_VENDOR_CIRRUS_PD672X	4

/*
 * Individual drivers will allocate their own memory and io regions. Memory
 * regions must be a multiple of 4k, aligned on a 4k boundary.
 */

#define	PCIC_MEM_ALIGN	PCIC_MEM_PAGESIZE

static void	pcic_init_socket(struct pcic_handle *);

static void	pcic_intr_socket(struct pcic_handle *);

static int	pcic_activate(device_t dev);
static void	pcic_intr(void *arg);

static void	pcic_attach_card(struct pcic_handle *);
static void	pcic_detach_card(struct pcic_handle *, int);

static void	pcic_chip_do_mem_map(struct pcic_handle *, int);
static void	pcic_chip_do_io_map(struct pcic_handle *, int);

void	pcic_create_event_thread(void *);
void	pcic_event_thread(void *);

void	pcic_queue_event(struct pcic_handle *, int);

static void	pcic_wait_ready(struct pcic_handle *);

static u_int8_t st_pcic_read(struct pcic_handle *, int);
static void st_pcic_write(struct pcic_handle *, int, u_int8_t);

/* XXX Should really be dynamic XXX */
static struct pcic_handle *handles[20];
static struct pcic_handle **lasthandle = handles;

static struct pcic_handle *
pcic_get_handle(device_t dev, device_t child)
{
	if (dev == child)
		return NULL;
	while (child && device_get_parent(child) != dev)
		child = device_get_parent(child);
	if (child == NULL)
		return NULL;
	return ((struct pcic_handle *) device_get_ivars(child));
}

int
pcic_ident_ok(int ident)
{
	/* this is very empirical and heuristic */

	if ((ident == 0) || (ident == 0xff) || (ident & PCIC_IDENT_ZERO))
		return (0);

	if ((ident & PCIC_IDENT_IFTYPE_MASK) != PCIC_IDENT_IFTYPE_MEM_AND_IO) {
#ifdef DIAGNOSTIC
		printf("pcic: does not support memory and I/O cards, "
		    "ignored (ident=%0x)\n", ident);
#endif
		return (0);
	}
	return (1);
}

int
pcic_vendor(struct pcic_handle *h)
{
	int reg;

	/*
	 * the chip_id of the cirrus toggles between 11 and 00 after a write.
	 * weird.
	 */

	pcic_write(h, PCIC_CIRRUS_CHIP_INFO, 0);
	reg = pcic_read(h, -1);

	if ((reg & PCIC_CIRRUS_CHIP_INFO_CHIP_ID) ==
	    PCIC_CIRRUS_CHIP_INFO_CHIP_ID) {
		reg = pcic_read(h, -1);
		if ((reg & PCIC_CIRRUS_CHIP_INFO_CHIP_ID) == 0) {
			if (reg & PCIC_CIRRUS_CHIP_INFO_SLOTS)
				return (PCIC_VENDOR_CIRRUS_PD672X);
			else
				return (PCIC_VENDOR_CIRRUS_PD6710);
		}
	}

	reg = pcic_read(h, PCIC_IDENT);

	if ((reg & PCIC_IDENT_REV_MASK) == PCIC_IDENT_REV_I82365SLR0)
		return (PCIC_VENDOR_I82365SLR0);
	else
		return (PCIC_VENDOR_I82365SLR1);

	return (PCIC_VENDOR_UNKNOWN);
}

char *
pcic_vendor_to_string(int vendor)
{
	switch (vendor) {
	case PCIC_VENDOR_I82365SLR0:
		return ("Intel 82365SL Revision 0");
	case PCIC_VENDOR_I82365SLR1:
		return ("Intel 82365SL Revision 1");
	case PCIC_VENDOR_CIRRUS_PD6710:
		return ("Cirrus PD6710");
	case PCIC_VENDOR_CIRRUS_PD672X:
		return ("Cirrus PD672X");
	}

	return ("Unknown controller");
}

static int
pcic_activate(device_t dev)
{
	struct pcic_softc *sc = PCIC_SOFTC(dev);
	int err;

	sc->port_rid = 0;
	sc->port_res = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->port_rid,
	    0, ~0, PCIC_IOSIZE, RF_ACTIVE);
	if (!sc->port_res) {
		device_printf(dev, "Cannot allocate ioport\n");
		return ENOMEM;
	}

	sc->irq_rid = 0;
	sc->irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->irq_rid, 
	    0, ~0, 1, RF_ACTIVE);
	if (sc->irq_res) {
		sc->irq = rman_get_start(sc->irq_res);
		if ((err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC, 
		    pcic_intr, sc, &sc->intrhand)) != 0) {
			device_printf(dev, "Cannot setup intr\n");
			pcic_deactivate(dev);
			return err;
		}
	} else {
		printf("Polling not supported\n");
		/* XXX Do polling */
		return (ENXIO);
	}

	/* XXX This might not be needed in future, get it directly from
	 * XXX parent */
	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->mem_rid, 
	    0, ~0, 1 << 13, RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate mem\n");
		pcic_deactivate(dev);
		return ENOMEM;
	}

	sc->iot = rman_get_bustag(sc->port_res);
	sc->ioh = rman_get_bushandle(sc->port_res);;
	sc->memt = rman_get_bustag(sc->mem_res);
	sc->memh = rman_get_bushandle(sc->mem_res);;
	
	return (0);
}

void
pcic_deactivate(device_t dev)
{
	struct pcic_softc *sc = PCIC_SOFTC(dev);
	
	if (sc->intrhand)
		bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	sc->intrhand = 0;
	if (sc->port_res)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->port_rid, 
		    sc->port_res);
	sc->port_res = 0;
	if (sc->irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, sc->irq_rid, 
		    sc->irq_res);
	sc->irq_res = 0;
	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid, 
		    sc->mem_res);
	sc->mem_res = 0;
	return;
}

int
pcic_attach(device_t dev)
{
	struct pcic_softc *sc = PCIC_SOFTC(dev);
	struct pcic_handle *h;
	int vendor, count, i, reg, error;

	sc->dev = dev;

	/* Activate our resources */
	if ((error = pcic_activate(dev)) != 0) {
		printf("pcic_attach (active) returns %d\n", error);
		return error;
	}

	/* now check for each controller/socket */

	/*
	 * this could be done with a loop, but it would violate the
	 * abstraction...  --- unknown
	 * so? I don't see the abstraction... --imp
	 */

	count = 0;

	VERBOSE(("pcic ident regs:"));

	sc->handle[0].sc = sc;
	sc->handle[0].sock = C0SA;
	/* initialise pcic_read and pcic_write functions */
	sc->handle[0].ph_read = st_pcic_read;
	sc->handle[0].ph_write = st_pcic_write;
	sc->handle[0].ph_bus_t = sc->iot;
	sc->handle[0].ph_bus_h = sc->ioh;
	if (pcic_ident_ok(reg = pcic_read(&sc->handle[0], PCIC_IDENT))) {
		sc->handle[0].flags = PCIC_FLAG_SOCKETP;
		count++;
	} else {
		sc->handle[0].flags = 0;
	}
	sc->handle[0].laststate = PCIC_LASTSTATE_EMPTY;

	VERBOSE((" 0x%02x", reg));

	sc->handle[1].sc = sc;
	sc->handle[1].sock = C0SB;
	/* initialise pcic_read and pcic_write functions */
	sc->handle[1].ph_read = st_pcic_read;
	sc->handle[1].ph_write = st_pcic_write;
	sc->handle[1].ph_bus_t = sc->iot;
	sc->handle[1].ph_bus_h = sc->ioh;
	if (pcic_ident_ok(reg = pcic_read(&sc->handle[1], PCIC_IDENT))) {
		sc->handle[1].flags = PCIC_FLAG_SOCKETP;
		count++;
	} else {
		sc->handle[1].flags = 0;
	}
	sc->handle[1].laststate = PCIC_LASTSTATE_EMPTY;

	VERBOSE((" 0x%02x", reg));

	/*
	 * The CL-PD6729 has only one controller and always returns 0
	 * if you try to read from the second one. Maybe pcic_ident_ok
	 * shouldn't accept 0?
	 */
	sc->handle[2].sc = sc;
	sc->handle[2].sock = C1SA;
	/* initialise pcic_read and pcic_write functions */
	sc->handle[2].ph_read = st_pcic_read;
	sc->handle[2].ph_write = st_pcic_write;
	sc->handle[2].ph_bus_t = sc->iot;
	sc->handle[2].ph_bus_h = sc->ioh;
	if (pcic_vendor(&sc->handle[0]) != PCIC_VENDOR_CIRRUS_PD672X ||
	    pcic_read(&sc->handle[2], PCIC_IDENT) != 0) {
		if (pcic_ident_ok(reg = pcic_read(&sc->handle[2],
						  PCIC_IDENT))) {
			sc->handle[2].flags = PCIC_FLAG_SOCKETP;
			count++;
		} else {
			sc->handle[2].flags = 0;
		}
		sc->handle[2].laststate = PCIC_LASTSTATE_EMPTY;

		VERBOSE((" 0x%02x", reg));

		sc->handle[3].sc = sc;
 		sc->handle[3].sock = C1SB;
		/* initialise pcic_read and pcic_write functions */
		sc->handle[3].ph_read = st_pcic_read;
		sc->handle[3].ph_write = st_pcic_write;
		sc->handle[3].ph_bus_t = sc->iot;
		sc->handle[3].ph_bus_h = sc->ioh;
		if (pcic_ident_ok(reg = pcic_read(&sc->handle[3],
						  PCIC_IDENT))) {
			sc->handle[3].flags = PCIC_FLAG_SOCKETP;
			count++;
		} else {
			sc->handle[3].flags = 0;
		}
		sc->handle[3].laststate = PCIC_LASTSTATE_EMPTY;

		VERBOSE((" 0x%02x\n", reg));
	} else {
		sc->handle[2].flags = 0;
		sc->handle[3].flags = 0;
	}

	if (count == 0)
		panic("pcic_attach: attach found no sockets");

	/* establish the interrupt */

	/* XXX block interrupts? */

	for (i = 0; i < PCIC_NSLOTS; i++) {
		/*
		 * this should work, but w/o it, setting tty flags hangs at
		 * boot time.
		 */
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
		{
			STAILQ_INIT(&sc->handle[i].events);
			pcic_write(&sc->handle[i], PCIC_CSC_INTR, 0);
			pcic_read(&sc->handle[i], PCIC_CSC);
		}
	}

	if ((sc->handle[0].flags & PCIC_FLAG_SOCKETP) ||
	    (sc->handle[1].flags & PCIC_FLAG_SOCKETP)) {
		vendor = pcic_vendor(&sc->handle[0]);

		device_printf(dev, "controller 0 (%s) has ",
		       pcic_vendor_to_string(vendor));

		if ((sc->handle[0].flags & PCIC_FLAG_SOCKETP) &&
		    (sc->handle[1].flags & PCIC_FLAG_SOCKETP))
			printf("sockets A and B\n");
		else if (sc->handle[0].flags & PCIC_FLAG_SOCKETP)
			printf("socket A only\n");
		else
			printf("socket B only\n");

		if (sc->handle[0].flags & PCIC_FLAG_SOCKETP)
			sc->handle[0].vendor = vendor;
		if (sc->handle[1].flags & PCIC_FLAG_SOCKETP)
			sc->handle[1].vendor = vendor;
	}
	if ((sc->handle[2].flags & PCIC_FLAG_SOCKETP) ||
	    (sc->handle[3].flags & PCIC_FLAG_SOCKETP)) {
		vendor = pcic_vendor(&sc->handle[2]);

		device_printf(dev, "controller 1 (%s) has ",
		       pcic_vendor_to_string(vendor));

		if ((sc->handle[2].flags & PCIC_FLAG_SOCKETP) &&
		    (sc->handle[3].flags & PCIC_FLAG_SOCKETP))
			printf("sockets A and B\n");
		else if (sc->handle[2].flags & PCIC_FLAG_SOCKETP)
			printf("socket A only\n");
		else
			printf("socket B only\n");

		if (sc->handle[2].flags & PCIC_FLAG_SOCKETP)
			sc->handle[2].vendor = vendor;
		if (sc->handle[3].flags & PCIC_FLAG_SOCKETP)
			sc->handle[3].vendor = vendor;
	}

	for (i = 0; i < PCIC_NSLOTS; i++) {
		if ((sc->handle[i].flags & PCIC_FLAG_SOCKETP) == 0)
			continue;
		h = &sc->handle[i];
		/* initialize the rest of the handle */
		h->shutdown = 0;
		h->memalloc = 0;
		h->ioalloc = 0;
		h->ih_irq = 0;
		h->sc = sc;
		h->dev = device_add_child(dev, "pccard", -1);
		device_set_ivars(h->dev, h);
		pcic_init_socket(h);
	}

	/*
	 * Probe and attach any children as were configured above.
	 */
	error = bus_generic_attach(dev);
	if (error)
		pcic_deactivate(dev);
	return error;
}

void
pcic_create_event_thread(void *arg)
{
	struct pcic_handle *h = arg;
	const char *cs;

	switch (h->sock) {
	case C0SA:
		cs = "0,0";
		break;
	case C0SB:
		cs = "0,1";
		break;
	case C1SA:
		cs = "1,0";
		break;
	case C1SB:
		cs = "1,1";
		break;
	default:
		panic("pcic_create_event_thread: unknown pcic socket");
	}

	if (kthread_create(pcic_event_thread, h, &h->event_thread,
	    RFTHREAD, "%s,%s", device_get_name(h->sc->dev), cs)) {
		device_printf(h->sc->dev,
		    "cannot create event thread for sock 0x%02x\n", h->sock);
		panic("pcic_create_event_thread");
	}
}

void
pcic_event_thread(void *arg)
{
	struct pcic_handle *h = arg;
	struct pcic_event *pe;
	int s;
	struct pcic_softc *sc = h->sc;

	while (h->shutdown == 0) {
		s = splhigh();
		if ((pe = STAILQ_FIRST(&h->events)) == NULL) {
			splx(s);
			(void) tsleep(&h->events, PWAIT, "pcicev", 0);
			continue;
		} else {
			splx(s);
			/* sleep .25s to be enqueued chatterling interrupts */
			(void) tsleep((caddr_t)pcic_event_thread, PWAIT, "pcicss", hz/4);
		}
		s = splhigh();
		STAILQ_REMOVE_HEAD_UNTIL(&h->events, pe, pe_q);
		splx(s);

		switch (pe->pe_type) {
		case PCIC_EVENT_INSERTION:
			s = splhigh();
			while (1) {
				struct pcic_event *pe1, *pe2;

				if ((pe1 = STAILQ_FIRST(&h->events)) == NULL)
					break;
				if (pe1->pe_type != PCIC_EVENT_REMOVAL)
					break;
				if ((pe2 = STAILQ_NEXT(pe1, pe_q)) == NULL)
					break;
				if (pe2->pe_type == PCIC_EVENT_INSERTION) {
					STAILQ_REMOVE_HEAD_UNTIL(&h->events, pe1, pe_q);
					free(pe1, M_TEMP);
					STAILQ_REMOVE_HEAD_UNTIL(&h->events, pe2, pe_q);
					free(pe2, M_TEMP);
				}
			}
			splx(s);
				
			DEVPRINTF((h->dev, "insertion event\n"));
			pcic_attach_card(h);
			break;

		case PCIC_EVENT_REMOVAL:
			s = splhigh();
			while (1) {
				struct pcic_event *pe1, *pe2;

				if ((pe1 = STAILQ_FIRST(&h->events)) == NULL)
					break;
				if (pe1->pe_type != PCIC_EVENT_INSERTION)
					break;
				if ((pe2 = STAILQ_NEXT(pe1, pe_q)) == NULL)
					break;
				if (pe2->pe_type == PCIC_EVENT_REMOVAL) {
					STAILQ_REMOVE_HEAD_UNTIL(&h->events, pe1, pe_q);
					free(pe1, M_TEMP);
					STAILQ_REMOVE_HEAD_UNTIL(&h->events, pe2, pe_q);
					free(pe2, M_TEMP);
				}
			}
			splx(s);

			DEVPRINTF((h->dev, "removal event\n"));
			pcic_detach_card(h, DETACH_FORCE);
			break;

		default:
			panic("pcic_event_thread: unknown event %d",
			    pe->pe_type);
		}
		free(pe, M_TEMP);
	}

	h->event_thread = NULL;

	/* In case parent is waiting for us to exit. */
	wakeup(sc);

	kthread_exit(0);
}

void
pcic_init_socket(struct pcic_handle *h)
{
	int reg;
	struct pcic_softc *sc = h->sc;

	/*
	 * queue creation of a kernel thread to handle insert/removal events.
	 */
	*lasthandle++ = h;

	/* set up the card to interrupt on card detect */

	pcic_write(h, PCIC_CSC_INTR, (sc->irq << PCIC_CSC_INTR_IRQ_SHIFT) |
	    PCIC_CSC_INTR_CD_ENABLE);
	pcic_write(h, PCIC_INTR, 0);
	pcic_read(h, PCIC_CSC);

	/* unsleep the cirrus controller */

	if ((h->vendor == PCIC_VENDOR_CIRRUS_PD6710) ||
	    (h->vendor == PCIC_VENDOR_CIRRUS_PD672X)) {
		reg = pcic_read(h, PCIC_CIRRUS_MISC_CTL_2);
		if (reg & PCIC_CIRRUS_MISC_CTL_2_SUSPEND) {
			DEVPRINTF((sc->dev, "socket %02x was suspended\n",
			    h->sock));
			reg &= ~PCIC_CIRRUS_MISC_CTL_2_SUSPEND;
			pcic_write(h, PCIC_CIRRUS_MISC_CTL_2, reg);
		}
	}
	h->laststate = PCIC_LASTSTATE_EMPTY;

#if 0
/* XXX */
/*	Should do this later */
/* maybe as part of interrupt routing verification */
	if ((reg & PCIC_IF_STATUS_CARDDETECT_MASK) ==
	    PCIC_IF_STATUS_CARDDETECT_PRESENT) {
		pcic_attach_card(h);
		h->laststate = PCIC_LASTSTATE_PRESENT;
	} else {
		h->laststate = PCIC_LASTSTATE_EMPTY;
	}
#endif
}

static void
pcic_intr(void *arg)
{
	struct pcic_softc *sc = arg;
	int i;

	DEVPRINTF((sc->dev, "intr\n"));

	for (i = 0; i < PCIC_NSLOTS; i++)
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			pcic_intr_socket(&sc->handle[i]);
}

static void
pcic_intr_socket(struct pcic_handle *h)
{
	int cscreg;

	cscreg = pcic_read(h, PCIC_CSC);

	cscreg &= (PCIC_CSC_GPI | PCIC_CSC_CD | PCIC_CSC_READY | 
	    PCIC_CSC_BATTWARN | PCIC_CSC_BATTDEAD);

	if (cscreg & PCIC_CSC_GPI) {
		DEVPRINTF((h->dev, "%02x GPI\n", h->sock));
	}
	if (cscreg & PCIC_CSC_CD) {
		int statreg;

		statreg = pcic_read(h, PCIC_IF_STATUS);

		DEVPRINTF((h->dev, "%02x CD %x\n", h->sock, statreg));

		if ((statreg & PCIC_IF_STATUS_CARDDETECT_MASK) ==
		    PCIC_IF_STATUS_CARDDETECT_PRESENT) {
			if (h->laststate != PCIC_LASTSTATE_PRESENT) {
				DEVPRINTF((h->dev, 
				    "enqueing INSERTION event\n"));
				pcic_queue_event(h, PCIC_EVENT_INSERTION);
			}
			h->laststate = PCIC_LASTSTATE_PRESENT;
		} else {
			if (h->laststate == PCIC_LASTSTATE_PRESENT) {
				/* Deactivate the card now. */
				DEVPRINTF((h->dev, "detaching card\n"));
				pcic_detach_card(h, DETACH_FORCE);
				DEVPRINTF((h->dev,"enqueing REMOVAL event\n"));
				pcic_queue_event(h, PCIC_EVENT_REMOVAL);
			}
			h->laststate = ((statreg & PCIC_IF_STATUS_CARDDETECT_MASK) == 0)
				? PCIC_LASTSTATE_EMPTY : PCIC_LASTSTATE_HALF;
		}
	}
	if (cscreg & PCIC_CSC_READY) {
		DEVPRINTF((h->dev, "%02x READY\n", h->sock));
		/* shouldn't happen */
	}
	if (cscreg & PCIC_CSC_BATTWARN) {
		DEVPRINTF((h->dev, "%02x BATTWARN\n", h->sock));
	}
	if (cscreg & PCIC_CSC_BATTDEAD) {
		DEVPRINTF((h->dev, "%02x BATTDEAD\n", h->sock));
	}
}

void
pcic_queue_event(struct pcic_handle *h, int event)
{
	struct pcic_event *pe;
	int s;

	pe = malloc(sizeof(*pe), M_TEMP, M_NOWAIT);
	if (pe == NULL)
		panic("pcic_queue_event: can't allocate event");

	pe->pe_type = event;
	s = splhigh();
	STAILQ_INSERT_TAIL(&h->events, pe, pe_q);
	splx(s);
	wakeup(&h->events);
}

static void
pcic_attach_card(struct pcic_handle *h)
{
	DPRINTF(("pcic_attach_card h %p h->dev %p\n", h, h->dev));
	if (!(h->flags & PCIC_FLAG_CARDP)) {
		DPRINTF(("Calling MI attach function\n"));
		/* call the MI attach function */
		CARD_ATTACH_CARD(h->dev);
		h->flags |= PCIC_FLAG_CARDP;
	} else {
		DPRINTF(("pcic_attach_card: already attached\n"));
	}
}

static void
pcic_detach_card(struct pcic_handle *h, int flags)
{
	DPRINTF(("pcic_detach_card h %p h->dev %p\n", h, h->dev));
	if (h->flags & PCIC_FLAG_CARDP) {
		h->flags &= ~PCIC_FLAG_CARDP;

		/* call the MI detach function */
		CARD_DETACH_CARD(h->dev, flags);
	} else {
		DPRINTF(("pcic_detach_card: already detached\n"));
	}
}

static int 
pcic_chip_mem_alloc(struct pcic_handle *h, struct resource *r, bus_size_t size,
    struct pccard_mem_handle *pcmhp)
{
	bus_space_handle_t memh;
	bus_addr_t addr;
	bus_size_t sizepg;
	int mask;
	struct pcic_softc *sc = h->sc;

	/* out of sc->memh, allocate as many pages as necessary */

	/* convert size to PCIC pages */
	sizepg = (size + (PCIC_MEM_ALIGN - 1)) / PCIC_MEM_ALIGN;
	if (sizepg > PCIC_MAX_MEM_PAGES)
		return (1);

	mask = (1 << sizepg) - 1;

	addr = rman_get_start(r);
	memh = addr;
	pcmhp->memt = sc->memt;
	pcmhp->memh = memh;
	pcmhp->addr = addr;
	pcmhp->size = size;
	pcmhp->realsize = sizepg * PCIC_MEM_PAGESIZE;
	return (0);
}

static void 
pcic_chip_mem_free(struct pcic_handle *h, struct pccard_mem_handle *pcmhp)
{
}

static struct mem_map_index_st {
	int	sysmem_start_lsb;
	int	sysmem_start_msb;
	int	sysmem_stop_lsb;
	int	sysmem_stop_msb;
	int	cardmem_lsb;
	int	cardmem_msb;
	int	memenable;
} mem_map_index[] = {
	{
		PCIC_SYSMEM_ADDR0_START_LSB,
		PCIC_SYSMEM_ADDR0_START_MSB,
		PCIC_SYSMEM_ADDR0_STOP_LSB,
		PCIC_SYSMEM_ADDR0_STOP_MSB,
		PCIC_CARDMEM_ADDR0_LSB,
		PCIC_CARDMEM_ADDR0_MSB,
		PCIC_ADDRWIN_ENABLE_MEM0,
	},
	{
		PCIC_SYSMEM_ADDR1_START_LSB,
		PCIC_SYSMEM_ADDR1_START_MSB,
		PCIC_SYSMEM_ADDR1_STOP_LSB,
		PCIC_SYSMEM_ADDR1_STOP_MSB,
		PCIC_CARDMEM_ADDR1_LSB,
		PCIC_CARDMEM_ADDR1_MSB,
		PCIC_ADDRWIN_ENABLE_MEM1,
	},
	{
		PCIC_SYSMEM_ADDR2_START_LSB,
		PCIC_SYSMEM_ADDR2_START_MSB,
		PCIC_SYSMEM_ADDR2_STOP_LSB,
		PCIC_SYSMEM_ADDR2_STOP_MSB,
		PCIC_CARDMEM_ADDR2_LSB,
		PCIC_CARDMEM_ADDR2_MSB,
		PCIC_ADDRWIN_ENABLE_MEM2,
	},
	{
		PCIC_SYSMEM_ADDR3_START_LSB,
		PCIC_SYSMEM_ADDR3_START_MSB,
		PCIC_SYSMEM_ADDR3_STOP_LSB,
		PCIC_SYSMEM_ADDR3_STOP_MSB,
		PCIC_CARDMEM_ADDR3_LSB,
		PCIC_CARDMEM_ADDR3_MSB,
		PCIC_ADDRWIN_ENABLE_MEM3,
	},
	{
		PCIC_SYSMEM_ADDR4_START_LSB,
		PCIC_SYSMEM_ADDR4_START_MSB,
		PCIC_SYSMEM_ADDR4_STOP_LSB,
		PCIC_SYSMEM_ADDR4_STOP_MSB,
		PCIC_CARDMEM_ADDR4_LSB,
		PCIC_CARDMEM_ADDR4_MSB,
		PCIC_ADDRWIN_ENABLE_MEM4,
	},
};

static void 
pcic_chip_do_mem_map(struct pcic_handle *h, int win)
{
	int reg;

	pcic_write(h, mem_map_index[win].sysmem_start_lsb,
	    (h->mem[win].addr >> PCIC_SYSMEM_ADDRX_SHIFT) & 0xff);
	pcic_write(h, mem_map_index[win].sysmem_start_msb,
	    ((h->mem[win].addr >> (PCIC_SYSMEM_ADDRX_SHIFT + 8)) &
	    PCIC_SYSMEM_ADDRX_START_MSB_ADDR_MASK));

#if 0
	/* XXX do I want 16 bit all the time? */
	PCIC_SYSMEM_ADDRX_START_MSB_DATASIZE_16BIT;
#endif

	pcic_write(h, mem_map_index[win].sysmem_stop_lsb,
	    ((h->mem[win].addr + h->mem[win].size) >>
	    PCIC_SYSMEM_ADDRX_SHIFT) & 0xff);
	pcic_write(h, mem_map_index[win].sysmem_stop_msb,
	    (((h->mem[win].addr + h->mem[win].size) >>
	    (PCIC_SYSMEM_ADDRX_SHIFT + 8)) &
	    PCIC_SYSMEM_ADDRX_STOP_MSB_ADDR_MASK) |
	    PCIC_SYSMEM_ADDRX_STOP_MSB_WAIT2);

	pcic_write(h, mem_map_index[win].cardmem_lsb,
	    (h->mem[win].offset >> PCIC_CARDMEM_ADDRX_SHIFT) & 0xff);
	pcic_write(h, mem_map_index[win].cardmem_msb,
	    ((h->mem[win].offset >> (PCIC_CARDMEM_ADDRX_SHIFT + 8)) &
	    PCIC_CARDMEM_ADDRX_MSB_ADDR_MASK) |
	    ((h->mem[win].kind == PCCARD_MEM_ATTR) ?
	    PCIC_CARDMEM_ADDRX_MSB_REGACTIVE_ATTR : 0));

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg |= (mem_map_index[win].memenable | PCIC_ADDRWIN_ENABLE_MEMCS16);
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	DELAY(100);

#ifdef PCICDEBUG
	{
		int r1, r2, r3, r4, r5, r6;

		r1 = pcic_read(h, mem_map_index[win].sysmem_start_msb);
		r2 = pcic_read(h, mem_map_index[win].sysmem_start_lsb);
		r3 = pcic_read(h, mem_map_index[win].sysmem_stop_msb);
		r4 = pcic_read(h, mem_map_index[win].sysmem_stop_lsb);
		r5 = pcic_read(h, mem_map_index[win].cardmem_msb);
		r6 = pcic_read(h, mem_map_index[win].cardmem_lsb);

		DPRINTF(("pcic_chip_do_mem_map window %d: %02x%02x %02x%02x "
		    "%02x%02x\n", win, r1, r2, r3, r4, r5, r6));
	}
#endif
}

static int 
pcic_chip_mem_map(struct pcic_handle *h, int kind, bus_addr_t card_addr,
    bus_size_t size, struct pccard_mem_handle *pcmhp, bus_addr_t *offsetp,
    int *windowp)
{
	bus_addr_t busaddr;
	long card_offset;
	int i, win;

	win = -1;
	for (i = 0; i < (sizeof(mem_map_index) / sizeof(mem_map_index[0]));
	    i++) {
		if ((h->memalloc & (1 << i)) == 0) {
			win = i;
			h->memalloc |= (1 << i);
			break;
		}
	}

	if (win == -1)
		return (1);

	*windowp = win;
	busaddr = pcmhp->addr;

	/*
	 * compute the address offset to the pccard address space for the
	 * pcic.  this is intentionally signed.  The masks and shifts below
	 * will cause TRT to happen in the pcic registers.  Deal with making
	 * sure the address is aligned, and return the alignment offset.
	 */

	*offsetp = card_addr % PCIC_MEM_ALIGN;
	card_addr -= *offsetp;

	DPRINTF(("pcic_chip_mem_map window %d bus %lx+%lx+%lx at card addr "
	    "%lx\n", win, (u_long) busaddr, (u_long) * offsetp, (u_long) size,
	    (u_long) card_addr));

	/*
	 * include the offset in the size, and decrement size by one, since
	 * the hw wants start/stop
	 */
	size += *offsetp - 1;

	card_offset = (((long) card_addr) - ((long) busaddr));

	h->mem[win].addr = busaddr;
	h->mem[win].size = size;
	h->mem[win].offset = card_offset;
	h->mem[win].kind = kind;

	pcic_chip_do_mem_map(h, win);

	return (0);
}

static void 
pcic_chip_mem_unmap(struct pcic_handle *h, int window)
{
	int reg;

	if (window >= (sizeof(mem_map_index) / sizeof(mem_map_index[0])))
		panic("pcic_chip_mem_unmap: window out of range");

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg &= ~mem_map_index[window].memenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	h->memalloc &= ~(1 << window);
}

static int 
pcic_chip_io_alloc(struct pcic_handle *h, bus_addr_t start, bus_size_t size,
    bus_size_t align, struct pccard_io_handle *pcihp)
{
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t ioaddr;
	int flags = 0;
	struct pcic_softc *sc = h->sc;

	/*
	 * Allocate some arbitrary I/O space.
	 */

	iot = sc->iot;

	ioaddr = start;
	if (start) {
		ioh = start;
		DPRINTF(("pcic_chip_io_alloc map port %lx+%lx\n",
		    (u_long) ioaddr, (u_long) size));
	} else {
		flags |= PCCARD_IO_ALLOCATED;
		ioh = start;
		DPRINTF(("pcic_chip_io_alloc alloc port %lx+%lx\n",
		    (u_long) ioaddr, (u_long) size));
	}

	pcihp->iot = iot;
	pcihp->ioh = ioh;
	pcihp->addr = ioaddr;
	pcihp->size = size;
	pcihp->flags = flags;

	return (0);
}

static void 
pcic_chip_io_free(struct pcic_handle *h, struct pccard_io_handle *pcihp)
{
}


static struct io_map_index_st {
	int	start_lsb;
	int	start_msb;
	int	stop_lsb;
	int	stop_msb;
	int	ioenable;
	int	ioctlmask;
	int	ioctlbits[3];		/* indexed by PCCARD_WIDTH_* */
}               io_map_index[] = {
	{
		PCIC_IOADDR0_START_LSB,
		PCIC_IOADDR0_START_MSB,
		PCIC_IOADDR0_STOP_LSB,
		PCIC_IOADDR0_STOP_MSB,
		PCIC_ADDRWIN_ENABLE_IO0,
		PCIC_IOCTL_IO0_WAITSTATE | PCIC_IOCTL_IO0_ZEROWAIT |
		PCIC_IOCTL_IO0_IOCS16SRC_MASK | PCIC_IOCTL_IO0_DATASIZE_MASK,
		{
			PCIC_IOCTL_IO0_IOCS16SRC_CARD,
			PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO0_DATASIZE_8BIT,
			PCIC_IOCTL_IO0_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO0_DATASIZE_16BIT,
		},
	},
	{
		PCIC_IOADDR1_START_LSB,
		PCIC_IOADDR1_START_MSB,
		PCIC_IOADDR1_STOP_LSB,
		PCIC_IOADDR1_STOP_MSB,
		PCIC_ADDRWIN_ENABLE_IO1,
		PCIC_IOCTL_IO1_WAITSTATE | PCIC_IOCTL_IO1_ZEROWAIT |
		PCIC_IOCTL_IO1_IOCS16SRC_MASK | PCIC_IOCTL_IO1_DATASIZE_MASK,
		{
			PCIC_IOCTL_IO1_IOCS16SRC_CARD,
			PCIC_IOCTL_IO1_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO1_DATASIZE_8BIT,
			PCIC_IOCTL_IO1_IOCS16SRC_DATASIZE |
			    PCIC_IOCTL_IO1_DATASIZE_16BIT,
		},
	},
};

static void 
pcic_chip_do_io_map(struct pcic_handle *h, int win)
{
	int reg;

	DPRINTF(("pcic_chip_do_io_map win %d addr %lx size %lx width %d\n",
	    win, (long) h->io[win].addr, (long) h->io[win].size,
	    h->io[win].width * 8));

	pcic_write(h, io_map_index[win].start_lsb, h->io[win].addr & 0xff);
	pcic_write(h, io_map_index[win].start_msb,
	    (h->io[win].addr >> 8) & 0xff);

	pcic_write(h, io_map_index[win].stop_lsb,
	    (h->io[win].addr + h->io[win].size - 1) & 0xff);
	pcic_write(h, io_map_index[win].stop_msb,
	    ((h->io[win].addr + h->io[win].size - 1) >> 8) & 0xff);

	reg = pcic_read(h, PCIC_IOCTL);
	reg &= ~io_map_index[win].ioctlmask;
	reg |= io_map_index[win].ioctlbits[h->io[win].width];
	pcic_write(h, PCIC_IOCTL, reg);

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg |= io_map_index[win].ioenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);
}

static int 
pcic_chip_io_map(struct pcic_handle *h, int width, bus_addr_t offset,
    bus_size_t size, struct pccard_io_handle *pcihp, int *windowp)
{
	bus_addr_t ioaddr = pcihp->addr + offset;
	int i, win;
#ifdef PCICDEBUG
	static char *width_names[] = { "auto", "io8", "io16" };
#endif
#if 0
	struct pcic_softc *sc = h->sc;
#endif

	/* XXX Sanity check offset/size. */

	win = -1;
	for (i = 0; i < (sizeof(io_map_index) / sizeof(io_map_index[0])); i++) {
		if ((h->ioalloc & (1 << i)) == 0) {
			win = i;
			h->ioalloc |= (1 << i);
			break;
		}
	}

	if (win == -1)
		return (1);

	*windowp = win;

#if 0
	/* XXX this is pretty gross */
	if (sc->iot != pcihp->iot)
		panic("pcic_chip_io_map iot is bogus");
#endif

	DPRINTF(("pcic_chip_io_map window %d %s port %lx+%lx\n",
		 win, width_names[width], (u_long) ioaddr, (u_long) size));

	/* XXX wtf is this doing here? */

	printf(" port 0x%lx", (u_long) ioaddr);
	if (size > 1)
		printf("-0x%lx", (u_long) ioaddr + (u_long) size - 1);

	h->io[win].addr = ioaddr;
	h->io[win].size = size;
	h->io[win].width = width;

	pcic_chip_do_io_map(h, win);

	return (0);
}

static void 
pcic_chip_io_unmap(struct pcic_handle *h, int window)
{
	int reg;

	if (window >= (sizeof(io_map_index) / sizeof(io_map_index[0])))
		panic("pcic_chip_io_unmap: window out of range");

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg &= ~io_map_index[window].ioenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	h->ioalloc &= ~(1 << window);
}

static void
pcic_wait_ready(struct pcic_handle *h)
{
	int i;

	for (i = 0; i < 10000; i++) {
		if (pcic_read(h, PCIC_IF_STATUS) & PCIC_IF_STATUS_READY)
			return;
		DELAY(500);
#ifdef PCICDEBUG
		if (pcic_debug) {
			if ((i>5000) && (i%100 == 99))
				printf(".");
		}
#endif
	}

#ifdef DIAGNOSTIC
	printf("pcic_wait_ready: ready never happened, status = %02x\n",
	    pcic_read(h, PCIC_IF_STATUS));
#endif
}

int
pcic_enable_socket(device_t dev, device_t child)
{
	struct pcic_handle *h = pcic_get_handle(dev, child);
	int cardtype, reg, win;

	/* this bit is mostly stolen from pcic_attach_card */

	/* power down the socket to reset it, clear the card reset pin */

	pcic_write(h, PCIC_PWRCTL, 0);

	/* 
	 * wait 300ms until power fails (Tpf).  Then, wait 100ms since
	 * we are changing Vcc (Toff).
	 */
	DELAY((300 + 100) * 1000);

#ifdef VADEM_POWER_HACK
	bus_space_write_1(sc->iot, sc->ioh, PCIC_REG_INDEX, 0x0e);
	bus_space_write_1(sc->iot, sc->ioh, PCIC_REG_INDEX, 0x37);
	printf("prcr = %02x\n", pcic_read(h, 0x02));
	printf("cvsr = %02x\n", pcic_read(h, 0x2f));
	printf("DANGER WILL ROBINSON!  Changing voltage select!\n");
	pcic_write(h, 0x2f, pcic_read(h, 0x2f) & ~0x03);
	printf("cvsr = %02x\n", pcic_read(h, 0x2f));
#endif
	
	/* power up the socket */

	pcic_write(h, PCIC_PWRCTL, PCIC_PWRCTL_DISABLE_RESETDRV
			   | PCIC_PWRCTL_PWR_ENABLE);

	/*
	 * wait 100ms until power raise (Tpr) and 20ms to become
	 * stable (Tsu(Vcc)).
	 *
	 * some machines require some more time to be settled
	 * (300ms is added here).
	 */
	DELAY((100 + 20 + 300) * 1000);

	pcic_write(h, PCIC_PWRCTL, PCIC_PWRCTL_DISABLE_RESETDRV | PCIC_PWRCTL_OE
			   | PCIC_PWRCTL_PWR_ENABLE);
	pcic_write(h, PCIC_INTR, 0);

	/*
	 * hold RESET at least 10us.
	 */
	DELAY(10);

	/* clear the reset flag */

	pcic_write(h, PCIC_INTR, PCIC_INTR_RESET);

	/* wait 20ms as per pc card standard (r2.01) section 4.3.6 */

	DELAY(20000);

	/* wait for the chip to finish initializing */

#ifdef DIAGNOSTIC
	reg = pcic_read(h, PCIC_IF_STATUS);
	if (!(reg & PCIC_IF_STATUS_POWERACTIVE)) {
		printf("pcic_chip_socket_enable: status %x", reg);
	}
#endif

	pcic_wait_ready(h);

	/* zero out the address windows */
	pcic_write(h, PCIC_ADDRWIN_ENABLE, 0);

	/* set the card type */
	CARD_GET_TYPE(h->dev, &cardtype);

	reg = pcic_read(h, PCIC_INTR);
	reg &= ~(PCIC_INTR_CARDTYPE_MASK | PCIC_INTR_IRQ_MASK | PCIC_INTR_ENABLE);
	reg |= ((cardtype == PCCARD_IFTYPE_IO) ?
		PCIC_INTR_CARDTYPE_IO :
		PCIC_INTR_CARDTYPE_MEM);
	reg |= h->ih_irq;
	pcic_write(h, PCIC_INTR, reg);

	DEVPRINTF((h->dev, "pcic_chip_socket_enable cardtype %s %02x\n",
	    ((cardtype == PCCARD_IFTYPE_IO) ? "io" : "mem"), reg));

	/* reinstall all the memory and io mappings */

	for (win = 0; win < PCIC_MEM_WINS; win++)
		if (h->memalloc & (1 << win))
			pcic_chip_do_mem_map(h, win);

	for (win = 0; win < PCIC_IO_WINS; win++)
		if (h->ioalloc & (1 << win))
			pcic_chip_do_io_map(h, win);

	return 0;
}

int
pcic_disable_socket(device_t dev, device_t child)
{
	struct pcic_handle *h = pcic_get_handle(dev, child);
	DPRINTF(("pcic_chip_socket_disable\n"));

	/* power down the socket */

	pcic_write(h, PCIC_PWRCTL, 0);

	/*
	 * wait 300ms until power fails (Tpf).
	 */
	DELAY(300 * 1000);

	return 0;
}

static u_int8_t
st_pcic_read(struct pcic_handle *h, int idx)
{
	if (idx != -1) {
		bus_space_write_1(h->ph_bus_t, h->ph_bus_h, PCIC_REG_INDEX, 
		    h->sock + idx);
	}
	return bus_space_read_1(h->ph_bus_t, h->ph_bus_h, PCIC_REG_DATA);
}

static void
st_pcic_write(struct pcic_handle *h, int idx, u_int8_t data)
{
	if (idx != -1) {
		bus_space_write_1(h->ph_bus_t, h->ph_bus_h, PCIC_REG_INDEX, 
		    h->sock + idx);
	}

	bus_space_write_1(h->ph_bus_t, h->ph_bus_h, PCIC_REG_DATA, data);
}

int
pcic_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	int err;
	int sz;
	int win;
	bus_addr_t off;
	struct pcic_handle *h = pcic_get_handle(dev, child);

	sz = rman_get_size(r);
	switch (type) {
	case SYS_RES_IOPORT:
		win = rid;
		err = pcic_chip_io_map(h, 0, 0, sz, &h->io[rid], &win);
		if (err) {
			pcic_chip_io_free(h, &h->io[rid]);
			return err;
		}
		break;
	case SYS_RES_MEMORY: 
		err = pcic_chip_mem_map(h, 0, 0, sz, &h->mem[rid], &off, &win);
		if (err) {
			pcic_chip_mem_free(h, &h->mem[rid]);
			return err;
		}
		break;
	default:
		break;
	}
	err = bus_generic_activate_resource(device_get_parent(dev), child,
	    type, rid, r);
	return (err);
}

int
pcic_deactivate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pcic_handle *h = pcic_get_handle(dev, child);
	int err = 0;

	switch (type) {
	case SYS_RES_IOPORT:
		pcic_chip_io_unmap(h, rid);
		break;
	case SYS_RES_MEMORY: 
		pcic_chip_mem_unmap(h, rid);
	default:
		break;
	}
	err = bus_generic_deactivate_resource(device_get_parent(dev), child,
	    type, rid, r);
	return (err);
}

int
pcic_setup_intr(device_t dev, device_t child, struct resource *irqres,
    int flags, driver_intr_t intr, void *arg, void **cookiep)
{
	struct pcic_handle *h = pcic_get_handle(dev, child);
	int reg;
	int irq;
	int err;

	err = bus_generic_setup_intr(device_get_parent(dev), child, irqres,
	    flags, intr, arg, cookiep);
	if (!err)
		return (err);

	irq = rman_get_start(irqres);
	reg = pcic_read(h, PCIC_INTR);
	reg &= ~(PCIC_INTR_IRQ_MASK | PCIC_INTR_ENABLE);
	reg |= irq;
	pcic_write(h, PCIC_INTR, reg);

	h->ih_irq = irq;

	device_printf(dev, "card irq %d\n", irq);

	return 0;
}

int
pcic_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookiep)
{
	int reg;
	struct pcic_handle *h = pcic_get_handle(dev, child);

	h->ih_irq = 0;

	reg = pcic_read(h, PCIC_INTR);
	reg &= ~(PCIC_INTR_IRQ_MASK | PCIC_INTR_ENABLE);
	pcic_write(h, PCIC_INTR, reg);

	return (bus_generic_teardown_intr(device_get_parent(dev), child, irq,
	    cookiep));
}

struct resource *
pcic_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	int sz;
	int err;
	struct resource *r;
	struct pcic_handle *h = pcic_get_handle(dev, child);

	/* Nearly default */
	if (type == SYS_RES_MEMORY && start == 0 && end == ~0 && count != 1) {
		start = 0xd0000;	/* XXX */
		end = 0xdffff;
	}

	r = bus_generic_alloc_resource(dev, child, type, rid, start, end,
	    count, flags);
	if (r == NULL)
		return r;
	sz = rman_get_size(r);
	switch (type) {
	case SYS_RES_IOPORT:
		err = pcic_chip_io_alloc(h, rman_get_start(r), sz, 0,
		    &h->io[*rid]);
		if (err) {
			bus_generic_release_resource(dev, child, type, *rid, 
			    r);
			return 0;
		}
		break;
	case SYS_RES_MEMORY: 
		err = pcic_chip_mem_alloc(h, r, sz, &h->mem[*rid]);
		if (err) {
			bus_generic_release_resource(dev, child, type, *rid,
			    r);
			return 0;
		}
		break;
	default:
		break;
	}
	return r;
}

int
pcic_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct pcic_handle *h = pcic_get_handle(dev, child);

	switch (type) {
	case SYS_RES_IOPORT:
		pcic_chip_io_free(h, &h->io[rid]);
		break;
	case SYS_RES_MEMORY: 
		pcic_chip_mem_free(h, &h->mem[rid]);
	default:
		break;
	}
	return bus_generic_release_resource(dev, child, type, rid, r);
}

int
pcic_suspend(device_t dev)
{
	/*
	 * Do nothing for now, maybe in time do what FreeBSD's current 
	 * pccard code does and detach my children.  That's the safest thing
	 * to do since we don't want to wake up and have different hardware
	 * in the slots.
	 */

	return 0;
}

int
pcic_resume(device_t dev)
{
	/* Need to port pcic_power from newer netbsd versions of this file */

	return 0;
}

int
pcic_set_res_flags(device_t dev, device_t child, int type, int rid, 
    u_int32_t flags)
{
	struct pcic_handle *h = pcic_get_handle(dev, child);

	DPRINTF(("%p %p %d %d %#x\n", dev, child, type, rid, flags));
	if (type != SYS_RES_MEMORY)
		return (EINVAL);
	h->mem[rid].kind = PCCARD_MEM_ATTR;
	pcic_chip_do_mem_map(h, rid);

	return 0;
}

int
pcic_set_memory_offset(device_t dev, device_t child, int rid, u_int32_t offset)
{
	return 0;
}

static void
pcic_start_threads(void *arg)
{
	struct pcic_handle **walker;
	walker = handles;
	while (*walker) {
		pcic_create_event_thread(*walker++);
	}
}

int
pcic_detach(device_t dev)
{
	device_t *kids;
	int nkids;
	int i;
	int ret;

	pcic_deactivate(dev);
	ret = bus_generic_detach(dev);
	if (ret != 0)
		return (ret);
	/*
	 * Normally, one wouldn't delete the children.  However, detach
	 * merely detaches the children w/o deleting them.  So if
	 * we were to reattach, we add additional children and wind up
	 * with duplicates.  So, we remove them here following the
	 * implicit "if you add it in attach, you should delete it in
	 * detach" rule that may or may not be documented.
	 */
	device_get_children(dev, &kids, &nkids);
	for (i = 0; i < nkids; i++) {
		if ((ret = device_delete_child(dev, kids[i])) != 0)
			device_printf(dev, "delete of %s failed: %d\n",
				device_get_nameunit(kids[i]), ret);
	}
	free(kids, M_TEMP);
	return 0;
}

SYSINIT(pcic, SI_SUB_KTHREAD_IDLE, SI_ORDER_ANY, pcic_start_threads, 0);
MODULE_VERSION(pcic, 1);
