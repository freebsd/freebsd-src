/*-
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
#include "opt_psim.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/cons.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include "ofw_bus_if.h"
#include "pic_if.h"

/*
 * The nexus (which is a pseudo-bus actually) iterates over the nodes that
 * exist in Open Firmware and adds them as devices to this bus so that drivers
 * can be attached to them.
 *
 * Maybe this code should get into dev/ofw to some extent, as some of it should
 * work for all Open Firmware based machines...
 */

static MALLOC_DEFINE(M_NEXUS, "nexus", "nexus device information");

enum nexus_ivars {
	NEXUS_IVAR_NODE,
	NEXUS_IVAR_NAME,
	NEXUS_IVAR_DEVICE_TYPE,
	NEXUS_IVAR_COMPATIBLE,
};

struct nexus_devinfo {
	phandle_t	ndi_node;
	/* Some common properties. */
	const char     	*ndi_name;
	const char     	*ndi_device_type;
	const char     	*ndi_compatible;
};

struct nexus_softc {
	struct rman	sc_rman;
};

/*
 * Device interface
 */
static int	nexus_probe(device_t);
static int	nexus_attach(device_t);

/*
 * Bus interface
 */
static device_t nexus_add_child(device_t, int, const char *, int);
static void	nexus_probe_nomatch(device_t, device_t);
static int	nexus_read_ivar(device_t, device_t, int, uintptr_t *);
static int	nexus_write_ivar(device_t, device_t, int, uintptr_t);
static int	nexus_setup_intr(device_t, device_t, struct resource *, int,
		    driver_filter_t *, driver_intr_t *, void *, void **);
static int	nexus_teardown_intr(device_t, device_t, struct resource *,
		    void *);
static struct	resource *nexus_alloc_resource(device_t, device_t, int, int *,
		    u_long, u_long, u_long, u_int);
static int	nexus_activate_resource(device_t, device_t, int, int,
		    struct resource *);
static int	nexus_deactivate_resource(device_t, device_t, int, int,
		    struct resource *);
static int	nexus_release_resource(device_t, device_t, int, int,
		    struct resource *);

/*
 * OFW bus interface.
 */
static phandle_t	 nexus_ofw_get_node(device_t, device_t);
static const char	*nexus_ofw_get_name(device_t, device_t);
static const char	*nexus_ofw_get_type(device_t, device_t);
static const char	*nexus_ofw_get_compat(device_t, device_t);

/*
 * Local routines
 */
static device_t	nexus_device_from_node(device_t, phandle_t);

static device_method_t nexus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_probe),
	DEVMETHOD(device_attach,	nexus_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* Bus interface. Resource management is business of the children... */
	DEVMETHOD(bus_add_child,	nexus_add_child),
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

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_node, nexus_ofw_get_node),
	DEVMETHOD(ofw_bus_get_name, nexus_ofw_get_name),
	DEVMETHOD(ofw_bus_get_type, nexus_ofw_get_type),
	DEVMETHOD(ofw_bus_get_compat, nexus_ofw_get_compat),

	{ 0, 0 }
};

static driver_t nexus_driver = {
	"nexus",
	nexus_methods,
	sizeof(struct nexus_softc),
};

static devclass_t nexus_devclass;

DRIVER_MODULE(nexus, root, nexus_driver, nexus_devclass, 0, 0);

static int
nexus_probe(device_t dev)
{
	bus_generic_probe(dev);
	device_set_desc(dev, "Open Firmware Nexus device");
	return (0);
}

static int
nexus_attach(device_t dev)
{
	phandle_t	root;
	phandle_t	child;
	struct		nexus_softc *sc;
	u_long		start, end;

	if ((root = OF_peer(0)) == -1)
		panic("nexus_probe: OF_peer failed.");

	sc = device_get_softc(dev);

	start = 0;
	end = INTR_VECTORS - 1;

	sc->sc_rman.rm_start = start;
	sc->sc_rman.rm_end = end;
	sc->sc_rman.rm_type = RMAN_ARRAY;
	sc->sc_rman.rm_descr = "Interrupt request lines";
	if (rman_init(&sc->sc_rman) ||
	    rman_manage_region(&sc->sc_rman, start, end))
		panic("nexus_probe IRQ rman");

	/*
	 * Now walk the OFW tree to locate top-level devices
	 */
	for (child = OF_child(root); child != 0; child = OF_peer(child)) {
		if (child == -1)
			panic("nexus_probe(): OF_child failed.");
		(void)nexus_device_from_node(dev, child);

	}

	return (bus_generic_attach(dev));
}

static void
nexus_probe_nomatch(device_t dev, device_t child)
{
	char	*name, *type;

	if (BUS_READ_IVAR(dev, child, NEXUS_IVAR_NAME,
	    (uintptr_t *)&name) != 0 ||
	    BUS_READ_IVAR(dev, child, NEXUS_IVAR_DEVICE_TYPE,
	    (uintptr_t *)&type) != 0)
		return;

	if (type == NULL)
		type = "(unknown)";

	if (bootverbose)
		device_printf(dev, "<%s>, type %s (no driver attached)\n",
		    name, type);
}

static device_t
nexus_add_child(device_t dev, int order, const char *name, int unit)
{
	device_t child;
	struct nexus_devinfo *dinfo;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL)
		return (NULL);

	dinfo = malloc(sizeof(struct nexus_devinfo), M_NEXUS, M_NOWAIT|M_ZERO);
	if (dinfo == NULL)
		return (NULL);

	dinfo->ndi_node = -1;
	dinfo->ndi_name = name;
	device_set_ivars(child, dinfo);

        return (child);
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
	case NEXUS_IVAR_COMPATIBLE:
		*result = (uintptr_t)dinfo->ndi_compatible;
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
	case NEXUS_IVAR_NAME:
		return (EINVAL);

	/* Identified devices may want to set these */
	case NEXUS_IVAR_NODE:
		dinfo->ndi_node = (phandle_t)value;
		break;

	case NEXUS_IVAR_DEVICE_TYPE:
		dinfo->ndi_device_type = (char *)value;
		break;

	default:
		return (ENOENT);
	}
	return 0;
}

static int
nexus_setup_intr(device_t dev, device_t child, struct resource *res, int flags,
    driver_filter_t *filter, driver_intr_t *ihand, void *arg, void **cookiep)
{
	driver_t	*driver;
	int		error;

	/* somebody tried to setup an irq that failed to allocate! */
	if (res == NULL)
		panic("nexus_setup_intr: NULL irq resource!");

	*cookiep = 0;
	if ((rman_get_flags(res) & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	driver = device_get_driver(child);

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(res);
	if (error)
		return (error);

	error = powerpc_setup_intr(device_get_nameunit(child),
	    rman_get_start(res), filter, ihand, arg, flags, cookiep);

	return (error);
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *cookie)
{

	return (powerpc_teardown_intr(cookie));
}

/*
 * Allocate resources at the behest of a child.  This only handles interrupts,
 * since I/O resources are handled by child busses.
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct nexus_softc *sc;
	struct resource *rv;

	if (type != SYS_RES_IRQ) {
		device_printf(bus, "unknown resource request from %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	if (count == 0 || start + count - 1 != end) {
		device_printf(bus, "invalid IRQ allocation from %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	sc = device_get_softc(bus);

	rv = rman_reserve_resource(&sc->sc_rman, start, end, count,
	    flags, child);
	if (rv == NULL) {
		device_printf(bus, "IRQ allocation failed for %s\n",
		    device_get_nameunit(child));
	} else
		rman_set_rid(rv, *rid);

	return (rv);
}

static int
nexus_activate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{

	/* Not much to be done yet... */
	return (rman_activate_resource(res));
}

static int
nexus_deactivate_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{

	/* Not much to be done yet... */
	return (rman_deactivate_resource(res));
}

static int
nexus_release_resource(device_t bus, device_t child, int type, int rid,
    struct resource *res)
{

	if (type != SYS_RES_IRQ) {
		device_printf(bus, "unknown resource request from %s\n",
		    device_get_nameunit(child));
		return (EINVAL);
	}

	return (rman_release_resource(res));
}

static device_t
nexus_device_from_node(device_t parent, phandle_t node)
{
	device_t	cdev;
	struct		nexus_devinfo *dinfo;
	char		*name, *type, *compatible;

	OF_getprop_alloc(node, "name", 1, (void **)&name);
	OF_getprop_alloc(node, "device_type", 1, (void **)&type);
	OF_getprop_alloc(node, "compatible", 1, (void **)&compatible);
	cdev = device_add_child(parent, NULL, -1);
	if (cdev != NULL) {
		dinfo = malloc(sizeof(*dinfo), M_NEXUS, M_WAITOK);
		dinfo->ndi_node = node;
		dinfo->ndi_name = name;
		dinfo->ndi_device_type = type;
		dinfo->ndi_compatible = compatible;
		device_set_ivars(cdev, dinfo);
	} else
		free(name, M_OFWPROP);

	return (cdev);
}

static const char *
nexus_ofw_get_name(device_t bus, device_t dev)
{
	struct nexus_devinfo *dinfo;

	if ((dinfo = device_get_ivars(dev)) == NULL)
		return (NULL);
	
	return (dinfo->ndi_name);
}

static phandle_t
nexus_ofw_get_node(device_t bus, device_t dev)
{
	struct nexus_devinfo *dinfo;

	if ((dinfo = device_get_ivars(dev)) == NULL)
		return (0);
	
	return (dinfo->ndi_node);
}

static const char *
nexus_ofw_get_type(device_t bus, device_t dev)
{
	struct nexus_devinfo *dinfo;

	if ((dinfo = device_get_ivars(dev)) == NULL)
		return (NULL);
	
	return (dinfo->ndi_device_type);
}

static const char *
nexus_ofw_get_compat(device_t bus, device_t dev)
{
	struct nexus_devinfo *dinfo;

	if ((dinfo = device_get_ivars(dev)) == NULL)
		return (NULL);
	
	return (dinfo->ndi_compatible);
}

