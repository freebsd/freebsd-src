/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Axiado Corporation
 * All rights reserved.
 *
 * This software was developed in part by Kristof Provost under contract for
 * Axiado Corporation.
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
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <dev/extres/clk/clk.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/openfirm.h>

static struct resource_spec prci_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	RESOURCE_SPEC_END
};

struct prci_softc {
	device_t		dev;

	struct mtx		mtx;

	struct clkdom		*clkdom;
	struct resource		*res;
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
};

struct prci_clk_pll_sc {
	struct prci_softc	*parent_sc;
};

#define	PRCI_LOCK(sc)			mtx_lock(&(sc)->mtx)
#define	PRCI_UNLOCK(sc)			mtx_unlock(&(sc)->mtx)
#define	PRCI_ASSERT_LOCKED(sc)		mtx_assert(&(sc)->mtx, MA_OWNED);
#define	PRCI_ASSERT_UNLOCKED(sc)	mtx_assert(&(sc)->mtx, MA_NOTOWNED);

#define	PRCI_COREPLL			0x4
#define		PRCI_COREPLL_DIVR_MASK	0x3f
#define		PRCI_COREPLL_DIVR_SHIFT	0
#define		PRCI_COREPLL_DIVF_MASK	0x7fc0
#define		PRCI_COREPLL_DIVF_SHIFT	6
#define		PRCI_COREPLL_DIVQ_MASK	0x38000
#define		PRCI_COREPLL_DIVQ_SHIFT	15

#define	PRCI_READ(_sc, _reg)		\
    bus_space_read_4((_sc)->bst, (_sc)->bsh, (_reg))

static int
prci_clk_pll_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);

	return (0);
}

static int
prci_clk_pll_recalc(struct clknode *clk, uint64_t *freq)
{
	struct prci_clk_pll_sc *sc;
	struct clknode *parent_clk;
	uint32_t val;
	uint64_t refclk, divf, divq, divr;
	int err;

	KASSERT(freq != NULL, ("freq cannot be NULL"));

	sc = clknode_get_softc(clk);

	PRCI_LOCK(sc->parent_sc);

	/* Get refclock frequency. */
	parent_clk = clknode_get_parent(clk);
	err = clknode_get_freq(parent_clk, &refclk);
	if (err) {
		device_printf(sc->parent_sc->dev,
		    "Failed to get refclk frequency\n");
		PRCI_UNLOCK(sc->parent_sc);
		return (err);
	}

	/* Calculate the PLL output */
	val = PRCI_READ(sc->parent_sc, PRCI_COREPLL);

	divf = (val & PRCI_COREPLL_DIVF_MASK) >> PRCI_COREPLL_DIVF_SHIFT;
	divq = (val & PRCI_COREPLL_DIVQ_MASK) >> PRCI_COREPLL_DIVQ_SHIFT;
	divr = (val & PRCI_COREPLL_DIVR_MASK) >> PRCI_COREPLL_DIVR_SHIFT;

	*freq = refclk / (divr + 1) * (2 * (divf + 1)) / (1 << divq);

	PRCI_UNLOCK(sc->parent_sc);

	return (0);
}

static clknode_method_t prci_clk_pll_clknode_methods[] = {
	CLKNODEMETHOD(clknode_init,		prci_clk_pll_init),
	CLKNODEMETHOD(clknode_recalc_freq,	prci_clk_pll_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(prci_clk_pll_clknode, prci_clk_pll_clknode_class,
    prci_clk_pll_clknode_methods, sizeof(struct prci_clk_pll_sc),
    clknode_class);

static int
prci_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "sifive,aloeprci0"))
		return (ENXIO);

	device_set_desc(dev, "SiFive FU540 Power Reset Clocking Interrupt");

	return (BUS_PROBE_DEFAULT);
}

static void
prci_pll_register(struct prci_softc *parent_sc, struct clknode_init_def *clkdef)
{
	struct clknode *clk;
	struct prci_clk_pll_sc *sc;

	clk = clknode_create(parent_sc->clkdom, &prci_clk_pll_clknode_class,
	    clkdef);
	if (clk == NULL)
		panic("Failed to create clknode");

	sc = clknode_get_softc(clk);
	sc->parent_sc = parent_sc;

	clknode_register(parent_sc->clkdom, clk);
}

static int
prci_attach(device_t dev)
{
	struct clknode_init_def clkdef;
	struct prci_softc *sc;
	clk_t clk_parent;
	phandle_t node;
	int i, ncells, error;

	sc = device_get_softc(dev);
	sc->dev = dev;

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	error = bus_alloc_resources(dev, prci_spec, &sc->res);
	if (error) {
		device_printf(dev, "Couldn't allocate resources\n");
		goto fail;
	}
	sc->bst = rman_get_bustag(sc->res);
	sc->bsh = rman_get_bushandle(sc->res);

	node = ofw_bus_get_node(dev);
	error = ofw_bus_parse_xref_list_get_length(node, "clocks",
	    "#clock-cells", &ncells);
	if (error != 0 || ncells != 1) {
		device_printf(dev, "couldn't find parent clock\n");
		goto fail;
	}

	bzero(&clkdef, sizeof(clkdef));
	clkdef.id = 0;
	clkdef.name = "coreclk";
	clkdef.parent_names = mallocarray(ncells, sizeof(char *), M_OFWPROP,
	    M_WAITOK);
	for (i = 0; i < ncells; i++) {
		error = clk_get_by_ofw_index(dev, 0, i, &clk_parent);
		if (error != 0) {
			device_printf(dev, "cannot get clock %d\n", error);
			goto fail1;
		}
		clkdef.parent_names[i] = clk_get_name(clk_parent);
		if (bootverbose)
			device_printf(dev, "clk parent: %s\n",
			    clkdef.parent_names[i]);
		clk_release(clk_parent);
	}
	clkdef.parent_cnt = ncells;

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "Couldn't create clock domain\n");
		goto fail;
	}

	/* We can't free a clkdom, so from now on we cannot fail. */
	prci_pll_register(sc, &clkdef);

	error = clkdom_finit(sc->clkdom);
	if (error)
		panic("Couldn't finalise clock domain");

	return (0);

fail1:
	free(clkdef.parent_names, M_OFWPROP);

fail:
	bus_release_resources(dev, prci_spec, &sc->res);
	mtx_destroy(&sc->mtx);
	return (error);
}

static device_method_t prci_methods[] = {
	DEVMETHOD(device_probe,		prci_probe),
	DEVMETHOD(device_attach,	prci_attach),

	DEVMETHOD_END
};

static driver_t prci_driver = {
	"fu540prci",
	prci_methods,
	sizeof(struct prci_softc)
};

static devclass_t prci_devclass;

EARLY_DRIVER_MODULE(fu540prci, simplebus, prci_driver, prci_devclass, 0, 0,
    BUS_PASS_BUS);
