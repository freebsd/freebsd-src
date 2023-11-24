/*-
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.org>
 *
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/fbio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/resource.h>
#include <machine/bus.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>

#include <arm/ti/ti_sysc.h>
#include <arm/ti/clk/clock_common.h>

#define DEBUG_SYSC	0

#if DEBUG_SYSC
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

/* Documentation/devicetree/bindings/bus/ti-sysc.txt
 *
 * Documentation/devicetree/clock/clock-bindings.txt
 * Defines phandle + optional pair
 * Documentation/devicetree/clock/ti-clkctl.txt
 */

static int ti_sysc_probe(device_t dev);
static int ti_sysc_attach(device_t dev);
static int ti_sysc_detach(device_t dev);

#define TI_SYSC_DRA7_MCAN	15
#define TI_SYSC_USB_HOST_FS	14
#define TI_SYSC_DRA7_MCASP	13
#define TI_SYSC_MCASP		12
#define TI_SYSC_OMAP_AES	11
#define TI_SYSC_OMAP3_SHAM	10
#define TI_SYSC_OMAP4_SR	9
#define TI_SYSC_OMAP3630_SR	8
#define TI_SYSC_OMAP3430_SR	7
#define TI_SYSC_OMAP4_TIMER	6
#define TI_SYSC_OMAP2_TIMER	5
/* Above needs special workarounds */
#define TI_SYSC_OMAP4_SIMPLE	4
#define TI_SYSC_OMAP4		3
#define TI_SYSC_OMAP2		2
#define TI_SYSC			1
#define TI_SYSC_END		0

static struct ofw_compat_data compat_data[] = {
	{ "ti,sysc-dra7-mcan",		TI_SYSC_DRA7_MCAN },
	{ "ti,sysc-usb-host-fs",	TI_SYSC_USB_HOST_FS },
	{ "ti,sysc-dra7-mcasp",		TI_SYSC_DRA7_MCASP },
	{ "ti,sysc-mcasp",		TI_SYSC_MCASP },
	{ "ti,sysc-omap-aes",		TI_SYSC_OMAP_AES },
	{ "ti,sysc-omap3-sham",		TI_SYSC_OMAP3_SHAM },
	{ "ti,sysc-omap4-sr",		TI_SYSC_OMAP4_SR },
	{ "ti,sysc-omap3630-sr",	TI_SYSC_OMAP3630_SR },
	{ "ti,sysc-omap3430-sr",	TI_SYSC_OMAP3430_SR },
	{ "ti,sysc-omap4-timer",	TI_SYSC_OMAP4_TIMER },
	{ "ti,sysc-omap2-timer",	TI_SYSC_OMAP2_TIMER },
	/* Above needs special workarounds */
	{ "ti,sysc-omap4-simple",	TI_SYSC_OMAP4_SIMPLE },
	{ "ti,sysc-omap4",		TI_SYSC_OMAP4 },
	{ "ti,sysc-omap2",		TI_SYSC_OMAP2 },
	{ "ti,sysc",			TI_SYSC },
	{ NULL,				TI_SYSC_END }
};

/* reg-names can be "rev", "sysc" and "syss" */
static const char * reg_names[] = { "rev", "sysc", "syss" };
#define REG_REV		0
#define REG_SYSC	1
#define REG_SYSS	2
#define REG_MAX		3

/* master idle / slave idle mode defined in 8.1.3.2.1 / 8.1.3.2.2 */
#include <dt-bindings/bus/ti-sysc.h>
#define SYSC_IDLE_MAX		4

struct sysc_reg {
	uint64_t	address;
	uint64_t	size;
};

struct clk_list {
	TAILQ_ENTRY(clk_list)	next;
	clk_t			clk;
};

struct ti_sysc_softc {
	struct simplebus_softc	sc;
	bool			attach_done;

	device_t		dev;
	int			device_type;

	struct sysc_reg		reg[REG_MAX];
	/* Offset from host base address */
	uint64_t		offset_reg[REG_MAX];

	uint32_t		ti_sysc_mask;
	int32_t			ti_sysc_midle[SYSC_IDLE_MAX];
	int32_t			ti_sysc_sidle[SYSC_IDLE_MAX];
	uint32_t		ti_sysc_delay_us;
	uint32_t		ti_syss_mask;

	int			num_clocks;
	TAILQ_HEAD(, clk_list)	clk_list;

	/* deprecated ti_hwmods */
	bool			ti_no_reset_on_init;
	bool			ti_no_idle_on_init;
	bool			ti_no_idle;
};

/*
 * All sysc seems to have a reg["rev"] register.
 * Lets use that for identification of which module the driver are connected to.
 */
uint64_t
ti_sysc_get_rev_address(device_t dev) {
	struct ti_sysc_softc *sc = device_get_softc(dev);

	return (sc->reg[REG_REV].address);
}

uint64_t
ti_sysc_get_rev_address_offset_host(device_t dev) {
	struct ti_sysc_softc *sc = device_get_softc(dev);

	return (sc->offset_reg[REG_REV]);
}

uint64_t
ti_sysc_get_sysc_address(device_t dev) {
	struct ti_sysc_softc *sc = device_get_softc(dev);

	return (sc->reg[REG_SYSC].address);
}

uint64_t
ti_sysc_get_sysc_address_offset_host(device_t dev) {
	struct ti_sysc_softc *sc = device_get_softc(dev);

	return (sc->offset_reg[REG_SYSC]);
}

uint64_t
ti_sysc_get_syss_address(device_t dev) {
	struct ti_sysc_softc *sc = device_get_softc(dev);

	return (sc->reg[REG_SYSS].address);
}

uint64_t
ti_sysc_get_syss_address_offset_host(device_t dev) {
	struct ti_sysc_softc *sc = device_get_softc(dev);

	return (sc->offset_reg[REG_SYSS]);
}

/*
 * Due no memory region is assigned the sysc driver the children needs to
 * handle the practical read/writes to the registers.
 * Check if sysc has reset bit.
 */
uint32_t
ti_sysc_get_soft_reset_bit(device_t dev) {
	struct ti_sysc_softc *sc = device_get_softc(dev);
	switch (sc->device_type) {
		case TI_SYSC_OMAP4_TIMER:
		case TI_SYSC_OMAP4_SIMPLE:
		case TI_SYSC_OMAP4:
			if (sc->ti_sysc_mask & SYSC_OMAP4_SOFTRESET) {
				return (SYSC_OMAP4_SOFTRESET);
			}
			break;

		case TI_SYSC_OMAP2_TIMER:
		case TI_SYSC_OMAP2:
		case TI_SYSC:
			if (sc->ti_sysc_mask & SYSC_OMAP2_SOFTRESET) {
				return (SYSC_OMAP2_SOFTRESET);
			}
			break;
		default:
			break;
	}

	return (0);
}

int
ti_sysc_clock_enable(device_t dev) {
	struct clk_list *clkp, *clkp_tmp;
	struct ti_sysc_softc *sc = device_get_softc(dev);
	int err;

	TAILQ_FOREACH_SAFE(clkp, &sc->clk_list, next, clkp_tmp) {
		err = clk_enable(clkp->clk);

		if (err) {
			DPRINTF(sc->dev, "clk_enable %s failed %d\n",
				clk_get_name(clkp->clk), err);
			break;
		}
	}
	return (err);
}

int
ti_sysc_clock_disable(device_t dev) {
	struct clk_list *clkp, *clkp_tmp;
	struct ti_sysc_softc *sc = device_get_softc(dev);
	int err = 0;

	TAILQ_FOREACH_SAFE(clkp, &sc->clk_list, next, clkp_tmp) {
		err = clk_disable(clkp->clk);

		if (err) {
			DPRINTF(sc->dev, "clk_enable %s failed %d\n",
				clk_get_name(clkp->clk), err);
			break;
		}
	}
	return (err);
}

static int
parse_regfields(struct ti_sysc_softc *sc) {
	phandle_t node;
	uint32_t parent_address_cells;
	uint32_t parent_size_cells;
	cell_t *reg;
	ssize_t nreg;
	int err, k, reg_i, prop_idx;
	uint32_t idx;

	node = ofw_bus_get_node(sc->dev);

	/* Get parents address and size properties */
	err = OF_searchencprop(OF_parent(node), "#address-cells",
		&parent_address_cells, sizeof(parent_address_cells));
	if (err == -1)
		return (ENXIO);
	if (!(parent_address_cells == 1 || parent_address_cells == 2)) {
		DPRINTF(sc->dev, "Expect parent #address-cells=[1||2]\n");
		return (ENXIO);
	}

	err = OF_searchencprop(OF_parent(node), "#size-cells",
		&parent_size_cells, sizeof(parent_size_cells));
	if (err == -1)
		return (ENXIO);

	if (!(parent_size_cells == 1 || parent_size_cells == 2)) {
		DPRINTF(sc->dev, "Expect parent #size-cells = [1||2]\n");
		return (ENXIO);
	}

	/* Grab the content of reg properties */
	nreg = OF_getproplen(node, "reg");
	if (nreg <= 0)
		return (ENXIO);

	reg = malloc(nreg, M_DEVBUF, M_WAITOK);
	OF_getencprop(node, "reg", reg, nreg);

	/* Make sure address & size are 0 */
	for (idx = 0; idx < REG_MAX; idx++) {
		sc->reg[idx].address = 0;
		sc->reg[idx].size = 0;
	}

	/* Loop through reg-names and figure out which reg-name corresponds to
	 * index populate the values into the reg array.
	*/
	for (idx = 0, reg_i = 0; idx < REG_MAX && reg_i < nreg; idx++) {
		err = ofw_bus_find_string_index(node, "reg-names",
		    reg_names[idx], &prop_idx);
		if (err != 0)
			continue;

		for (k = 0; k < parent_address_cells; k++) {
			sc->reg[prop_idx].address <<= 32;
			sc->reg[prop_idx].address |= reg[reg_i++];
		}

		for (k = 0; k < parent_size_cells; k++) {
			sc->reg[prop_idx].size <<= 32;
			sc->reg[prop_idx].size |= reg[reg_i++];
		}

		if (sc->sc.nranges == 0)
			sc->offset_reg[prop_idx] = sc->reg[prop_idx].address;
		else
			sc->offset_reg[prop_idx] = sc->reg[prop_idx].address -
			    sc->sc.ranges[REG_REV].host;

		DPRINTF(sc->dev, "reg[%s] address %#jx size %#jx\n",
			reg_names[idx],
			sc->reg[prop_idx].address,
			sc->reg[prop_idx].size);
	}
	free(reg, M_DEVBUF);
	return (0);
}

static void
parse_idle(struct ti_sysc_softc *sc, const char *name, uint32_t *idle) {
	phandle_t node;
	cell_t	value[SYSC_IDLE_MAX];
	int len, no, i;

	node = ofw_bus_get_node(sc->dev);

	if (!OF_hasprop(node, name)) {
		return;
	}

	len = OF_getproplen(node, name);
	no = len / sizeof(cell_t);
	if (no >= SYSC_IDLE_MAX) {
		DPRINTF(sc->dev, "Limit %s\n", name);
		no = SYSC_IDLE_MAX-1;
		len = no * sizeof(cell_t);
	}

	OF_getencprop(node, name, value, len);
	for (i = 0; i < no; i++) {
		idle[i] = value[i];
#if DEBUG_SYSC
		DPRINTF(sc->dev, "%s[%d] = %d ",
			name, i, value[i]);
		switch(value[i]) {
		case SYSC_IDLE_FORCE:
			DPRINTF(sc->dev, "SYSC_IDLE_FORCE\n");
			break;
		case SYSC_IDLE_NO:
			DPRINTF(sc->dev, "SYSC_IDLE_NO\n");
			break;
		case SYSC_IDLE_SMART:
			DPRINTF(sc->dev, "SYSC_IDLE_SMART\n");
			break;
		case SYSC_IDLE_SMART_WKUP:
			DPRINTF(sc->dev, "SYSC_IDLE_SMART_WKUP\n");
			break;
		}
#endif
	}
	for ( ; i < SYSC_IDLE_MAX; i++)
		idle[i] = -1;
}

static int
ti_sysc_attach_clocks(struct ti_sysc_softc *sc) {
	clk_t *clk;
	struct clk_list *clkp;
	int index, err;

	clk = malloc(sc->num_clocks*sizeof(clk_t), M_DEVBUF, M_WAITOK | M_ZERO);

	/* Check if all clocks can be found */
	for (index = 0; index < sc->num_clocks; index++) {
		err = clk_get_by_ofw_index(sc->dev, 0, index, &clk[index]);

		if (err != 0) {
			free(clk, M_DEVBUF);
			return (1);
		}
	}

	/* All clocks are found, add to list */
	for (index = 0; index < sc->num_clocks; index++) {
		clkp = malloc(sizeof(*clkp), M_DEVBUF, M_WAITOK | M_ZERO);
		clkp->clk = clk[index];
		TAILQ_INSERT_TAIL(&sc->clk_list, clkp, next);
	}

	/* Release the clk array */
	free(clk, M_DEVBUF);
	return (0);
}

static int
ti_sysc_simplebus_attach_child(device_t dev) {
	device_t cdev;
	phandle_t node, child;
	struct ti_sysc_softc *sc = device_get_softc(dev);

	node = ofw_bus_get_node(sc->dev);

	for (child = OF_child(node); child > 0; child = OF_peer(child)) {
		cdev = simplebus_add_device(sc->dev, child, 0, NULL, -1, NULL);
		if (cdev != NULL)
			device_probe_and_attach(cdev);
	}
	return (0);
}

/* Device interface */
static int
ti_sysc_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI SYSC Interconnect");

	return (BUS_PROBE_DEFAULT);
}

static int
ti_sysc_attach(device_t dev)
{
	struct ti_sysc_softc *sc;
	phandle_t node;
	int err;
	cell_t	value;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->device_type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	node = ofw_bus_get_node(sc->dev);
	/* ranges - use simplebus */
	simplebus_init(sc->dev, node);
	if (simplebus_fill_ranges(node, &sc->sc) < 0) {
		DPRINTF(sc->dev, "could not get ranges\n");
		return (ENXIO);
	}

	if (sc->sc.nranges == 0) {
		DPRINTF(sc->dev, "nranges == 0\n");
		return (ENXIO);
	}

	/* Required field reg & reg-names - assume at least "rev" exists */
	err = parse_regfields(sc);
	if (err) {
		DPRINTF(sc->dev, "parse_regfields failed %d\n", err);
		return (ENXIO);
	}

	/* Optional */
	if (OF_hasprop(node, "ti,sysc-mask")) {
		OF_getencprop(node, "ti,sysc-mask", &value, sizeof(cell_t));
		sc->ti_sysc_mask = value;
	}
	if (OF_hasprop(node, "ti,syss-mask")) {
		OF_getencprop(node, "ti,syss-mask", &value, sizeof(cell_t));
		sc->ti_syss_mask = value;
	}
	if (OF_hasprop(node, "ti,sysc-delay-us")) {
		OF_getencprop(node, "ti,sysc-delay-us", &value, sizeof(cell_t));
		sc->ti_sysc_delay_us = value;
	}

	DPRINTF(sc->dev, "sysc_mask %x syss_mask %x delay_us %x\n",
		sc->ti_sysc_mask, sc->ti_syss_mask, sc->ti_sysc_delay_us);

	parse_idle(sc, "ti,sysc-midle", sc->ti_sysc_midle);
	parse_idle(sc, "ti,sysc-sidle", sc->ti_sysc_sidle);

	if (OF_hasprop(node, "ti,no-reset-on-init"))
		sc->ti_no_reset_on_init = true;
	else
		sc->ti_no_reset_on_init = false;

	if (OF_hasprop(node, "ti,no-idle-on-init"))
		sc->ti_no_idle_on_init = true;
	else
		sc->ti_no_idle_on_init = false;

	if (OF_hasprop(node, "ti,no-idle"))
		sc->ti_no_idle = true;
	else
		sc->ti_no_idle = false;

	DPRINTF(sc->dev,
		"no-reset-on-init %d, no-idle-on-init %d, no-idle %d\n",
		sc->ti_no_reset_on_init,
		sc->ti_no_idle_on_init,
		sc->ti_no_idle);

	if (OF_hasprop(node, "clocks")) {
		struct clock_cell_info cell_info;
		read_clock_cells(sc->dev, &cell_info);
		free(cell_info.clock_cells, M_DEVBUF);
		free(cell_info.clock_cells_ncells, M_DEVBUF);

		sc->num_clocks = cell_info.num_real_clocks;
		TAILQ_INIT(&sc->clk_list);

		err = ti_sysc_attach_clocks(sc);
		if (err) {
			DPRINTF(sc->dev, "Failed to attach clocks\n");
			return (bus_generic_attach(sc->dev));
		}
	}

	err = ti_sysc_simplebus_attach_child(sc->dev);
	if (err) {
		DPRINTF(sc->dev, "ti_sysc_simplebus_attach_child %d\n",
		    err);
		return (err);
	}

	sc->attach_done = true;

	return (bus_generic_attach(sc->dev));
}

static int
ti_sysc_detach(device_t dev)
{
	return (EBUSY);
}

/* Bus interface */
static void
ti_sysc_new_pass(device_t dev)
{
	struct ti_sysc_softc *sc;
	int err;
	phandle_t node;

	sc = device_get_softc(dev);

	if (sc->attach_done) {
		bus_generic_new_pass(sc->dev);
		return;
	}

	node = ofw_bus_get_node(sc->dev);
	if (OF_hasprop(node, "clocks")) {
		err = ti_sysc_attach_clocks(sc);
		if (err) {
			DPRINTF(sc->dev, "Failed to attach clocks\n");
			return;
		}
	}

	err = ti_sysc_simplebus_attach_child(sc->dev);
	if (err) {
		DPRINTF(sc->dev,
		    "ti_sysc_simplebus_attach_child failed %d\n", err);
		return;
	}
	sc->attach_done = true;

	bus_generic_attach(sc->dev);
}

static device_method_t ti_sysc_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_sysc_probe),
	DEVMETHOD(device_attach,	ti_sysc_attach),
	DEVMETHOD(device_detach,	ti_sysc_detach),

	/* Bus interface */
	DEVMETHOD(bus_new_pass,		ti_sysc_new_pass),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ti_sysc, ti_sysc_driver, ti_sysc_methods,
	sizeof(struct ti_sysc_softc), simplebus_driver);

EARLY_DRIVER_MODULE(ti_sysc, simplebus, ti_sysc_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_FIRST);
