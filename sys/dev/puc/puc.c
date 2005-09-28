/*	$NetBSD: puc.c,v 1.7 2000/07/29 17:43:38 jlam Exp $	*/

/*-
 * Copyright (c) 2002 JF Hay.  All rights reserved.
 * Copyright (c) 2000 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1996, 1998, 1999
 *	Christopher G. Demetriou.  All rights reserved.
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
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * PCI "universal" communication card device driver, glues com, lpt,
 * and similar ports to PCI via bridge chip often much larger than
 * the devices being glued.
 *
 * Author: Christopher G. Demetriou, May 14, 1998 (derived from NetBSD
 * sys/dev/pci/pciide.c, revision 1.6).
 *
 * These devices could be (and some times are) described as
 * communications/{serial,parallel}, etc. devices with known
 * programming interfaces, but those programming interfaces (in
 * particular the BAR assignments for devices, etc.) in fact are not
 * particularly well defined.
 *
 * After I/we have seen more of these devices, it may be possible
 * to generalize some of these bits.  In particular, devices which
 * describe themselves as communications/serial/16[45]50, and
 * communications/parallel/??? might be attached via direct
 * 'com' and 'lpt' attachments to pci.
 */

#include "opt_puc.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#define PUC_ENTRAILS	1
#include <dev/puc/pucvar.h>

struct puc_device {
	struct resource_list resources;
	int	port;
	int	regshft;
	u_int	serialfreq;
	u_int	subtype;
};

static void puc_intr(void *arg);

static int puc_find_free_unit(char *);
#ifdef PUC_DEBUG
static void puc_print_resource_list(struct resource_list *);
#endif

devclass_t puc_devclass;

static int
puc_port_bar_index(struct puc_softc *sc, int bar)
{
	int i;

	for (i = 0; i < PUC_MAX_BAR; i += 1) {
		if (!sc->sc_bar_mappings[i].used)
			break;
		if (sc->sc_bar_mappings[i].bar == bar)
			return (i);
	}
	if (i == PUC_MAX_BAR) {
		printf("%s: out of bars!\n", __func__);
		return (-1);
	}
	sc->sc_bar_mappings[i].bar = bar;
	sc->sc_bar_mappings[i].used = 1;
	return (i);
}

static int
puc_probe_ilr(struct puc_softc *sc, struct resource *res)
{
	u_char t1, t2;
	int i;

	switch (sc->sc_desc.ilr_type) {
	case PUC_ILR_TYPE_DIGI:
		sc->ilr_st = rman_get_bustag(res);
		sc->ilr_sh = rman_get_bushandle(res);
		for (i = 0; i < 2 && sc->sc_desc.ilr_offset[i] != 0; i++) {
			t1 = bus_space_read_1(sc->ilr_st, sc->ilr_sh,
			    sc->sc_desc.ilr_offset[i]);
			t1 = ~t1;
			bus_space_write_1(sc->ilr_st, sc->ilr_sh,
			    sc->sc_desc.ilr_offset[i], t1);
			t2 = bus_space_read_1(sc->ilr_st, sc->ilr_sh,
			    sc->sc_desc.ilr_offset[i]);
			if (t2 == t1)
				return (0);
		}
		return (1);

	default:
		break;
	}
	return (0);
}

int
puc_attach(device_t dev, const struct puc_device_description *desc)
{
	char *typestr;
	int bidx, childunit, i, irq_setup, ressz, rid, type;
	struct puc_softc *sc;
	struct puc_device *pdev;
	struct resource *res;
	struct resource_list_entry *rle;
	bus_space_handle_t bh;

	if (desc == NULL)
		return (ENXIO);

	sc = (struct puc_softc *)device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->sc_desc = *desc;

#ifdef PUC_DEBUG
	bootverbose = 1;

	printf("puc: name: %s\n", sc->sc_desc.name);
#endif
	rid = 0;
	res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE | RF_SHAREABLE);
	if (!res)
		return (ENXIO);

	sc->irqres = res;
	sc->irqrid = rid;
#ifdef PUC_FASTINTR
	irq_setup = BUS_SETUP_INTR(device_get_parent(dev), dev, res,
	    INTR_TYPE_TTY | INTR_FAST, puc_intr, sc, &sc->intr_cookie);
	if (irq_setup == 0)
		sc->fastintr = INTR_FAST;
	else
		irq_setup = BUS_SETUP_INTR(device_get_parent(dev), dev, res,
		    INTR_TYPE_TTY, puc_intr, sc, &sc->intr_cookie);
#else
	irq_setup = BUS_SETUP_INTR(device_get_parent(dev), dev, res,
	    INTR_TYPE_TTY, puc_intr, sc, &sc->intr_cookie);
#endif
	if (irq_setup != 0)
		return (ENXIO);

	rid = 0;
	for (i = 0; PUC_PORT_VALID(sc->sc_desc, i); i++) {
		if (i > 0 && rid == sc->sc_desc.ports[i].bar)
			sc->barmuxed = 1;
		rid = sc->sc_desc.ports[i].bar;
		bidx = puc_port_bar_index(sc, rid);

		if (bidx < 0 || sc->sc_bar_mappings[bidx].res != NULL)
			continue;

		type = (sc->sc_desc.ports[i].flags & PUC_FLAGS_MEMORY)
		    ? SYS_RES_MEMORY : SYS_RES_IOPORT;

		res = bus_alloc_resource_any(dev, type, &rid,
		    RF_ACTIVE);
		if (res == NULL &&
		    sc->sc_desc.ports[i].flags & PUC_FLAGS_ALTRES) {
			type = (type == SYS_RES_IOPORT)
			    ? SYS_RES_MEMORY : SYS_RES_IOPORT;
			res = bus_alloc_resource_any(dev, type, &rid,
			    RF_ACTIVE);
		}
		if (res == NULL) {
			device_printf(dev, "could not get resource\n");
			continue;
		}
		sc->sc_bar_mappings[bidx].type = type;
		sc->sc_bar_mappings[bidx].res = res;

		if (sc->sc_desc.ilr_type != PUC_ILR_TYPE_NONE) {
			sc->ilr_enabled = puc_probe_ilr(sc, res);
			if (sc->ilr_enabled)
				device_printf(dev, "ILR enabled\n");
			else
				device_printf(dev, "ILR disabled\n");
		}
#ifdef PUC_DEBUG
		printf("%s rid %d bst %lx, start %lx, end %lx\n",
		    (type == SYS_RES_MEMORY) ? "memory" : "port", rid,
		    (u_long)rman_get_bustag(res), (u_long)rman_get_start(res),
		    (u_long)rman_get_end(res));
#endif
	}

	if (desc->init != NULL) {
		i = desc->init(sc);
		if (i != 0)
			return (i);
	}

	for (i = 0; PUC_PORT_VALID(sc->sc_desc, i); i++) {
		rid = sc->sc_desc.ports[i].bar;
		bidx = puc_port_bar_index(sc, rid);
		if (bidx < 0 || sc->sc_bar_mappings[bidx].res == NULL)
			continue;

		switch (sc->sc_desc.ports[i].type & ~PUC_PORT_SUBTYPE_MASK) {
		case PUC_PORT_TYPE_COM:
			typestr = "sio";
			break;
		case PUC_PORT_TYPE_LPT:
			typestr = "ppc";
			break;
		case PUC_PORT_TYPE_UART:
			typestr = "uart";
			break;
		default:
			continue;
		}
		switch (sc->sc_desc.ports[i].type & PUC_PORT_SUBTYPE_MASK) {
		case PUC_PORT_UART_SAB82532:
			ressz = 64;
			break;
		case PUC_PORT_UART_Z8530:
			ressz = 2;
			break;
		default:
			ressz = 8;
			break;
		}
		pdev = malloc(sizeof(struct puc_device), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
		if (!pdev)
			continue;
		resource_list_init(&pdev->resources);

		/* First fake up an IRQ resource. */
		resource_list_add(&pdev->resources, SYS_RES_IRQ, 0,
		    rman_get_start(sc->irqres), rman_get_end(sc->irqres),
		    rman_get_end(sc->irqres) - rman_get_start(sc->irqres) + 1);
		rle = resource_list_find(&pdev->resources, SYS_RES_IRQ, 0);
		rle->res = sc->irqres;

		/* Now fake an IOPORT or MEMORY resource */
		res = sc->sc_bar_mappings[bidx].res;
		type = sc->sc_bar_mappings[bidx].type;
		resource_list_add(&pdev->resources, type, 0,
		    rman_get_start(res) + sc->sc_desc.ports[i].offset,
		    rman_get_start(res) + sc->sc_desc.ports[i].offset
		    + ressz - 1, ressz);
		rle = resource_list_find(&pdev->resources, type, 0);

		if (sc->barmuxed == 0) {
			rle->res = sc->sc_bar_mappings[bidx].res;
		} else {
			rle->res = rman_secret_puc_alloc_resource(M_WAITOK);
			if (rle->res == NULL) {
				free(pdev, M_DEVBUF);
				return (ENOMEM);
			}

			rman_set_start(rle->res, rman_get_start(res) +
			    sc->sc_desc.ports[i].offset);
			rman_set_end(rle->res, rman_get_start(rle->res) +
			    ressz - 1);
			rman_set_bustag(rle->res, rman_get_bustag(res));
			bus_space_subregion(rman_get_bustag(rle->res),
			    rman_get_bushandle(res),
			    sc->sc_desc.ports[i].offset, ressz,
			    &bh);
			rman_set_bushandle(rle->res, bh);
		}

		pdev->port = i + 1;
		pdev->serialfreq = sc->sc_desc.ports[i].serialfreq;
		pdev->subtype = sc->sc_desc.ports[i].type &
		    PUC_PORT_SUBTYPE_MASK;
		pdev->regshft = sc->sc_desc.ports[i].regshft;

		childunit = puc_find_free_unit(typestr);
		if (childunit < 0 && strcmp(typestr, "uart") != 0) {
			typestr = "uart";
			childunit = puc_find_free_unit(typestr);
		}
		sc->sc_ports[i].dev = device_add_child(dev, typestr,
		    childunit);
		if (sc->sc_ports[i].dev == NULL) {
			if (sc->barmuxed) {
				bus_space_unmap(rman_get_bustag(rle->res),
				    rman_get_bushandle(rle->res), ressz);
				rman_secret_puc_free_resource(rle->res);
				free(pdev, M_DEVBUF);
			}
			continue;
		}
		device_set_ivars(sc->sc_ports[i].dev, pdev);
		device_set_desc(sc->sc_ports[i].dev, sc->sc_desc.name);
#ifdef PUC_DEBUG
		printf("puc: type %d, bar %x, offset %x\n",
		    sc->sc_desc.ports[i].type,
		    sc->sc_desc.ports[i].bar,
		    sc->sc_desc.ports[i].offset);
		puc_print_resource_list(&pdev->resources);
#endif
		device_set_flags(sc->sc_ports[i].dev,
		    sc->sc_desc.ports[i].flags);
		if (device_probe_and_attach(sc->sc_ports[i].dev) != 0) {
			if (sc->barmuxed) {
				bus_space_unmap(rman_get_bustag(rle->res),
				    rman_get_bushandle(rle->res), ressz);
				rman_secret_puc_free_resource(rle->res);
				free(pdev, M_DEVBUF);
			}
		}
	}

#ifdef PUC_DEBUG
	bootverbose = 0;
#endif
	return (0);
}

static u_int32_t
puc_ilr_read(struct puc_softc *sc)
{
	u_int32_t mask;
	int i;

	mask = 0;
	switch (sc->sc_desc.ilr_type) {
	case PUC_ILR_TYPE_DIGI:
		for (i = 1; i >= 0 && sc->sc_desc.ilr_offset[i] != 0; i--) {
			mask = (mask << 8) | (bus_space_read_1(sc->ilr_st,
			    sc->ilr_sh, sc->sc_desc.ilr_offset[i]) & 0xff);
		}
		break;

	default:
		mask = 0xffffffff;
		break;
	}
	return (mask);
}

/*
 * This is an interrupt handler. For boards that can't tell us which
 * device generated the interrupt it just calls all the registered
 * handlers sequencially, but for boards that can tell us which
 * device(s) generated the interrupt it calls only handlers for devices
 * that actually generated the interrupt.
 */
static void
puc_intr(void *arg)
{
	int i;
	u_int32_t ilr_mask;
	struct puc_softc *sc;

	sc = (struct puc_softc *)arg;
	ilr_mask = sc->ilr_enabled ? puc_ilr_read(sc) : 0xffffffff;
	for (i = 0; i < PUC_MAX_PORTS; i++)
		if (sc->sc_ports[i].ihand != NULL &&
		    ((ilr_mask >> i) & 0x00000001))
			(sc->sc_ports[i].ihand)(sc->sc_ports[i].ihandarg);
}

static int
puc_find_free_unit(char *name)
{
	devclass_t dc;
	int start;
	int unit;

	unit = 0;
	start = 0;
	while (resource_int_value(name, unit, "port", &start) == 0 && 
	    start > 0)
		unit++;
	dc = devclass_find(name);
	if (dc == NULL)
		return (-1);
	while (devclass_get_device(dc, unit))
		unit++;
#ifdef PUC_DEBUG
	printf("puc: Using %s%d\n", name, unit);
#endif
	return (unit);
}

#ifdef PUC_DEBUG
static void
puc_print_resource_list(struct resource_list *rl)
{
#if 0
	struct resource_list_entry *rle;

	printf("print_resource_list: rl %p\n", rl);
	SLIST_FOREACH(rle, rl, link)
		printf("  type %x, rid %x start %lx end %lx count %lx\n",
		    rle->type, rle->rid, rle->start, rle->end, rle->count);
	printf("print_resource_list: end.\n");
#endif
}
#endif

struct resource *
puc_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct puc_device *pdev;
	struct resource *retval;
	struct resource_list *rl;
	struct resource_list_entry *rle;
	device_t my_child;

	/* 
	 * in the case of a child of child we need to find our immediate child
	 */
	for (my_child = child; device_get_parent(my_child) != dev; 
	     my_child = device_get_parent(my_child));

	pdev = device_get_ivars(my_child);
	rl = &pdev->resources;

#ifdef PUC_DEBUG
	printf("puc_alloc_resource: pdev %p, looking for t %x, r %x\n",
	    pdev, type, *rid);
	puc_print_resource_list(rl);
#endif
	retval = NULL;
	rle = resource_list_find(rl, type, *rid);
	if (rle) {
#ifdef PUC_DEBUG
		printf("found rle, %lx, %lx, %lx\n", rle->start, rle->end,
		    rle->count);
#endif
		retval = rle->res;
	} 
#ifdef PUC_DEBUG
	else
		printf("oops rle is gone\n");
#endif

	return (retval);
}

int
puc_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{
	return (0);
}

int
puc_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct puc_device *pdev;
	struct resource_list *rl;
	struct resource_list_entry *rle;

	pdev = device_get_ivars(child);
	rl = &pdev->resources;

#ifdef PUC_DEBUG
	printf("puc_get_resource: pdev %p, looking for t %x, r %x\n", pdev,
	    type, rid);
	puc_print_resource_list(rl);
#endif
	rle = resource_list_find(rl, type, rid);
	if (rle) {
#ifdef PUC_DEBUG
		printf("found rle %p,", rle);
#endif
		if (startp != NULL)
			*startp = rle->start;
		if (countp != NULL)
			*countp = rle->count;
#ifdef PUC_DEBUG
		printf(" %lx, %lx\n", rle->start, rle->count);
#endif
		return (0);
	} else
		printf("oops rle is gone\n");
	return (ENXIO);
}

int
puc_setup_intr(device_t dev, device_t child, struct resource *r, int flags,
	       void (*ihand)(void *), void *arg, void **cookiep)
{
	int i;
	struct puc_softc *sc;

	sc = (struct puc_softc *)device_get_softc(dev);
	if ((flags & INTR_FAST) != sc->fastintr)
		return (ENXIO);
	for (i = 0; PUC_PORT_VALID(sc->sc_desc, i); i++) {
		if (sc->sc_ports[i].dev == child) {
			if (sc->sc_ports[i].ihand != 0)
				return (ENXIO);
			sc->sc_ports[i].ihand = ihand;
			sc->sc_ports[i].ihandarg = arg;
			*cookiep = arg;
			return (0);
		}
	}
	return (ENXIO);
}

int
puc_teardown_intr(device_t dev, device_t child, struct resource *r,
		  void *cookie)
{
	int i;
	struct puc_softc *sc;

	sc = (struct puc_softc *)device_get_softc(dev);
	for (i = 0; PUC_PORT_VALID(sc->sc_desc, i); i++) {
		if (sc->sc_ports[i].dev == child) {
			sc->sc_ports[i].ihand = NULL;
			sc->sc_ports[i].ihandarg = NULL;
			return (0);
		}
	}
	return (ENXIO);
}

int
puc_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct puc_device *pdev;

	pdev = device_get_ivars(child);
	if (pdev == NULL)
		return (ENOENT);

	switch(index) {
	case PUC_IVAR_FREQ:
		*result = pdev->serialfreq;
		break;
	case PUC_IVAR_PORT:
		*result = pdev->port;
		break;
	case PUC_IVAR_REGSHFT:
		*result = pdev->regshft;
		break;
	case PUC_IVAR_SUBTYPE:
		*result = pdev->subtype;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}
