/*-
 * Copyright (c) 2015 The FreeBSD Foundation
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

/* PCIe root complex driver for Cavium Thunder SOC */
#include "opt_platform.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>
#include <sys/rman.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/cpuset.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_private.h>
#include <machine/cpu.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include "thunder_pcie_common.h"

#include "pcib_if.h"

/* Assembling ECAM Configuration Address */
#define	PCIE_BUS_SHIFT		20
#define	PCIE_SLOT_SHIFT		15
#define	PCIE_FUNC_SHIFT		12
#define	PCIE_BUS_MASK		0xFF
#define	PCIE_SLOT_MASK		0x1F
#define	PCIE_FUNC_MASK		0x07
#define	PCIE_REG_MASK		0xFFF

#define	PCIE_ADDR_OFFSET(bus, slot, func, reg)			\
    ((((bus) & PCIE_BUS_MASK) << PCIE_BUS_SHIFT)	|	\
    (((slot) & PCIE_SLOT_MASK) << PCIE_SLOT_SHIFT)	|	\
    (((func) & PCIE_FUNC_MASK) << PCIE_FUNC_SHIFT)	|	\
    ((reg) & PCIE_REG_MASK))

#define	THUNDER_ECAM0_CFG_BASE	0x848000000000UL
#define	THUNDER_ECAM1_CFG_BASE	0x849000000000UL
#define	THUNDER_ECAM2_CFG_BASE	0x84a000000000UL
#define	THUNDER_ECAM3_CFG_BASE	0x84b000000000UL
#define	THUNDER_ECAM4_CFG_BASE	0x948000000000UL
#define	THUNDER_ECAM5_CFG_BASE	0x949000000000UL
#define	THUNDER_ECAM6_CFG_BASE	0x94a000000000UL
#define	THUNDER_ECAM7_CFG_BASE	0x94b000000000UL

/*
 * ThunderX supports up to 4 ethernet interfaces, so it's good
 * value to use as default for numbers of VFs, since each eth
 * interface represents separate virtual function.
 */
static int thunder_pcie_max_vfs = 4;
SYSCTL_INT(_hw, OID_AUTO, thunder_pcie_max_vfs, CTLFLAG_RWTUN,
    &thunder_pcie_max_vfs, 0, "Max VFs supported by ThunderX internal PCIe");

/* Forward prototypes */
static int thunder_pcie_identify_pcib(device_t);
static int thunder_pcie_maxslots(device_t);
static uint32_t thunder_pcie_read_config(device_t, u_int, u_int, u_int, u_int,
    int);
static int thunder_pcie_read_ivar(device_t, device_t, int, uintptr_t *);
static void thunder_pcie_write_config(device_t, u_int, u_int,
    u_int, u_int, uint32_t, int);
static int thunder_pcie_write_ivar(device_t, device_t, int, uintptr_t);

int
thunder_pcie_attach(device_t dev)
{
	int rid;
	struct thunder_pcie_softc *sc;
	int error;
	int tuple;
	uint64_t base, size;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Identify pcib domain */
	if (thunder_pcie_identify_pcib(dev))
		return (ENXIO);

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "could not map memory.\n");
		return (ENXIO);
	}

	sc->mem_rman.rm_type = RMAN_ARRAY;
	sc->mem_rman.rm_descr = "PCIe Memory";

	/* Initialize rman and allocate memory regions */
	error = rman_init(&sc->mem_rman);
	if (error) {
		device_printf(dev, "rman_init() failed. error = %d\n", error);
		return (error);
	}

	for (tuple = 0; tuple < RANGES_TUPLES_MAX; tuple++) {
		base = sc->ranges[tuple].phys_base;
		size = sc->ranges[tuple].size;
		if ((base == 0) || (size == 0))
			continue; /* empty range element */

		error = rman_manage_region(&sc->mem_rman, base, base + size - 1);
		if (error) {
			device_printf(dev, "rman_manage_region() failed. error = %d\n", error);
			rman_fini(&sc->mem_rman);
			return (error);
		}
	}
	device_add_child(dev, "pci", -1);

	return (bus_generic_attach(dev));
}

static uint32_t
thunder_pcie_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	uint64_t offset;
	uint32_t data;
	struct thunder_pcie_softc *sc;
	bus_space_tag_t	t;
	bus_space_handle_t h;

	if ((bus > PCI_BUSMAX) || (slot > PCI_SLOTMAX) ||
	    (func > PCI_FUNCMAX) || (reg > PCIE_REGMAX))
		return (~0U);

	sc = device_get_softc(dev);

	offset = PCIE_ADDR_OFFSET(bus, slot, func, reg);
	t = rman_get_bustag(sc->res);
	h = rman_get_bushandle(sc->res);

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
thunder_pcie_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	uint64_t offset;
	struct thunder_pcie_softc *sc;
	bus_space_tag_t	t;
	bus_space_handle_t h;

	if ((bus > PCI_BUSMAX) || (slot > PCI_SLOTMAX) ||
	    (func > PCI_FUNCMAX) || (reg > PCIE_REGMAX))
		return ;

	sc = device_get_softc(dev);

	offset = PCIE_ADDR_OFFSET(bus, slot, func, reg);
	t = rman_get_bustag(sc->res);
	h = rman_get_bushandle(sc->res);

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
thunder_pcie_maxslots(device_t dev)
{

	/* max slots per bus acc. to standard */
	return (PCI_SLOTMAX);
}

static int
thunder_pcie_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{
	struct thunder_pcie_softc *sc;

	sc = device_get_softc(dev);

	if (index == PCIB_IVAR_BUS) {
		/* this pcib is always on bus 0 */
		*result = 0;
		return (0);
	}
	if (index == PCIB_IVAR_DOMAIN) {
		*result = sc->ecam;
		return (0);
	}

	return (ENOENT);
}

static int
thunder_pcie_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value)
{

	return (ENOENT);
}

int
thunder_pcie_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{

	if (type != SYS_RES_MEMORY)
		return (bus_generic_release_resource(dev, child,
		    type, rid, res));

	return (rman_release_resource(res));
}

struct resource *
thunder_pcie_alloc_resource(device_t dev, device_t child, int type, int *rid,
    rman_res_t start, rman_res_t end, rman_res_t count, u_int flags)
{
	struct thunder_pcie_softc *sc = device_get_softc(dev);
	struct rman *rm = NULL;
	struct resource *res;
	pci_addr_t map, testval;

	switch (type) {
	case SYS_RES_IOPORT:
		goto fail;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->mem_rman;
		break;
	default:
		return (bus_generic_alloc_resource(dev, child,
		    type, rid, start, end, count, flags));
	};

	if (RMAN_IS_DEFAULT_RANGE(start, end)) {

		/* Read BAR manually to get resource address and size */
		pci_read_bar(child, *rid, &map, &testval, NULL);

		/* Mask the information bits */
		if (PCI_BAR_MEM(map))
			map &= PCIM_BAR_MEM_BASE;
		else
			map &= PCIM_BAR_IO_BASE;

		if (PCI_BAR_MEM(testval))
			testval &= PCIM_BAR_MEM_BASE;
		else
			testval &= PCIM_BAR_IO_BASE;

		start = map;
		count = (~testval) + 1;
		/*
		 * Internal ThunderX devices supports up to 3 64-bit BARs.
		 * If we're allocating anything above, that means upper layer
		 * wants us to allocate VF-BAR. In that case reserve bigger
		 * slice to make a room for other VFs adjacent to this one.
		 */
		if (*rid > PCIR_BAR(5))
			count = count * thunder_pcie_max_vfs;
		end = start + count - 1;
	}

	/* Convert input BUS address to required PHYS */
	if (range_addr_is_pci(sc->ranges, start, count) == 0)
		goto fail;
	start = range_addr_pci_to_phys(sc->ranges, start);
	end = start + count - 1;

	if (bootverbose) {
		device_printf(dev,
		    "rman_reserve_resource: start=%#lx, end=%#lx, count=%#lx\n",
		    start, end, count);
	}

	res = rman_reserve_resource(rm, start, end, count, flags, child);
	if (res == NULL)
		goto fail;

	rman_set_rid(res, *rid);

	if ((flags & RF_ACTIVE) != 0)
		if (bus_activate_resource(child, type, *rid, res)) {
			rman_release_resource(res);
			goto fail;
		}

	return (res);

fail:
	if (bootverbose) {
		device_printf(dev, "%s FAIL: type=%d, rid=%d, "
		    "start=%016lx, end=%016lx, count=%016lx, flags=%x\n",
		    __func__, type, *rid, start, end, count, flags);
	}

	return (NULL);
}

static int
thunder_pcie_identify_pcib(device_t dev)
{
	struct thunder_pcie_softc *sc;
	rman_res_t start;

	sc = device_get_softc(dev);
	start = bus_get_resource_start(dev, SYS_RES_MEMORY, 0);

	switch(start) {
	case THUNDER_ECAM0_CFG_BASE:
		sc->ecam = 0;
		break;
	case THUNDER_ECAM1_CFG_BASE:
		sc->ecam = 1;
		break;
	case THUNDER_ECAM2_CFG_BASE:
		sc->ecam = 2;
		break;
	case THUNDER_ECAM3_CFG_BASE:
		sc->ecam = 3;
		break;
	case THUNDER_ECAM4_CFG_BASE:
		sc->ecam = 4;
		break;
	case THUNDER_ECAM5_CFG_BASE:
		sc->ecam = 5;
		break;
	case THUNDER_ECAM6_CFG_BASE:
		sc->ecam = 6;
		break;
	case THUNDER_ECAM7_CFG_BASE:
		sc->ecam = 7;
		break;
	default:
		device_printf(dev,
		    "error: incorrect resource address=%#lx.\n", start);
		return (ENXIO);
	}
	return (0);
}

static device_method_t thunder_pcie_methods[] = {
	DEVMETHOD(pcib_maxslots,		thunder_pcie_maxslots),
	DEVMETHOD(pcib_read_config,		thunder_pcie_read_config),
	DEVMETHOD(pcib_write_config,		thunder_pcie_write_config),
	DEVMETHOD(bus_read_ivar,		thunder_pcie_read_ivar),
	DEVMETHOD(bus_write_ivar,		thunder_pcie_write_ivar),
	DEVMETHOD(bus_alloc_resource,		thunder_pcie_alloc_resource),
	DEVMETHOD(bus_release_resource,		thunder_pcie_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),

	DEVMETHOD(pcib_map_msi,			arm_map_msi),
	DEVMETHOD(pcib_alloc_msix,		arm_alloc_msix),
	DEVMETHOD(pcib_release_msix,		arm_release_msix),
	DEVMETHOD(pcib_alloc_msi,		arm_alloc_msi),
	DEVMETHOD(pcib_release_msi,		arm_release_msi),

	DEVMETHOD_END
};

DEFINE_CLASS_0(pcib, thunder_pcie_driver, thunder_pcie_methods,
    sizeof(struct thunder_pcie_softc));
