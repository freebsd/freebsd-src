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
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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

/* PCIe external MAC root complex driver (PEM) for Cavium Thunder SOC */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/endian.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/smp.h>
#include <machine/intr.h>

#include "thunder_pcie_common.h"
#include "pcib_if.h"

#define	THUNDER_PEM_DEVICE_ID		0xa020
#define	THUNDER_PEM_VENDOR_ID		0x177d
#define	THUNDER_PEM_DESC		"ThunderX PEM"

/* ThunderX specific defines */
#define	THUNDER_PEMn_REG_BASE(unit)	(0x87e0c0000000UL | ((unit) << 24))
#define	PCIERC_CFG002			0x08
#define	PCIERC_CFG006			0x18
#define	PCIERC_CFG032			0x80
#define	PCIERC_CFG006_SEC_BUS(reg)	(((reg) >> 8) & 0xFF)
#define	PEM_CFG_RD_REG_ALIGN(reg)	((reg) & ~0x3)
#define	PEM_CFG_RD_REG_DATA(val)	(((val) >> 32) & 0xFFFFFFFF)
#define	PEM_CFG_RD			0x30
#define	PEM_CFG_LINK_MASK		0x3
#define	PEM_CFG_LINK_RDY		0x3
#define	PEM_CFG_SLIX_TO_REG(slix)	((slix) << 4)
#define	SBNUM_OFFSET			0x8
#define	SBNUM_MASK			0xFF
#define	PEM_ON_REG			0x420
#define	PEM_CTL_STATUS			0x0
#define	PEM_LINK_ENABLE			(1 << 4)
#define	PEM_LINK_DLLA			(1 << 29)
#define	PEM_LINK_LT			(1 << 27)
#define	PEM_BUS_SHIFT			(24)
#define	PEM_SLOT_SHIFT			(19)
#define	PEM_FUNC_SHIFT			(16)
#define	SLIX_S2M_REGX_ACC		0x874001000000UL
#define	SLIX_S2M_REGX_ACC_SIZE		0x1000
#define	SLIX_S2M_REGX_ACC_SPACING	0x001000000000UL
#define	SLI_BASE			0x880000000000UL
#define	SLI_WINDOW_SPACING		0x004000000000UL
#define	SLI_WINDOW_SIZE			0x0000FF000000UL
#define	SLI_PCI_OFFSET			0x001000000000UL
#define	SLI_NODE_SHIFT			(44)
#define	SLI_NODE_MASK			(3)
#define	SLI_GROUP_SHIFT			(40)
#define	SLI_ID_SHIFT			(24)
#define	SLI_ID_MASK			(7)
#define	SLI_PEMS_PER_GROUP		(3)
#define	SLI_GROUPS_PER_NODE		(2)
#define	SLI_PEMS_PER_NODE		(SLI_PEMS_PER_GROUP * SLI_GROUPS_PER_NODE)
#define	SLI_ACC_REG_CNT			(256)

/*
 * Each PEM device creates its own bus with
 * own address translation, so we can adjust bus addresses
 * as we want. To support 32-bit cards let's assume
 * PCI window assignment looks as following:
 *
 * 0x00000000 - 0x000FFFFF	IO
 * 0x00100000 - 0xFFFFFFFF	Memory
 */
#define	PCI_IO_BASE		0x00000000UL
#define	PCI_IO_SIZE		0x00100000UL
#define	PCI_MEMORY_BASE		PCI_IO_SIZE
#define	PCI_MEMORY_SIZE		0xFFF00000UL

struct thunder_pem_softc {
	device_t		dev;
	struct resource		*reg;
	bus_space_tag_t		reg_bst;
	bus_space_handle_t	reg_bsh;
	struct pcie_range	ranges[MAX_RANGES_TUPLES];
	struct rman		mem_rman;
	struct rman		io_rman;
	bus_space_handle_t	pem_sli_base;
	uint32_t		node;
	uint32_t		id;
	uint32_t		sli;
	uint32_t		sli_group;
	uint64_t		sli_window_base;
};

static struct resource * thunder_pem_alloc_resource(device_t, device_t, int,
    int *, u_long, u_long, u_long, u_int);
static int thunder_pem_attach(device_t);
static int thunder_pem_detach(device_t);
static uint64_t thunder_pem_config_reg_read(struct thunder_pem_softc *, int);
static int thunder_pem_link_init(struct thunder_pem_softc *);
static int thunder_pem_maxslots(device_t);
static int thunder_pem_probe(device_t);
static uint32_t thunder_pem_read_config(device_t, u_int, u_int, u_int, u_int,
    int);
static int thunder_pem_read_ivar(device_t, device_t, int, uintptr_t *);
static void thunder_pem_release_all(device_t);
static int thunder_pem_release_resource(device_t, device_t, int, int,
    struct resource *);
static void thunder_pem_slix_s2m_regx_acc_modify(struct thunder_pem_softc *,
    int, int);
static void thunder_pem_write_config(device_t, u_int, u_int, u_int, u_int,
    uint32_t, int);
static int thunder_pem_write_ivar(device_t, device_t, int, uintptr_t);

/* Global handlers for SLI interface */
static bus_space_handle_t sli0_s2m_regx_base = 0;
static bus_space_handle_t sli1_s2m_regx_base = 0;

static device_method_t thunder_pem_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,			thunder_pem_probe),
	DEVMETHOD(device_attach,		thunder_pem_attach),
	DEVMETHOD(device_detach,		thunder_pem_detach),
	DEVMETHOD(pcib_maxslots,		thunder_pem_maxslots),
	DEVMETHOD(pcib_read_config,		thunder_pem_read_config),
	DEVMETHOD(pcib_write_config,		thunder_pem_write_config),
	DEVMETHOD(bus_read_ivar,		thunder_pem_read_ivar),
	DEVMETHOD(bus_write_ivar,		thunder_pem_write_ivar),
	DEVMETHOD(bus_alloc_resource,		thunder_pem_alloc_resource),
	DEVMETHOD(bus_release_resource,		thunder_pem_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,	bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr,		bus_generic_teardown_intr),
	DEVMETHOD(pcib_map_msi,			thunder_common_map_msi),
	DEVMETHOD(pcib_alloc_msix,		thunder_common_alloc_msix),
	DEVMETHOD(pcib_release_msix,		thunder_common_release_msix),
	DEVMETHOD(pcib_alloc_msi,		thunder_common_alloc_msi),
	DEVMETHOD(pcib_release_msi,		thunder_common_release_msi),
	DEVMETHOD_END
};

static driver_t thunder_pem_driver = {
	"pcib",
	thunder_pem_methods,
	sizeof(struct thunder_pem_softc),
};

static int
thunder_pem_maxslots(device_t dev)
{

#if 0
	/* max slots per bus acc. to standard */
	return (PCI_SLOTMAX);
#else
	/*
	 * ARM64TODO Workaround - otherwise an em(4) interface appears to be
	 * present on every PCI function on the bus to which it is connected
	 */
	return (0);
#endif
}

static int
thunder_pem_read_ivar(device_t dev, device_t child, int index,
    uintptr_t *result)
{
	struct thunder_pem_softc *sc;
	int secondary_bus = 0;

	sc = device_get_softc(dev);

	if (index == PCIB_IVAR_BUS) {
		secondary_bus = thunder_pem_config_reg_read(sc, PCIERC_CFG006);
		*result = PCIERC_CFG006_SEC_BUS(secondary_bus);
		return (0);
	}
	if (index == PCIB_IVAR_DOMAIN) {
		*result = sc->id;
		return (0);
	}

	return (ENOENT);
}

static int
thunder_pem_write_ivar(device_t dev, device_t child, int index,
    uintptr_t value)
{

	return (ENOENT);
}

static int
thunder_pem_identify(device_t dev)
{
	struct thunder_pem_softc *sc;
	u_long start;

	sc = device_get_softc(dev);
	start = rman_get_start(sc->reg);

	/* Calculate PEM designations from its address */
	sc->node = (start >> SLI_NODE_SHIFT) & SLI_NODE_MASK;
	sc->id = ((start >> SLI_ID_SHIFT) & SLI_ID_MASK) +
	    (SLI_PEMS_PER_NODE * sc->node);
	sc->sli = sc->id % SLI_PEMS_PER_GROUP;
	sc->sli_group = (sc->id / SLI_PEMS_PER_GROUP) % SLI_GROUPS_PER_NODE;
	sc->sli_window_base = SLI_BASE |
	    (((uint64_t)sc->node) << SLI_NODE_SHIFT) |
	    ((uint64_t)sc->sli_group << SLI_GROUP_SHIFT);
	sc->sli_window_base += SLI_WINDOW_SPACING * sc->sli;

	return (0);
}

static void
thunder_pem_slix_s2m_regx_acc_modify(struct thunder_pem_softc *sc,
    int sli_group, int slix)
{
	uint64_t regval;
	bus_space_handle_t handle = 0;

	KASSERT(slix >= 0 && slix <= SLI_ACC_REG_CNT, ("Invalid SLI index"));

	if (sli_group == 0)
		handle = sli0_s2m_regx_base;
	else if (sli_group == 1)
		handle = sli1_s2m_regx_base;
	else
		device_printf(sc->dev, "SLI group is not correct\n");

	if (handle) {
		/* Clear lower 32-bits of the SLIx register */
		regval = bus_space_read_8(sc->reg_bst, handle,
		    PEM_CFG_SLIX_TO_REG(slix));
		regval &= ~(0xFFFFFFFFUL);
		bus_space_write_8(sc->reg_bst, handle,
		    PEM_CFG_SLIX_TO_REG(slix), regval);
	}
}

static int
thunder_pem_link_init(struct thunder_pem_softc *sc)
{
	uint64_t regval;

	/* check whether PEM is safe to access. */
	regval = bus_space_read_8(sc->reg_bst, sc->reg_bsh, PEM_ON_REG);
	if ((regval & PEM_CFG_LINK_MASK) != PEM_CFG_LINK_RDY) {
		device_printf(sc->dev, "PEM%d is not ON\n", sc->id);
		return (ENXIO);
	}

	regval = bus_space_read_8(sc->reg_bst, sc->reg_bsh, PEM_CTL_STATUS);
	regval |= PEM_LINK_ENABLE;
	bus_space_write_8(sc->reg_bst, sc->reg_bsh, PEM_CTL_STATUS, regval);

	/* Wait 1ms as per Cavium specification */
	DELAY(1000);

	regval = thunder_pem_config_reg_read(sc, PCIERC_CFG032);

	if (((regval & PEM_LINK_DLLA) == 0) || ((regval & PEM_LINK_LT) != 0)) {
		device_printf(sc->dev, "PCIe RC: Port %d Link Timeout\n",
		    sc->id);
		return (ENXIO);
	}

	return (0);
}

static int
thunder_pem_init(struct thunder_pem_softc *sc)
{
	int i, retval = 0;

	retval = thunder_pem_link_init(sc);
	if (retval) {
		device_printf(sc->dev, "%s failed\n", __func__);
		return retval;
	}

	retval = bus_space_map(sc->reg_bst, sc->sli_window_base,
	    SLI_WINDOW_SIZE, 0, &sc->pem_sli_base);
	if (retval) {
		device_printf(sc->dev,
		    "Unable to map RC%d pem_addr base address", sc->id);
		return (ENOMEM);
	}

	/* To support 32-bit PCIe devices, set S2M_REGx_ACC[BA]=0x0 */
	for (i = 0; i < SLI_ACC_REG_CNT; i++) {
		thunder_pem_slix_s2m_regx_acc_modify(sc, sc->sli_group, i);
	}

	return (retval);
}

static uint64_t
thunder_pem_config_reg_read(struct thunder_pem_softc *sc, int reg)
{
	uint64_t data;

	/* Write to ADDR register */
	bus_space_write_8(sc->reg_bst, sc->reg_bsh, PEM_CFG_RD,
	    PEM_CFG_RD_REG_ALIGN(reg));
	bus_space_barrier(sc->reg_bst, sc->reg_bsh, PEM_CFG_RD, 8,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	/* Read from DATA register */
	data = PEM_CFG_RD_REG_DATA(bus_space_read_8(sc->reg_bst, sc->reg_bsh,
	    PEM_CFG_RD));

	return (data);
}

static uint32_t
thunder_pem_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	uint64_t offset;
	uint32_t data;
	struct thunder_pem_softc *sc;
	bus_space_tag_t	t;
	bus_space_handle_t h;

	if ((bus > PCI_BUSMAX) || (slot > PCI_SLOTMAX) ||
	    (func > PCI_FUNCMAX) || (reg > PCIE_REGMAX))
		return (~0U);

	sc = device_get_softc(dev);

	/* Calculate offset */
	offset = (bus << PEM_BUS_SHIFT) | (slot << PEM_SLOT_SHIFT) |
	    (func << PEM_FUNC_SHIFT) | reg;
	t = sc->reg_bst;
	h = sc->pem_sli_base;

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
thunder_pem_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	uint64_t offset;
	struct thunder_pem_softc *sc;
	bus_space_tag_t	t;
	bus_space_handle_t h;

	if ((bus > PCI_BUSMAX) || (slot > PCI_SLOTMAX) ||
	    (func > PCI_FUNCMAX) || (reg > PCIE_REGMAX))
		return;

	sc = device_get_softc(dev);

	/* Calculate offset */
	offset = (bus << PEM_BUS_SHIFT) | (slot << PEM_SLOT_SHIFT) |
	    (func << PEM_FUNC_SHIFT) | reg;
	t = sc->reg_bst;
	h = sc->pem_sli_base;

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

static struct resource *
thunder_pem_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct thunder_pem_softc *sc = device_get_softc(dev);
	struct rman *rm = NULL;
	struct resource *res;
	device_t parent_dev;

	switch (type) {
	case SYS_RES_IOPORT:
		rm = &sc->io_rman;
		break;
	case SYS_RES_MEMORY:
		rm = &sc->mem_rman;
		break;
	default:
		/* Find parent device. On ThunderX we know an exact path. */
		parent_dev = device_get_parent(device_get_parent(dev));
		return (BUS_ALLOC_RESOURCE(parent_dev, dev, type, rid, start,
		    end, count, flags));
	};

	if ((start == 0UL) && (end == ~0UL)) {
		device_printf(dev,
		    "Cannot allocate resource with unspecified range\n");
		goto fail;
	}

	/* Translate PCI address to host PHYS */
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

	if (flags & RF_ACTIVE)
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
thunder_pem_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *res)
{
	device_t parent_dev;

	/* Find parent device. On ThunderX we know an exact path. */
	parent_dev = device_get_parent(device_get_parent(dev));

	if ((type != SYS_RES_MEMORY) && (type != SYS_RES_IOPORT))
		return (BUS_RELEASE_RESOURCE(parent_dev, child,
		    type, rid, res));

	return (rman_release_resource(res));
}

static int
thunder_pem_probe(device_t dev)
{
	uint16_t pci_vendor_id;
	uint16_t pci_device_id;

	pci_vendor_id = pci_get_vendor(dev);
	pci_device_id = pci_get_device(dev);

	if ((pci_vendor_id == THUNDER_PEM_VENDOR_ID) &&
	    (pci_device_id == THUNDER_PEM_DEVICE_ID)) {
		device_set_desc_copy(dev, THUNDER_PEM_DESC);
		return (0);
	}

	return (ENXIO);
}

static int
thunder_pem_attach(device_t dev)
{
	struct thunder_pem_softc *sc;
	int error;
	int rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Allocate memory for BAR(0) */
	rid = PCIR_BAR(0);
	sc->reg = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (sc->reg == NULL) {
		device_printf(dev, "Failed to allocate resource\n");
		return (ENXIO);
	}
	sc->reg_bst = rman_get_bustag(sc->reg);
	sc->reg_bsh = rman_get_bushandle(sc->reg);

	/* Map SLI, do it only once */
	if (!sli0_s2m_regx_base) {
		bus_space_map(sc->reg_bst, SLIX_S2M_REGX_ACC,
		    SLIX_S2M_REGX_ACC_SIZE, 0, &sli0_s2m_regx_base);
	}
	if (!sli1_s2m_regx_base) {
		bus_space_map(sc->reg_bst, SLIX_S2M_REGX_ACC +
		    SLIX_S2M_REGX_ACC_SPACING, SLIX_S2M_REGX_ACC_SIZE, 0,
		    &sli1_s2m_regx_base);
	}

	if ((sli0_s2m_regx_base == 0) || (sli1_s2m_regx_base == 0)) {
		device_printf(dev,
		    "bus_space_map failed to map slix_s2m_regx_base\n");
		goto fail;
	}

	/* Identify PEM */
	if (thunder_pem_identify(dev) != 0)
		goto fail;

	/* Initialize rman and allocate regions */
	sc->mem_rman.rm_type = RMAN_ARRAY;
	sc->mem_rman.rm_descr = "PEM PCIe Memory";
	error = rman_init(&sc->mem_rman);
	if (error != 0) {
		device_printf(dev, "memory rman_init() failed. error = %d\n",
		    error);
		goto fail;
	}
	sc->io_rman.rm_type = RMAN_ARRAY;
	sc->io_rman.rm_descr = "PEM PCIe IO";
	error = rman_init(&sc->io_rman);
	if (error != 0) {
		device_printf(dev, "IO rman_init() failed. error = %d\n",
		    error);
		goto fail_mem;
	}

	/* Fill memory window */
	sc->ranges[0].pci_base = PCI_MEMORY_BASE;
	sc->ranges[0].size = PCI_MEMORY_SIZE;
	sc->ranges[0].phys_base = sc->sli_window_base + SLI_PCI_OFFSET +
	    sc->ranges[0].pci_base;
	rman_manage_region(&sc->mem_rman, sc->ranges[0].phys_base,
	    sc->ranges[0].phys_base + sc->ranges[0].size - 1);

	/* Fill IO window */
	sc->ranges[1].pci_base = PCI_IO_BASE;
	sc->ranges[1].size = PCI_IO_SIZE;
	sc->ranges[1].phys_base = sc->sli_window_base + SLI_PCI_OFFSET +
	    sc->ranges[1].pci_base;
	rman_manage_region(&sc->io_rman, sc->ranges[1].phys_base,
	    sc->ranges[1].phys_base + sc->ranges[1].size - 1);

	if (thunder_pem_init(sc)) {
		device_printf(dev, "Failure during PEM init\n");
		goto fail_io;
	}

	device_add_child(dev, "pci", -1);

	return (bus_generic_attach(dev));

fail_io:
	rman_fini(&sc->io_rman);
fail_mem:
	rman_fini(&sc->mem_rman);
fail:
	bus_free_resource(dev, SYS_RES_MEMORY, sc->reg);
	return (ENXIO);
}

static void
thunder_pem_release_all(device_t dev)
{
	struct thunder_pem_softc *sc;

	sc = device_get_softc(dev);

	rman_fini(&sc->io_rman);
	rman_fini(&sc->mem_rman);

	if (sc->reg != NULL)
		bus_free_resource(dev, SYS_RES_MEMORY, sc->reg);
}

static int
thunder_pem_detach(device_t dev)
{

	thunder_pem_release_all(dev);

	return (0);
}

static devclass_t thunder_pem_devclass;

DRIVER_MODULE(thunder_pem, pci, thunder_pem_driver, thunder_pem_devclass, 0, 0);
MODULE_DEPEND(thunder_pem, pci, 1, 1, 1);
