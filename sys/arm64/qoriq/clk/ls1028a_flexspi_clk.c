/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Alstom Group.
 * Copyright (c) 2021 Semihalf.
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/extres/clk/clk_div.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clkdev_if.h"
#include "syscon_if.h"


struct ls1028a_flexspi_clk_softc {
	device_t		dev;
	struct clkdom		*clkdom;
	uint64_t 		reg_offset;
	struct syscon		*syscon;
	struct clk_div_def	clk_def;
	struct mtx		mtx;
};

static struct clk_div_table ls1028a_flexspi_div_tbl[] = {
	{ .value = 0, .divider = 1, },
	{ .value = 1, .divider = 2, },
	{ .value = 2, .divider = 3, },
	{ .value = 3, .divider = 4, },
	{ .value = 4, .divider = 5, },
	{ .value = 5, .divider = 6, },
	{ .value = 6, .divider = 7, },
	{ .value = 7, .divider = 8, },
	{ .value = 11, .divider = 12, },
	{ .value = 15, .divider = 16, },
	{ .value = 16, .divider = 20, },
	{ .value = 17, .divider = 24, },
	{ .value = 18, .divider = 28, },
	{ .value = 19, .divider = 32, },
	{ .value = 20, .divider = 80, },
	{}
};
static struct clk_div_table lx2160a_flexspi_div_tbl[] = {
	{ .value = 1, .divider = 2, },
	{ .value = 3, .divider = 4, },
	{ .value = 5, .divider = 6, },
	{ .value = 7, .divider = 8, },
	{ .value = 11, .divider = 12, },
	{ .value = 15, .divider = 16, },
	{ .value = 16, .divider = 20, },
	{ .value = 17, .divider = 24, },
	{ .value = 18, .divider = 28, },
	{ .value = 19, .divider = 32, },
	{ .value = 20, .divider = 80, },
	{}
};

static struct ofw_compat_data compat_data[] = {
	{ "fsl,ls1028a-flexspi-clk",	(uintptr_t)ls1028a_flexspi_div_tbl },
	{ "fsl,lx2160a-flexspi-clk",	(uintptr_t)lx2160a_flexspi_div_tbl },
	{ NULL, 0 }
};

static int
ls1028a_flexspi_clk_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
		device_set_desc(dev, "NXP FlexSPI clock driver");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
ls1028a_flexspi_clk_attach(device_t dev)
{
	struct ls1028a_flexspi_clk_softc *sc;
	const char *oclkname = NULL;
	const char *pclkname[1];
	uint32_t acells;
	uint32_t scells;
	pcell_t cells[4];
	phandle_t node;
	uint64_t reg_size;
	int ret;
	clk_t clk;

	sc = device_get_softc(dev);
	sc->dev = dev;
	node = ofw_bus_get_node(dev);

	/* Parse address-cells and size-cells from the parent node as a fallback */
	if (OF_getencprop(node, "#address-cells", &acells,
	    sizeof(acells)) == -1) {
		if (OF_getencprop(OF_parent(node), "#address-cells", &acells,
		    sizeof(acells)) == -1) {
			acells = 2;
		}
	}
	if (OF_getencprop(node, "#size-cells", &scells,
	    sizeof(scells)) == -1) {
		if (OF_getencprop(OF_parent(node), "#size-cells", &scells,
		    sizeof(scells)) == -1) {
			scells = 1;
		}
	}
	ret = OF_getencprop(node, "reg", cells, (acells + scells) * sizeof(pcell_t));
	if (ret < 0) {
		device_printf(dev, "ERROR: failed to read REG property\n");
		return (ENOMEM);
	}
	sc->reg_offset = (uint64_t)cells[0];
	if (acells == 2)
		sc->reg_offset = (sc->reg_offset << 32) | (uint64_t)cells[1];
	reg_size = (uint64_t)cells[acells];
	if (scells == 2)
		reg_size = (reg_size << 32) | (uint64_t)cells[acells + 1];

	if (reg_size != 4) {
		device_printf(dev, "ERROR, expected only single register\n");
		return (EINVAL);
	}
	if (sc->reg_offset >> 32UL) {
		device_printf(dev, "ERROR, only 32-bit address offset is supported\n");
		return (EINVAL);
	}

	/* Get syscon handle */
	ret = SYSCON_GET_HANDLE(dev, &sc->syscon);
	if ((ret != 0) || (sc->syscon == NULL)) {
		device_printf(dev, "ERROR: failed to get syscon\n");
		return (EFAULT);
	}

	/* Initialize access mutex */
	mtx_init(&sc->mtx, "FSL clock mtx", NULL, MTX_DEF);

	/* Get clock names */
	ret = clk_get_by_ofw_index(dev, node, 0, &clk);
	if (ret) {
		device_printf(dev, "ERROR: failed to get parent clock\n");
		return (EINVAL);
	}
	pclkname[0] = clk_get_name(clk);
	ret = clk_parse_ofw_clk_name(dev, node, &oclkname);
	if (ret) {
		device_printf(dev, "ERROR: failed to get output clock name\n");
		return (EINVAL);
	}

#ifdef DEBUG
	device_printf(dev, "INFO: pclkname %s, oclkname %s\n", pclkname[0], oclkname);
#endif

	/* Fixup CLK structure */
	sc->clk_def.clkdef.name = oclkname;
	sc->clk_def.clkdef.parent_names = (const char **)pclkname;
	sc->clk_def.offset = (uint32_t)sc->reg_offset;
	sc->clk_def.clkdef.id = 1;
	sc->clk_def.clkdef.parent_cnt = 1;
	sc->clk_def.clkdef.flags =  0;
	sc->clk_def.div_flags = CLK_DIV_WITH_TABLE;
	sc->clk_def.i_shift = 0;
	sc->clk_def.i_width = 5;
	sc->clk_def.div_table = (struct clk_div_table*)ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	/* Create clock */
	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL)
		panic("clkdom == NULL");
	ret = clknode_div_register(sc->clkdom, &sc->clk_def);
	if (ret) {
		device_printf(dev, "ERROR: unable to register clock\n");
		return (EINVAL);
	}
	clkdom_finit(sc->clkdom);

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	return (0);
}

static int
ls1028a_flexspi_clk_detach(device_t dev)
{

	/* Clock detaching is not supported */
	return (EACCES);
}

static int
ls1028a_flexspi_clk_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct ls1028a_flexspi_clk_softc *sc;
	sc = device_get_softc(dev);

	*val = SYSCON_READ_4(sc->syscon, addr);

	return (0);
}

static int
ls1028a_flexspi_clk_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct ls1028a_flexspi_clk_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	ret = SYSCON_WRITE_4(sc->syscon, addr, val);

	return (ret);
}

static int
ls1028a_flexspi_clk_modify_4(device_t dev, bus_addr_t addr, uint32_t clr, uint32_t set)
{
	struct ls1028a_flexspi_clk_softc *sc;
	int ret;

	sc = device_get_softc(dev);

	ret = SYSCON_MODIFY_4(sc->syscon, addr, clr, set);

	return (ret);
}

static void
ls1028a_flexspi_clk_device_lock(device_t dev)
{
	struct ls1028a_flexspi_clk_softc *sc;
	sc = device_get_softc(dev);

	mtx_lock(&sc->mtx);
}

static void
ls1028a_flexspi_clk_device_unlock(device_t dev)
{
	struct ls1028a_flexspi_clk_softc *sc;

	sc = device_get_softc(dev);

	mtx_unlock(&sc->mtx);
}

static device_method_t ls1028a_flexspi_clk_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ls1028a_flexspi_clk_probe),
	DEVMETHOD(device_attach,	ls1028a_flexspi_clk_attach),
	DEVMETHOD(device_detach,	ls1028a_flexspi_clk_detach),

	DEVMETHOD(clkdev_read_4,	ls1028a_flexspi_clk_read_4),
	DEVMETHOD(clkdev_write_4,	ls1028a_flexspi_clk_write_4),
	DEVMETHOD(clkdev_modify_4,	ls1028a_flexspi_clk_modify_4),
	DEVMETHOD(clkdev_device_lock,	ls1028a_flexspi_clk_device_lock),
	DEVMETHOD(clkdev_device_unlock,	ls1028a_flexspi_clk_device_unlock),

	DEVMETHOD_END
};

static DEFINE_CLASS_0(fspi_clk, ls1028a_flexspi_clk_driver, ls1028a_flexspi_clk_methods,
    sizeof(struct ls1028a_flexspi_clk_softc));
EARLY_DRIVER_MODULE(ls1028a_flexspi_clk, simple_mfd, ls1028a_flexspi_clk_driver,
    NULL, NULL, BUS_PASS_TIMER);
