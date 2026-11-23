/*
 * Copyright 2016 Michal Meloun <mmel@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/cdefs.h>
#include "opt_platform.h"
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/gpio/gpiobusvar.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/regulator/regulator.h>

#include <dev/clk/clk_fixed.h>

#define	CLK_TYPE_FIXED		1
#define	CLK_TYPE_FIXED_FACTOR	2
#define	CLK_TYPE_GATED_FIXED	3

#define	CLK_FIXED_OK		0
#define	CLK_FIXED_NEW_PASS	1
#define	CLK_FIXED_FAIL		2

static int clknode_fixed_init(struct clknode *clk, device_t dev);
static int clknode_fixed_recalc(struct clknode *clk, uint64_t *freq);
static int clknode_fixed_set_freq(struct clknode *clk, uint64_t fin,
    uint64_t *fout, int flags, int *stop);
static int clknode_fixed_set_gate(struct clknode *clk, bool enable);
static int clknode_fixed_get_gate(struct clknode *clk, bool *enable);


struct clk_fixed_softc {
	device_t		dev;
	phandle_t 		node;
	struct clkdom		*clkdom;
	bool			attach_done;

	bool			have_gpio;
	bool			gpio_done;
	struct gpiobus_pin	*gpio_pin;

	bool			have_reg;
	bool			reg_done;
	regulator_t		reg_vdd;
};

struct clknode_fixed_sc {
	int			fixed_flags;
	uint64_t		freq;
	uint32_t		mult;
	uint32_t		div;
};

static clknode_method_t clknode_fixed_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,	   clknode_fixed_init),
	CLKNODEMETHOD(clknode_recalc_freq, clknode_fixed_recalc),
	CLKNODEMETHOD(clknode_set_freq,	   clknode_fixed_set_freq),
	CLKNODEMETHOD(clknode_get_gate,	   clknode_fixed_get_gate),
	CLKNODEMETHOD(clknode_set_gate,	   clknode_fixed_set_gate),

	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(clknode_fixed, clknode_fixed_class, clknode_fixed_methods,
   sizeof(struct clknode_fixed_sc), clknode_class);

static int
clknode_fixed_init(struct clknode *clk, device_t dev)
{
	struct clknode_fixed_sc *sc;

	sc = clknode_get_softc(clk);
	if (sc->freq == 0)
		clknode_init_parent_idx(clk, 0);
	return(0);
}

static int
clknode_fixed_recalc(struct clknode *clk, uint64_t *freq)
{
	struct clknode_fixed_sc *sc;

	sc = clknode_get_softc(clk);

	if ((sc->mult != 0) && (sc->div != 0))
		*freq = (*freq / sc->div) * sc->mult;
	else
		*freq = sc->freq;
	return (0);
}

static int
clknode_fixed_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{
	struct clknode_fixed_sc *sc;

	sc = clknode_get_softc(clk);
	if (sc->mult == 0 || sc->div == 0) {
		/* Fixed frequency clock. */
		*stop = 1;
		if (*fout != sc->freq)
			return (ERANGE);
		return (0);
	}
	/* Fixed factor clock. */
	*stop = 0;
	*fout = (*fout / sc->mult) *  sc->div;
	return (0);
}

static int
clknode_fixed_set_gate(struct clknode *clk, bool enable)
{
	struct clk_fixed_softc *sc_dev;
	device_t dev;
	int rv;

	dev = clknode_get_device(clk);
	sc_dev = device_get_softc(dev);

	if (!sc_dev->have_gpio && !sc_dev->have_reg) {
		return (enable ? 0 : ENXIO);
	}
	if (sc_dev->have_gpio) {
		if (!sc_dev->gpio_done) {
			panic("%s: attempt to use not yet resolved GPIO",
			    __func__);
		}
		rv = gpio_pin_set_active(sc_dev->gpio_pin, enable);
		if (rv != 0) {
			device_printf(dev, "Cannot set GPIO pin: %d\n", rv);
			return (rv);
		}
	}
	if (sc_dev->have_reg) {
		if (!sc_dev->reg_done) {
			panic("%s: attempt to use not yet resolved regulator",
			    __func__);
		}
		if (enable)
			rv = regulator_enable(sc_dev->reg_vdd);
		else
			rv = regulator_disable(sc_dev->reg_vdd);
		if (rv != 0) {
			device_printf(dev, "Cannot enable regulator: %d\n", rv);
			return (rv);
		}
	}
	return(0);
}

static int
clknode_fixed_get_gate(struct clknode *clk, bool *enabled)
{
	struct clk_fixed_softc *sc_dev;
	device_t dev;
	uint32_t status;
	int rv;

	dev = clknode_get_device(clk);
	sc_dev = device_get_softc(dev);

	*enabled = true;
	if (sc_dev->have_gpio) {
		if (!sc_dev->gpio_done) {
			panic("%s: attempt to use not yet resolved GPIO",
			    __func__);
		}
		rv = gpio_pin_is_active(sc_dev->gpio_pin, enabled);
		if (rv != 0) {
			device_printf(dev, "Cannot get GPIO pin: %d\n", rv);
			return (rv);
		}
	}

	if (sc_dev->have_reg) {
		if (!sc_dev->reg_done) {
			panic("%s: attempt to use not yet resolved regulator",
			    __func__);
		}
		rv = regulator_status(sc_dev->reg_vdd, &status);
		if (rv != 0) {
			device_printf(dev, "Cannot get regulator: %d\n", rv);
			return (rv);
		}
		if ((status & REGULATOR_STATUS_ENABLED) == 0)
			*enabled = false;
	}

	return(0);
}

int
clknode_fixed_register(struct clkdom *clkdom, struct clk_fixed_def *clkdef)
{
	struct clknode *clk;
	struct clknode_fixed_sc *sc;

	clk = clknode_create(clkdom, &clknode_fixed_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->fixed_flags = clkdef->fixed_flags;
	sc->freq = clkdef->freq;
	sc->mult = clkdef->mult;
	sc->div = clkdef->div;

	clknode_register(clkdom, clk);
	return (0);
}

static struct ofw_compat_data compat_data[] = {
	{"fixed-clock",		CLK_TYPE_FIXED},
	{"fixed-factor-clock",  CLK_TYPE_FIXED_FACTOR},
	{"gated-fixed-clock",	CLK_TYPE_GATED_FIXED},
	{NULL,		 	0},
};

static int
clk_fixed_init_fixed(struct clk_fixed_softc *sc,
    struct clk_fixed_def *def)
{
	uint32_t freq;
	int rv;

	def->clkdef.id = 1;
	rv = OF_getencprop(sc->node, "clock-frequency", &freq,  sizeof(freq));
	if (rv <= 0) {
		device_printf(sc->dev, "Cannot get output frequency: %d\n", rv);
		return (ENXIO);
	}
	def->freq = freq;

	return (0);
}


static int
clk_fixed_init_fixed_factor(struct clk_fixed_softc *sc,
    struct clk_fixed_def *def)
{
	int rv;
	clk_t parent;

	def->clkdef.id = 1;
	def->clkdef.parent_cnt = 1;

	rv = OF_getencprop(sc->node, "clock-mult", &def->mult,
	    sizeof(def->mult));
	if (rv <= 0) {
		device_printf(sc->dev,
		    "Cannot get 'clock-mult' property: %d\n", rv);
		return (ENXIO);
	}
	rv = OF_getencprop(sc->node, "clock-div", &def->div,
	    sizeof(def->div));
	if (rv <= 0) {
		device_printf(sc->dev,
		    "Cannot get 'clock-div' property: %d\n", rv);
		return (ENXIO);
	}
	/* Get name of parent clock */
	rv = clk_get_by_ofw_index(sc->dev, 0, 0, &parent);
	if (rv != 0) {
		device_printf(sc->dev,
		    "Cannot get parent clock: %d\n", rv);
		return (ENXIO);
	}
	def->clkdef.parent_names = malloc(sizeof(char *), M_OFWPROP, M_WAITOK);
	def->clkdef.parent_names[0] = clk_get_name(parent);
	def->clkdef.parent_cnt = 1;
	clk_release(parent);
	return (0);
}


static int
clk_fixed_get_gpio(struct clk_fixed_softc * sc)
{
	uint32_t flags;
	int rv;

	if (sc->gpio_done)
		return (CLK_FIXED_OK);

	rv = gpio_pin_get_by_ofw_property(sc->dev, sc->node, "enable-gpios",
	    &sc->gpio_pin);
	if (rv == ENOENT) {
		/* No property exist */
		sc->gpio_done = true;
		return (CLK_FIXED_OK);
	}

	if (rv == ENODEV) {
		/* GPIO ctrl is not yet available */
		sc->have_gpio = true;
		return (CLK_FIXED_NEW_PASS);
	}

	if (rv != 0) {
		device_printf(sc->dev, "Cannot get GPIO pin: %d\n", rv);
		sc->gpio_done = true;
		return (CLK_FIXED_FAIL);
	}

	/* Configure GPIO pin to output */
	rv = GPIO_PIN_GETFLAGS(sc->gpio_pin->dev, sc->gpio_pin->pin,
	     &flags);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get GPIO flags: %d\n", rv);
		return (CLK_FIXED_FAIL);
	}
	if ((flags & GPIO_PIN_OUTPUT) == 0) {
		flags |= GPIO_PIN_OUTPUT;
		flags &= ~GPIO_PIN_INPUT;
		rv = GPIO_PIN_SETFLAGS(sc->gpio_pin->dev, sc->gpio_pin->pin,
		    GPIO_PIN_OUTPUT);
		if (rv != 0) {
			device_printf(sc->dev, "Cannot set GPIO flags: %d\n",
			    rv);
			return (CLK_FIXED_FAIL);
		}
	}

	sc->have_gpio = true;
	sc->gpio_done = true;

	return (CLK_FIXED_OK);
}

static int
clk_fixed_get_regulator(struct clk_fixed_softc * sc)
{
	int rv;

	if (sc->reg_done)
		return (CLK_FIXED_OK);

	rv = regulator_get_by_ofw_property(sc->dev, sc->node, "vdd-supply",
	     &sc->reg_vdd);
	if (rv == ENOENT) {
		/* No property exist */
		sc->reg_done = true;
		return (CLK_FIXED_OK);
	}
	if (rv == ENODEV) {
		/* Regulator is not yet available */
		sc->have_reg = true;
		return (CLK_FIXED_NEW_PASS);
	}
	if (rv != 0) {
		device_printf(sc->dev, "Cannot get 'vdd' regulator: %d\n", rv);
		sc->reg_done = true;
		return (CLK_FIXED_FAIL);
	}

	rv = regulator_enable(sc->reg_vdd);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot enable 'vdd' regulator: %d\n",
		    rv);
		sc->reg_done = true;
		return (CLK_FIXED_FAIL);
	}

	sc->have_reg = true;
	sc->reg_done = true;

	return (CLK_FIXED_OK);
}

static int
clk_fixed_probe(device_t dev)
{
	intptr_t clk_type;

	clk_type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	switch (clk_type) {
	case CLK_TYPE_FIXED:
	case CLK_TYPE_GATED_FIXED:
		if (!OF_hasprop(ofw_bus_get_node(dev), "clock-frequency")) {
			if (bootverbose)
				device_printf(dev,
				    "(gated-)clock-fixed has no "
				    "clock-frequency\n");
			return (ENXIO);
		}
		device_set_desc(dev, "Fixed clock");
		break;
	case CLK_TYPE_FIXED_FACTOR:
		device_set_desc(dev, "Fixed factor clock");
		break;
	default:
		return (ENXIO);
	}

	if (!bootverbose)
		device_quiet(dev);

	return (BUS_PROBE_DEFAULT);
}

static void
clk_fixed_new_pass(device_t dev)
{
	struct clk_fixed_softc * sc;
	int rv;

	sc = device_get_softc(dev);
	if (sc->attach_done)
		return;

	/* Try to get and configure GPIO. */
	rv = 0;

	if (sc->have_gpio && !sc->gpio_done)
		rv = clk_fixed_get_gpio(sc);
	if (rv == CLK_FIXED_FAIL)
		goto fail;

	/* Try to get  regulator. */
	if (sc->have_reg && !sc->reg_done)
		rv = clk_fixed_get_regulator(sc);
	if (rv == CLK_FIXED_FAIL)
		goto fail;

	if ((sc->have_gpio && !sc->gpio_done) ||
	    (sc->have_reg && !sc->reg_done)) {
		bus_generic_new_pass(dev);
		return;
	}

fail:
	sc->attach_done = true;
	return;
}

static int
clk_fixed_attach(device_t dev)
{
	struct clk_fixed_softc *sc;
	intptr_t clk_type;
	struct clk_fixed_def def;
	int rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->node  = ofw_bus_get_node(dev);
	memset(&def, 0, sizeof(def));
	clk_type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	if (clk_type == CLK_TYPE_FIXED || clk_type == CLK_TYPE_GATED_FIXED)
		rv = clk_fixed_init_fixed(sc, &def);
	else if (clk_type == CLK_TYPE_FIXED_FACTOR)
		rv = clk_fixed_init_fixed_factor(sc, &def);
	else
		rv = ENXIO;
	if (rv != 0) {
		device_printf(sc->dev, "Cannot read FDT parameters.\n");
		goto fail;
	}

	rv = clk_parse_ofw_clk_name(dev, sc->node, &def.clkdef.name);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot parse clock name.\n");
		goto fail;
	}
	if (clk_type == CLK_TYPE_GATED_FIXED) {
		/* Try to get and configure GPIO. */
		rv = clk_fixed_get_gpio(sc);
		if (rv == CLK_FIXED_FAIL)
			goto fail;

		/* Try to get and configure regulator. */
		rv = clk_fixed_get_regulator(sc);
		if (rv == CLK_FIXED_FAIL)
			goto fail;
	}

	sc->clkdom = clkdom_create(dev);
	KASSERT(sc->clkdom != NULL, ("Clock domain is NULL"));

	rv = clknode_fixed_register(sc->clkdom, &def);
	if (rv != 0) {
		device_printf(sc->dev, "Cannot register fixed clock.\n");
		rv = ENXIO;
		goto fail;
	}

	rv = clkdom_finit(sc->clkdom);
	if (rv != 0) {
		device_printf(sc->dev, "Clk domain finit fails.\n");
		rv = ENXIO;
		goto fail;
	}

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	if ((sc->have_gpio == sc->gpio_done) && (sc->have_reg == sc->reg_done))
		sc->attach_done = true;
	return (0);

fail:
	OF_prop_free(__DECONST(char *, def.clkdef.name));
	OF_prop_free(def.clkdef.parent_names);
	return (rv);
}

static device_method_t clk_fixed_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		clk_fixed_probe),
	DEVMETHOD(device_attach,	clk_fixed_attach),

	/* Bus interface */
	DEVMETHOD(bus_new_pass,		clk_fixed_new_pass),

	DEVMETHOD_END
};

DEFINE_CLASS_0(clk_fixed, clk_fixed_driver, clk_fixed_methods,
    sizeof(struct clk_fixed_softc));
EARLY_DRIVER_MODULE(clk_fixed, simplebus, clk_fixed_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(clk_fixed, 1);
