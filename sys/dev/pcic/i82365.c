/*	$NetBSD: i82365.c,v 1.23 1999/02/19 03:14:00 mycroft Exp $	*/
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

#include <sys/proc.h>
#include <sys/wait.h>
#include <sys/kthread.h>
#include <vm/vm.h>

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include <dev/pcic/i82365reg.h>
#include <dev/pcic/i82365var.h>

#ifdef __FreeBSD__
#define delay(x) DELAY(x)
#endif

#ifdef PCICDEBUG
int	pcic_debug = 0;
#define	DPRINTF(arg) if (pcic_debug) printf arg;
#else
#define	DPRINTF(arg)
#endif

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

void	pcic_attach_socket __P((struct pcic_handle *));
void	pcic_init_socket __P((struct pcic_handle *));

#if XXX
int	pcic_submatch __P((struct device *, struct cfdata *, void *));
int	pcic_print  __P((void *arg, const char *pnp));
#endif
int	pcic_intr_socket __P((struct pcic_handle *));

void	pcic_attach_card __P((struct pcic_handle *));
void	pcic_detach_card __P((struct pcic_handle *, int));
void	pcic_deactivate_card __P((struct pcic_handle *));

void	pcic_chip_do_mem_map __P((struct pcic_handle *, int));
void	pcic_chip_do_io_map __P((struct pcic_handle *, int));

void	pcic_create_event_thread __P((void *));
void	pcic_event_thread __P((void *));

void	pcic_queue_event __P((struct pcic_handle *, int));

static void	pcic_wait_ready __P((struct pcic_handle *));

int
pcic_ident_ok(ident)
	int ident;
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
pcic_vendor(h)
	struct pcic_handle *h;
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
pcic_vendor_to_string(vendor)
	int vendor;
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

void
pcic_attach(device_t dev)
{
	struct pcic_softc *sc = (struct pcic_softc *)
	    device_get_softc(dev);
	int vendor, count, i, reg;

	/* now check for each controller/socket */

	/*
	 * this could be done with a loop, but it would violate the
	 * abstraction
	 */

	count = 0;

	DPRINTF(("pcic ident regs:"));

	sc->handle[0].sc = sc;
	sc->handle[0].sock = C0SA;
	if (pcic_ident_ok(reg = pcic_read(&sc->handle[0], PCIC_IDENT))) {
		sc->handle[0].flags = PCIC_FLAG_SOCKETP;
		count++;
	} else {
		sc->handle[0].flags = 0;
	}
	sc->handle[0].laststate = PCIC_LASTSTATE_EMPTY;

	DPRINTF((" 0x%02x", reg));

	sc->handle[1].sc = sc;
	sc->handle[1].sock = C0SB;
	if (pcic_ident_ok(reg = pcic_read(&sc->handle[1], PCIC_IDENT))) {
		sc->handle[1].flags = PCIC_FLAG_SOCKETP;
		count++;
	} else {
		sc->handle[1].flags = 0;
	}
	sc->handle[1].laststate = PCIC_LASTSTATE_EMPTY;

	DPRINTF((" 0x%02x", reg));

	/*
	 * The CL-PD6729 has only one controller and always returns 0
	 * if you try to read from the second one. Maybe pcic_ident_ok
	 * shouldn't accept 0?
	 */
	sc->handle[2].sc = sc;
	sc->handle[2].sock = C1SA;
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

		DPRINTF((" 0x%02x", reg));

		sc->handle[3].sc = sc;
		sc->handle[3].sock = C1SB;
		if (pcic_ident_ok(reg = pcic_read(&sc->handle[3],
						  PCIC_IDENT))) {
			sc->handle[3].flags = PCIC_FLAG_SOCKETP;
			count++;
		} else {
			sc->handle[3].flags = 0;
		}
		sc->handle[3].laststate = PCIC_LASTSTATE_EMPTY;

		DPRINTF((" 0x%02x\n", reg));
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
}

void
pcic_attach_sockets(sc)
	struct pcic_softc *sc;
{
	int i;

	for (i = 0; i < PCIC_NSLOTS; i++)
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			pcic_attach_socket(&sc->handle[i]);
}

void
pcic_attach_socket(h)
	struct pcic_handle *h;
{
	struct pccardbus_attach_args paa;

	/* initialize the rest of the handle */

	h->shutdown = 0;
	h->memalloc = 0;
	h->ioalloc = 0;
	h->ih_irq = 0;

	/* now, config one pccard device per socket */
	paa.paa_busname = "pccard";
	paa.pct = (pccard_chipset_tag_t) h->sc->pct;
	paa.pch = (pccard_chipset_handle_t) h;
	paa.iobase = h->sc->iobase;
	paa.iosize = h->sc->iosize;

#if XXX
	h->pccard = config_found_sm(&h->sc->dev, &paa, pcic_print,
	    pcic_submatch);
#endif

	/* if there's actually a pccard device attached, initialize the slot */

	if (h->pccard)
		pcic_init_socket(h);
}

void
pcic_create_event_thread(arg)
	void *arg;
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
	    "%s,%s", device_get_name(h->sc->dev), cs)) {
		device_printf(h->sc->dev,
		    "cannot create event thread for sock 0x%02x\n", h->sock);
		panic("pcic_create_event_thread");
	} else
		device_printf(h->sc->dev, 
		    "create event thread for sock 0x%02x\n", h->sock);

}

void
pcic_event_thread(arg)
	void *arg;
{
	struct pcic_handle *h = arg;
	struct pcic_event *pe;
	int s;

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
				
			DPRINTF(("%s: insertion event\n", h->sc->dev.dv_xname));
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

			DPRINTF(("%s: removal event\n", h->sc->dev.dv_xname));
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
	wakeup(h->sc);

	kthread_exit(0);
}

void
pcic_init_socket(h)
	struct pcic_handle *h;
{
	int reg;

	/*
	 * queue creation of a kernel thread to handle insert/removal events.
	 */
#ifdef DIAGNOSTIC
	if (h->event_thread != NULL)
		panic("pcic_attach_socket: event thread");
#endif
	pcic_create_event_thread(h);

	/* set up the card to interrupt on card detect */

	pcic_write(h, PCIC_CSC_INTR, (h->sc->irq << PCIC_CSC_INTR_IRQ_SHIFT) |
	    PCIC_CSC_INTR_CD_ENABLE);
	pcic_write(h, PCIC_INTR, 0);
	pcic_read(h, PCIC_CSC);

	/* unsleep the cirrus controller */

	if ((h->vendor == PCIC_VENDOR_CIRRUS_PD6710) ||
	    (h->vendor == PCIC_VENDOR_CIRRUS_PD672X)) {
		reg = pcic_read(h, PCIC_CIRRUS_MISC_CTL_2);
		if (reg & PCIC_CIRRUS_MISC_CTL_2_SUSPEND) {
			DPRINTF(("%s: socket %02x was suspended\n",
			    h->sc->dev.dv_xname, h->sock));
			reg &= ~PCIC_CIRRUS_MISC_CTL_2_SUSPEND;
			pcic_write(h, PCIC_CIRRUS_MISC_CTL_2, reg);
		}
	}
	/* if there's a card there, then attach it. */

	reg = pcic_read(h, PCIC_IF_STATUS);

	if ((reg & PCIC_IF_STATUS_CARDDETECT_MASK) ==
	    PCIC_IF_STATUS_CARDDETECT_PRESENT) {
		pcic_attach_card(h);
		h->laststate = PCIC_LASTSTATE_PRESENT;
	} else {
		h->laststate = PCIC_LASTSTATE_EMPTY;
	}
}

#if XXX
int
pcic_submatch(parent, cf, aux)
	struct device *parent;
	struct cfdata *cf;
	void *aux;
{

	struct pccardbus_attach_args *paa = aux;
	struct pcic_handle *h = (struct pcic_handle *) paa->pch;

	switch (h->sock) {
	case C0SA:
		if (cf->cf_loc[PCCARDBUSCF_CONTROLLER] !=
		    PCCARDBUSCF_CONTROLLER_DEFAULT &&
		    cf->cf_loc[PCCARDBUSCF_CONTROLLER] != 0)
			return 0;
		if (cf->cf_loc[PCCARDBUSCF_SOCKET] !=
		    PCCARDBUSCF_SOCKET_DEFAULT &&
		    cf->cf_loc[PCCARDBUSCF_SOCKET] != 0)
			return 0;

		break;
	case C0SB:
		if (cf->cf_loc[PCCARDBUSCF_CONTROLLER] !=
		    PCCARDBUSCF_CONTROLLER_DEFAULT &&
		    cf->cf_loc[PCCARDBUSCF_CONTROLLER] != 0)
			return 0;
		if (cf->cf_loc[PCCARDBUSCF_SOCKET] !=
		    PCCARDBUSCF_SOCKET_DEFAULT &&
		    cf->cf_loc[PCCARDBUSCF_SOCKET] != 1)
			return 0;

		break;
	case C1SA:
		if (cf->cf_loc[PCCARDBUSCF_CONTROLLER] !=
		    PCCARDBUSCF_CONTROLLER_DEFAULT &&
		    cf->cf_loc[PCCARDBUSCF_CONTROLLER] != 1)
			return 0;
		if (cf->cf_loc[PCCARDBUSCF_SOCKET] !=
		    PCCARDBUSCF_SOCKET_DEFAULT &&
		    cf->cf_loc[PCCARDBUSCF_SOCKET] != 0)
			return 0;

		break;
	case C1SB:
		if (cf->cf_loc[PCCARDBUSCF_CONTROLLER] !=
		    PCCARDBUSCF_CONTROLLER_DEFAULT &&
		    cf->cf_loc[PCCARDBUSCF_CONTROLLER] != 1)
			return 0;
		if (cf->cf_loc[PCCARDBUSCF_SOCKET] !=
		    PCCARDBUSCF_SOCKET_DEFAULT &&
		    cf->cf_loc[PCCARDBUSCF_SOCKET] != 1)
			return 0;

		break;
	default:
		panic("unknown pcic socket");
	}

	return ((*cf->cf_attach->ca_match)(parent, cf, aux));
}
#endif

#if XXX
int
pcic_print(arg, pnp)
	void *arg;
	const char *pnp;
{
	struct pccardbus_attach_args *paa = arg;
	struct pcic_handle *h = (struct pcic_handle *) paa->pch;

	/* Only "pccard"s can attach to "pcic"s... easy. */
	if (pnp)
		printf("pccard at %s", pnp);

	switch (h->sock) {
	case C0SA:
		printf(" controller 0 socket 0");
		break;
	case C0SB:
		printf(" controller 0 socket 1");
		break;
	case C1SA:
		printf(" controller 1 socket 0");
		break;
	case C1SB:
		printf(" controller 1 socket 1");
		break;
	default:
		panic("unknown pcic socket");
	}

	return (UNCONF);
}
#endif

int
pcic_intr(arg)
	void *arg;
{
	struct pcic_softc *sc = arg;
	int i, ret = 0;

	DPRINTF(("%s: intr\n", sc->dev.dv_xname));

	for (i = 0; i < PCIC_NSLOTS; i++)
		if (sc->handle[i].flags & PCIC_FLAG_SOCKETP)
			ret += pcic_intr_socket(&sc->handle[i]);

	return (ret ? 1 : 0);
}

int
pcic_intr_socket(h)
	struct pcic_handle *h;
{
	int cscreg;

	cscreg = pcic_read(h, PCIC_CSC);

	cscreg &= (PCIC_CSC_GPI |
		   PCIC_CSC_CD |
		   PCIC_CSC_READY |
		   PCIC_CSC_BATTWARN |
		   PCIC_CSC_BATTDEAD);

	if (cscreg & PCIC_CSC_GPI) {
		DPRINTF(("%s: %02x GPI\n", h->sc->dev.dv_xname, h->sock));
	}
	if (cscreg & PCIC_CSC_CD) {
		int statreg;

		statreg = pcic_read(h, PCIC_IF_STATUS);

		DPRINTF(("%s: %02x CD %x\n", h->sc->dev.dv_xname, h->sock,
		    statreg));

		if ((statreg & PCIC_IF_STATUS_CARDDETECT_MASK) ==
		    PCIC_IF_STATUS_CARDDETECT_PRESENT) {
			if (h->laststate != PCIC_LASTSTATE_PRESENT) {
				DPRINTF(("%s: enqueing INSERTION event\n",
						 h->sc->dev.dv_xname));
				pcic_queue_event(h, PCIC_EVENT_INSERTION);
			}
			h->laststate = PCIC_LASTSTATE_PRESENT;
		} else {
			if (h->laststate == PCIC_LASTSTATE_PRESENT) {
				/* Deactivate the card now. */
				DPRINTF(("%s: deactivating card\n",
						 h->sc->dev.dv_xname));
				pcic_deactivate_card(h);

				DPRINTF(("%s: enqueing REMOVAL event\n",
						 h->sc->dev.dv_xname));
				pcic_queue_event(h, PCIC_EVENT_REMOVAL);
			}
			h->laststate = ((statreg & PCIC_IF_STATUS_CARDDETECT_MASK) == 0)
				? PCIC_LASTSTATE_EMPTY : PCIC_LASTSTATE_HALF;
		}
	}
	if (cscreg & PCIC_CSC_READY) {
		DPRINTF(("%s: %02x READY\n", h->sc->dev.dv_xname, h->sock));
		/* shouldn't happen */
	}
	if (cscreg & PCIC_CSC_BATTWARN) {
		DPRINTF(("%s: %02x BATTWARN\n", h->sc->dev.dv_xname, h->sock));
	}
	if (cscreg & PCIC_CSC_BATTDEAD) {
		DPRINTF(("%s: %02x BATTDEAD\n", h->sc->dev.dv_xname, h->sock));
	}
	return (cscreg ? 1 : 0);
}

void
pcic_queue_event(h, event)
	struct pcic_handle *h;
	int event;
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

void
pcic_attach_card(h)
	struct pcic_handle *h;
{
        struct pccard_softc *psc = (void*)h->pccard;
	if (!(h->flags & PCIC_FLAG_CARDP)) {
#if XXX
		/* call the MI attach function */
		psc->sc_if.if_card_attach (psc);
#endif

		h->flags |= PCIC_FLAG_CARDP;
	} else {
		DPRINTF(("pcic_attach_card: already attached"));
	}
}

void
pcic_detach_card(h, flags)
	struct pcic_handle *h;
	int flags;		/* DETACH_* */
{
        struct pccard_softc *psc = (void*)h->pccard;
	if (h->flags & PCIC_FLAG_CARDP) {
		h->flags &= ~PCIC_FLAG_CARDP;

		/* call the MI detach function */
#if XXX
		psc->sc_if.if_card_detach (psc, flags);
#endif
	} else {
		DPRINTF(("pcic_detach_card: already detached"));
	}
}

void
pcic_deactivate_card(h)
	struct pcic_handle *h;
{
        struct pccard_softc *psc = (void*)h->pccard;
	/* call the MI deactivate function */
#if XXX
	psc->sc_if.if_card_deactivate (psc);
#endif

	/* power down the socket */
	pcic_write(h, PCIC_PWRCTL, 0);

	/* reset the socket */
	pcic_write(h, PCIC_INTR, 0);
}

int 
pcic_chip_mem_alloc(pch, size, pcmhp)
	pccard_chipset_handle_t pch;
	bus_size_t size;
	struct pccard_mem_handle *pcmhp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_space_handle_t memh;
	bus_addr_t addr;
	bus_size_t sizepg;
	int i, mask, mhandle;

	/* out of sc->memh, allocate as many pages as necessary */

	/* convert size to PCIC pages */
	sizepg = (size + (PCIC_MEM_ALIGN - 1)) / PCIC_MEM_ALIGN;
	if (sizepg > PCIC_MAX_MEM_PAGES)
		return (1);

	mask = (1 << sizepg) - 1;

	addr = 0;		/* XXX gcc -Wuninitialized */
	mhandle = 0;		/* XXX gcc -Wuninitialized */

	for (i = 0; i <= PCIC_MAX_MEM_PAGES - sizepg; i++) {
		if ((h->sc->subregionmask & (mask << i)) == (mask << i)) {
#if XXX
			if (bus_space_subregion(h->sc->memt, h->sc->memh,
			    i * PCIC_MEM_PAGESIZE,
			    sizepg * PCIC_MEM_PAGESIZE, &memh))
				return (1);
#endif
			mhandle = mask << i;
			addr = h->sc->membase + (i * PCIC_MEM_PAGESIZE);
			h->sc->subregionmask &= ~(mhandle);
			pcmhp->memt = h->sc->memt;
			pcmhp->memh = memh;
			pcmhp->addr = addr;
			pcmhp->size = size;
			pcmhp->mhandle = mhandle;
			pcmhp->realsize = sizepg * PCIC_MEM_PAGESIZE;
			return (0);
		}
	}

	return (1);
}

void 
pcic_chip_mem_free(pch, pcmhp)
	pccard_chipset_handle_t pch;
	struct pccard_mem_handle *pcmhp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;

	h->sc->subregionmask |= pcmhp->mhandle;
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

void 
pcic_chip_do_mem_map(h, win)
	struct pcic_handle *h;
	int win;
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

	delay(100);

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

int 
pcic_chip_mem_map(pch, kind, card_addr, size, pcmhp, offsetp, windowp)
	pccard_chipset_handle_t pch;
	int kind;
	bus_addr_t card_addr;
	bus_size_t size;
	struct pccard_mem_handle *pcmhp;
	bus_addr_t *offsetp;
	int *windowp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
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

	/* XXX this is pretty gross */

	if (h->sc->memt != pcmhp->memt)
		panic("pcic_chip_mem_map memt is bogus");

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

void 
pcic_chip_mem_unmap(pch, window)
	pccard_chipset_handle_t pch;
	int window;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	int reg;

	if (window >= (sizeof(mem_map_index) / sizeof(mem_map_index[0])))
		panic("pcic_chip_mem_unmap: window out of range");

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg &= ~mem_map_index[window].memenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	h->memalloc &= ~(1 << window);
}

int 
pcic_chip_io_alloc(pch, start, size, align, pcihp)
	pccard_chipset_handle_t pch;
	bus_addr_t start;
	bus_size_t size;
	bus_size_t align;
	struct pccard_io_handle *pcihp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_space_tag_t iot;
	bus_space_handle_t ioh;
	bus_addr_t ioaddr;
	int flags = 0;

	/*
	 * Allocate some arbitrary I/O space.
	 */

	iot = h->sc->iot;

	if (start) {
		ioaddr = start;
#if XXX
		if (bus_space_map(iot, start, size, 0, &ioh))
			return (1);
#endif
		DPRINTF(("pcic_chip_io_alloc map port %lx+%lx\n",
		    (u_long) ioaddr, (u_long) size));
	} else {
		flags |= PCCARD_IO_ALLOCATED;
#if XXX
		if (bus_space_alloc(iot, h->sc->iobase,
		    h->sc->iobase + h->sc->iosize, size, align, 0, 0,
		    &ioaddr, &ioh))
			return (1);
#endif
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

void 
pcic_chip_io_free(pch, pcihp)
	pccard_chipset_handle_t pch;
	struct pccard_io_handle *pcihp;
{
	bus_space_tag_t iot = pcihp->iot;
	bus_space_handle_t ioh = pcihp->ioh;
	bus_size_t size = pcihp->size;

#if XXX
	if (pcihp->flags & PCCARD_IO_ALLOCATED)
		bus_space_free(iot, ioh, size);
	else
		bus_space_unmap(iot, ioh, size);
#endif
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

void 
pcic_chip_do_io_map(h, win)
	struct pcic_handle *h;
	int win;
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

int 
pcic_chip_io_map(pch, width, offset, size, pcihp, windowp)
	pccard_chipset_handle_t pch;
	int width;
	bus_addr_t offset;
	bus_size_t size;
	struct pccard_io_handle *pcihp;
	int *windowp;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	bus_addr_t ioaddr = pcihp->addr + offset;
	int i, win;
#ifdef PCICDEBUG
	static char *width_names[] = { "auto", "io8", "io16" };
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

	/* XXX this is pretty gross */

	if (h->sc->iot != pcihp->iot)
		panic("pcic_chip_io_map iot is bogus");

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

void 
pcic_chip_io_unmap(pch, window)
	pccard_chipset_handle_t pch;
	int window;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
	int reg;

	if (window >= (sizeof(io_map_index) / sizeof(io_map_index[0])))
		panic("pcic_chip_io_unmap: window out of range");

	reg = pcic_read(h, PCIC_ADDRWIN_ENABLE);
	reg &= ~io_map_index[window].ioenable;
	pcic_write(h, PCIC_ADDRWIN_ENABLE, reg);

	h->ioalloc &= ~(1 << window);
}

static void
pcic_wait_ready(h)
	struct pcic_handle *h;
{
	int i;

	for (i = 0; i < 10000; i++) {
		if (pcic_read(h, PCIC_IF_STATUS) & PCIC_IF_STATUS_READY)
			return;
		delay(500);
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

void
pcic_chip_socket_enable(pch)
	pccard_chipset_handle_t pch;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;
        struct pccard_softc *psc = (void*)h->pccard;
	int cardtype, reg, win;

	/* this bit is mostly stolen from pcic_attach_card */

	/* power down the socket to reset it, clear the card reset pin */

	pcic_write(h, PCIC_PWRCTL, 0);

	/* 
	 * wait 300ms until power fails (Tpf).  Then, wait 100ms since
	 * we are changing Vcc (Toff).
	 */
	delay((300 + 100) * 1000);

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
	delay((100 + 20 + 300) * 1000);

	pcic_write(h, PCIC_PWRCTL, PCIC_PWRCTL_DISABLE_RESETDRV | PCIC_PWRCTL_OE
			   | PCIC_PWRCTL_PWR_ENABLE);
	pcic_write(h, PCIC_INTR, 0);

	/*
	 * hold RESET at least 10us.
	 */
	delay(10);

	/* clear the reset flag */

	pcic_write(h, PCIC_INTR, PCIC_INTR_RESET);

	/* wait 20ms as per pc card standard (r2.01) section 4.3.6 */

	delay(20000);

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

#if XXX
	cardtype = psc->sc_if.if_card_gettype (psc);
#endif

	reg = pcic_read(h, PCIC_INTR);
	reg &= ~(PCIC_INTR_CARDTYPE_MASK | PCIC_INTR_IRQ_MASK | PCIC_INTR_ENABLE);
	reg |= ((cardtype == PCCARD_IFTYPE_IO) ?
		PCIC_INTR_CARDTYPE_IO :
		PCIC_INTR_CARDTYPE_MEM);
	reg |= h->ih_irq;
	pcic_write(h, PCIC_INTR, reg);

	DPRINTF(("%s: pcic_chip_socket_enable %02x cardtype %s %02x\n",
	    h->sc->dev.dv_xname, h->sock,
	    ((cardtype == PCCARD_IFTYPE_IO) ? "io" : "mem"), reg));

	/* reinstall all the memory and io mappings */

	for (win = 0; win < PCIC_MEM_WINS; win++)
		if (h->memalloc & (1 << win))
			pcic_chip_do_mem_map(h, win);

	for (win = 0; win < PCIC_IO_WINS; win++)
		if (h->ioalloc & (1 << win))
			pcic_chip_do_io_map(h, win);
}

void
pcic_chip_socket_disable(pch)
	pccard_chipset_handle_t pch;
{
	struct pcic_handle *h = (struct pcic_handle *) pch;

	DPRINTF(("pcic_chip_socket_disable\n"));

	/* power down the socket */

	pcic_write(h, PCIC_PWRCTL, 0);

	/*
	 * wait 300ms until power fails (Tpf).
	 */
	delay(300 * 1000);
}
