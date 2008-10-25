/*-
 * Copyright (c) 2006-2008, Juniper Networks, Inc.
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <machine/bus.h>
#include <machine/ocpbus.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <powerpc/mpc85xx/lbc.h>

struct lbc_softc {
	device_t		sc_dev;

	struct resource		*sc_res;
	bus_space_handle_t	sc_bsh;
	bus_space_tag_t		sc_bst;
	int			sc_rid;

	struct rman		sc_rman;
	uintptr_t		sc_kva;
};

struct lbc_devinfo {
	int		lbc_devtype;
	int		lbc_memtype;
	/* Also the BAR number */
	int		lbc_unit;
};

static int lbc_probe(device_t);
static int lbc_attach(device_t);
static int lbc_shutdown(device_t);
static int lbc_get_resource(device_t, device_t, int, int, u_long *,
    u_long *);
static struct resource *lbc_alloc_resource(device_t, device_t, int, int *,
    u_long, u_long, u_long, u_int);
static int lbc_print_child(device_t, device_t);
static int lbc_release_resource(device_t, device_t, int, int,
    struct resource *);
static int lbc_read_ivar(device_t, device_t, int, uintptr_t *);

/*
 * Bus interface definition
 */
static device_method_t lbc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lbc_probe),
	DEVMETHOD(device_attach,	lbc_attach),
	DEVMETHOD(device_shutdown,	lbc_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	lbc_print_child),
	DEVMETHOD(bus_read_ivar,	lbc_read_ivar),
	DEVMETHOD(bus_setup_intr,	bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,	NULL),

	DEVMETHOD(bus_get_resource,	NULL),
	DEVMETHOD(bus_alloc_resource,	lbc_alloc_resource),
	DEVMETHOD(bus_release_resource,	lbc_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),

	{ 0, 0 }
};

static driver_t lbc_driver = {
	"lbc",
	lbc_methods,
	sizeof(struct lbc_softc)
};
devclass_t lbc_devclass;
DRIVER_MODULE(lbc, ocpbus, lbc_driver, lbc_devclass, 0, 0);

static device_t
lbc_mk_child(device_t dev, int type, int mtype, int unit)
{
	struct lbc_devinfo *dinfo;
	device_t child;

	child = device_add_child(dev, NULL, -1);
	if (child == NULL) {
		device_printf(dev, "could not add child device\n");
		return (NULL);
	}
	dinfo = malloc(sizeof(struct lbc_devinfo), M_DEVBUF, M_WAITOK | M_ZERO);
	dinfo->lbc_devtype = type;
	dinfo->lbc_memtype = mtype;
	dinfo->lbc_unit = unit;
	device_set_ivars(child, dinfo);
	return (child);
}

static int
lbc_probe(device_t dev)
{
	device_t parent;
	uintptr_t devtype;
	int error;

	parent = device_get_parent(dev);
	error = BUS_READ_IVAR(parent, dev, OCPBUS_IVAR_DEVTYPE, &devtype);
	if (error)
		return (error);
	if (devtype != OCPBUS_DEVTYPE_LBC)
		return (ENXIO);

	device_set_desc(dev, "Freescale MPC85xx Local Bus Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
lbc_attach(device_t dev)
{
	struct lbc_softc *sc;
	struct rman *rm;
	u_long start, size;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->sc_rid = 0;
	sc->sc_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->sc_rid,
	    RF_ACTIVE);
	if (sc->sc_res == NULL)
		return (ENXIO);

	sc->sc_bst = rman_get_bustag(sc->sc_res);
	sc->sc_bsh = rman_get_bushandle(sc->sc_res);

	error = bus_get_resource(dev, SYS_RES_MEMORY, 1, &start, &size);
	if (error)
		goto fail;

	rm = &sc->sc_rman;
	rm->rm_type = RMAN_ARRAY;
	rm->rm_descr = "MPC85XX Local Bus Space";
	rm->rm_start = start;
	rm->rm_end = start + size - 1;
	error = rman_init(rm);
	if (error)
		goto fail;

	error = rman_manage_region(rm, rm->rm_start, rm->rm_end);
	if (error) {
		rman_fini(rm);
		goto fail;
	}

	sc->sc_kva = (uintptr_t)pmap_mapdev(start, size);

	lbc_mk_child(dev, LBC_DEVTYPE_CFI, 0, 0);

	return (bus_generic_attach(dev));

 fail:
	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rid, sc->sc_res);
	return (error);
}

static int
lbc_shutdown(device_t dev)
{

	/* TODO */
	return(0);
}

static struct resource *
lbc_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct lbc_softc *sc;
	struct resource *rv;
	struct rman *rm;
	int error;

	sc = device_get_softc(dev);

	if (type != SYS_RES_MEMORY && type != SYS_RES_IRQ)
		return (NULL);

	/* We only support default allocations. */
	if (start != 0ul || end != ~0ul)
		return (NULL);

	if (type == SYS_RES_IRQ)
		return (bus_alloc_resource(dev, type, rid, start, end, count,
		    flags));

	error = lbc_get_resource(dev, child, type, *rid, &start, &count);
	if (error)
		return (NULL);

	rm = &sc->sc_rman;
	end = start + count - 1;
	rv = rman_reserve_resource(rm, start, end, count, flags, child);
	if (rv != NULL) {
		rman_set_bustag(rv, &bs_be_tag);
		rman_set_bushandle(rv, sc->sc_kva + rman_get_start(rv) -
		    rm->rm_start);
	}
	return (rv);
}

static int
lbc_print_child(device_t dev, device_t child)
{
	u_long size, start;
	int error, retval, rid;

	retval = bus_print_child_header(dev, child);

	rid = 0;
	while (1) {
		error = lbc_get_resource(dev, child, SYS_RES_MEMORY, rid,
		    &start, &size);
		if (error)
			break;
		retval += (rid == 0) ? printf(" iomem ") : printf(",");
		retval += printf("%#lx", start);
		if (size > 1)
			retval += printf("-%#lx", start + size - 1);
		rid++;
	}

	retval += bus_print_child_footer(dev, child);
	return (retval);
}

static int
lbc_read_ivar(device_t dev, device_t child, int index, uintptr_t *result)
{
	struct lbc_devinfo *dinfo;

	if (device_get_parent(child) != dev)
		return (EINVAL);

	dinfo = device_get_ivars(child);

	switch (index) {
	case LBC_IVAR_DEVTYPE:
		*result = dinfo->lbc_devtype;
		return (0);
	case LBC_IVAR_CLOCK:
		*result = 1843200;
		return (0);
	case LBC_IVAR_REGSHIFT:
		*result = 0;
		return (0);
	default:
		break;
	}
	return (EINVAL);
}

static int
lbc_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{

	return (rman_release_resource(res));
}

static int
lbc_get_resource(device_t dev, device_t child, int type, int rid,
    u_long *startp, u_long *countp)
{
	struct lbc_softc *sc;
	struct lbc_devinfo *dinfo;

	if (type != SYS_RES_MEMORY)
		return (ENOENT);

	/* Currently all devices have a single RID per type. */
	if (rid != 0)
		return (ENOENT);

	sc = device_get_softc(dev);
	dinfo = device_get_ivars(child);

	switch (dinfo->lbc_devtype) {
	case LBC_DEVTYPE_CFI:
		*startp = sc->sc_rman.rm_start;
		*countp = sc->sc_rman.rm_end - sc->sc_rman.rm_start + 1;
		break;
	default:
		return(EDOOFUS);
	}
	return(0);
}
