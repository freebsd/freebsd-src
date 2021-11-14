/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018-2021 Emmanuel Vadot <manu@FreeBSD.org>
 * Copyright (c) 2021 Bjoern A. Zeeb <bz@FreeBSD.org>
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/clock.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>

#include <dev/iicbus/pmic/rockchip/rk8xx.h>

/* Clock class and method */
struct rk8xx_clk_sc {
	device_t		base_dev;
};

#define	CLK32OUT_REG		0x20
#define	CLK32OUT_CLKOUT2_EN	1

static int
rk8xx_clk_set_gate_1(struct clknode *clk, bool enable)
{
	struct rk8xx_clk_sc *sc;
	uint8_t val;

	sc = clknode_get_softc(clk);

	rk8xx_read(sc->base_dev, CLK32OUT_REG, &val, sizeof(val));
	if (enable)
		val |= CLK32OUT_CLKOUT2_EN;
	else
		val &= ~CLK32OUT_CLKOUT2_EN;
	rk8xx_write(sc->base_dev, CLK32OUT_REG, &val, 1);

	return (0);
}

static int
rk8xx_clk_recalc(struct clknode *clk, uint64_t *freq)
{

	*freq = 32768;
	return (0);
}

static clknode_method_t rk8xx_clk_clknode_methods_0[] = {
	CLKNODEMETHOD(clknode_recalc_freq,	rk8xx_clk_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(rk8xx_clk_clknode_0, rk8xx_clk_clknode_class_0,
    rk8xx_clk_clknode_methods_0, sizeof(struct rk8xx_clk_sc),
    clknode_class);

static clknode_method_t rk8xx_clk_clknode_methods_1[] = {
	CLKNODEMETHOD(clknode_set_gate,		rk8xx_clk_set_gate_1),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(rk8xx_clk_clknode_1, rk8xx_clk_clknode_class_1,
    rk8xx_clk_clknode_methods_1, sizeof(struct rk8xx_clk_sc),
    rk8xx_clk_clknode_class_0);

int
rk8xx_export_clocks(device_t dev)
{
	struct clkdom *clkdom;
	struct clknode_init_def clkidef;
	struct clknode *clk;
	struct rk8xx_clk_sc *clksc;
	const char **clknames;
	phandle_t node;
	int nclks, rv;

	node = ofw_bus_get_node(dev);

	/* clock-output-names are optional. Could use them for clkidef.name. */
	nclks = ofw_bus_string_list_to_array(node, "clock-output-names",
	    &clknames);

	clkdom = clkdom_create(dev);

	memset(&clkidef, 0, sizeof(clkidef));
	clkidef.id = 0;
	clkidef.name = (nclks = 2) ? clknames[0] : "clk32kout1";
	clk = clknode_create(clkdom, &rk8xx_clk_clknode_class_0, &clkidef);
	if (clk == NULL) {
		device_printf(dev, "Cannot create '%s'.\n", clkidef.name);
		return (ENXIO);
	}
	clksc = clknode_get_softc(clk);
	clksc->base_dev = dev;
	clknode_register(clkdom, clk);

	memset(&clkidef, 0, sizeof(clkidef));
	clkidef.id = 1;
	clkidef.name = (nclks = 2) ? clknames[1] : "clk32kout2";
	clk = clknode_create(clkdom, &rk8xx_clk_clknode_class_1, &clkidef);
	if (clk == NULL) {
		device_printf(dev, "Cannot create '%s'.\n", clkidef.name);
		return (ENXIO);
	}
	clksc = clknode_get_softc(clk);
	clksc->base_dev = dev;
	clknode_register(clkdom, clk);

	rv = clkdom_finit(clkdom);
	if (rv != 0) {
		device_printf(dev, "Cannot finalize clkdom initialization: "
		    "%d\n", rv);
		return (ENXIO);
	}

	if (bootverbose)
		clkdom_dump(clkdom);

	return (0);
}
