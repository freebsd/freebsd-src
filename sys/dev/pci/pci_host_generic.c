/*-
 * Copyright (c) 2015, 2020 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2014 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Semihalf under
 * the sponsorship of the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* Generic ECAM PCIe driver */

#include <sys/cdefs.h>
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_host_generic.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include "pcib_if.h"

#if defined(VM_MEMATTR_DEVICE_NP)
#define	PCI_UNMAPPED
#define	PCI_RF_FLAGS	RF_UNMAPPED
#else
#define	PCI_RF_FLAGS	0
#endif


/* Forward prototypes */

static uint32_t generic_pcie_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes);
static void generic_pcie_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes);
static int generic_pcie_maxslots(device_t dev);
static int generic_pcie_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result);
static int generic_pcie_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value);

int
pci_host_generic_core_attach(device_t dev)
{
#ifdef PCI_UNMAPPED
	struct resource_map_request req;
	struct resource_map map;
#endif
	struct generic_pcie_core_softc *sc;
	uint64_t phys_base;
	uint64_t pci_base;
	uint64_t size;
	char buf[64];
	int domain, error;
	int flags, rid, tuple, type;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Create the parent DMA tag to pass down the coherent flag */
	error = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
	    1, 0,				/* alignment, bounds */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    BUS_SPACE_MAXSIZE,			/* maxsize */
	    BUS_SPACE_UNRESTRICTED,		/* nsegments */
	    BUS_SPACE_MAXSIZE,			/* maxsegsize */
	    sc->coherent ? BUS_DMA_COHERENT : 0, /* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &sc->dmat);
	if (error != 0)
		return (error);

	/*
	 * Attempt to set the domain. If it's missing, or we are unable to
	 * set it then memory allocations may be placed in the wrong domain.
	 */
	if (bus_get_domain(dev, &domain) == 0)
		(void)bus_dma_tag_set_domain(sc->dmat, domain);

	if ((sc->quirks & PCIE_CUSTOM_CONFIG_SPACE_QUIRK) == 0) {
		rid = 0;
		sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
		    PCI_RF_FLAGS | RF_ACTIVE);
		if (sc->res == NULL) {
			device_printf(dev, "could not allocate memory.\n");
			error = ENXIO;
			goto err_resource;
		}
#ifdef PCI_UNMAPPED
		resource_init_map_request(&req);
		req.memattr = VM_MEMATTR_DEVICE_NP;
		error = bus_map_resource(dev, SYS_RES_MEMORY, sc->res, &req,
		    &map);
		if (error != 0) {
			device_printf(dev, "could not map memory.\n");
			return (error);
		}
		rman_set_mapping(sc->res, &map);
#endif
	}

	sc->has_pmem = false;
	sc->pmem_rman.rm_type = RMAN_ARRAY;
	snprintf(buf, sizeof(buf), "%s prefetch window",
	    device_get_nameunit(dev));
	sc->pmem_rman.rm_descr = strdup(buf, M_DEVBUF);

	sc->mem_rman.rm_type = RMAN_ARRAY;
	snprintf(buf, sizeof(buf), "%s memory window",
	    device_get_nameunit(dev));
	sc->mem_rman.rm_descr = strdup(buf, M_DEVBUF);

	sc->io_rman.rm_type = RMAN_ARRAY;
	snprintf(buf, sizeof(buf), "%s I/O port window",
	    device_get_nameunit(dev));
	sc->io_rman.rm_descr = strdup(buf, M_DEVBUF);

	/* Initialize rman and allocate memory regions */
	error = rman_init(&sc->pmem_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		goto err_pmem_rman;
	}

	error = rman_init(&sc->mem_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		goto err_mem_rman;
	}

	error = rman_init(&sc->io_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		goto err_io_rman;
	}

	for (tuple = 0; tuple < MAX_RANGES_TUPLES; tuple++) {
		phys_base = sc->ranges[tuple].phys_base;
		pci_base = sc->ranges[tuple].pci_base;
		size = sc->ranges[tuple].size;
		rid = tuple + 1;
		if (size == 0)
			continue; /* empty range element */
		switch (FLAG_TYPE(sc->ranges[tuple].flags)) {
		case FLAG_TYPE_PMEM:
			sc->has_pmem = true;
			flags = RF_PREFETCHABLE;
			type = SYS_RES_MEMORY;
			error = rman_manage_region(&sc->pmem_rman,
			   pci_base, pci_base + size - 1);
			break;
		case FLAG_TYPE_MEM:
			flags = 0;
			type = SYS_RES_MEMORY;
			error = rman_manage_region(&sc->mem_rman,
			   pci_base, pci_base + size - 1);
			break;
		case FLAG_TYPE_IO:
			flags = 0;
			type = SYS_RES_IOPORT;
			error = rman_manage_region(&sc->io_rman,
			   pci_base, pci_base + size - 1);
			break;
		default:
			continue;
		}
		if (error) {
			device_printf(dev, "rman_manage_region() failed."
						"error = %d\n", error);
			goto err_rman_manage;
		}
		error = bus_set_resource(dev, type, rid, phys_base, size);
		if (error != 0) {
			device_printf(dev,
			    "failed to set resource for range %d: %d\n", tuple,
			    error);
			goto err_rman_manage;
		}
		sc->ranges[tuple].res = bus_alloc_resource_any(dev, type, &rid,
		    RF_ACTIVE | RF_UNMAPPED | flags);
		if (sc->ranges[tuple].res == NULL) {
			device_printf(dev,
			    "failed to allocate resource for range %d\n", tuple);
			error = ENXIO;
			goto err_rman_manage;
		}
	}

	return (0);

err_rman_manage:
	for (tuple = 0; tuple < MAX_RANGES_TUPLES; tuple++) {
		if (sc->ranges[tuple].size == 0)
			continue; /* empty range element */
		switch (FLAG_TYPE(sc->ranges[tuple].flags)) {
		case FLAG_TYPE_PMEM:
		case FLAG_TYPE_MEM:
			type = SYS_RES_MEMORY;
			break;
		case FLAG_TYPE_IO:
			type = SYS_RES_IOPORT;
			break;
		default:
			continue;
		}
		if (sc->ranges[tuple].res != NULL)
			bus_release_resource(dev, type, tuple + 1,
			    sc->ranges[tuple].res);
		bus_delete_resource(dev, type, tuple + 1);
	}
	rman_fini(&sc->io_rman);
err_io_rman:
	rman_fini(&sc->mem_rman);
err_mem_rman:
	rman_fini(&sc->pmem_rman);
err_pmem_rman:
	free(__DECONST(char *, sc->io_rman.rm_descr), M_DEVBUF);
	free(__DECONST(char *, sc->mem_rman.rm_descr), M_DEVBUF);
	free(__DECONST(char *, sc->pmem_rman.rm_descr), M_DEVBUF);
	if (sc->res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->res);
err_resource:
	bus_dma_tag_destroy(sc->dmat);
	return (error);
}

int
pci_host_generic_core_detach(device_t dev)
{
	struct generic_pcie_core_softc *sc;
	int error, tuple, type;

	sc = device_get_softc(dev);

	error = bus_generic_detach(dev);
	if (error != 0)
		return (error);

	for (tuple = 0; tuple < MAX_RANGES_TUPLES; tuple++) {
		if (sc->ranges[tuple].size == 0)
			continue; /* empty range element */
		switch (FLAG_TYPE(sc->ranges[tuple].flags)) {
		case FLAG_TYPE_PMEM:
		case FLAG_TYPE_MEM:
			type = SYS_RES_MEMORY;
			break;
		case FLAG_TYPE_IO:
			type = SYS_RES_IOPORT;
			break;
		default:
			continue;
		}
		if (sc->ranges[tuple].res != NULL)
			bus_release_resource(dev, type, tuple + 1,
			    sc->ranges[tuple].res);
		bus_delete_resource(dev, type, tuple + 1);
	}
	rman_fini(&sc->io_rman);
	rman_fini(&sc->mem_rman);
	rman_fini(&sc->pmem_rman);
	free(__DECONST(char *, sc->io_rman.rm_descr), M_DEVBUF);
	free(__DECONST(char *, sc->mem_rman.rm_descr), M_DEVBUF);
	free(__DECONST(char *, sc->pmem_rman.rm_descr), M_DEVBUF);
	if (sc->res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->res);
	bus_dma_tag_destroy(sc->dmat);

	return (0);
}

static uint32_t
generic_pcie_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	struct generic_pcie_core_softc *sc;
	uint64_t offset;
	uint32_t data;

	sc = device_get_softc(dev);
	if ((bus < sc->bus_start) || (bus > sc->bus_end))
		return (~0U);
	if ((slot > PCI_SLOTMAX) || (func > PCI_FUNCMAX) ||
	    (reg > PCIE_REGMAX))
		return (~0U);
	if ((sc->quirks & PCIE_ECAM_DESIGNWARE_QUIRK) && bus == 0 && slot > 0)
		return (~0U);

	offset = PCIE_ADDR_OFFSET(bus - sc->bus_start, slot, func, reg);

	switch (bytes) {
	case 1:
		data = bus_read_1(sc->res, offset);
		break;
	case 2:
		data = le16toh(bus_read_2(sc->res, offset));
		break;
	case 4:
		data = le32toh(bus_read_4(sc->res, offset));
		break;
	default:
		return (~0U);
	}

	return (data);
}

static void
generic_pcie_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	struct generic_pcie_core_softc *sc;
	uint64_t offset;

	sc = device_get_softc(dev);
	if ((bus < sc->bus_start) || (bus > sc->bus_end))
		return;
	if ((slot > PCI_SLOTMAX) || (func > PCI_FUNCMAX) ||
	    (reg > PCIE_REGMAX))
		return;

	offset = PCIE_ADDR_OFFSET(bus - sc->bus_start, slot, func, reg);

	switch (bytes) {
	case 1:
		bus_write_1(sc->res, offset, val);
		break;
	case 2:
		bus_write_2(sc->res, offset, htole16(val));
		break;
	case 4:
		bus_write_4(sc->res, offset, htole32(val));
		break;
	default:
		return;
	}
}

static int
generic_pcie_maxslots(device_t dev)
{

	return (31); /* max slots per bus acc. to standard */
}

static int
generic_pcie_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{
	struct generic_pcie_core_softc *sc;

	sc = device_get_softc(dev);

	if (index == PCIB_IVAR_BUS) {
		*result = sc->bus_start;
		return (0);
	}

	if (index == PCIB_IVAR_DOMAIN) {
		*result = sc->ecam;
		return (0);
	}

	if (bootverbose)
		device_printf(dev, "ERROR: Unknown index %d.\n", index);
	return (ENOENT);
}

static int
generic_pcie_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value)
{

	return (ENOENT);
}

static struct rman *
generic_pcie_get_rman(device_t dev, int type, u_int flags)
{
	struct generic_pcie_core_softc *sc = device_get_softc(dev);

	switch (type) {
	case SYS_RES_IOPORT:
		return (&sc->io_rman);
	case SYS_RES_MEMORY:
		if (sc->has_pmem && (flags & RF_PREFETCHABLE) != 0)
			return (&sc->pmem_rman);
		return (&sc->mem_rman);
	default:
		break;
	}

	return (NULL);
}

int
pci_host_generic_core_release_resource(device_t dev, device_t child, int type,
    int rid, struct resource *res)
{
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	struct generic_pcie_core_softc *sc;

	sc = device_get_softc(dev);
#endif
	switch (type) {
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	case PCI_RES_BUS:
		return (pci_domain_release_bus(sc->ecam, child, rid, res));
#endif
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		return (bus_generic_rman_release_resource(dev, child, type, rid,
		    res));
	default:
		return (bus_generic_release_resource(dev, child, type, rid,
		    res));
	}
}

static struct pcie_range *
generic_pcie_containing_range(device_t dev, int type, rman_res_t start,
    rman_res_t end)
{
	struct generic_pcie_core_softc *sc = device_get_softc(dev);
	uint64_t pci_base;
	uint64_t size;
	int i, space;

	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		break;
	default:
		return (NULL);
	}

	for (i = 0; i < MAX_RANGES_TUPLES; i++) {
		pci_base = sc->ranges[i].pci_base;
		size = sc->ranges[i].size;
		if (size == 0)
			continue; /* empty range element */

		if (start < pci_base || end >= pci_base + size)
			continue;

		switch (FLAG_TYPE(sc->ranges[i].flags)) {
		case FLAG_TYPE_MEM:
		case FLAG_TYPE_PMEM:
			space = SYS_RES_MEMORY;
			break;
		case FLAG_TYPE_IO:
			space = SYS_RES_IOPORT;
			break;
		default:
			continue;
		}

		if (type == space)
			return (&sc->ranges[i]);
	}
	return (NULL);
}

static int
generic_pcie_translate_resource_common(device_t dev, int type, rman_res_t start,
    rman_res_t end, rman_res_t *new_start, rman_res_t *new_end)
{
	struct pcie_range *range;

	/* Translate the address from a PCI address to a physical address */
	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		range = generic_pcie_containing_range(dev, type, start, end);
		if (range == NULL)
			return (ENOENT);
		if (range != NULL) {
			*new_start = start - range->pci_base + range->phys_base;
			*new_end = end - range->pci_base + range->phys_base;
		}
		break;
	default:
		/* No translation for non-memory types */
		*new_start = start;
		*new_end = end;
		break;
	}

	return (0);
}

static int
generic_pcie_translate_resource(device_t bus, int type,
    rman_res_t start, rman_res_t *newstart)
{
	rman_res_t newend; /* unused */

	return (generic_pcie_translate_resource_common(
	    bus, type, start, 0, newstart, &newend));
}

struct resource *
pci_host_generic_core_alloc_resource(device_t dev, device_t child, int type,
    int *rid, rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	struct generic_pcie_core_softc *sc;
#endif
	struct resource *res;

#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	sc = device_get_softc(dev);
#endif

	switch (type) {
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	case PCI_RES_BUS:
		res = pci_domain_alloc_bus(sc->ecam, child, rid, start, end,
		    count, flags);
		break;
#endif
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		res = bus_generic_rman_alloc_resource(dev, child, type, rid,
		    start, end, count, flags);
		break;
	default:
		res = bus_generic_alloc_resource(dev, child, type, rid, start,
		    end, count, flags);
		break;
	}
	if (res == NULL) {
		device_printf(dev, "%s FAIL: type=%d, rid=%d, "
		    "start=%016jx, end=%016jx, count=%016jx, flags=%x\n",
		    __func__, type, *rid, start, end, count, flags);
	}
	return (res);
}

static int
generic_pcie_activate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	struct generic_pcie_core_softc *sc;

	sc = device_get_softc(dev);
#endif
	switch (type) {
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	case PCI_RES_BUS:
		return (pci_domain_activate_bus(sc->ecam, child, rid, r));
#endif
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		return (bus_generic_rman_activate_resource(dev, child, type,
		    rid, r));
	default:
		return (bus_generic_activate_resource(dev, child, type, rid,
		    r));
	}
}

static int
generic_pcie_deactivate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	struct generic_pcie_core_softc *sc;

	sc = device_get_softc(dev);
#endif
	switch (type) {
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	case PCI_RES_BUS:
		return (pci_domain_deactivate_bus(sc->ecam, child, rid, r));
#endif
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		return (bus_generic_rman_deactivate_resource(dev, child, type,
		    rid, r));
	default:
		return (bus_generic_deactivate_resource(dev, child, type, rid,
		    r));
	}
}

static int
generic_pcie_adjust_resource(device_t dev, device_t child, int type,
    struct resource *res, rman_res_t start, rman_res_t end)
{
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	struct generic_pcie_core_softc *sc;

	sc = device_get_softc(dev);
#endif
	switch (type) {
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	case PCI_RES_BUS:
		return (pci_domain_adjust_bus(sc->ecam, child, res, start,
		    end));
#endif
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		return (bus_generic_rman_adjust_resource(dev, child, type, res,
		    start, end));
	default:
		return (bus_generic_adjust_resource(dev, child, type, res,
		    start, end));
	}
}

static int
generic_pcie_map_resource(device_t dev, device_t child, int type,
    struct resource *r, struct resource_map_request *argsp,
    struct resource_map *map)
{
	struct resource_map_request args;
	struct pcie_range *range;
	rman_res_t length, start;
	int error;

	switch (type) {
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	case PCI_RES_BUS:
		return (EINVAL);
#endif
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		break;
	default:
		return (bus_generic_map_resource(dev, child, type, r, argsp,
		    map));
	}

	/* Resources must be active to be mapped. */
	if (!(rman_get_flags(r) & RF_ACTIVE))
		return (ENXIO);

	resource_init_map_request(&args);
	error = resource_validate_map_request(r, argsp, &args, &start, &length);
	if (error)
		return (error);

	range = generic_pcie_containing_range(dev, type, rman_get_start(r),
	    rman_get_end(r));
	if (range == NULL || range->res == NULL)
		return (ENOENT);

	args.offset = start - range->pci_base;
	args.length = length;
	return (bus_generic_map_resource(dev, child, type, range->res, &args,
	    map));
}

static int
generic_pcie_unmap_resource(device_t dev, device_t child, int type,
    struct resource *r, struct resource_map *map)
{
	struct pcie_range *range;

	switch (type) {
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	case PCI_RES_BUS:
		return (EINVAL);
#endif
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		range = generic_pcie_containing_range(dev, type,
		    rman_get_start(r), rman_get_end(r));
		if (range == NULL || range->res == NULL)
			return (ENOENT);
		r = range->res;
		break;
	default:
		break;
	}
	return (bus_generic_unmap_resource(dev, child, type, r, map));
}

static bus_dma_tag_t
generic_pcie_get_dma_tag(device_t dev, device_t child)
{
	struct generic_pcie_core_softc *sc;

	sc = device_get_softc(dev);
	return (sc->dmat);
}

static device_method_t generic_pcie_methods[] = {
	DEVMETHOD(device_attach,		pci_host_generic_core_attach),
	DEVMETHOD(device_detach,		pci_host_generic_core_detach),

	DEVMETHOD(bus_get_rman,			generic_pcie_get_rman),
	DEVMETHOD(bus_read_ivar,		generic_pcie_read_ivar),
	DEVMETHOD(bus_write_ivar,		generic_pcie_write_ivar),
	DEVMETHOD(bus_alloc_resource,		pci_host_generic_core_alloc_resource),
	DEVMETHOD(bus_adjust_resource,		generic_pcie_adjust_resource),
	DEVMETHOD(bus_activate_resource,	generic_pcie_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	generic_pcie_deactivate_resource),
	DEVMETHOD(bus_release_resource,		pci_host_generic_core_release_resource),
	DEVMETHOD(bus_translate_resource,	generic_pcie_translate_resource),
	DEVMETHOD(bus_map_resource,		generic_pcie_map_resource),
	DEVMETHOD(bus_unmap_resource,		generic_pcie_unmap_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),

	DEVMETHOD(bus_get_dma_tag,		generic_pcie_get_dma_tag),

	/* pcib interface */
	DEVMETHOD(pcib_maxslots,		generic_pcie_maxslots),
	DEVMETHOD(pcib_read_config,		generic_pcie_read_config),
	DEVMETHOD(pcib_write_config,		generic_pcie_write_config),

	DEVMETHOD_END
};

DEFINE_CLASS_0(pcib, generic_pcie_core_driver,
    generic_pcie_methods, sizeof(struct generic_pcie_core_softc));
