/*-
 * Copyright (c) 2003 Jake Burkholder.
 * All rights reserved.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/nexusvar.h>
#include <machine/ofw_upa.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <sparc64/central/centralvar.h>
#include <sparc64/sbus/ofw_sbus.h>
#include <sparc64/sbus/sbusreg.h>

struct central_devinfo {
	char			*cdi_name;
	char			*cdi_type;
	phandle_t		cdi_node;
};

struct central_softc {
	phandle_t		sc_node;
	int			sc_nrange;
	struct sbus_ranges	*sc_ranges;
};

static int central_probe(device_t dev);
static int central_attach(device_t dev);

static void central_probe_nomatch(device_t dev, device_t child);
static int central_read_ivar(device_t, device_t, int, uintptr_t *);
static int central_write_ivar(device_t, device_t, int, uintptr_t);
static struct resource *central_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);

static device_method_t central_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		central_probe),
	DEVMETHOD(device_attach,	central_attach),

	/* Bus interface. */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_probe_nomatch,	central_probe_nomatch),
	DEVMETHOD(bus_read_ivar,	central_read_ivar),
	DEVMETHOD(bus_write_ivar,	central_write_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource,	central_alloc_resource),
	DEVMETHOD(bus_release_resource,	bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),

	{ NULL, NULL }
};

static driver_t central_driver = {
	"central",
	central_methods,
	sizeof(struct central_softc),
};

static devclass_t central_devclass;

DRIVER_MODULE(central, nexus, central_driver, central_devclass, 0, 0);

static int
central_probe(device_t dev)
{

	if (strcmp(nexus_get_name(dev), "central") == 0) {
		device_set_desc(dev, "central");
		return (0);
	}
	return (ENXIO);
}

static int
central_attach(device_t dev)
{
	struct central_devinfo *cdi;
	struct central_softc *sc;
	phandle_t child;
	phandle_t node;
	device_t cdev;
	char *name;

	sc = device_get_softc(dev);
	node = nexus_get_node(dev);
	sc->sc_node = node;

	sc->sc_nrange = OF_getprop_alloc(node, "ranges",
	    sizeof(*sc->sc_ranges), (void **)&sc->sc_ranges);
	if (sc->sc_nrange == -1) {
		device_printf(dev, "can't get ranges");
		return (ENXIO);
	}

	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		if ((OF_getprop_alloc(child, "name", 1, (void **)&name)) == -1)
			continue;
		cdev = device_add_child(dev, NULL, -1);
		if (cdev != NULL) {
			cdi = malloc(sizeof(*cdi), M_DEVBUF, M_ZERO);
			cdi->cdi_name = name;
			cdi->cdi_node = child;
			OF_getprop_alloc(child, "device_type", 1,
			    (void **)&cdi->cdi_type);
			device_set_ivars(cdev, cdi);
		} else
			free(name, M_OFWPROP);
	}

	return (bus_generic_attach(dev));
}

static void
central_probe_nomatch(device_t dev, device_t child)
{
	struct central_devinfo *cdi;

	cdi = device_get_ivars(child);
	device_printf(dev, "<%s> type %s (no driver attached)\n",
	    cdi->cdi_name, cdi->cdi_type != NULL ? cdi->cdi_type : "unknown");
}

static int
central_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct central_devinfo *cdi;

	if ((cdi = device_get_ivars(child)) == 0)
		return (ENOENT);
	switch (which) {
	case CENTRAL_IVAR_NAME:
		*result = (uintptr_t)cdi->cdi_name;
		break;
	case CENTRAL_IVAR_NODE:
		*result = cdi->cdi_node;
		break;
	case CENTRAL_IVAR_TYPE:
		*result = (uintptr_t)cdi->cdi_type;
		break;
	default:
		return (ENOENT);
	}
	return (0);
}

static int
central_write_ivar(device_t dev, device_t child, int which, uintptr_t value)
{
	struct central_devinfo *cdi;

	if ((cdi = device_get_ivars(child)) == 0)
		return (ENOENT);
	switch (which) {
	case CENTRAL_IVAR_NAME:
	case CENTRAL_IVAR_NODE:
	case CENTRAL_IVAR_TYPE:
		return (EINVAL);
	default:
		return (ENOENT);
	}
	return (0);
}

static struct resource *
central_alloc_resource(device_t bus, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct central_softc *sc;
	struct resource *res;
	bus_addr_t coffset;
	bus_addr_t cend;
	bus_addr_t phys;
	int i;

	if (type != SYS_RES_MEMORY) {
		return (bus_generic_alloc_resource(bus, child, type, rid,
		    start, end, count, flags));
	}
	res = NULL;
	sc = device_get_softc(bus);
	for (i = 0; i < sc->sc_nrange; i++) {
		coffset = sc->sc_ranges[i].coffset;
		cend = coffset + sc->sc_ranges[i].size - 1;
		if (start >= coffset && end <= cend) {
			start -= coffset;
			end -= coffset;
			phys = sc->sc_ranges[i].poffset |
			    ((bus_addr_t)sc->sc_ranges[i].pspace << 32);
			res = bus_generic_alloc_resource(bus, child, type,
			    rid, phys + start, phys + end, count, flags);
			break;
		}
	}
	return (res);
}
