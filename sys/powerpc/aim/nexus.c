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
#include <machine/nexusvar.h>
#include <machine/resource.h>

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
};

struct nexus_softc {
};

static int nexus_probe(device_t);
static void nexus_probe_nomatch(device_t, device_t);
static int nexus_read_ivar(device_t, device_t, int, uintptr_t *);
static int nexus_write_ivar(device_t, device_t, int, uintptr_t);

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
	phandle_t	child, new_node, temp_node;
	device_t	cdev;
	struct		nexus_devinfo *dinfo;
	struct		nexus_softc *sc;
	char		*name, *type;

	if ((root = OF_peer(0)) == -1)
		panic("nexus_probe: OF_peer failed.");

	child = root;
	while (child != 0) {
		OF_getprop_alloc(child, "name", 1, (void **)&name);
		OF_getprop_alloc(child, "device_type", 1, (void **)&type);
		cdev = device_add_child(dev, NULL, -1);
		if (cdev != NULL) {
			dinfo = malloc(sizeof(*dinfo), M_NEXUS, M_WAITOK);
			dinfo->ndi_node = child;
			dinfo->ndi_name = name;
			dinfo->ndi_device_type = type;
			device_set_ivars(cdev, dinfo);
		} else
			free(name, M_OFWPROP);

next:
		new_node = OF_child(child);
		if (new_node == -1)
			panic("nexus_probe: OF_child return -1");
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
