/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2021 Emmanuel Vadot <manu@freebsd.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/gpio.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>

#include "mmc_pwrseq_if.h"

enum pwrseq_type {
	PWRSEQ_SIMPLE = 1,
	PWRSEQ_EMMC,
};

static struct ofw_compat_data compat_data[] = {
	{ "mmc-pwrseq-simple",	PWRSEQ_SIMPLE },
	{ "mmc-pwrseq-emmc",	PWRSEQ_EMMC },
	{ NULL,			0 }
};

struct mmc_pwrseq_softc {
	enum pwrseq_type	type;
	clk_t			ext_clock;
	struct gpiobus_pin	*reset_gpio;

	uint32_t		post_power_on_delay_ms;
	uint32_t		power_off_delay_us;
};

static int
mmc_pwrseq_probe(device_t dev)
{
	enum pwrseq_type type;

	if (!ofw_bus_search_compatible(dev, compat_data)->ocd_data)
		return (ENXIO);

	type = (enum pwrseq_type)ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	switch (type) {
	case PWRSEQ_SIMPLE:
		device_set_desc(dev, "MMC Simple Power sequence");
		break;
	case PWRSEQ_EMMC:
		device_set_desc(dev, "MMC eMMC Power sequence");
		break;
	}
	return (BUS_PROBE_DEFAULT);
}

static int
mmc_pwrseq_attach(device_t dev)
{
	struct mmc_pwrseq_softc *sc;
	phandle_t node;
	int rv;

	sc = device_get_softc(dev);
	sc->type = (enum pwrseq_type)ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	node = ofw_bus_get_node(dev);

	if (sc->type == PWRSEQ_SIMPLE) {
		if (OF_hasprop(node, "clocks")) {
			rv = clk_get_by_ofw_name(dev, 0, "ext_clock", &sc->ext_clock);
			if (rv != 0) {
				device_printf(dev,
				    "Node have a clocks property but no clocks named \"ext_clock\"\n");
				return (ENXIO);
			}
		}
		OF_getencprop(node, "post-power-on-delay-ms", &sc->post_power_on_delay_ms, sizeof(uint32_t));
		OF_getencprop(node, "power-off-delay-us", &sc->power_off_delay_us, sizeof(uint32_t));
	}

	if (OF_hasprop(node, "reset-gpios")) {
		if (gpio_pin_get_by_ofw_property(dev, node, "reset-gpios",
		    &sc->reset_gpio) != 0) {
			device_printf(dev, "Cannot get the reset-gpios\n");
			return (ENXIO);
		}
		gpio_pin_setflags(sc->reset_gpio, GPIO_PIN_OUTPUT);
		gpio_pin_set_active(sc->reset_gpio, true);
	}

	OF_device_register_xref(OF_xref_from_node(node), dev);
	return (0);
}

static int
mmc_pwrseq_detach(device_t dev)
{

	return (EBUSY);
}

static int
mmv_pwrseq_set_power(device_t dev, bool power_on)
{
	struct mmc_pwrseq_softc *sc;
	int rv;

	sc = device_get_softc(dev);

	if (power_on) {
		if (sc->ext_clock) {
			rv = clk_enable(sc->ext_clock);
			if (rv != 0)
				return (rv);
		}

		if (sc->reset_gpio) {
			rv = gpio_pin_set_active(sc->reset_gpio, false);
			if (rv != 0)
				return (rv);
		}

		if (sc->post_power_on_delay_ms)
			DELAY(sc->post_power_on_delay_ms * 1000);
	} else {
		if (sc->reset_gpio) {
			rv = gpio_pin_set_active(sc->reset_gpio, true);
			if (rv != 0)
				return (rv);
		}

		if (sc->ext_clock) {
			rv = clk_stop(sc->ext_clock);
			if (rv != 0)
				return (rv);
		}
		if (sc->power_off_delay_us)
			DELAY(sc->power_off_delay_us);
	}

	return (0);
}

static device_method_t mmc_pwrseq_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mmc_pwrseq_probe),
	DEVMETHOD(device_attach,	mmc_pwrseq_attach),
	DEVMETHOD(device_detach,	mmc_pwrseq_detach),

	DEVMETHOD(mmc_pwrseq_set_power,	mmv_pwrseq_set_power),
	DEVMETHOD_END
};

static driver_t mmc_pwrseq_driver = {
	"mmc_pwrseq",
	mmc_pwrseq_methods,
	sizeof(struct mmc_pwrseq_softc),
};

EARLY_DRIVER_MODULE(mmc_pwrseq, simplebus, mmc_pwrseq_driver, 0, 0,
	BUS_PASS_SUPPORTDEV + BUS_PASS_ORDER_FIRST);
MODULE_VERSION(mmc_pwrseq, 1);
SIMPLEBUS_PNP_INFO(compat_data);
