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
#include "opt_psim.h"

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
#include <machine/resource.h>

#include <sys/rman.h>

#include "pic_if.h"

/*
 * The nexus (which is a pseudo-bus actually) iterates over the nodes that
 * exist in OpenFirmware and adds them as devices to this bus so that drivers
 * can be attached to them.
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
	char		*ndi_compatible;
};

struct nexus_softc {
	device_t	sc_pic;
};

/*
 * Device interface
 */
static int	nexus_probe(device_t);
static void	nexus_probe_nomatch(device_t, device_t);

/*
 * Bus interface
 */
static int	nexus_read_ivar(device_t, device_t, int, uintptr_t *);
static int	nexus_write_ivar(device_t, device_t, int, uintptr_t);
static int	nexus_setup_intr(device_t, device_t, struct resource *, int,
		    driver_intr_t *, void *, void **);
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
 * Local routines
 */
static device_t	create_device_from_node(device_t, phandle_t);

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

static int
nexus_probe(device_t dev)
{
	phandle_t	root;
	phandle_t	pic, cpus, child, new_node, temp_node;
	struct		nexus_softc *sc;

	if ((root = OF_peer(0)) == -1)
		panic("nexus_probe: OF_peer failed.");

	sc = device_get_softc(dev);

	if ((cpus = OF_finddevice("/cpus")) != -1) {
		for (child = OF_child(cpus); child; child = OF_peer(child))
			(void)create_device_from_node(dev, child);
	}

	if ((child = OF_finddevice("/chosen")) == -1)
		printf("nexus_probe: can't find /chosen");

	if (OF_getprop(child, "interrupt-controller", &pic, 4) != 4)
#ifndef PSIM
		printf("nexus_probe: can't get interrupt-controller");
#else
                pic = OF_finddevice("/iobus/opic");
#endif

	sc->sc_pic = create_device_from_node(dev, pic);

	if (sc->sc_pic == NULL)
		printf("nexus_probe: failed to create PIC device");

	child = root;
	while (child != 0) {
		if (child != pic)
			(void)create_device_from_node(dev, child);

		if (child == cpus)
			new_node = 0;
		else
			new_node = OF_child(child);
		if (new_node == -1)
			panic("nexus_probe: OF_child returned -1");
		if (new_node == 0)
			new_node = OF_peer(child);
		if (new_node == 0) {
			temp_node = child;
			while (new_node == 0) {
				temp_node = OF_parent(temp_node);
				if (temp_node == 0)
					break;
				new_node = OF_peer(temp_node);
			}
		}
		child = new_node;
	}
	device_set_desc(dev, "OpenFirmware Nexus device");

	return (0);
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
#if 0
	device_printf(dev, "<%s>, type %s (no driver attached)\n",
	    name, type);
#endif
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
	case NEXUS_IVAR_NODE:
	case NEXUS_IVAR_NAME:
	case NEXUS_IVAR_DEVICE_TYPE:
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
	struct	nexus_softc *sc;

	sc = device_get_softc(dev);

	if (device_get_state(sc->sc_pic) != DS_ATTACHED)
		panic("nexus_setup_intr: no pic attached\n");

	return (PIC_SETUP_INTR(sc->sc_pic, child, res, flags, intr, arg,
	    cookiep));
}

static int
nexus_teardown_intr(device_t dev, device_t child, struct resource *res,
    void *ih)
{
	struct	nexus_softc *sc;

	sc = device_get_softc(dev);

	if (device_get_state(sc->sc_pic) != DS_ATTACHED)
		panic("nexus_teardown_intr: no pic attached\n");

	return (PIC_TEARDOWN_INTR(sc->sc_pic, child, res, ih));
}

/*
 * Allocate resources at the behest of a child.  This only handles interrupts,
 * since I/O resources are handled by child busses.
 */
static struct resource *
nexus_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct	nexus_softc *sc;
	struct	resource *rv;

	sc = device_get_softc(bus);

	if (type != SYS_RES_IRQ) {
		device_printf(bus, "unknown resource request from %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	if (device_get_state(sc->sc_pic) != DS_ATTACHED)
		panic("nexus_alloc_resource: no pic attached\n");

	rv = PIC_ALLOCATE_INTR(sc->sc_pic, child, rid, start, flags);

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
	struct	nexus_softc *sc;

	sc = device_get_softc(bus);

	if (type != SYS_RES_IRQ) {
		device_printf(bus, "unknown resource request from %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}

	if (device_get_state(sc->sc_pic) != DS_ATTACHED)
		panic("nexus_release_resource: no pic attached\n");

	return (PIC_RELEASE_INTR(sc->sc_pic, child, rid, res));
}

static device_t
create_device_from_node(device_t parent, phandle_t node)
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
