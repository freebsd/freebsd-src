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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/ofw/ofw_subr.h>
#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/syscon/syscon.h>
#include <dev/extres/phy/phy.h>

#include <dev/mmc/bridge.h>

#include <dev/sdhci/sdhci.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"

#include "clkdev_if.h"
#include "syscon_if.h"

#define	MAX_SLOTS		6
#define	SDHCI_FDT_ARMADA38X	1
#define	SDHCI_FDT_GENERIC	2
#define	SDHCI_FDT_XLNX_ZY7	3
#define	SDHCI_FDT_QUALCOMM	4
#define	SDHCI_FDT_RK3399	5
#define	SDHCI_FDT_RK3568	6

#define	RK3399_GRF_EMMCCORE_CON0		0xf000
#define	 RK3399_CORECFG_BASECLKFREQ		0xff00
#define	 RK3399_CORECFG_TIMEOUTCLKUNIT		(1 << 7)
#define	 RK3399_CORECFG_TUNINGCOUNT		0x3f
#define	RK3399_GRF_EMMCCORE_CON11		0xf02c
#define	 RK3399_CORECFG_CLOCKMULTIPLIER		0xff

#define	RK3568_EMMC_HOST_CTRL			0x0508
#define	RK3568_EMMC_EMMC_CTRL			0x052c
#define	RK3568_EMMC_ATCTRL			0x0540
#define	RK3568_EMMC_DLL_CTRL			0x0800
#define	 DLL_CTRL_SRST				0x00000001
#define	 DLL_CTRL_START				0x00000002
#define	 DLL_CTRL_START_POINT_DEFAULT		0x00050000
#define	 DLL_CTRL_INCREMENT_DEFAULT		0x00000200

#define	RK3568_EMMC_DLL_RXCLK			0x0804
#define	 DLL_RXCLK_DELAY_ENABLE			0x08000000
#define	 DLL_RXCLK_NO_INV			0x20000000

#define	RK3568_EMMC_DLL_TXCLK			0x0808
#define	 DLL_TXCLK_DELAY_ENABLE			0x08000000
#define	 DLL_TXCLK_TAPNUM_DEFAULT		0x00000008
#define	 DLL_TXCLK_TAPNUM_FROM_SW		0x01000000

#define	RK3568_EMMC_DLL_STRBIN			0x080c
#define	 DLL_STRBIN_DELAY_ENABLE		0x08000000
#define	 DLL_STRBIN_TAPNUM_DEFAULT		0x00000008
#define	DLL_STRBIN_TAPNUM_FROM_SW		0x01000000

#define	RK3568_EMMC_DLL_STATUS0			0x0840
#define	 DLL_STATUS0_DLL_LOCK			0x00000100
#define	 DLL_STATUS0_DLL_TIMEOUT		0x00000200

#define	LOWEST_SET_BIT(mask)	((((mask) - 1) & (mask)) ^ (mask))
#define	SHIFTIN(x, mask)	((x) * LOWEST_SET_BIT(mask))

#define	EMMCCARDCLK_ID		1000

static struct ofw_compat_data compat_data[] = {
	{ "marvell,armada-380-sdhci",	SDHCI_FDT_ARMADA38X },
	{ "sdhci_generic",		SDHCI_FDT_GENERIC },
	{ "qcom,sdhci-msm-v4",		SDHCI_FDT_QUALCOMM },
	{ "rockchip,rk3399-sdhci-5.1",	SDHCI_FDT_RK3399 },
	{ "xlnx,zy7_sdhci",		SDHCI_FDT_XLNX_ZY7 },
	{ "rockchip,rk3568-dwcmshc",	SDHCI_FDT_RK3568 },
	{ NULL, 0 }
};

struct sdhci_fdt_softc {
	device_t	dev;		/* Controller device */
	u_int		quirks;		/* Chip specific quirks */
	u_int		caps;		/* If we override SDHCI_CAPABILITIES */
	uint32_t	max_clk;	/* Max possible freq */
	uint8_t		sdma_boundary;	/* If we override the SDMA boundary */
	struct resource *irq_res;	/* IRQ resource */
	void		*intrhand;	/* Interrupt handle */

	int		num_slots;	/* Number of slots on this controller*/
	struct sdhci_slot slots[MAX_SLOTS];
	struct resource	*mem_res[MAX_SLOTS];	/* Memory resource */

	bool		wp_inverted;	/* WP pin is inverted */
	bool		no_18v;		/* No 1.8V support */

	clk_t		clk_xin;	/* xin24m fixed clock */
	clk_t		clk_ahb;	/* ahb clock */
	clk_t		clk_core;	/* core clock */
	phy_t		phy;		/* phy to be used */
};

struct rk3399_emmccardclk_sc {
	device_t	clkdev;
	bus_addr_t	reg;
};

static int
rk3399_emmccardclk_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);
	return (0);
}

static clknode_method_t rk3399_emmccardclk_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,	rk3399_emmccardclk_init),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(rk3399_emmccardclk_clknode, rk3399_emmccardclk_clknode_class,
    rk3399_emmccardclk_clknode_methods, sizeof(struct rk3399_emmccardclk_sc),
    clknode_class);

static int
rk3399_ofw_map(struct clkdom *clkdom, uint32_t ncells,
    phandle_t *cells, struct clknode **clk)
{

	if (ncells == 0)
		*clk = clknode_find_by_id(clkdom, EMMCCARDCLK_ID);
	else
		return (ERANGE);

	if (*clk == NULL)
		return (ENXIO);
	return (0);
}

static void
sdhci_init_rk3399_emmccardclk(device_t dev)
{
	struct clknode_init_def def;
	struct rk3399_emmccardclk_sc *sc;
	struct clkdom *clkdom;
	struct clknode *clk;
	clk_t clk_parent;
	bus_addr_t paddr;
	bus_size_t psize;
	const char **clknames;
	phandle_t node;
	int i, nclocks, ncells, error;

	node = ofw_bus_get_node(dev);

	if (ofw_reg_to_paddr(node, 0, &paddr, &psize, NULL) != 0) {
		device_printf(dev, "cannot parse 'reg' property\n");
		return;
	}

	error = ofw_bus_parse_xref_list_get_length(node, "clocks",
	    "#clock-cells", &ncells);
	if (error != 0 || ncells != 2) {
		device_printf(dev, "couldn't find parent clocks\n");
		return;
	}

	nclocks = ofw_bus_string_list_to_array(node, "clock-output-names",
	    &clknames);
	/* No clocks to export */
	if (nclocks <= 0)
		return;

	if (nclocks != 1) {
		device_printf(dev, "Having %d clock instead of 1, aborting\n",
		    nclocks);
		return;
	}

	clkdom = clkdom_create(dev);
	clkdom_set_ofw_mapper(clkdom, rk3399_ofw_map);

	memset(&def, 0, sizeof(def));
	def.id = EMMCCARDCLK_ID;
	def.name = clknames[0];
	def.parent_names = malloc(sizeof(char *) * ncells, M_OFWPROP, M_WAITOK);
	for (i = 0; i < ncells; i++) {
		error = clk_get_by_ofw_index(dev, 0, i, &clk_parent);
		if (error != 0) {
			device_printf(dev, "cannot get clock %d\n", error);
			return;
		}
		def.parent_names[i] = clk_get_name(clk_parent);
		if (bootverbose)
			device_printf(dev, "clk parent: %s\n",
			    def.parent_names[i]);
		clk_release(clk_parent);
	}
	def.parent_cnt = ncells;

	clk = clknode_create(clkdom, &rk3399_emmccardclk_clknode_class, &def);
	if (clk == NULL) {
		device_printf(dev, "cannot create clknode\n");
		return;
	}

	sc = clknode_get_softc(clk);
	sc->reg = paddr;
	sc->clkdev = device_get_parent(dev);

	clknode_register(clkdom, clk);

	if (clkdom_finit(clkdom) != 0) {
		device_printf(dev, "cannot finalize clkdom initialization\n");
		return;
	}

	if (bootverbose)
		clkdom_dump(clkdom);
}

static int
sdhci_init_rk3399(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	struct syscon *grf = NULL;
	phandle_t node;
	uint64_t freq;
	uint32_t mask, val;
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
	error = clk_get_freq(sc->clk_xin, &freq);
	if (error != 0) {
		device_printf(dev, "cannot get xin clock frequency\n");
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

	/* Register clock */
	sdhci_init_rk3399_emmccardclk(dev);

	/* Enable PHY */
	error = phy_get_by_ofw_name(dev, 0, "phy_arasan", &sc->phy);
	if (error != 0) {
		device_printf(dev, "Could not get phy\n");
		return (ENXIO);
	}
	error = phy_enable(sc->phy);
	if (error != 0) {
		device_printf(dev, "Could not enable phy\n");
		return (ENXIO);
	}
	/* Get syscon */
	node = ofw_bus_get_node(dev);
	if (OF_hasprop(node, "arasan,soc-ctl-syscon") &&
	    syscon_get_by_ofw_property(dev, node,
	    "arasan,soc-ctl-syscon", &grf) != 0) {
		device_printf(dev, "cannot get grf driver handle\n");
		return (ENXIO);
	}

	/* Disable clock multiplier */
	mask = RK3399_CORECFG_CLOCKMULTIPLIER;
	val = 0;
	SYSCON_WRITE_4(grf, RK3399_GRF_EMMCCORE_CON11, (mask << 16) | val);

	/* Set base clock frequency */
	mask = RK3399_CORECFG_BASECLKFREQ;
	val = SHIFTIN((freq + (1000000 / 2)) / 1000000,
	    RK3399_CORECFG_BASECLKFREQ);
	SYSCON_WRITE_4(grf, RK3399_GRF_EMMCCORE_CON0, (mask << 16) | val);

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

	return (sdhci_generic_get_ro(bus, dev) ^ sc->wp_inverted);
}

static int
sdhci_fdt_set_clock(device_t dev, struct sdhci_slot *slot, int clock)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	int32_t val;
	int i;

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data ==
	    SDHCI_FDT_RK3568) {
		if (clock == 400000)
			clock = 375000;

		if (clock) {
			clk_set_freq(sc->clk_core, clock, 0);

			if (clock <= 52000000) {
				bus_write_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_CTRL, 0x0);
				bus_write_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_RXCLK, DLL_RXCLK_NO_INV);
				bus_write_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_TXCLK, 0x0);
				bus_write_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_STRBIN, 0x0);
				return (clock);
			}

			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_CTRL, DLL_CTRL_START);
			DELAY(1000);
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_CTRL, 0);
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_CTRL, DLL_CTRL_START_POINT_DEFAULT |
			    DLL_CTRL_INCREMENT_DEFAULT | DLL_CTRL_START);
			for (i = 0; i < 500; i++) {
				val = bus_read_4(sc->mem_res[slot->num],
				    RK3568_EMMC_DLL_STATUS0);
				if (val & DLL_STATUS0_DLL_LOCK &&
				    !(val & DLL_STATUS0_DLL_TIMEOUT))
					break;
				DELAY(1000);
			}
			bus_write_4(sc->mem_res[slot->num], RK3568_EMMC_ATCTRL,
			    (0x1 << 16 | 0x2 << 17 | 0x3 << 19));
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_RXCLK,
			    DLL_RXCLK_DELAY_ENABLE | DLL_RXCLK_NO_INV);
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_TXCLK, DLL_TXCLK_DELAY_ENABLE |
			    DLL_TXCLK_TAPNUM_DEFAULT|DLL_TXCLK_TAPNUM_FROM_SW);
			bus_write_4(sc->mem_res[slot->num],
			    RK3568_EMMC_DLL_STRBIN, DLL_STRBIN_DELAY_ENABLE |
			    DLL_STRBIN_TAPNUM_DEFAULT |
			    DLL_STRBIN_TAPNUM_FROM_SW);
		}
	}
	return (clock);
}

static int
sdhci_fdt_probe(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	phandle_t node;
	pcell_t cid;

	sc->quirks = 0;
	sc->num_slots = 1;
	sc->max_clk = 0;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	switch (ofw_bus_search_compatible(dev, compat_data)->ocd_data) {
	case SDHCI_FDT_ARMADA38X:
		sc->quirks = SDHCI_QUIRK_BROKEN_AUTO_STOP;
		device_set_desc(dev, "ARMADA38X SDHCI controller");
		break;
	case SDHCI_FDT_GENERIC:
		device_set_desc(dev, "generic fdt SDHCI controller");
		break;
	case SDHCI_FDT_QUALCOMM:
		sc->quirks = SDHCI_QUIRK_ALL_SLOTS_NON_REMOVABLE |
		    SDHCI_QUIRK_BROKEN_SDMA_BOUNDARY;
		sc->sdma_boundary = SDHCI_BLKSZ_SDMA_BNDRY_4K;
		device_set_desc(dev, "Qualcomm FDT SDHCI controller");
		break;
	case SDHCI_FDT_RK3399:
		device_set_desc(dev, "Rockchip RK3399 fdt SDHCI controller");
		break;
	case SDHCI_FDT_XLNX_ZY7:
		sc->quirks = SDHCI_QUIRK_DATA_TIMEOUT_USES_SDCLK;
		device_set_desc(dev, "Zynq-7000 generic fdt SDHCI controller");
		break;
	case SDHCI_FDT_RK3568:
		device_set_desc(dev, "Rockchip RK3568 fdt SDHCI controller");
		break;
	default:
		return (ENXIO);
	}

	node = ofw_bus_get_node(dev);

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

	return (0);
}

static int
sdhci_fdt_attach(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	struct sdhci_slot *slot;
	int err, slots, rid, i;

	sc->dev = dev;

	/* Allocate IRQ. */
	rid = 0;
	sc->irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (sc->irq_res == NULL) {
		device_printf(dev, "Can't allocate IRQ\n");
		return (ENOMEM);
	}

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data ==
	    SDHCI_FDT_RK3399) {
		/* Initialize SDHCI */
		err = sdhci_init_rk3399(dev);
		if (err != 0) {
			device_printf(dev, "Cannot init RK3399 SDHCI\n");
			return (err);
		}
	}

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data ==
	    SDHCI_FDT_RK3568) {
		/* setup & enable clocks */
		if (clk_get_by_ofw_name(dev, 0, "core", &sc->clk_core)) {
			device_printf(dev, "cannot get core clock\n");
			return (ENXIO);
		}
		clk_enable(sc->clk_core);
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

static int
sdhci_fdt_detach(device_t dev)
{
	struct sdhci_fdt_softc *sc = device_get_softc(dev);
	int i;

	bus_generic_detach(dev);
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

static driver_t sdhci_fdt_driver = {
	"sdhci_fdt",
	sdhci_fdt_methods,
	sizeof(struct sdhci_fdt_softc),
};

DRIVER_MODULE(sdhci_fdt, simplebus, sdhci_fdt_driver, NULL, NULL);
SDHCI_DEPEND(sdhci_fdt);
#ifndef MMCCAM
MMC_DECLARE_BRIDGE(sdhci_fdt);
#endif
