/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alstom Group.
 * Copyright (c) 2020 Semihalf.
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk_fixed.h>

#include <arm64/qoriq/clk/qoriq_clkgen.h>

#include "clkdev_if.h"

MALLOC_DEFINE(M_QORIQ_CLKGEN, "qoriq_clkgen", "qoriq_clkgen");

static struct resource_spec qoriq_clkgen_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static const char *qoriq_pll_parents_coreclk[] = {
	QORIQ_CORECLK_NAME
};

static const char *qoriq_pll_parents_sysclk[] = {
	QORIQ_SYSCLK_NAME
};

static int
qoriq_clkgen_ofw_mapper(struct clkdom *clkdom, uint32_t ncells,
    phandle_t *cells, struct clknode **clk)
{

	if (ncells != 2)
		return (EINVAL);

	if (cells[0] > 5)
		return (EINVAL);

	if (cells[0] == QORIQ_TYPE_SYSCLK || cells[0] == QORIQ_TYPE_CORECLK)
		if (cells[1] != 0)
			return (EINVAL);

	*clk = clknode_find_by_id(clkdom, QORIQ_CLK_ID(cells[0], cells[1]));

	if (*clk == NULL)
		return (EINVAL);

	return (0);
}

static int
qoriq_clkgen_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct qoriq_clkgen_softc *sc;

	sc = device_get_softc(dev);

	if (sc->flags & QORIQ_LITTLE_ENDIAN)
		bus_write_4(sc->res, addr, htole32(val));
	else
		bus_write_4(sc->res, addr, htobe32(val));
	return (0);
}

static int
qoriq_clkgen_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct qoriq_clkgen_softc *sc;

	sc = device_get_softc(dev);

	if (sc->flags & QORIQ_LITTLE_ENDIAN)
		*val = le32toh(bus_read_4(sc->res, addr));
	else
		*val = be32toh(bus_read_4(sc->res, addr));
	return (0);
}

static int
qoriq_clkgen_modify_4(device_t dev, bus_addr_t addr, uint32_t clr,
    uint32_t set)
{
	struct qoriq_clkgen_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	if (sc->flags & QORIQ_LITTLE_ENDIAN)
		reg = le32toh(bus_read_4(sc->res, addr));
	else
		reg = be32toh(bus_read_4(sc->res, addr));

	reg &= ~clr;
	reg |= set;

	if (sc->flags & QORIQ_LITTLE_ENDIAN)
		bus_write_4(sc->res, addr, htole32(reg));
	else
		bus_write_4(sc->res, addr, htobe32(reg));

	return (0);
}

static void
qoriq_clkgen_device_lock(device_t dev)
{
	struct qoriq_clkgen_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
qoriq_clkgen_device_unlock(device_t dev)
{
	struct qoriq_clkgen_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

static device_method_t qoriq_clkgen_methods[] = {
	DEVMETHOD(clkdev_write_4,	qoriq_clkgen_write_4),
	DEVMETHOD(clkdev_read_4,	qoriq_clkgen_read_4),
	DEVMETHOD(clkdev_modify_4,	qoriq_clkgen_modify_4),
	DEVMETHOD(clkdev_device_lock,	qoriq_clkgen_device_lock),
	DEVMETHOD(clkdev_device_unlock,	qoriq_clkgen_device_unlock),

	DEVMETHOD_END
};

DEFINE_CLASS_0(qoriq_clkgen, qoriq_clkgen_driver, qoriq_clkgen_methods,
    sizeof(struct qoriq_clkgen_softc));

static int
qoriq_clkgen_create_sysclk(device_t dev)
{
	struct qoriq_clkgen_softc *sc;
	struct clk_fixed_def def;
	const char *clkname;
	phandle_t node;
	uint32_t freq;
	clk_t clock;
	int rv;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	sc->has_coreclk = false;

	memset(&def, 0, sizeof(def));

	rv = OF_getencprop(node, "clock-frequency", &freq, sizeof(freq));
	if (rv > 0) {
		def.clkdef.name = QORIQ_SYSCLK_NAME;
		def.clkdef.id = QORIQ_CLK_ID(QORIQ_TYPE_SYSCLK, 0);
		def.freq = freq;

		rv = clknode_fixed_register(sc->clkdom, &def);
		return (rv);
	} else {
		/*
		 * As both sysclk and coreclk need to be accessible from
		 * device tree, create internal 1:1 divider nodes.
		 */
		def.clkdef.parent_cnt = 1;
		def.freq = 0;
		def.mult = 1;
		def.div = 1;

		rv = clk_get_by_ofw_name(dev, node, "coreclk", &clock);
		if (rv == 0) {
			def.clkdef.name = QORIQ_CORECLK_NAME;
			clkname = clk_get_name(clock);
			def.clkdef.parent_names = &clkname;
			def.clkdef.id = QORIQ_CLK_ID(QORIQ_TYPE_CORECLK, 0);

			rv = clknode_fixed_register(sc->clkdom, &def);
			if (rv)
				return (rv);

			sc->has_coreclk = true;
		}

		rv = clk_get_by_ofw_name(dev, node, "sysclk", &clock);
		if (rv != 0) {
			rv = clk_get_by_ofw_index(dev, node, 0, &clock);
			if (rv != 0)
				return (rv);
		}

		clkname = clk_get_name(clock);
		def.clkdef.name = QORIQ_SYSCLK_NAME;
		def.clkdef.id = QORIQ_CLK_ID(QORIQ_TYPE_SYSCLK, 0);
		def.clkdef.parent_names = &clkname;

		rv = clknode_fixed_register(sc->clkdom, &def);
		return (rv);
	}
}

int
qoriq_clkgen_attach(device_t dev)
{
	struct qoriq_clkgen_softc *sc;
	int i, error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, qoriq_clkgen_spec, &sc->res) != 0) {
		device_printf(dev, "Cannot allocate resources.\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL)
		panic("Cannot create clock domain.\n");

	error = qoriq_clkgen_create_sysclk(dev);
	if (error != 0) {
		device_printf(dev, "Cannot create sysclk.\n");
		return (error);
	}

	sc->pltfrm_pll_def->clkdef.parent_names = qoriq_pll_parents_sysclk;
	sc->pltfrm_pll_def->clkdef.parent_cnt = 1;
	error = qoriq_clk_pll_register(sc->clkdom, sc->pltfrm_pll_def);
	if (error != 0) {
		device_printf(dev, "Cannot create platform PLL.\n");
		return (error);
	}

	for (i = 0; i < sc->cga_pll_num; i++) {
		if (sc->has_coreclk)
			sc->cga_pll[i]->clkdef.parent_names = qoriq_pll_parents_coreclk;
		else
			sc->cga_pll[i]->clkdef.parent_names = qoriq_pll_parents_sysclk;
		sc->cga_pll[i]->clkdef.parent_cnt = 1;

		error = qoriq_clk_pll_register(sc->clkdom, sc->cga_pll[i]);
		if (error != 0) {
			device_printf(dev, "Cannot create CGA PLLs\n.");
			return (error);
		}
	}

	/*
	 * Both CMUX and HWACCEL multiplexer nodes can be represented
	 * by using built in clk_mux nodes.
	 */
	for (i = 0; i < sc->mux_num; i++) {
		error = clknode_mux_register(sc->clkdom, sc->mux[i]);
		if (error != 0) {
			device_printf(dev, "Cannot create MUX nodes.\n");
			return (error);
		}
	}

	if (sc->init_func != NULL) {
		error = sc->init_func(dev);
		if (error) {
			device_printf(dev, "Clock init function failed.\n");
			return (error);
		}
	}

	clkdom_set_ofw_mapper(sc->clkdom, qoriq_clkgen_ofw_mapper);

	if (clkdom_finit(sc->clkdom) != 0)
		panic("Cannot finalize clock domain initialization.\n");

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	return (0);
}
