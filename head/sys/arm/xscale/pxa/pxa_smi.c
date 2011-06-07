/*-
 * Copyright (c) 2006 Benno Rice.  All rights reserved.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/timetc.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <arm/xscale/pxa/pxavar.h>
#include <arm/xscale/pxa/pxareg.h>

MALLOC_DEFINE(M_PXASMI, "PXA SMI", "Data for static memory interface devices.");

struct pxa_smi_softc {
	struct resource	*ps_res[1];
	struct rman	ps_mem;
	bus_space_tag_t	ps_bst;
	bus_addr_t	ps_base;
};

struct smi_ivars {
	struct resource_list	smid_resources;
	bus_addr_t		smid_mem;
};

static struct resource_spec pxa_smi_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static int	pxa_smi_probe(device_t);
static int	pxa_smi_attach(device_t);

static int	pxa_smi_print_child(device_t, device_t);

static int	pxa_smi_read_ivar(device_t, device_t, int, uintptr_t *);

static struct resource *	pxa_smi_alloc_resource(device_t, device_t,
				    int, int *, u_long, u_long, u_long, u_int);
static int			pxa_smi_release_resource(device_t, device_t,
				    int, int, struct resource *);
static int			pxa_smi_activate_resource(device_t, device_t,
				    int, int, struct resource *);

static void	pxa_smi_add_device(device_t, const char *, int);

static int
pxa_smi_probe(device_t dev)
{

	if (resource_disabled("smi", device_get_unit(dev)))
		return (ENXIO);

	device_set_desc(dev, "Static Memory Interface");
	return (0);
}

static int
pxa_smi_attach(device_t dev)
{
	int		error, i, dunit;
	const char	*dname;
	struct		pxa_smi_softc *sc;

	sc = (struct pxa_smi_softc *)device_get_softc(dev);

	error = bus_alloc_resources(dev, pxa_smi_spec, sc->ps_res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		return (ENXIO);
	}

	sc->ps_mem.rm_type = RMAN_ARRAY;
	sc->ps_mem.rm_descr = device_get_nameunit(dev);
	if (rman_init(&sc->ps_mem) != 0)
		panic("pxa_smi_attach: failed to init mem rman");
	if (rman_manage_region(&sc->ps_mem, 0, PXA2X0_CS_SIZE * 6) != 0)
		panic("pxa_smi_attach: failed ot set up mem rman");

	sc->ps_bst = base_tag;
	sc->ps_base = rman_get_start(sc->ps_res[0]);

	i = 0;
	while (resource_find_match(&i, &dname, &dunit, "at",
	    device_get_nameunit(dev)) == 0) {
		pxa_smi_add_device(dev, dname, dunit);
	}

	bus_generic_probe(dev);
	bus_generic_attach(dev);

	return (0);
}

static int
pxa_smi_print_child(device_t dev, device_t child)
{
	struct	smi_ivars *smid;
	int	retval;

	smid = (struct smi_ivars *)device_get_ivars(child);
	if (smid == NULL) {
		device_printf(dev, "unknown device: %s\n",
		    device_get_nameunit(child));
		return (0);
	}

	retval = 0;

	retval += bus_print_child_header(dev, child);

	retval += resource_list_print_type(&smid->smid_resources, "at mem",
	    SYS_RES_MEMORY, "%#lx");
	retval += resource_list_print_type(&smid->smid_resources, "irq",
	    SYS_RES_IRQ, "%ld");

	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static int
pxa_smi_read_ivar(device_t dev, device_t child, int which, uintptr_t *result)
{
	struct	pxa_smi_softc *sc;
	struct	smi_ivars *smid;

	sc = device_get_softc(dev);
	smid = device_get_ivars(child);

	switch (which) {
	case SMI_IVAR_PHYSBASE:
		*((bus_addr_t *)result) = smid->smid_mem;
		break;

	default:
		return (ENOENT);
	}

	return (0);
}

static struct resource *
pxa_smi_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct	pxa_smi_softc *sc;
	struct	smi_ivars *smid;
	struct	resource *rv;
	struct	resource_list *rl;
	struct	resource_list_entry *rle;
	int	needactivate;

	sc = (struct pxa_smi_softc *)device_get_softc(dev);
	smid = (struct smi_ivars *)device_get_ivars(child);
	rl = &smid->smid_resources;

	if (type == SYS_RES_IOPORT)
		type = SYS_RES_MEMORY;

	rle = resource_list_find(rl, type, *rid);
	if (rle == NULL)
		return (NULL);
	if (rle->res != NULL)
		panic("pxa_smi_alloc_resource: resource is busy");

	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	switch (type) {
	case SYS_RES_MEMORY:
		rv = rman_reserve_resource(&sc->ps_mem, rle->start, rle->end,
		    rle->count, flags, child);
		if (rv == NULL)
			return (NULL);
		rle->res = rv;
		rman_set_rid(rv, *rid);
		rman_set_bustag(rv, sc->ps_bst);
		rman_set_bushandle(rv, rle->start);
		if (needactivate) {
			if (bus_activate_resource(child, type, *rid, rv) != 0) {
				rman_release_resource(rv);
				return (NULL);
			}
		}

		break;

	case SYS_RES_IRQ:
		rv = bus_alloc_resource(dev, type, rid, rle->start, rle->end,
		    rle->count, flags);
		if (rv == NULL)
			return (NULL);
		if (needactivate) {
			if (bus_activate_resource(child, type, *rid, rv) != 0) {
				bus_release_resource(dev, type, *rid, rv);
				return (NULL);
			}
		}

		break;

	default:
		return (NULL);
	}

	return (rv);
}

static int
pxa_smi_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct	smi_ivars *smid;
	struct	resource_list *rl;
	struct	resource_list_entry *rle;

	if (type == SYS_RES_IRQ)
		return (bus_release_resource(dev, SYS_RES_IRQ, rid, r));

	smid = (struct smi_ivars *)device_get_ivars(child);
	rl = &smid->smid_resources;

	if (type == SYS_RES_IOPORT)
		type = SYS_RES_MEMORY;

	rle = resource_list_find(rl, type, rid);
	if (rle == NULL)
		panic("pxa_smi_release_resource: can't find resource");
	if (rle->res == NULL)
		panic("pxa_smi_release_resource: resource entry not busy");

	rman_release_resource(rle->res);
	rle->res = NULL;

	return (0);
}

static int
pxa_smi_activate_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{
	struct	pxa_smi_softc *sc;

	sc = (struct pxa_smi_softc *)device_get_softc(dev);

	if (type == SYS_RES_IRQ)
		return (bus_activate_resource(dev, SYS_RES_IRQ, rid, r));

	rman_set_bushandle(r, (bus_space_handle_t)pmap_mapdev(rman_get_start(r),
	    rman_get_size(r)));
	return (rman_activate_resource(r));
}

static device_method_t pxa_smi_methods[] = {
	DEVMETHOD(device_probe, pxa_smi_probe),
	DEVMETHOD(device_attach, pxa_smi_attach),

	DEVMETHOD(bus_print_child, pxa_smi_print_child),

	DEVMETHOD(bus_read_ivar, pxa_smi_read_ivar),

	DEVMETHOD(bus_setup_intr, bus_generic_setup_intr),

	DEVMETHOD(bus_alloc_resource, pxa_smi_alloc_resource),
	DEVMETHOD(bus_release_resource, pxa_smi_release_resource),
	DEVMETHOD(bus_activate_resource, pxa_smi_activate_resource),

	{0, 0}
};

static driver_t pxa_smi_driver = {
	"smi",
	pxa_smi_methods,
	sizeof(struct pxa_smi_softc),
};

static devclass_t pxa_smi_devclass;

DRIVER_MODULE(smi, pxa, pxa_smi_driver, pxa_smi_devclass, 0, 0);

static void
pxa_smi_add_device(device_t dev, const char *name, int unit)
{
	device_t	child;
	int		start, count;
	struct		smi_ivars *ivars;

	ivars = (struct smi_ivars *)malloc(
	    sizeof(struct smi_ivars), M_PXASMI, M_WAITOK);
	if (ivars == NULL)
		return;

	child = device_add_child(dev, name, unit);
	if (child == NULL) {
		free(ivars, M_PXASMI);
		return;
	}

	device_set_ivars(child, ivars);
	resource_list_init(&ivars->smid_resources);

	start = 0;
	count = 0;
	resource_int_value(name, unit, "mem", &start);
	resource_int_value(name, unit, "size", &count);
	if (start > 0 || count > 0) {
		resource_list_add(&ivars->smid_resources, SYS_RES_MEMORY, 0,
		    start, start + count, count);
		ivars->smid_mem = (bus_addr_t)start;
	}

	start = -1;
	count = 0;
	resource_int_value(name, unit, "irq", &start);
	if (start > -1)
		resource_list_add(&ivars->smid_resources, SYS_RES_IRQ, 0, start,
		     start, 1);

	if (resource_disabled(name, unit))
		device_disable(child);
}
