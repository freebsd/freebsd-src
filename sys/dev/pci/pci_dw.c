/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Michal Meloun <mmel@FreeBSD.org>
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
 *
 */

/* Base class for all Synopsys DesignWare PCI/PCIe drivers */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/devmap.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_pci.h>
#include <dev/ofw/ofwpci.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcib_private.h>
#include <dev/pci/pci_dw.h>

#include "pcib_if.h"
#include "pci_dw_if.h"

#ifdef DEBUG
#define	debugf(fmt, args...) do { printf(fmt,##args); } while (0)
#else
#define	debugf(fmt, args...)
#endif

#define	DBI_WR1(sc, reg, val)	pci_dw_dbi_wr1((sc)->dev, reg, val)
#define	DBI_WR2(sc, reg, val)	pci_dw_dbi_wr2((sc)->dev, reg, val)
#define	DBI_WR4(sc, reg, val)	pci_dw_dbi_wr4((sc)->dev, reg, val)
#define	DBI_RD1(sc, reg)	pci_dw_dbi_rd1((sc)->dev, reg)
#define	DBI_RD2(sc, reg)	pci_dw_dbi_rd2((sc)->dev, reg)
#define	DBI_RD4(sc, reg)	pci_dw_dbi_rd4((sc)->dev, reg)

#define	IATU_UR_WR4(sc, reg, val)	\
    bus_write_4((sc)->iatu_ur_res, (sc)->iatu_ur_offset + (reg), (val))
#define	IATU_UR_RD4(sc, reg)		\
    bus_read_4((sc)->iatu_ur_res, (sc)->iatu_ur_offset + (reg))

#define	PCI_BUS_SHIFT		20
#define	PCI_SLOT_SHIFT		15
#define	PCI_FUNC_SHIFT		12
#define	PCI_BUS_MASK		0xFF
#define	PCI_SLOT_MASK		0x1F
#define	PCI_FUNC_MASK		0x07
#define	PCI_REG_MASK		0xFFF

#define	IATU_CFG_BUS(bus)	((uint64_t)((bus)  & 0xff) << 24)
#define	IATU_CFG_SLOT(slot)	((uint64_t)((slot) & 0x1f) << 19)
#define	IATU_CFG_FUNC(func)	((uint64_t)((func) & 0x07) << 16)

static uint32_t
pci_dw_dbi_read(device_t dev, u_int reg, int width)
{
	struct pci_dw_softc *sc;

	sc = device_get_softc(dev);
	MPASS(sc->dbi_res != NULL);

	switch (width) {
	case 4:
		return (bus_read_4(sc->dbi_res, reg));
	case 2:
		return (bus_read_2(sc->dbi_res, reg));
	case 1:
		return (bus_read_1(sc->dbi_res, reg));
	default:
		device_printf(sc->dev, "Unsupported width: %d\n", width);
		return (0xFFFFFFFF);
	}
}

static void
pci_dw_dbi_write(device_t dev, u_int reg, uint32_t val, int width)
{
	struct pci_dw_softc *sc;

	sc = device_get_softc(dev);
	MPASS(sc->dbi_res != NULL);

	switch (width) {
	case 4:
		bus_write_4(sc->dbi_res, reg, val);
		break;
	case 2:
		bus_write_2(sc->dbi_res, reg, val);
		break;
	case 1:
		bus_write_1(sc->dbi_res, reg, val);
		break;
	default:
		device_printf(sc->dev, "Unsupported width: %d\n", width);
		break;
	}
}

static void
pci_dw_dbi_protect(struct pci_dw_softc *sc, bool protect)
{
	uint32_t reg;

	reg = DBI_RD4(sc, DW_MISC_CONTROL_1);
	if (protect)
		reg &= ~DBI_RO_WR_EN;
	else
		reg |= DBI_RO_WR_EN;
	DBI_WR4(sc, DW_MISC_CONTROL_1, reg);
}

static bool
pci_dw_check_dev(struct pci_dw_softc *sc, u_int bus, u_int slot, u_int func,
    u_int reg)
{
	bool status;
	int rv;

	if (bus < sc->bus_start || bus > sc->bus_end || slot > PCI_SLOTMAX ||
	    func > PCI_FUNCMAX || reg > PCIE_REGMAX)
		return (false);

	/* link is needed for access to all non-root busses */
	if (bus != sc->root_bus) {
		rv = PCI_DW_GET_LINK(sc->dev, &status);
		if (rv != 0 || !status)
			return (false);
		return (true);
	}

	/* we have only 1 device with 1 function root port */
	if (slot > 0 || func > 0)
		return (false);
	return (true);
}

static bool
pci_dw_detect_atu_unroll(struct pci_dw_softc *sc)
{
	return (DBI_RD4(sc, DW_IATU_VIEWPORT) == 0xFFFFFFFFU);
}

static int
pci_dw_detect_out_atu_regions_unroll(struct pci_dw_softc *sc)
{
	int num_regions, i;
	uint32_t reg;

	num_regions = sc->iatu_ur_size / DW_IATU_UR_STEP;

	for (i = 0; i < num_regions; ++i) {
		IATU_UR_WR4(sc, DW_IATU_UR_REG(i, LWR_TARGET_ADDR),
		    0x12340000);
		reg = IATU_UR_RD4(sc, DW_IATU_UR_REG(i, LWR_TARGET_ADDR));
		if (reg != 0x12340000)
			break;
	}

	sc->num_out_regions = i;

	return (0);
}

static int
pci_dw_detect_out_atu_regions_legacy(struct pci_dw_softc *sc)
{
	int num_viewports, i;
	uint32_t reg;

	/* Find out how many viewports there are in total */
	DBI_WR4(sc, DW_IATU_VIEWPORT, IATU_REGION_INDEX(~0U));
	reg = DBI_RD4(sc, DW_IATU_VIEWPORT);
	if (reg > IATU_REGION_INDEX(~0U)) {
		device_printf(sc->dev,
		    "Cannot detect number of output iATU regions; read %#x\n",
		    reg);
		return (ENXIO);
	}

	num_viewports = reg + 1;

	/*
	 * Find out how many of them are outbound by seeing whether a dummy
	 * page-aligned address sticks.
	 */
	for (i = 0; i < num_viewports; ++i) {
		DBI_WR4(sc, DW_IATU_VIEWPORT, IATU_REGION_INDEX(i));
		DBI_WR4(sc, DW_IATU_LWR_TARGET_ADDR, 0x12340000);
		reg = DBI_RD4(sc, DW_IATU_LWR_TARGET_ADDR);
		if (reg != 0x12340000)
			break;
	}

	sc->num_out_regions = i;

	return (0);
}

static int
pci_dw_detect_out_atu_regions(struct pci_dw_softc *sc)
{
	if (sc->iatu_ur_res)
		return (pci_dw_detect_out_atu_regions_unroll(sc));
	else
		return (pci_dw_detect_out_atu_regions_legacy(sc));
}

static int
pci_dw_map_out_atu_unroll(struct pci_dw_softc *sc, int idx, int type,
    uint64_t pa, uint64_t pci_addr, uint32_t size)
{
	uint32_t reg;
	int i;

	if (size == 0)
		return (0);

	IATU_UR_WR4(sc, DW_IATU_UR_REG(idx, LWR_BASE_ADDR),
	    pa & 0xFFFFFFFF);
	IATU_UR_WR4(sc, DW_IATU_UR_REG(idx, UPPER_BASE_ADDR),
	    (pa >> 32) & 0xFFFFFFFF);
	IATU_UR_WR4(sc, DW_IATU_UR_REG(idx, LIMIT_ADDR),
	    (pa + size - 1) & 0xFFFFFFFF);
	IATU_UR_WR4(sc, DW_IATU_UR_REG(idx, LWR_TARGET_ADDR),
	    pci_addr & 0xFFFFFFFF);
	IATU_UR_WR4(sc, DW_IATU_UR_REG(idx, UPPER_TARGET_ADDR),
	    (pci_addr  >> 32) & 0xFFFFFFFF);
	IATU_UR_WR4(sc, DW_IATU_UR_REG(idx, CTRL1),
	    IATU_CTRL1_TYPE(type));
	IATU_UR_WR4(sc, DW_IATU_UR_REG(idx, CTRL2),
	    IATU_CTRL2_REGION_EN);

	/* Wait until setup becomes valid */
	for (i = 10; i > 0; i--) {
		reg = IATU_UR_RD4(sc, DW_IATU_UR_REG(idx, CTRL2));
		if (reg & IATU_CTRL2_REGION_EN)
			return (0);
		DELAY(5);
	}

	device_printf(sc->dev,
	    "Cannot map outbound region %d in unroll mode iATU\n", idx);
	return (ETIMEDOUT);
}

static int
pci_dw_map_out_atu_legacy(struct pci_dw_softc *sc, int idx, int type,
    uint64_t pa, uint64_t pci_addr, uint32_t size)
{
	uint32_t reg;
	int i;

	if (size == 0)
		return (0);

	DBI_WR4(sc, DW_IATU_VIEWPORT, IATU_REGION_INDEX(idx));
	DBI_WR4(sc, DW_IATU_LWR_BASE_ADDR, pa & 0xFFFFFFFF);
	DBI_WR4(sc, DW_IATU_UPPER_BASE_ADDR, (pa >> 32) & 0xFFFFFFFF);
	DBI_WR4(sc, DW_IATU_LIMIT_ADDR, (pa + size - 1) & 0xFFFFFFFF);
	DBI_WR4(sc, DW_IATU_LWR_TARGET_ADDR, pci_addr & 0xFFFFFFFF);
	DBI_WR4(sc, DW_IATU_UPPER_TARGET_ADDR, (pci_addr  >> 32) & 0xFFFFFFFF);
	DBI_WR4(sc, DW_IATU_CTRL1, IATU_CTRL1_TYPE(type));
	DBI_WR4(sc, DW_IATU_CTRL2, IATU_CTRL2_REGION_EN);

	/* Wait until setup becomes valid */
	for (i = 10; i > 0; i--) {
		reg = DBI_RD4(sc, DW_IATU_CTRL2);
		if (reg & IATU_CTRL2_REGION_EN)
			return (0);
		DELAY(5);
	}

	device_printf(sc->dev,
	    "Cannot map outbound region %d in legacy mode iATU\n", idx);
	return (ETIMEDOUT);
}

/* Map one outbound ATU region */
static int
pci_dw_map_out_atu(struct pci_dw_softc *sc, int idx, int type,
    uint64_t pa, uint64_t pci_addr, uint32_t size)
{
	if (sc->iatu_ur_res)
		return (pci_dw_map_out_atu_unroll(sc, idx, type, pa,
		    pci_addr, size));
	else
		return (pci_dw_map_out_atu_legacy(sc, idx, type, pa,
		    pci_addr, size));
}

static int
pci_dw_setup_hw(struct pci_dw_softc *sc)
{
	uint32_t reg;
	int rv, i;

	pci_dw_dbi_protect(sc, false);

	/* Setup config registers */
	DBI_WR1(sc, PCIR_CLASS, PCIC_BRIDGE);
	DBI_WR1(sc, PCIR_SUBCLASS, PCIS_BRIDGE_PCI);
	DBI_WR4(sc, PCIR_BAR(0), 4);
	DBI_WR4(sc, PCIR_BAR(1), 0);
	DBI_WR1(sc, PCIR_INTPIN, 1);
	DBI_WR1(sc, PCIR_PRIBUS_1, sc->root_bus);
	DBI_WR1(sc, PCIR_SECBUS_1, sc->sub_bus);
	DBI_WR1(sc, PCIR_SUBBUS_1, sc->bus_end);
	DBI_WR2(sc, PCIR_COMMAND,
	   PCIM_CMD_PORTEN | PCIM_CMD_MEMEN |
	   PCIM_CMD_BUSMASTEREN | PCIM_CMD_SERRESPEN);
	pci_dw_dbi_protect(sc, true);

	/* Setup outbound memory windows */
	for (i = 0; i < min(sc->num_mem_ranges, sc->num_out_regions - 1); ++i) {
		rv = pci_dw_map_out_atu(sc, i + 1, IATU_CTRL1_TYPE_MEM,
		    sc->mem_ranges[i].host, sc->mem_ranges[i].pci,
		    sc->mem_ranges[i].size);
		if (rv != 0)
			return (rv);
	}

	/* If we have enough regions ... */
	if (sc->num_mem_ranges + 1 < sc->num_out_regions &&
	    sc->io_range.size != 0) {
		/* Setup outbound I/O window */
		rv = pci_dw_map_out_atu(sc, sc->num_mem_ranges + 1,
		    IATU_CTRL1_TYPE_IO, sc->io_range.host, sc->io_range.pci,
		    sc->io_range.size);
		if (rv != 0)
			return (rv);
	}

	/* Adjust number of lanes */
	reg = DBI_RD4(sc, DW_PORT_LINK_CTRL);
	reg &= ~PORT_LINK_CAPABLE(~0);
	switch (sc->num_lanes) {
	case 1:
		reg |= PORT_LINK_CAPABLE(PORT_LINK_CAPABLE_1);
		break;
	case 2:
		reg |= PORT_LINK_CAPABLE(PORT_LINK_CAPABLE_2);
		break;
	case 4:
		reg |= PORT_LINK_CAPABLE(PORT_LINK_CAPABLE_4);
		break;
	case 8:
		reg |= PORT_LINK_CAPABLE(PORT_LINK_CAPABLE_8);
		break;
	case 16:
		reg |= PORT_LINK_CAPABLE(PORT_LINK_CAPABLE_16);
		break;
	case 32:
		reg |= PORT_LINK_CAPABLE(PORT_LINK_CAPABLE_32);
		break;
	default:
		device_printf(sc->dev,
		    "'num-lanes' property have invalid value: %d\n",
		    sc->num_lanes);
		return (EINVAL);
	}
	DBI_WR4(sc, DW_PORT_LINK_CTRL, reg);

	/* And link width */
	reg = DBI_RD4(sc, DW_GEN2_CTRL);
	reg &= ~GEN2_CTRL_NUM_OF_LANES(~0);
	switch (sc->num_lanes) {
	case 1:
		reg |= GEN2_CTRL_NUM_OF_LANES(GEN2_CTRL_NUM_OF_LANES_1);
		break;
	case 2:
		reg |= GEN2_CTRL_NUM_OF_LANES(GEN2_CTRL_NUM_OF_LANES_2);
		break;
	case 4:
		reg |= GEN2_CTRL_NUM_OF_LANES(GEN2_CTRL_NUM_OF_LANES_4);
		break;
	case 8:
		reg |= GEN2_CTRL_NUM_OF_LANES(GEN2_CTRL_NUM_OF_LANES_8);
		break;
	case 16:
		reg |= GEN2_CTRL_NUM_OF_LANES(GEN2_CTRL_NUM_OF_LANES_16);
		break;
	case 32:
		reg |= GEN2_CTRL_NUM_OF_LANES(GEN2_CTRL_NUM_OF_LANES_32);
		break;
	}
	DBI_WR4(sc, DW_GEN2_CTRL, reg);

	reg = DBI_RD4(sc, DW_GEN2_CTRL);
	reg |= DIRECT_SPEED_CHANGE;
	DBI_WR4(sc, DW_GEN2_CTRL, reg);

	return (0);
}

static int
pci_dw_decode_ranges(struct pci_dw_softc *sc, struct ofw_pci_range *ranges,
     int nranges)
{
	int i, nmem, rv;

	nmem = 0;
	for (i = 0; i < nranges; i++) {
		if ((ranges[i].pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) ==
		    OFW_PCI_PHYS_HI_SPACE_MEM32)
			++nmem;
	}

	sc->mem_ranges = malloc(nmem * sizeof(*sc->mem_ranges), M_DEVBUF,
	    M_WAITOK);
	sc->num_mem_ranges = nmem;

	nmem = 0;
	for (i = 0; i < nranges; i++) {
		if ((ranges[i].pci_hi & OFW_PCI_PHYS_HI_SPACEMASK)  ==
		    OFW_PCI_PHYS_HI_SPACE_IO) {
			if (sc->io_range.size != 0) {
				device_printf(sc->dev,
				    "Duplicated IO range found in DT\n");
				rv = ENXIO;
				goto out;
			}

			sc->io_range = ranges[i];
			if (sc->io_range.size > UINT32_MAX) {
				device_printf(sc->dev,
				    "ATU IO window size is too large. "
				    "Up to 4GB windows are supported, "
				    "trimming window size to 4GB\n");
				sc->io_range.size = UINT32_MAX;
			}
		}
		if ((ranges[i].pci_hi & OFW_PCI_PHYS_HI_SPACEMASK) ==
		    OFW_PCI_PHYS_HI_SPACE_MEM32) {
			MPASS(nmem < sc->num_mem_ranges);
			sc->mem_ranges[nmem] = ranges[i];
			if (sc->mem_ranges[nmem].size > UINT32_MAX) {
				device_printf(sc->dev,
				    "ATU MEM window size is too large. "
				    "Up to 4GB windows are supported, "
				    "trimming window size to 4GB\n");
				sc->mem_ranges[nmem].size = UINT32_MAX;
			}
			++nmem;
		}
	}

	MPASS(nmem == sc->num_mem_ranges);

	if (nmem == 0) {
		device_printf(sc->dev,
		    "Missing required memory range in DT\n");
		return (ENXIO);
	}

	return (0);

out:
	free(sc->mem_ranges, M_DEVBUF);
	return (rv);
}

/*-----------------------------------------------------------------------------
 *
 *  P C I B   I N T E R F A C E
 */

static uint32_t
pci_dw_read_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, int bytes)
{
	struct pci_dw_softc *sc;
	struct resource	*res;
	uint32_t data;
	uint64_t addr;
	int type, rv;

	sc = device_get_softc(dev);

	if (!pci_dw_check_dev(sc, bus, slot, func, reg))
		return (0xFFFFFFFFU);

	if (bus == sc->root_bus) {
		res = (sc->dbi_res);
	} else {
		addr = IATU_CFG_BUS(bus) | IATU_CFG_SLOT(slot) |
		    IATU_CFG_FUNC(func);
		if (bus == sc->sub_bus)
			type = IATU_CTRL1_TYPE_CFG0;
		else
			type = IATU_CTRL1_TYPE_CFG1;
		rv = pci_dw_map_out_atu(sc, 0, type,
		    sc->cfg_pa, addr, sc->cfg_size);
		if (rv != 0)
			return (0xFFFFFFFFU);
		res = sc->cfg_res;
	}

	switch (bytes) {
	case 1:
		data = bus_read_1(res, reg);
		break;
	case 2:
		data = bus_read_2(res, reg);
		break;
	case 4:
		data = bus_read_4(res, reg);
		break;
	default:
		data =  0xFFFFFFFFU;
	}

	return (data);

}

static void
pci_dw_write_config(device_t dev, u_int bus, u_int slot,
    u_int func, u_int reg, uint32_t val, int bytes)
{
	struct pci_dw_softc *sc;
	struct resource	*res;
	uint64_t addr;
	int type, rv;

	sc = device_get_softc(dev);
	if (!pci_dw_check_dev(sc, bus, slot, func, reg))
		return;

	if (bus == sc->root_bus) {
		res = (sc->dbi_res);
	} else {
		addr = IATU_CFG_BUS(bus) | IATU_CFG_SLOT(slot) |
		    IATU_CFG_FUNC(func);
		if (bus == sc->sub_bus)
			type = IATU_CTRL1_TYPE_CFG0;
		else
			type = IATU_CTRL1_TYPE_CFG1;
		rv = pci_dw_map_out_atu(sc, 0, type,
		    sc->cfg_pa, addr, sc->cfg_size);
		if (rv != 0)
			return ;
		res = sc->cfg_res;
	}

	switch (bytes) {
	case 1:
		bus_write_1(res, reg, val);
		break;
	case 2:
		bus_write_2(res, reg, val);
		break;
	case 4:
		bus_write_4(res, reg, val);
		break;
	default:
		break;
	}
}

static int
pci_dw_alloc_msi(device_t pci, device_t child, int count,
    int maxcount, int *irqs)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);

	return (intr_alloc_msi(pci, child, msi_parent, count, maxcount,
	    irqs));
}

static int
pci_dw_release_msi(device_t pci, device_t child, int count, int *irqs)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);
	return (intr_release_msi(pci, child, msi_parent, count, irqs));
}

static int
pci_dw_map_msi(device_t pci, device_t child, int irq, uint64_t *addr,
    uint32_t *data)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);

	return (intr_map_msi(pci, child, msi_parent, irq, addr, data));
}

static int
pci_dw_alloc_msix(device_t pci, device_t child, int *irq)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);
	return (intr_alloc_msix(pci, child, msi_parent, irq));
}

static int
pci_dw_release_msix(device_t pci, device_t child, int irq)
{
	phandle_t msi_parent;
	int rv;

	rv = ofw_bus_msimap(ofw_bus_get_node(pci), pci_get_rid(child),
	    &msi_parent, NULL);
	if (rv != 0)
		return (rv);
	return (intr_release_msix(pci, child, msi_parent, irq));
}

static int
pci_dw_get_id(device_t pci, device_t child, enum pci_id_type type,
    uintptr_t *id)
{
	phandle_t node;
	int rv;
	uint32_t rid;
	uint16_t pci_rid;

	if (type != PCI_ID_MSI)
		return (pcib_get_id(pci, child, type, id));

	node = ofw_bus_get_node(pci);
	pci_rid = pci_get_rid(child);

	rv = ofw_bus_msimap(node, pci_rid, NULL, &rid);
	if (rv != 0)
		return (rv);
	*id = rid;

	return (0);
}

/*-----------------------------------------------------------------------------
 *
 *  B U S  / D E V I C E   I N T E R F A C E
 */
static bus_dma_tag_t
pci_dw_get_dma_tag(device_t dev, device_t child)
{
	struct pci_dw_softc *sc;

	sc = device_get_softc(dev);
	return (sc->dmat);
}

int
pci_dw_init(device_t dev)
{
	struct pci_dw_softc *sc;
	int rv, rid;
	bool unroll_mode;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->node = ofw_bus_get_node(dev);

	mtx_init(&sc->mtx, "pci_dw_mtx", NULL, MTX_DEF);

	/* XXXn Should not be this configurable ? */
	sc->bus_start = 0;
	sc->bus_end = 255;
	sc->root_bus = 0;
	sc->sub_bus = 1;

	/* Read FDT properties */
	if (!sc->coherent)
		sc->coherent = OF_hasprop(sc->node, "dma-coherent");

	rv = OF_getencprop(sc->node, "num-lanes", &sc->num_lanes,
	    sizeof(sc->num_lanes));
	if (rv != sizeof(sc->num_lanes))
		sc->num_lanes = 1;
	if (sc->num_lanes != 1 && sc->num_lanes != 2 &&
	    sc->num_lanes != 4 && sc->num_lanes != 8) {
		device_printf(dev,
		    "invalid number of lanes: %d\n",sc->num_lanes);
		sc->num_lanes = 0;
		rv = ENXIO;
		goto out;
	}

	rid = 0;
	rv = ofw_bus_find_string_index(sc->node, "reg-names", "config", &rid);
	if (rv != 0) {
		device_printf(dev, "Cannot get config space memory\n");
		rv = ENXIO;
		goto out;
	}
	sc->cfg_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->cfg_res == NULL) {
		device_printf(dev, "Cannot allocate config space(rid: %d)\n",
		    rid);
		rv = ENXIO;
		goto out;
	}

	/* Fill up config region related variables */
	sc->cfg_size = rman_get_size(sc->cfg_res);
	sc->cfg_pa = rman_get_start(sc->cfg_res) ;

	if (bootverbose)
		device_printf(dev, "Bus is%s cache-coherent\n",
		    sc->coherent ? "" : " not");
	rv = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
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
	if (rv != 0)
		goto out;

	rv = ofw_pcib_init(dev);
	if (rv != 0)
		goto out;
	rv = pci_dw_decode_ranges(sc, sc->ofw_pci.sc_range,
	    sc->ofw_pci.sc_nrange);
	if (rv != 0)
		goto out;

	unroll_mode = pci_dw_detect_atu_unroll(sc);
	if (bootverbose)
		device_printf(dev, "Using iATU %s mode\n",
		    unroll_mode ? "unroll" : "legacy");
	if (unroll_mode) {
		rid = 0;
		rv = ofw_bus_find_string_index(sc->node, "reg-names", "atu", &rid);
		if (rv == 0) {
			sc->iatu_ur_res = bus_alloc_resource_any(dev,
			    SYS_RES_MEMORY, &rid, RF_ACTIVE);
			if (sc->iatu_ur_res == NULL) {
				device_printf(dev,
				    "Cannot allocate iATU space (rid: %d)\n",
				    rid);
				rv = ENXIO;
				goto out;
			}
			sc->iatu_ur_offset = 0;
			sc->iatu_ur_size = rman_get_size(sc->iatu_ur_res);
		} else if (rv == ENOENT) {
			sc->iatu_ur_res = sc->dbi_res;
			sc->iatu_ur_offset = DW_DEFAULT_IATU_UR_DBI_OFFSET;
			sc->iatu_ur_size = DW_DEFAULT_IATU_UR_DBI_SIZE;
		} else {
			device_printf(dev, "Cannot get iATU space memory\n");
			rv = ENXIO;
			goto out;
		}
	}

	rv = pci_dw_detect_out_atu_regions(sc);
	if (rv != 0)
		goto out;

	if (bootverbose)
		device_printf(sc->dev, "Detected outbound iATU regions: %d\n",
		    sc->num_out_regions);

	rv = pci_dw_setup_hw(sc);
	if (rv != 0)
		goto out;

	device_add_child(dev, "pci", -1);

	return (0);
out:
	/* XXX Cleanup */
	return (rv);
}

static device_method_t pci_dw_methods[] = {
	/* Bus interface */
	DEVMETHOD(bus_get_dma_tag,	pci_dw_get_dma_tag),

	/* pcib interface */
	DEVMETHOD(pcib_read_config,	pci_dw_read_config),
	DEVMETHOD(pcib_write_config,	pci_dw_write_config),
	DEVMETHOD(pcib_alloc_msi,	pci_dw_alloc_msi),
	DEVMETHOD(pcib_release_msi,	pci_dw_release_msi),
	DEVMETHOD(pcib_alloc_msix,	pci_dw_alloc_msix),
	DEVMETHOD(pcib_release_msix,	pci_dw_release_msix),
	DEVMETHOD(pcib_map_msi,		pci_dw_map_msi),
	DEVMETHOD(pcib_get_id,		pci_dw_get_id),

	/* OFW bus interface */
	DEVMETHOD(ofw_bus_get_compat,	ofw_bus_gen_get_compat),
	DEVMETHOD(ofw_bus_get_model,	ofw_bus_gen_get_model),
	DEVMETHOD(ofw_bus_get_name,	ofw_bus_gen_get_name),
	DEVMETHOD(ofw_bus_get_node,	ofw_bus_gen_get_node),
	DEVMETHOD(ofw_bus_get_type,	ofw_bus_gen_get_type),

	/* PCI DW interface  */
	DEVMETHOD(pci_dw_dbi_read,	pci_dw_dbi_read),
	DEVMETHOD(pci_dw_dbi_write,	pci_dw_dbi_write),
	DEVMETHOD_END
};

DEFINE_CLASS_1(pcib, pci_dw_driver, pci_dw_methods,
    sizeof(struct pci_dw_softc), ofw_pcib_driver);
