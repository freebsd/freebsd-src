/*	$NetBSD: pcmcia.c,v 1.13 1998/12/24 04:51:59 marc Exp $	*/
/* $FreeBSD: src/sys/dev/pccard/pccard.c,v 1.8.2.1 2000/05/23 03:56:59 imp Exp $ */

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

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>

#include "power_if.h"
#include "card_if.h"

#define PCCARDDEBUG

#ifdef PCCARDDEBUG
int	pccard_debug = 1;
#define	DPRINTF(arg) if (pccard_debug) printf arg
#define	DEVPRINTF(arg) if (pccard_debug) device_printf arg
#else
#define	DPRINTF(arg)
#define	DEVPRINTF(arg)
#endif

#ifdef PCCARDVERBOSE
int	pccard_verbose = 1;
#else
int	pccard_verbose = 0;
#endif

int	pccard_print(void *, const char *);

int
pccard_ccr_read(pf, ccr)
	struct pccard_function *pf;
	int ccr;
{
	return (bus_space_read_1(pf->pf_ccrt, pf->pf_ccrh,
	    pf->pf_ccr_offset + ccr));
}

void
pccard_ccr_write(pf, ccr, val)
	struct pccard_function *pf;
	int ccr;
	int val;
{

	if ((pf->ccr_mask) & (1 << (ccr / 2))) {
		bus_space_write_1(pf->pf_ccrt, pf->pf_ccrh,
		    pf->pf_ccr_offset + ccr, val);
	}
}

static int
pccard_attach_card(device_t dev)
{
	struct pccard_softc *sc = (struct pccard_softc *) 
	    device_get_softc(dev);
	struct pccard_function *pf;
	int attached;

	DEVPRINTF((dev, "pccard_card_attach\n"));
	/*
	 * this is here so that when socket_enable calls gettype, trt happens
	 */
	STAILQ_INIT(&sc->card.pf_head);

	DEVPRINTF((dev, "chip_socket_enable\n"));
	POWER_ENABLE_SOCKET(device_get_parent(dev), dev);

	DEVPRINTF((dev, "read_cis\n"));
	pccard_read_cis(sc);

	DEVPRINTF((dev, "chip_socket_disable\n"));
	POWER_DISABLE_SOCKET(device_get_parent(dev), dev);

	DEVPRINTF((dev, "check_cis_quirks\n"));
	pccard_check_cis_quirks(dev);

	/*
	 * bail now if the card has no functions, or if there was an error in
	 * the cis.
	 */

	if (sc->card.error)
		return (1);
	if (STAILQ_EMPTY(&sc->card.pf_head))
		return (1);

	if (pccard_verbose)
		pccard_print_cis(dev);

	attached = 0;

	DEVPRINTF((dev, "functions scanning\n"));
	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_EMPTY(&pf->cfe_head))
			continue;

		pf->sc = sc;
		pf->cfe = NULL;
		pf->ih_fct = NULL;
		pf->ih_arg = NULL;
	}

	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_EMPTY(&pf->cfe_head))
			continue;

#if XXX
		if (attach_child()) {
			attached++;

			DEVPRINTF((sc->dev, "function %d CCR at %d "
			     "offset %lx: %x %x %x %x, %x %x %x %x, %x\n",
			     pf->number, pf->pf_ccr_window, pf->pf_ccr_offset,
			     pccard_ccr_read(pf, 0x00),
			pccard_ccr_read(pf, 0x02), pccard_ccr_read(pf, 0x04),
			pccard_ccr_read(pf, 0x06), pccard_ccr_read(pf, 0x0A),
			pccard_ccr_read(pf, 0x0C), pccard_ccr_read(pf, 0x0E),
			pccard_ccr_read(pf, 0x10), pccard_ccr_read(pf, 0x12)));
		}
#endif
	}

	return (attached ? 0 : 1);
}

static int
pccard_detach_card(device_t dev, int flags)
{
	struct pccard_softc *sc = (struct pccard_softc *) 
	    device_get_softc(dev);
	struct pccard_function *pf;
#if XXX
	int error;
#endif

	/*
	 * We are running on either the PCCARD socket's event thread
	 * or in user context detaching a device by user request.
	 */
	STAILQ_FOREACH(pf, &sc->card.pf_head, pf_list) {
		if (STAILQ_FIRST(&pf->cfe_head) == NULL)
			continue;
#if XXX
		DEVPRINTF((sc->dev, "detaching %s (function %d)\n",
		    device_get_name(pf->child), pf->number));
		if ((error = config_detach(pf->child, flags)) != 0) {
			device_printf(sc->dev, 
			    "error %d detaching %s (function %d)\n",
			    error, device_get_name(pf->child), pf->number);
		} else
			pf->child = NULL;
#endif
	}
	return 0;
}

static int 
pccard_card_gettype(device_t dev, int *type)
{
	struct pccard_softc *sc = (struct pccard_softc *)
	    device_get_softc(dev);
	struct pccard_function *pf;

	/*
	 * set the iftype to memory if this card has no functions (not yet
	 * probed), or only one function, and that is not initialized yet or
	 * that is memory.
	 */
	pf = STAILQ_FIRST(&sc->card.pf_head);
	if (pf == NULL ||
	    (STAILQ_NEXT(pf, pf_list) == NULL &&
	    (pf->cfe == NULL || pf->cfe->iftype == PCCARD_IFTYPE_MEMORY)))
		*type = PCCARD_IFTYPE_MEMORY;
	else
		*type = PCCARD_IFTYPE_IO;
	return 0;
}

/*
 * Initialize a PCCARD function.  May be called as long as the function is
 * disabled.
 */
void
pccard_function_init(struct pccard_function *pf, 
    struct pccard_config_entry *cfe)
{
	if (pf->pf_flags & PFF_ENABLED)
		panic("pccard_function_init: function is enabled");

	/* Remember which configuration entry we are using. */
	pf->cfe = cfe;
}

/* Enable a PCCARD function */
int
pccard_function_enable(struct pccard_function *pf)
{
	struct pccard_function *tmp;
	int reg;
	device_t dev = pf->sc->dev;

	if (pf->cfe == NULL)
		panic("pccard_function_enable: function not initialized");

	/*
	 * Increase the reference count on the socket, enabling power, if
	 * necessary.
	 */
	if (pf->sc->sc_enabled_count++ == 0)
		POWER_ENABLE_SOCKET(device_get_parent(dev), dev);
	DEVPRINTF((dev, "++enabled_count = %d\n", pf->sc->sc_enabled_count));

	if (pf->pf_flags & PFF_ENABLED) {
		/*
		 * Don't do anything if we're already enabled.
		 */
		return (0);
	}

	/*
	 * it's possible for different functions' CCRs to be in the same
	 * underlying page.  Check for that.
	 */

	STAILQ_FOREACH(tmp, &pf->sc->card.pf_head, pf_list) {
		if ((tmp->pf_flags & PFF_ENABLED) &&
		    (pf->ccr_base >= (tmp->ccr_base - tmp->pf_ccr_offset)) &&
		    ((pf->ccr_base + PCCARD_CCR_SIZE) <=
		     (tmp->ccr_base - tmp->pf_ccr_offset +
		      tmp->pf_ccr_realsize))) {
			pf->pf_ccrt = tmp->pf_ccrt;
			pf->pf_ccrh = tmp->pf_ccrh;
			pf->pf_ccr_realsize = tmp->pf_ccr_realsize;

			/*
			 * pf->pf_ccr_offset = (tmp->pf_ccr_offset -
			 * tmp->ccr_base) + pf->ccr_base;
			 */
			pf->pf_ccr_offset =
			    (tmp->pf_ccr_offset + pf->ccr_base) -
			    tmp->ccr_base;
			pf->pf_ccr_window = tmp->pf_ccr_window;
			break;
		}
	}

	if (tmp == NULL) {
		pf->ccr_rid = 0;
		pf->ccr_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
		    &pf->ccr_rid, pf->ccr_base, pf->ccr_base + PCCARD_CCR_SIZE,
		    PCCARD_CCR_SIZE, RF_ACTIVE);
		/* XXX SET MEM_ATTR */
		if (!pf->ccr_res)
			goto bad;
		pf->pf_ccrt = rman_get_bustag(pf->ccr_res);
		pf->pf_ccrh = rman_get_bushandle(pf->ccr_res);
		pf->pf_ccr_offset = rman_get_start(pf->ccr_res);
		pf->pf_ccr_realsize = 1;
	}

	reg = (pf->cfe->number & PCCARD_CCR_OPTION_CFINDEX);
	reg |= PCCARD_CCR_OPTION_LEVIREQ;
	if (pccard_mfc(pf->sc)) {
		reg |= (PCCARD_CCR_OPTION_FUNC_ENABLE |
			PCCARD_CCR_OPTION_ADDR_DECODE);
		if (pf->ih_fct)
			reg |= PCCARD_CCR_OPTION_IREQ_ENABLE;

	}
	pccard_ccr_write(pf, PCCARD_CCR_OPTION, reg);

	reg = 0;

	if ((pf->cfe->flags & PCCARD_CFE_IO16) == 0)
		reg |= PCCARD_CCR_STATUS_IOIS8;
	if (pf->cfe->flags & PCCARD_CFE_AUDIO)
		reg |= PCCARD_CCR_STATUS_AUDIO;
	pccard_ccr_write(pf, PCCARD_CCR_STATUS, reg);

	pccard_ccr_write(pf, PCCARD_CCR_SOCKETCOPY, 0);

	if (pccard_mfc(pf->sc)) {
		long tmp, iosize;

		tmp = pf->pf_mfc_iomax - pf->pf_mfc_iobase;
		/* round up to nearest (2^n)-1 */
		for (iosize = 1; iosize < tmp; iosize <<= 1)
			;
		iosize--;

		pccard_ccr_write(pf, PCCARD_CCR_IOBASE0,
				 pf->pf_mfc_iobase & 0xff);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE1,
				 (pf->pf_mfc_iobase >> 8) & 0xff);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE2, 0);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE3, 0);

		pccard_ccr_write(pf, PCCARD_CCR_IOSIZE, iosize);
	}

#ifdef PCCARDDEBUG
	if (pccard_debug) {
		STAILQ_FOREACH(tmp, &pf->sc->card.pf_head, pf_list) {
			device_printf(tmp->sc->dev, 
			    "function %d CCR at %d offset %x: "
			    "%x %x %x %x, %x %x %x %x, %x\n",
			    tmp->number, tmp->pf_ccr_window, 
			    tmp->pf_ccr_offset,
			    pccard_ccr_read(tmp, 0x00),
			    pccard_ccr_read(tmp, 0x02),
			    pccard_ccr_read(tmp, 0x04),
			    pccard_ccr_read(tmp, 0x06),
			    pccard_ccr_read(tmp, 0x0A),
			    pccard_ccr_read(tmp, 0x0C), 
			    pccard_ccr_read(tmp, 0x0E),
			    pccard_ccr_read(tmp, 0x10),
			    pccard_ccr_read(tmp, 0x12));
		}
	}
#endif
	pf->pf_flags |= PFF_ENABLED;
	return (0);

 bad:
	/*
	 * Decrement the reference count, and power down the socket, if
	 * necessary.
	 */
	if (--pf->sc->sc_enabled_count == 0)
		POWER_DISABLE_SOCKET(device_get_parent(dev), dev);
	DEVPRINTF((dev, "--enabled_count = %d\n", pf->sc->sc_enabled_count));

	return (1);
}

/* Disable PCCARD function. */
void
pccard_function_disable(struct pccard_function *pf)
{
	struct pccard_function *tmp;
	device_t dev = pf->sc->dev;

	if (pf->cfe == NULL)
		panic("pccard_function_enable: function not initialized");

	if ((pf->pf_flags & PFF_ENABLED) == 0) {
		/*
		 * Don't do anything if we're already disabled.
		 */
		return;
	}

	/*
	 * it's possible for different functions' CCRs to be in the same
	 * underlying page.  Check for that.  Note we mark us as disabled
	 * first to avoid matching ourself.
	 */

	pf->pf_flags &= ~PFF_ENABLED;
	STAILQ_FOREACH(tmp, &pf->sc->card.pf_head, pf_list) {
		if ((tmp->pf_flags & PFF_ENABLED) &&
		    (pf->ccr_base >= (tmp->ccr_base - tmp->pf_ccr_offset)) &&
		    ((pf->ccr_base + PCCARD_CCR_SIZE) <=
		(tmp->ccr_base - tmp->pf_ccr_offset + tmp->pf_ccr_realsize)))
			break;
	}

	/* Not used by anyone else; unmap the CCR. */
	if (tmp == NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, pf->ccr_rid, 
		    pf->ccr_res);
		pf->ccr_res = NULL;
	}

	/*
	 * Decrement the reference count, and power down the socket, if
	 * necessary.
	 */
	if (--pf->sc->sc_enabled_count == 0)
		POWER_DISABLE_SOCKET(device_get_parent(dev), dev);
	DEVPRINTF((dev, "--enabled_count = %d\n", pf->sc->sc_enabled_count));
}

#if 0
/* XXX These functions are needed, but not like this XXX */
int
pccard_io_map(struct pccard_function *pf, int width, bus_addr_t offset,
    bus_size_t size, struct pccard_io_handle *pcihp, int *windowp)
{
	int reg;

	if (pccard_chip_io_map(pf->sc->pct, pf->sc->pch,
	    width, offset, size, pcihp, windowp))
		return (1);

	/*
	 * XXX in the multifunction multi-iospace-per-function case, this
	 * needs to cooperate with io_alloc to make sure that the spaces
	 * don't overlap, and that the ccr's are set correctly
	 */

	if (pccard_mfc(pf->sc)) {
		long tmp, iosize;

		if (pf->pf_mfc_iomax == 0) {
			pf->pf_mfc_iobase = pcihp->addr + offset;
			pf->pf_mfc_iomax = pf->pf_mfc_iobase + size;
		} else {
			/* this makes the assumption that nothing overlaps */
			if (pf->pf_mfc_iobase > pcihp->addr + offset)
				pf->pf_mfc_iobase = pcihp->addr + offset;
			if (pf->pf_mfc_iomax < pcihp->addr + offset + size)
				pf->pf_mfc_iomax = pcihp->addr + offset + size;
		}

		tmp = pf->pf_mfc_iomax - pf->pf_mfc_iobase;
		/* round up to nearest (2^n)-1 */
		for (iosize = 1; iosize >= tmp; iosize <<= 1)
			;
		iosize--;

		pccard_ccr_write(pf, PCCARD_CCR_IOBASE0, 
		    pf->pf_mfc_iobase & 0xff);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE1,
		    (pf->pf_mfc_iobase >> 8) & 0xff);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE2, 0);
		pccard_ccr_write(pf, PCCARD_CCR_IOBASE3, 0);

		pccard_ccr_write(pf, PCCARD_CCR_IOSIZE, iosize);

		reg = pccard_ccr_read(pf, PCCARD_CCR_OPTION);
		reg |= PCCARD_CCR_OPTION_ADDR_DECODE;
		pccard_ccr_write(pf, PCCARD_CCR_OPTION, reg);
	}
	return (0);
}

void
pccard_io_unmap(struct pccard_function *pf, int window)
{

	pccard_chip_io_unmap(pf->sc->pct, pf->sc->pch, window);

	/* XXX Anything for multi-function cards? */
}
#endif

#define PCCARD_NPORT	2
#define PCCARD_NMEM	5
#define PCCARD_NIRQ	1
#define PCCARD_NDRQ	0

static int
pccard_add_children(device_t dev, int busno)
{
	/* Call parent to scan for any current children */
	return 0;
}

static int
pccard_probe(device_t dev)
{
	device_set_desc(dev, "PC Card bus -- newconfig version");
	return pccard_add_children(dev, device_get_unit(dev));
}

static int
pccard_attach(device_t dev)
{
	struct pccard_softc *sc;
	
	sc = (struct pccard_softc *) device_get_softc(dev);
	sc->dev = dev;

	return bus_generic_attach(dev);
}

static void
pccard_print_resources(struct resource_list *rl, const char *name, int type,
    int count, const char *format)
{
	struct resource_list_entry *rle;
	int printed;
	int i;

	printed = 0;
	for (i = 0; i < count; i++) {
		rle = resource_list_find(rl, type, i);
		if (rle) {
			if (printed == 0)
				printf(" %s ", name);
			else if (printed > 0)
				printf(",");
			printed++;
			printf(format, rle->start);
			if (rle->count > 1) {
				printf("-");
				printf(format, rle->start + rle->count - 1);
			}
		} else if (i > 3) {
			/* check the first few regardless */
			break;
		}
	}
}

static int
pccard_print_child(device_t dev, device_t child)
{
	struct pccard_ivar *devi = (struct pccard_ivar *) device_get_ivars(child);
	struct resource_list *rl = &devi->resources;
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf(" at");

	if (devi) {
		pccard_print_resources(rl, "port", SYS_RES_IOPORT,
		    PCCARD_NPORT, "%#lx");
		pccard_print_resources(rl, "iomem", SYS_RES_MEMORY,
		    PCCARD_NMEM, "%#lx");
		pccard_print_resources(rl, "irq", SYS_RES_IRQ, PCCARD_NIRQ,
		    "%ld");
		pccard_print_resources(rl, "drq", SYS_RES_DRQ, PCCARD_NDRQ, 
		    "%ld");
		retval += printf(" slot %d", devi->slotnum);
	}

	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static int
pccard_set_resource(device_t dev, device_t child, int type, int rid,
		 u_long start, u_long count)
{
	struct pccard_ivar *devi = (struct pccard_ivar *) device_get_ivars(child);
	struct resource_list *rl = &devi->resources;

	if (type != SYS_RES_IOPORT && type != SYS_RES_MEMORY
	    && type != SYS_RES_IRQ && type != SYS_RES_DRQ)
		return EINVAL;
	if (rid < 0)
		return EINVAL;
	if (type == SYS_RES_IOPORT && rid >= PCCARD_NPORT)
		return EINVAL;
	if (type == SYS_RES_MEMORY && rid >= PCCARD_NMEM)
		return EINVAL;
	if (type == SYS_RES_IRQ && rid >= PCCARD_NIRQ)
		return EINVAL;
	if (type == SYS_RES_DRQ && rid >= PCCARD_NDRQ)
		return EINVAL;

	resource_list_add(rl, type, rid, start, start + count - 1, count);

	return 0;
}

static int
pccard_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct pccard_ivar *devi = (struct pccard_ivar *) device_get_ivars(child);
	struct resource_list *rl = &devi->resources;
	struct resource_list_entry *rle;

	rle = resource_list_find(rl, type, rid);
	if (!rle)
		return ENOENT;
	
	if (startp)
		*startp = rle->start;
	if (countp)
		*countp = rle->count;

	return 0;
}

static void
pccard_delete_resource(device_t dev, device_t child, int type, int rid)
{
	struct pccard_ivar *devi = (struct pccard_ivar *) device_get_ivars(child);
	struct resource_list *rl = &devi->resources;
	resource_list_delete(rl, type, rid);
}

static int
pccard_set_res_flags(device_t dev, device_t child, int type, int rid,
    u_int32_t flags)
{
	return CARD_SET_RES_FLAGS(device_get_parent(dev), child, type,
	    rid, flags);
}

static int
pccard_set_memory_offset(device_t dev, device_t child, int rid,
     u_int32_t offset)
{
	return CARD_SET_MEMORY_OFFSET(device_get_parent(dev), child, rid,
	    offset);
}

static device_method_t pccard_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pccard_probe),
	DEVMETHOD(device_attach,	pccard_attach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	pccard_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	DEVMETHOD(bus_alloc_resource,	bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_set_resource,	pccard_set_resource),
	DEVMETHOD(bus_get_resource,	pccard_get_resource),
	DEVMETHOD(bus_delete_resource,	pccard_delete_resource),

	/* Card Interface */
	DEVMETHOD(card_set_res_flags,	pccard_set_res_flags),
	DEVMETHOD(card_set_memory_offset, pccard_set_memory_offset),
	DEVMETHOD(card_get_type,	pccard_card_gettype),
	DEVMETHOD(card_attach_card,	pccard_attach_card),
	DEVMETHOD(card_detach_card,	pccard_detach_card),

	{ 0, 0 }
};

static driver_t pccard_driver = {
	"pccard",
	pccard_methods,
	1,			/* no softc */
};

devclass_t	pccard_devclass;

DRIVER_MODULE(pccard, pcic, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, pc98pcic, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, pccbb, pccard_driver, pccard_devclass, 0, 0);
DRIVER_MODULE(pccard, tcic, pccard_driver, pccard_devclass, 0, 0);
