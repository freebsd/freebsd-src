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
__FBSDID("$FreeBSD$");

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
	struct generic_pcie_core_softc *sc;
	uint64_t phys_base;
	uint64_t pci_base;
	uint64_t size;
	int error;
	int rid, tuple;

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

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not allocate memory.\n");
		error = ENXIO;
		goto err_resource;
	}

	sc->bst = rman_get_bustag(sc->res);
	sc->bsh = rman_get_bushandle(sc->res);

	sc->has_pmem = false;
	sc->pmem_rman.rm_type = RMAN_ARRAY;
	sc->pmem_rman.rm_descr = "PCIe Prefetch Memory";

	sc->mem_rman.rm_type = RMAN_ARRAY;
	sc->mem_rman.rm_descr = "PCIe Memory";

	sc->io_rman.rm_type = RMAN_ARRAY;
	sc->io_rman.rm_descr = "PCIe IO window";

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
		if (phys_base == 0 || size == 0)
			continue; /* empty range element */
		switch (FLAG_TYPE(sc->ranges[tuple].flags)) {
		case FLAG_TYPE_PMEM:
			sc->has_pmem = true;
			error = rman_manage_region(&sc->pmem_rman,
			   pci_base, pci_base + size - 1);
			break;
		case FLAG_TYPE_MEM:
			error = rman_manage_region(&sc->mem_rman,
			   pci_base, pci_base + size - 1);
			break;
		case FLAG_TYPE_IO:
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
	}

	return (0);

err_rman_manage:
	rman_fini(&sc->io_rman);
err_io_rman:
	rman_fini(&sc->mem_rman);
err_mem_rman:
	rman_fini(&sc->pmem_rman);
err_pmem_rman:
	bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->res);
err_resource:
	bus_dma_tag_destroy(sc->dmat);
	return (error);
}

int
pci_host_generic_core_detach(device_t dev)
{
	struct generic_pcie_core_softc *sc;
	int error;

	sc = device_get_softc(dev);

	error = bus_generic_detach(dev);
	if (error != 0)
		return (error);

	rman_fini(&sc->io_rman);
	rman_fini(&sc->mem_rman);
	rman_fini(&sc->pmem_rman);
	bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->res);
	bus_dma_tag_destroy(sc->dmat);

	return (0);
}

static uint32_t
generic_pcie_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	struct generic_pcie_core_softc *sc;
	bus_space_handle_t h;
	bus_space_tag_t	t;
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
	t = sc->bst;
	h = sc->bsh;

	switch (bytes) {
	case 1:
		data = bus_space_read_1(t, h, offset);
		break;
	case 2:
		data = le16toh(bus_space_read_2(t, h, offset));
		break;
	case 4:
		data = le32toh(bus_space_read_4(t, h, offset));
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
	bus_space_handle_t h;
	bus_space_tag_t t;
	uint64_t offset;

	sc = device_get_softc(dev);
	if ((bus < sc->bus_start) || (bus > sc->bus_end))
		return;
	if ((slot > PCI_SLOTMAX) || (func > PCI_FUNCMAX) ||
	    (reg > PCIE_REGMAX))
		return;

	offset = PCIE_ADDR_OFFSET(bus - sc->bus_start, slot, func, reg);

	t = sc->bst;
	h = sc->bsh;

	switch (bytes) {
	case 1:
		bus_space_write_1(t, h, offset, val);
		break;
	case 2:
		bus_space_write_2(t, h, offset, htole16(val));
		break;
	case 4:
		bus_space_write_4(t, h, offset, htole32(val));
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
generic_pcie_rman(struct generic_pcie_core_softc *sc, int type, int flags)
{

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
	struct generic_pcie_core_softc *sc;
	struct rman *rm;
	int error;

	sc = device_get_softc(dev);

#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	if (type == PCI_RES_BUS) {
		return (pci_domain_release_bus(sc->ecam, child, rid, res));
	}
#endif

	rm = generic_pcie_rman(sc, type, rman_get_flags(res));
	if (rm != NULL) {
		KASSERT(rman_is_region_manager(res, rm), ("rman mismatch"));
		if (rman_get_flags(res) & RF_ACTIVE) {
			error = bus_deactivate_resource(child, type, rid, res);
			if (error)
				return (error);
		}
		return (rman_release_resource(res));
	}

	return (bus_generic_release_resource(dev, child, type, rid, res));
}

static int
generic_pcie_translate_resource_common(device_t dev, int type, rman_res_t start,
    rman_res_t end, rman_res_t *new_start, rman_res_t *new_end)
{
	struct generic_pcie_core_softc *sc;
	uint64_t phys_base;
	uint64_t pci_base;
	uint64_t size;
	int i, space;
	bool found;

	sc = device_get_softc(dev);
	/* Translate the address from a PCI address to a physical address */
	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
		found = false;
		for (i = 0; i < MAX_RANGES_TUPLES; i++) {
			pci_base = sc->ranges[i].pci_base;
			phys_base = sc->ranges[i].phys_base;
			size = sc->ranges[i].size;

			if (start < pci_base || start >= pci_base + size)
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
				space = -1;
				continue;
			}

			if (type == space) {
				*new_start = start - pci_base + phys_base;
				*new_end = end - pci_base + phys_base;
				found = true;
				break;
			}
		}
		break;
	default:
		/* No translation for non-memory types */
		*new_start = start;
		*new_end = end;
		found = true;
		break;
	}

	return (found ? 0 : ENOENT);
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
	struct generic_pcie_core_softc *sc;
	struct resource *res;
	struct rman *rm;
	rman_res_t phys_start, phys_end;

	sc = device_get_softc(dev);

#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	if (type == PCI_RES_BUS) {
		return (pci_domain_alloc_bus(sc->ecam, child, rid, start, end,
		    count, flags));
	}
#endif

	rm = generic_pcie_rman(sc, type, flags);
	if (rm == NULL)
		return (BUS_ALLOC_RESOURCE(device_get_parent(dev), child,
		    type, rid, start, end, count, flags));

	/* Translate the address from a PCI address to a physical address */
	if (generic_pcie_translate_resource_common(dev, type, start, end,
	    &phys_start, &phys_end) != 0) {
		device_printf(dev,
		    "Failed to translate resource %jx-%jx type %x for %s\n",
		    (uintmax_t)start, (uintmax_t)end, type,
		    device_get_nameunit(child));
		return (NULL);
	}

	if (bootverbose) {
		device_printf(dev,
		    "rman_reserve_resource: start=%#jx, end=%#jx, count=%#jx\n",
		    start, end, count);
	}

	res = rman_reserve_resource(rm, start, end, count, flags, child);
	if (res == NULL)
		goto fail;

	rman_set_rid(res, *rid);

	if (flags & RF_ACTIVE)
		if (bus_activate_resource(child, type, *rid, res)) {
			rman_release_resource(res);
			goto fail;
		}

	return (res);

fail:
	device_printf(dev, "%s FAIL: type=%d, rid=%d, "
	    "start=%016jx, end=%016jx, count=%016jx, flags=%x\n",
	    __func__, type, *rid, start, end, count, flags);

	return (NULL);
}

static int
generic_pcie_activate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	rman_res_t start, end;
	int res;

	if ((res = rman_activate_resource(r)) != 0)
		return (res);

	start = rman_get_start(r);
	end = rman_get_end(r);
	res = generic_pcie_translate_resource_common(dev, type, start, end,
	    &start, &end);
	if (res != 0) {
		rman_deactivate_resource(r);
		return (res);
	}
	rman_set_start(r, start);
	rman_set_end(r, end);

	return (BUS_ACTIVATE_RESOURCE(device_get_parent(dev), child, type,
	    rid, r));
}

static int
generic_pcie_deactivate_resource(device_t dev, device_t child, int type,
    int rid, struct resource *r)
{
	int res;

	if ((res = rman_deactivate_resource(r)) != 0)
		return (res);

	switch (type) {
	case SYS_RES_IOPORT:
	case SYS_RES_MEMORY:
	case SYS_RES_IRQ:
		res = BUS_DEACTIVATE_RESOURCE(device_get_parent(dev), child,
		    type, rid, r);
		break;
	default:
		break;
	}

	return (res);
}

static int
generic_pcie_adjust_resource(device_t dev, device_t child, int type,
    struct resource *res, rman_res_t start, rman_res_t end)
{
	struct generic_pcie_core_softc *sc;
	struct rman *rm;

	sc = device_get_softc(dev);
#if defined(NEW_PCIB) && defined(PCI_RES_BUS)
	if (type == PCI_RES_BUS)
		return (pci_domain_adjust_bus(sc->ecam, child, res, start,
		    end));
#endif

	rm = generic_pcie_rman(sc, type, rman_get_flags(res));
	if (rm != NULL)
		return (rman_adjust_resource(res, start, end));
	return (bus_generic_adjust_resource(dev, child, type, res, start, end));
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

	DEVMETHOD(bus_read_ivar,		generic_pcie_read_ivar),
	DEVMETHOD(bus_write_ivar,		generic_pcie_write_ivar),
	DEVMETHOD(bus_alloc_resource,		pci_host_generic_core_alloc_resource),
	DEVMETHOD(bus_adjust_resource,		generic_pcie_adjust_resource),
	DEVMETHOD(bus_activate_resource,	generic_pcie_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	generic_pcie_deactivate_resource),
	DEVMETHOD(bus_release_resource,		pci_host_generic_core_release_resource),
	DEVMETHOD(bus_translate_resource,	generic_pcie_translate_resource),
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
