/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2016 Michal Meloun <mmel@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/fbio.h>
#include <sys/kernel.h>
#include <sys/module.h>
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

#include <arm/ti/clk/ti_clk_clkctrl.h>
#include <arm/ti/ti_omap4_cm.h>
#include <arm/ti/ti_cpuid.h>

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

#define L4LS_CLKCTRL_38 	2
#define L4_WKUP_CLKCTRL_0	1
#define NO_SPECIAL_REG		0

/* Documentation/devicetree/bindings/clock/ti-clkctrl.txt */

#define TI_CLKCTRL_L4_WKUP	5
#define TI_CLKCTRL_L4_SECURE	4
#define TI_CLKCTRL_L4_PER	3
#define TI_CLKCTRL_L4_CFG	2
#define TI_CLKCTRL		1
#define TI_CLKCTRL_END		0

static struct ofw_compat_data compat_data[] = {
	{ "ti,clkctrl-l4-wkup",		TI_CLKCTRL_L4_WKUP },
	{ "ti,clkctrl-l4-secure",	TI_CLKCTRL_L4_SECURE },
	{ "ti,clkctrl-l4-per",		TI_CLKCTRL_L4_PER },
	{ "ti,clkctrl-l4-cfg",		TI_CLKCTRL_L4_CFG },
	{ "ti,clkctrl",			TI_CLKCTRL },
	{ NULL,				TI_CLKCTRL_END }
};

struct ti_clkctrl_softc {
	device_t			dev;

	struct clkdom			*clkdom;
};

static int ti_clkctrl_probe(device_t dev);
static int ti_clkctrl_attach(device_t dev);
static int ti_clkctrl_detach(device_t dev);
int clkctrl_ofw_map(struct clkdom *clkdom, uint32_t ncells,
    phandle_t *cells, struct clknode **clk);
static int
create_clkctrl(struct ti_clkctrl_softc *sc, cell_t *reg, uint32_t index, uint32_t reg_offset,
    uint64_t parent_offset, const char *org_name, bool special_gdbclk_reg);

static int
ti_clkctrl_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI clkctrl");

	return (BUS_PROBE_DEFAULT);
}

static int
ti_clkctrl_attach(device_t dev)
{
	struct ti_clkctrl_softc *sc;
	phandle_t node;
	cell_t	*reg;
	ssize_t numbytes_reg;
	int num_reg, err, ti_clock_cells;
	uint32_t index, reg_offset, reg_address;
	const char *org_name;
	uint64_t parent_offset;
	uint8_t special_reg = NO_SPECIAL_REG;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	/* Sanity check */
	err = OF_searchencprop(node, "#clock-cells",
		&ti_clock_cells, sizeof(ti_clock_cells));
	if (err == -1) {
		device_printf(sc->dev, "Failed to get #clock-cells\n");
		return (ENXIO);
	}

	if (ti_clock_cells != 2) {
		device_printf(sc->dev, "clock cells(%d) != 2\n",
			ti_clock_cells);
		return (ENXIO);
	}

	/* Grab the content of reg properties */
	numbytes_reg = OF_getproplen(node, "reg");
	if (numbytes_reg == 0) {
		device_printf(sc->dev, "reg property empty - check your devicetree\n");
		return (ENXIO);
	}
	num_reg = numbytes_reg / sizeof(cell_t);

	reg = malloc(numbytes_reg, M_DEVBUF, M_WAITOK);
	OF_getencprop(node, "reg", reg, numbytes_reg);

	/* Create clock domain */
	sc->clkdom = clkdom_create(sc->dev);
	if (sc->clkdom == NULL) {
		free(reg, M_DEVBUF);
		DPRINTF(sc->dev, "Failed to create clkdom\n");
		return (ENXIO);
	}
	clkdom_set_ofw_mapper(sc->clkdom, clkctrl_ofw_map);

	/* Create clock nodes */
	/* name */
	clk_parse_ofw_clk_name(sc->dev, node, &org_name);

	/* Get parent range */
	parent_offset = ti_omap4_cm_get_simplebus_base_host(device_get_parent(dev));

	/* Check if this is a clkctrl with special registers like gpio */
	switch (ti_chip()) {
#ifdef SOC_OMAP4
	case CHIP_OMAP_4:
		/* FIXME: Todo */
		break;

#endif /* SOC_OMAP4 */
#ifdef SOC_TI_AM335X
	/* Checkout TRM 8.1.12.1.29 - 8.1.12.31 and 8.1.12.2.3
	 * and the DTS.
	 */
	case CHIP_AM335X:
		if (strcmp(org_name, "l4ls-clkctrl@38") == 0)
			special_reg = L4LS_CLKCTRL_38;
		else if (strcmp(org_name, "l4-wkup-clkctrl@0") == 0)
			special_reg = L4_WKUP_CLKCTRL_0;
		break;
#endif /* SOC_TI_AM335X */
	default:
		break;
	}

	/* reg property has a pair of (base address, length) */
	for (index = 0; index < num_reg; index += 2) {
		for (reg_offset = 0; reg_offset < reg[index+1]; reg_offset += sizeof(cell_t)) {
			err = create_clkctrl(sc, reg, index, reg_offset, parent_offset,
			    org_name, false);
			if (err)
				goto cleanup;

			/* Create special clkctrl for GDBCLK in GPIO registers */
			switch (special_reg) {
			case NO_SPECIAL_REG:
				break;
			case L4LS_CLKCTRL_38:
				reg_address = reg[index] + reg_offset-reg[0];
				if (reg_address == 0x74 ||
				    reg_address == 0x78 ||
				    reg_address == 0x7C)
				{
					err = create_clkctrl(sc, reg, index, reg_offset,
					    parent_offset, org_name, true);
					if (err)
						goto cleanup;
				}
				break;
			case L4_WKUP_CLKCTRL_0:
				reg_address = reg[index] + reg_offset - reg[0];
				if (reg_address == 0x8)
				{
					err = create_clkctrl(sc, reg, index, reg_offset,
					    parent_offset, org_name, true);
					if (err)
						goto cleanup;
				}
				break;
			} /* switch (special_reg) */
		} /* inner for */
	} /* for */

	err = clkdom_finit(sc->clkdom);
	if (err) {
		DPRINTF(sc->dev, "Clk domain finit fails %x.\n", err);
		err = ENXIO;
		goto cleanup;
	}

cleanup:
	OF_prop_free(__DECONST(char *, org_name));

	free(reg, M_DEVBUF);

	if (err)
		return (err);

	return (bus_generic_attach(dev));
}

static int
ti_clkctrl_detach(device_t dev)
{
	return (EBUSY);
}

/* modified version of default mapper from clk.c */
int
clkctrl_ofw_map(struct clkdom *clkdom, uint32_t ncells,
    phandle_t *cells, struct clknode **clk) {
	if (ncells == 0)
		*clk = clknode_find_by_id(clkdom, 1);
	else if (ncells == 1)
		*clk = clknode_find_by_id(clkdom, cells[0]);
	else if (ncells == 2) {
		/* To avoid collision with other IDs just add one.
		 * All other registers has an offset of 4 from each other.
		 */
		if (cells[1])
			*clk = clknode_find_by_id(clkdom, cells[0]+1);
		else
			*clk = clknode_find_by_id(clkdom, cells[0]);
	}
	else
		return (ERANGE);

	if (*clk == NULL)
		return (ENXIO);

	return (0);
}

static int
create_clkctrl(struct ti_clkctrl_softc *sc, cell_t *reg, uint32_t index, uint32_t reg_offset,
    uint64_t parent_offset, const char *org_name, bool special_gdbclk_reg) {
	struct ti_clk_clkctrl_def def;
	char *name;
	size_t name_len;
	int err;

	name_len = strlen(org_name) + 1 + 5; /* 5 = _xxxx */
	name = malloc(name_len, M_OFWPROP, M_WAITOK);

	/*
	 * Check out XX_CLKCTRL-INDEX(offset)-macro dance in
	 * sys/gnu/dts/dts/include/dt-bindings/clock/am3.h
	 * sys/gnu/dts/dts/include/dt-bindings/clock/am4.h
	 * sys/gnu/dts/dts/include/dt-bindings/clock/dra7.h
	 * reg[0] are in practice the same as the offset described in the dts.
	 */
	/* special_gdbclk_reg are 0 or 1 */
	def.clkdef.id = reg[index] + reg_offset - reg[0] + special_gdbclk_reg;
	def.register_offset = parent_offset + reg[index] + reg_offset;

	/* Indicate this clkctrl is special and dont use IDLEST/MODULEMODE */
	def.gdbclk = special_gdbclk_reg;

	/* Make up an uniq name in the namespace for each clkctrl */
	snprintf(name, name_len, "%s_%x",
		org_name, def.clkdef.id);
	def.clkdef.name = (const char *) name;

	DPRINTF(sc->dev, "ti_clkctrl_attach: reg[%d]: %s %x\n",
		index, def.clkdef.name, def.clkdef.id);

	/* No parent name */
	def.clkdef.parent_cnt = 0;

	/* set flags */
	def.clkdef.flags = 0x0;

	/* Register the clkctrl */
	err = ti_clknode_clkctrl_register(sc->clkdom, &def);
	if (err) {
		DPRINTF(sc->dev,
			"ti_clknode_clkctrl_register[%d:%d] failed %x\n",
			index, reg_offset, err);
		err = ENXIO;
	}
	OF_prop_free(name);
	return (err);
}

static device_method_t ti_clkctrl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ti_clkctrl_probe),
	DEVMETHOD(device_attach,	ti_clkctrl_attach),
	DEVMETHOD(device_detach,	ti_clkctrl_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(ti_clkctrl, ti_clkctrl_driver, ti_clkctrl_methods,
    sizeof(struct ti_clkctrl_softc));

EARLY_DRIVER_MODULE(ti_clkctrl, simplebus, ti_clkctrl_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);

MODULE_VERSION(ti_clkctrl, 1);
