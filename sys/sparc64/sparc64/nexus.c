/*
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.  All rights reserved.
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
 *
 * 	from: FreeBSD: src/sys/i386/i386/nexus.c,v 1.43 2001/02/09
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/nexusvar.h>
#include <machine/ofw_upa.h>
#include <machine/resource.h>
#include <machine/upa.h>

#include <sys/rman.h>

/*
 * The nexus (which is a pseudo-bus actually) iterates over the nodes that
 * hang from the OpenFirmware root node and add them as devices to this bus
 * (except some special nodes which are excluded) so that drivers can be
 * attached to them. This saves lots of detection work.
 * Usually, all devices and bridges that are attached to the UltraSparc UPA
 * bus will show up here, plus some pseudo-nodes which are excluded.
 * For now, the only node that gets used is probably the pci bus one.
 *
 * Additionally, interrupt setup/teardown and some resource management are
 * done at this level.
 *
 * Maybe this code should get into dev/ofw to some extent, as some of it should
 * work for all OpenFirmware based machines...
 */

static MALLOC_DEFINE(M_NEXUS, "nexus", "nexus device information");

struct nexus_devinfo {
	phandle_t	ndi_node;
	/* Some common properties. */
	char		*ndi_name;
	char		*ndi_device_type;
	char		*ndi_model;
	struct		upa_regs *ndi_reg;
	int		ndi_nreg;
	u_int		*ndi_interrupts;
	int		ndi_ninterrupts;
	bus_space_tag_t	ndi_bustag;
	bus_dma_tag_t	ndi_dmatag;
};

struct nexus_softc {
	struct rman	sc_intr_rman;
	struct rman	sc_mem_rman;
};

static int nexus_probe(device_t);
static void nexus_probe_nomatch(device_t, device_t);
static int nexus_read_ivar(device_t, device_t, int, uintptr_t *);
static int nexus_write_ivar(device_t, device_t, int, uintptr_t);
static int nexus_setup_intr(device_t, device_t, struct resource *, int,
    driver_intr_t *, void *, void **);
static int nexus_teardown_intr(device_t, device_t, struct resource *,
    void *);
static struct resource *nexus_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int nexus_activate_resource(device_t, device_t, int, int,
    struct resource *);
static int nexus_deactivate_resource(device_t, device_t, int, int,
    struct resource *);
static int nexus_release_resource(device_t, device_t, int, int,
    struct resource *);

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface. Resource management is business of the children... */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_probe_nomatch,	nexus_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	nexus_read_ivar),
	DEVMETHOD(bus_write_ivar,	nexus_write_ivar),
	DEVMETHOD(bus_setup_intr,	nexus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	nexus_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	nexus_alloc_resource),
	DEVMETHOD(bus_activate_resource,	nexus_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	nexus_deactivate_resource),
	DEVMETHOD(bus_release_resource,	nexus_release_resource),

	{ 0, 0 }
};

static driver_t nexus_driver = {
	"nexus",
	nexus_methods,
	sizeof(struct nexus_softc),
};

static devclass_t nexus_devclass;

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, 0, 0);

static char *nexus_excl_name[] = {
	"aliases",
	"chosen",
	"counter-timer",	/* No separate device; handled by psycho/sbus */
	"memory",
	"openprom",
	"options",
	"packages",
	"virtual-memory",
	NULL
};

static char *nexus_excl_type[] = {
	"cpu",
	NULL
};

extern struct bus_space_tag nexus_bustag;
extern struct bus_dma_tag nexus_dmatag;

static int
nexus_inlist(char *name, char *list[])
{
	int i;

	for (i = 0; list[i] != NULL; i++)
		if (strcmp(name, list[i]) == 0)
			return (1);
	return (0);
}

#define	NEXUS_EXCLUDED(name, type)					\
	(nexus_inlist((name), nexus_excl_name) ||			\
	((type) != NULL && nexus_inlist((type), nexus_excl_type)))

static int
nexus_probe(device_t dev)
{
	phandle_t root;
	phandle_t child;
	device_t cdev;
	struct nexus_devinfo *dinfo;
	struct nexus_softc *sc;
	char *name, *type;

	if ((root = OF_peer(0)) == -1)
		panic("nexus_probe: OF_peer failed.");

	sc = device_get_softc(dev);
	sc->sc_intr_rman.rm_type = RMAN_ARRAY;
	sc->sc_intr_rman.rm_descr = "Interrupts";
	sc->sc_mem_rman.rm_type = RMAN_ARRAY;
	sc->sc_mem_rman.rm_descr = "UPA Device Memory";
	if (rman_init(&sc->sc_intr_rman) != 0 ||
	    rman_init(&sc->sc_mem_rman) != 0 ||
	    rman_manage_region(&sc->sc_intr_rman, 0, IV_MAX - 1) != 0 ||
	    rman_manage_region(&sc->sc_mem_rman, UPA_MEMSTART, UPA_MEMEND) != 0)
		panic("nexus_probe: failed to set up rmans");
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (child == -1)
			panic("nexus_probe(): OF_child failed.");
		if (OF_getprop_alloc(child, "name", 1, (void **)&name) == -1)
			continue;
		OF_getprop_alloc(child, "device_type", 1, (void **)&type);
		if (NEXUS_EXCLUDED(name, type)) {
			free(name, M_OFWPROP);
			if (type != NULL)
				free(type, M_OFWPROP);
			continue;
		}
		cdev = device_add_child(dev, NULL, -1);
		if (cdev != NULL) {
			dinfo = malloc(sizeof(*dinfo), M_NEXUS, 0);
			dinfo->ndi_node = child;
			dinfo->ndi_name = name;
			dinfo->ndi_device_type = type;
			OF_getprop_alloc(child, "model", 1,
			    (void **)&dinfo->ndi_model);
			dinfo->ndi_nreg = OF_getprop_alloc(child, "reg",
			    sizeof(*dinfo->ndi_reg), (void **)&dinfo->ndi_reg);
			dinfo->ndi_ninterrupts = OF_getprop_alloc(child,
			    "interrupts", sizeof(*dinfo->ndi_interrupts),
			    (void **)&dinfo->ndi_interrupts);
			dinfo->ndi_bustag = &nexus_bustag;
			dinfo->ndi_dmatag = &nexus_dmatag;
			device_set_ivars(cdev, dinfo);
		} else
			free(name, M_OFWPROP);

	}
	device_set_desc(dev, "OpenFirmware Nexus device");
	return (0);
}

static void
nexus_probe_nomatch(device_t dev, device_t child)
{
	char *name;
	char *type;

	if (BUS_READ_IVAR(dev, child, NEXUS_IVAR_NAME,
	    (uintptr_t *)&name) != 0 ||
	    BUS_READ_IVAR(dev, child, NEXUS_IVAR_DEVICE_TYPE,
	    (uintptr_t *)&type) != 0)
		return;

	if (type == NULL)
		type = "(unknown)";
	device_printf(dev, "<%s>, type %s (no driver attached)\n",
	    name, type);
}

static int
nexus_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct nexus_devinfo *dinfo;

	if ((dinfo = device_get_ivars(child)) == 0)
		return (ENOENT);
	switch (which) {
	case NEXUS_IVAR_NODE:
		*result = dinfo->ndi_node;
		break;
	case NEXUS_IVAR_NAME:
		*result = (uintptr_t)dinfo->ndi_name;
		break;
	case NEXUS_IVAR_DEVICE_TYPE:
		*result = (uintptr_t)dinfo->ndi_device_type;
		break;
	case NEXUS_IVAR_MODEL:
		*result = (uintptr_t)dinfo->ndi_model;
		break;
	case NEXUS_IVAR_REG:
		*result = (uintptr_t)dinfo->ndi_reg;
		break;
	case NEXUS_IVAR_NREG:
		*result = dinfo->ndi_nreg;
		break;
	case NEXUS_IVAR_INTERRUPTS:
		*result = (uintptr_t)dinfo->ndi_interrupts;
		break;
	case NEXUS_IVAR_NINTERRUPTS:
		*result = dinfo->ndi_ninterrupts;
		break;
	case NEXUS_IVAR_DMATAG:
		*result = (uintptr_t)dinfo->ndi_dmatag;
		break;
	default:
		return (ENOENT);
	}
	return 0;
}

static int
nexus_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct nexus_devinfo *dinfo;

	if ((dinfo = device_get_ivars(child)) == 0)
		return (ENOENT);

	switch (which) {
	case NEXUS_IVAR_NODE:
	case NEXUS_IVAR_NAME:
	case NEXUS_IVAR_DEVICE_TYPE:
	case NEXUS_IVAR_MODEL:
	case NEXUS_IVAR_REG:
	case NEXUS_IVAR_NREG:
	case NEXUS_IVAR_INTERRUPTS:
	case NEXUS_IVAR_NINTERRUPTS:
	case NEXUS_IVAR_DMATAG:
		return (EINVAL);
	default:
		return (ENOENT);
	}
	return 0;
}

static int
nexus_setup_intr(device_t dev, device_t child, struct resource *res, int flags,
    driver_intr_t *intr, void *arg, void **cookiep)
{
	int error;

	if (res == NULL)
		panic("nexus_setup_intr: NULL interrupt resource!");

	if ((res->r_flags & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(res);
	if (error)
		return (error);

	error = inthand_add(device_get_nameunit(child), res->r_start,
	    intr, arg, flags, cookiep);

	return (error);
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *r, void *ih)
{
	inthand_remove(r->r_start, ih);
	return (0);
}

/*
 * Allocate resources at the behalf of a child. This only handles interrupts,
 * since i/o resources are usually set up by the firmware, and thus need not
 * be handled here.
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct	nexus_softc *sc = device_get_softc(bus);
	struct	resource *rv;
	struct	rman *rm;
	int needactivate = flags & RF_ACTIVE;

	flags &= ~RF_ACTIVE;

	switch (type) {
	case SYS_RES_IRQ:
		rm = &sc->sc_intr_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->sc_mem_rman;
		break;
	default:
		return (NULL);
	}

	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv == NULL)
		return (NULL);
	if (type == SYS_RES_MEMORY) {
		rman_set_bustag(rv, &nexus_bustag);
		rman_set_bushandle(rv, rman_get_start(rv));
	}

	if (needactivate) {
		if (bus_activate_resource(child, type, *rid, rv)) {
			rman_release_resource(rv);
			return (NULL);
		}
	}

	return (rv);
}

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	/* Not much to be done yet... */
	return (rman_activate_resource(r));
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *r)
{

	/* Not much to be done yet... */
	return (rman_deactivate_resource(r));
}

static int
nexus_release_resource(device_t bus, device_t child, int type, int rid,
		       struct resource *r)
{
	int error;

	if (rman_get_flags(r) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, type, rid, r);
		if (error)
			return error;
	}
	return (rman_release_resource(r));
}
