/*	$NetBSD: puc.c,v 1.7 2000/07/29 17:43:38 jlam Exp $	*/

/*-
 * Copyright (c) 2002 JF Hay.  All rights reserved.
 * Copyright (c) 2000 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/*
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
	u_int serialfreq;
};

static void puc_intr(void *arg);

static int puc_find_free_unit(char *);
#ifdef PUC_DEBUG
static void puc_print_resource_list(struct resource_list *);
#endif

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
	sc->sc_bar_mappings[i].bar = bar;
	sc->sc_bar_mappings[i].used = 1;
	return (i);
}

int
puc_attach(device_t dev, const struct puc_device_description *desc)
{
	char *typestr;
	int bidx, childunit, i, irq_setup, rid;
	struct puc_softc *sc;
	struct puc_device *pdev;
	struct resource *res;
	struct resource_list_entry *rle;

	sc = (struct puc_softc *)device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->sc_desc = desc;
	if (sc->sc_desc == NULL)
		return (ENXIO);

#ifdef PUC_DEBUG
	bootverbose = 1;

	printf("puc: name: %s\n", sc->sc_desc->name);
#endif
	rid = 0;
	res = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1,
	    RF_ACTIVE | RF_SHAREABLE);
	if (!res)
		return (ENXIO);

	sc->irqres = res;
	sc->irqrid = rid;
#ifdef PUC_FASTINTR
	irq_setup = BUS_SETUP_INTR(device_get_parent(dev), dev, res,
	    INTR_TYPE_TTY | INTR_FAST, puc_intr, sc, &sc->intr_cookie);
#else
	irq_setup = ENXIO;
#endif
	if (irq_setup != 0)
		irq_setup = BUS_SETUP_INTR(device_get_parent(dev), dev, res,
		    INTR_TYPE_TTY, puc_intr, sc, &sc->intr_cookie);
	if (irq_setup != 0)
		return (ENXIO);

	rid = 0;
	for (i = 0; PUC_PORT_VALID(sc->sc_desc, i); i++) {
		if (i > 0 && rid == sc->sc_desc->ports[i].bar)
			sc->barmuxed = 1;
		rid = sc->sc_desc->ports[i].bar;
		bidx = puc_port_bar_index(sc, rid);

		if (sc->sc_bar_mappings[bidx].res != NULL)
			continue;
		res = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
		    0ul, ~0ul, 1, RF_ACTIVE);
		if (res == NULL) {
			printf("could not get resource\n");
			continue;
		}
		sc->sc_bar_mappings[bidx].res = res;
#ifdef PUC_DEBUG
		printf("port rid %d bst %x, start %x, end %x\n", rid,
		    (u_int)rman_get_bustag(res), (u_int)rman_get_start(res),
		    (u_int)rman_get_end(res));
#endif
	}

	if (desc->init != NULL) {
		i = desc->init(sc);
		if (i != 0)
			return (i);
	}

	for (i = 0; PUC_PORT_VALID(sc->sc_desc, i); i++) {
		rid = sc->sc_desc->ports[i].bar;
		bidx = puc_port_bar_index(sc, rid);
		if (sc->sc_bar_mappings[bidx].res == NULL)
			continue;

		switch (sc->sc_desc->ports[i].type) {
		case PUC_PORT_TYPE_COM:
			typestr = "sio";
			break;
		default:
			continue;
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

		/* Now fake an IOPORT resource */
		res = sc->sc_bar_mappings[bidx].res;
		resource_list_add(&pdev->resources, SYS_RES_IOPORT, 0,
		    rman_get_start(res) + sc->sc_desc->ports[i].offset,
		    rman_get_start(res) + sc->sc_desc->ports[i].offset + 8 - 1,
		    8);
		rle = resource_list_find(&pdev->resources, SYS_RES_IOPORT, 0);

		if (sc->barmuxed == 0) {
			rle->res = sc->sc_bar_mappings[bidx].res;
		} else {
			rle->res = malloc(sizeof(struct resource), M_DEVBUF,
			    M_WAITOK | M_ZERO);
			if (rle->res == NULL) {
				free(pdev, M_DEVBUF);
				return (ENOMEM);
			}

			rle->res->r_start = rman_get_start(res) +
			    sc->sc_desc->ports[i].offset;
			rle->res->r_end = rle->res->r_start + 8 - 1;
			rle->res->r_bustag = rman_get_bustag(res);
			bus_space_subregion(rle->res->r_bustag,
			    rman_get_bushandle(res),
			    sc->sc_desc->ports[i].offset, 8,
			    &rle->res->r_bushandle);
		}

		pdev->serialfreq = sc->sc_desc->ports[i].serialfreq;

		childunit = puc_find_free_unit(typestr);
		sc->sc_ports[i].dev = device_add_child(dev, typestr, childunit);
		if (sc->sc_ports[i].dev == NULL) {
			if (sc->barmuxed) {
				bus_space_unmap(rman_get_bustag(rle->res),
						rman_get_bushandle(rle->res),
						8);
				free(rle->res, M_DEVBUF);
				free(pdev, M_DEVBUF);
			}
			continue;
		}
		device_set_ivars(sc->sc_ports[i].dev, pdev);
		device_set_desc(sc->sc_ports[i].dev, sc->sc_desc->name);
		if (!bootverbose)
			device_quiet(sc->sc_ports[i].dev);
#ifdef PUC_DEBUG
		printf("puc: type %d, bar %x, offset %x\n",
		    sc->sc_desc->ports[i].type,
		    sc->sc_desc->ports[i].bar,
		    sc->sc_desc->ports[i].offset);
		puc_print_resource_list(&pdev->resources);
#endif
		if (device_probe_and_attach(sc->sc_ports[i].dev) != 0) {
			if (sc->barmuxed) {
				bus_space_unmap(rman_get_bustag(rle->res),
						rman_get_bushandle(rle->res),
						8);
				free(rle->res, M_DEVBUF);
				free(pdev, M_DEVBUF);
			}
		}
	}

#ifdef PUC_DEBUG
	bootverbose = 0;
#endif
	return (0);
}

/*
 * This is just an brute force interrupt handler. It just calls all the
 * registered handlers sequencially.
 *
 * Later on we should maybe have a different handler for boards that can
 * tell us which device generated the interrupt.
 */
static void
puc_intr(void *arg)
{
	int i;
	struct puc_softc *sc;

printf("puc_intr\n");
	sc = (struct puc_softc *)arg;
	for (i = 0; i < PUC_MAX_PORTS; i++)
		if (sc->sc_ports[i].ihand != NULL)
			(sc->sc_ports[i].ihand)(sc->sc_ports[i].ihandarg);
}

const struct puc_device_description *
puc_find_description(uint32_t vend, uint32_t prod, uint32_t svend, 
    uint32_t sprod)
{
	int i;

#define checkreg(val, index) \
    (((val) & puc_devices[i].rmask[(index)]) == puc_devices[i].rval[(index)])

	for (i = 0; puc_devices[i].name != NULL; i++) {
		if (checkreg(vend, PUC_REG_VEND) &&
		    checkreg(prod, PUC_REG_PROD) &&
		    checkreg(svend, PUC_REG_SVEND) &&
		    checkreg(sprod, PUC_REG_SPROD))
			return (&puc_devices[i]);
	}

#undef checkreg

	return (NULL);
}
static int puc_find_free_unit(char *name)
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
		printf("  type %x, rid %x start %x end %x count %x\n",
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

	pdev = device_get_ivars(child);
	rl = &pdev->resources;

#ifdef PUC_DEBUG
	printf("puc_alloc_resource: pdev %p, looking for t %x, r %x\n",
	    pdev, type, *rid);
	puc_print_resource_list(rl);
#endif
	retval = NULL;
	rle = resource_list_find(rl, type, *rid);
	if (rle) {
		start = rle->start;
		end = rle->end;
		count = rle->count;
#ifdef PUC_DEBUG
		printf("found rle, %lx, %lx, %lx\n", start, end, count);
#endif
		retval = rle->res;
	} else
		printf("oops rle is gone\n");

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

printf("puc_setup_intr()\n");
	sc = (struct puc_softc *)device_get_softc(dev);
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

printf("puc_teardown_intr()\n");
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
	default:
		return (ENOENT);
	}
	return (0);
}

devclass_t puc_devclass;

