/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 Thomas Skibo
 * Copyright (c) 2008 Alexander Motin <mav@FreeBSD.org>
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

/* Generic driver to attach sdhci controllers on simplebus.
 * Derived mainly from sdhci_pci.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>

#include <dev/clk/clk.h>
#include <dev/clk/clk_fixed.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/ofw/ofw_subr.h>
#include <dev/ofw/openfirm.h>
#include <dev/syscon/syscon.h>
#include <dev/phy/phy.h>

#include <dev/mmc/bridge.h>

#include <dev/sdhci/sdhci.h>
#include <dev/sdhci/sdhci_fdt.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"

#define	SDHCI_FDT_ARMADA38X	1
#define	SDHCI_FDT_XLNX_ZY7	2
#define	SDHCI_FDT_QUALCOMM	3

static struct ofw_compat_data compat_data[] = {
	{ "marvell,armada-380-sdhci",	SDHCI_FDT_ARMADA38X },
	{ "qcom,sdhci-msm-v4",		SDHCI_FDT_QUALCOMM },
	{ "xlnx,zy7_sdhci",		SDHCI_FDT_XLNX_ZY7 },
	{ NULL, 0 }
};

struct sdhci_exported_clocks_sc {
	device_t	clkdev;
};

static int
sdhci_exported_clocks_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);
	return (0);
}

static clknode_method_t sdhci_exported_clocks_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,	sdhci_exported_clocks_init),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(sdhci_exported_clocks_clknode, sdhci_exported_clocks_clknode_class,
    sdhci_exported_clocks_clknode_methods, sizeof(struct sdhci_exported_clocks_sc),
    clknode_class);

int
sdhci_clock_ofw_map(struct clkdom *clkdom, uint32_t ncells,
    phandle_t *cells, struct clknode **clk)
{
	int id = 1; /* Our clock id starts at 1 */

	if (ncells != 0)
		id = cells[1];
	*clk = clknode_find_by_id(clkdom, id);

	if (*clk == NULL)
		return (ENXIO);
	return (0);
}

void
sdhci_export_clocks(struct sdhci_fdt_softc *sc)
{
	struct clknode_init_def def;
	struct sdhci_exported_clocks_sc *clksc;
	struct clkdom *clkdom;
	struct clknode *clk;
	bus_addr_t paddr;
	bus_size_t psize;
	const char **clknames;
	phandle_t node;
	int i, nclocks, ncells, error;

	node = ofw_bus_get_node(sc->dev);

	if (ofw_reg_to_paddr(node, 0, &paddr, &psize, NULL) != 0) {
		device_printf(sc->dev, "cannot parse 'reg' property\n");
		return;
	}

	error = ofw_bus_parse_xref_list_get_length(node, "clocks",
	    "#clock-cells", &ncells);
	if (error != 0 || ncells != 2) {
		device_printf(sc->dev, "couldn't find parent clocks\n");
		return;
	}

	nclocks = ofw_bus_string_list_to_array(node, "clock-output-names",
	    &clknames);
	/* No clocks to export */
	if (nclocks <= 0)
		return;

	clkdom = clkdom_create(sc->dev);
	clkdom_set_ofw_mapper(clkdom, sdhci_clock_ofw_map);

	for (i = 0; i < nclocks; i++) {
		memset(&def, 0, sizeof(def));
		def.id = i + 1; /* Exported clock IDs starts at 1 */
		def.name = clknames[i];
		def.parent_names = malloc(sizeof(char *) * 1, M_OFWPROP, M_WAITOK);
		def.parent_names[0] = clk_get_name(sc->clk_xin);
		def.parent_cnt = 1;

		clk = clknode_create(clkdom, &sdhci_exported_clocks_clknode_class, &def);
		if (clk == NULL) {
			device_printf(sc->dev, "cannot create clknode\n");
			return;
		}

		clksc = clknode_get_softc(clk);
		clksc->clkdev = device_get_parent(sc->dev);

		clknode_register(clkdom, clk);
	}

	if (clkdom_finit(clkdom) != 0) {
		device_printf(sc->dev, "cannot finalize clkdom initialization\n");
		return;
	}

	if (bootverbose)
		clkdom_dump(clkdom);
}

int
sdhci_init_clocks(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	int error;

	/* Get and activate clocks */
	error = clk_get_by_ofw_name(dev, 0, "clk_xin", &sc->clk_xin);
	if (error != 0) {
		device_printf(dev, "cannot get xin clock\n");
		return (ENXIO);
	}
	error = clk_enable(sc->clk_xin);
	if (error != 0) {
		device_printf(dev, "cannot enable xin clock\n");
		return (ENXIO);
	}
	error = clk_get_by_ofw_name(dev, 0, "clk_ahb", &sc->clk_ahb);
	if (error != 0) {
		device_printf(dev, "cannot get ahb clock\n");
		return (ENXIO);
	}
	error = clk_enable(sc->clk_ahb);
	if (error != 0) {
		device_printf(dev, "cannot enable ahb clock\n");
		return (ENXIO);
	}

	return (0);
}

int
sdhci_init_phy(struct sdhci_fdt_softc *sc)
{
	int error;

	/* Enable PHY */
	error = phy_get_by_ofw_name(sc->dev, 0, "phy_arasan", &sc->phy);
	if (error == ENOENT)
		return (0);
	if (error != 0) {
		device_printf(sc->dev, "Could not get phy\n");
		return (ENXIO);
	}
	error = phy_enable(sc->phy);
	if (error != 0) {
		device_printf(sc->dev, "Could not enable phy\n");
		return (ENXIO);
	}

	return (0);
}

int
sdhci_get_syscon(struct sdhci_fdt_softc *sc)
{
	phandle_t node;

	/* Get syscon */
	node = ofw_bus_get_node(sc->dev);
	if (OF_hasprop(node, "arasan,soc-ctl-syscon") &&
	    syscon_get_by_ofw_property(sc->dev, node,
	    "arasan,soc-ctl-syscon", &sc->syscon) != 0) {
		device_printf(sc->dev, "cannot get syscon handle\n");
		return (ENXIO);
	}

	return (0);
}

static uint8_t
sdhci_fdt_read_1(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	return (bus_read_1(sc->mem_res[slot->num], off));
}

static void
sdhci_fdt_write_1(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint8_t val)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	bus_write_1(sc->mem_res[slot->num], off, val);
}

static uint16_t
sdhci_fdt_read_2(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	return (bus_read_2(sc->mem_res[slot->num], off));
}

static void
sdhci_fdt_write_2(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint16_t val)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	bus_write_2(sc->mem_res[slot->num], off, val);
}

static uint32_t
sdhci_fdt_read_4(device_t dev, struct sdhci_slot *slot, bus_size_t off)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	uint32_t val32;

	val32 = bus_read_4(sc->mem_res[slot->num], off);
	if (off == SDHCI_CAPABILITIES && sc->no_18v)
		val32 &= ~SDHCI_CAN_VDD_180;

	return (val32);
}

static void
sdhci_fdt_write_4(device_t dev, struct sdhci_slot *slot, bus_size_t off,
    uint32_t val)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	bus_write_4(sc->mem_res[slot->num], off, val);
}

static void
sdhci_fdt_read_multi_4(device_t dev, struct sdhci_slot *slot,
    bus_size_t off, uint32_t *data, bus_size_t count)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	bus_read_multi_4(sc->mem_res[slot->num], off, data, count);
}

static void
sdhci_fdt_write_multi_4(device_t dev, struct sdhci_slot *slot,
    bus_size_t off, uint32_t *data, bus_size_t count)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	bus_write_multi_4(sc->mem_res[slot->num], off, data, count);
}

static void
sdhci_fdt_intr(void *arg)
{
	struct sdhci_fdt_softc *sc = (struct sdhci_fdt_softc *)arg;
	int i;

	for (i = 0; i < sc->num_slots; i++)
		sdhci_generic_intr(&sc->slots[i]);
}

static int
sdhci_fdt_get_ro(device_t bus, device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(bus);

	if (sc->wp_disabled)
		return (false);
	return (sdhci_generic_get_ro(bus, dev) ^ sc->wp_inverted);
}

static int
sdhci_fdt_probe(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	sc->quirks = 0;
	switch (ofw_bus_search_compatible(dev, compat_data)->ocd_data) {
	case SDHCI_FDT_ARMADA38X:
		sc->quirks = SDHCI_QUIRK_BROKEN_AUTO_STOP;
		device_set_desc(dev, "ARMADA38X SDHCI controller");
		break;
	case SDHCI_FDT_QUALCOMM:
		sc->quirks = SDHCI_QUIRK_ALL_SLOTS_NON_REMOVABLE |
		    SDHCI_QUIRK_BROKEN_SDMA_BOUNDARY;
		sc->sdma_boundary = SDHCI_BLKSZ_SDMA_BNDRY_4K;
		device_set_desc(dev, "Qualcomm FDT SDHCI controller");
		break;
	case SDHCI_FDT_XLNX_ZY7:
		sc->quirks = SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK;
		device_set_desc(dev, "Zynq-7000 generic fdt SDHCI controller");
		break;
	default:
		return (ENXIO);
	}

	return (0);
}

int
sdhci_fdt_attach(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	struct sdhci_slot *slot;
	int err, slots, rid, i;
	phandle_t node;
	pcell_t cid;

	sc->dev = dev;

	node = ofw_bus_get_node(dev);

	sc->num_slots = 1;
	sc->max_clk = 0;

	/* Allow dts to patch quirks, slots, and max-frequency. */
	if ((OF_getencprop(node, "quirks", &cid, sizeof(cid))) > 0)
		sc->quirks = cid;
	if ((OF_getencprop(node, "num-slots", &cid, sizeof(cid))) > 0)
		sc->num_slots = cid;
	if ((OF_getencprop(node, "max-frequency", &cid, sizeof(cid))) > 0)
		sc->max_clk = cid;
	if (OF_hasprop(node, "no-1-8-v"))
		sc->no_18v = true;
	if (OF_hasprop(node, "wp-inverted"))
		sc->wp_inverted = true;
	if (OF_hasprop(node, "disable-wp"))
		sc->wp_disabled = true;

	/* Allocate IRQ. */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Can't allocate IRQ\n");
		return (ENOMEM);
	}

	/* Scan all slots. */
	slots = sc->num_slots;	/* number of slots determined in probe(). */
	sc->num_slots = 0;
	for (i = 0; i < slots; i++) {
		slot = &sc->slots[sc->num_slots];

		/* Allocate memory. */
		rid = 0;
		sc->mem_res[i] = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
							&rid, RF_ACTIVE);
		if (sc->mem_res[i] == NULL) {
			device_printf(dev,
			    "Can't allocate memory for slot %d\n", i);
			continue;
		}

		slot->quirks = sc->quirks;
		slot->caps = sc->caps;
		slot->max_clk = sc->max_clk;
		slot->sdma_boundary = sc->sdma_boundary;

		if (sdhci_init_slot(dev, slot, i) != 0)
			continue;
		sc->num_slots++;
	}
	device_printf(dev, "%d slot(s) allocated\n", sc->num_slots);

	/* Activate the interrupt */
	err = bus_setup_intr(dev, sc->irq_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, sdhci_fdt_intr, sc, &sc->intrhand);
	if (err) {
		device_printf(dev, "Cannot setup IRQ\n");
		return (err);
	}

	/* Process cards detection. */
	for (i = 0; i < sc->num_slots; i++)
		sdhci_start_slot(&sc->slots[i]);

	return (0);
}

int
sdhci_fdt_detach(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	int i;

	bus_detach_children(dev);
	bus_teardown_intr(dev, sc->irq_res, sc->intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, rman_get_rid(sc->irq_res),
	    sc->irq_res);

	for (i = 0; i < sc->num_slots; i++) {
		sdhci_cleanup_slot(&sc->slots[i]);
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res[i]), sc->mem_res[i]);
	}

	return (0);
}

int
sdhci_fdt_set_clock(device_t dev, struct sdhci_slot *slot, int clock)
{

	return (clock);
}


static device_method_t sdhci_fdt_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe,		sdhci_fdt_probe),
	DEVMETHOD(device_attach,	sdhci_fdt_attach),
	DEVMETHOD(device_detach,	sdhci_fdt_detach),

	/* Bus interface */
	DEVMETHOD(bus_read_ivar,	sdhci_generic_read_ivar),
	DEVMETHOD(bus_write_ivar,	sdhci_generic_write_ivar),

	/* mmcbr_if */
	DEVMETHOD(mmcbr_update_ios,	sdhci_generic_update_ios),
	DEVMETHOD(mmcbr_request,	sdhci_generic_request),
	DEVMETHOD(mmcbr_get_ro,		sdhci_fdt_get_ro),
	DEVMETHOD(mmcbr_acquire_host,	sdhci_generic_acquire_host),
	DEVMETHOD(mmcbr_release_host,	sdhci_generic_release_host),

	/* SDHCI registers accessors */
	DEVMETHOD(sdhci_read_1,		sdhci_fdt_read_1),
	DEVMETHOD(sdhci_read_2,		sdhci_fdt_read_2),
	DEVMETHOD(sdhci_read_4,		sdhci_fdt_read_4),
	DEVMETHOD(sdhci_read_multi_4,	sdhci_fdt_read_multi_4),
	DEVMETHOD(sdhci_write_1,	sdhci_fdt_write_1),
	DEVMETHOD(sdhci_write_2,	sdhci_fdt_write_2),
	DEVMETHOD(sdhci_write_4,	sdhci_fdt_write_4),
	DEVMETHOD(sdhci_write_multi_4,	sdhci_fdt_write_multi_4),
	DEVMETHOD(sdhci_set_clock,	sdhci_fdt_set_clock),

	DEVMETHOD_END
};

driver_t sdhci_fdt_driver = {
	"sdhci_fdt",
	sdhci_fdt_methods,
	sizeof(struct sdhci_fdt_softc),
};

DRIVER_MODULE(sdhci_fdt, simplebus, sdhci_fdt_driver, NULL, NULL);
SDHCI_DEPEND(sdhci_fdt);
#ifndef MMCCAM
MMC_DECLARE_BRIDGE(sdhci_fdt);
#endif
