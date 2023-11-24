/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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

/*
 * This is the shared pinmux code that the qualcomm SoCs use for their
 * specific way of configuring up pins.
 *
 * For now this does use the IPQ4018 TLMM related softc, but that
 * may change as I extend the driver to support multiple kinds of
 * qualcomm chipsets in the future.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <dev/gpio/gpiobusvar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/fdt/fdt_pinctrl.h>

#include "qcom_tlmm_var.h"
#include "qcom_tlmm_debug.h"

/*
 * For now we're hard-coded to doing IPQ4018 stuff here, but
 * it's not going to be very hard to flip it to being generic.
 */
#include "qcom_tlmm_ipq4018_hw.h"

#include "gpio_if.h"

/* Parameters */
static const struct qcom_tlmm_prop_name prop_names[] = {
	{ "bias-disable", PIN_ID_BIAS_DISABLE, 0 },
	{ "bias-high-impedance", PIN_ID_BIAS_HIGH_IMPEDANCE, 0 },
	{ "bias-bus-hold", PIN_ID_BIAS_BUS_HOLD, 0 },
	{ "bias-pull-up", PIN_ID_BIAS_PULL_UP, 0 },
	{ "bias-pull-down", PIN_ID_BIAS_PULL_DOWN, 0 },
	{ "bias-pull-pin-default", PIN_ID_BIAS_PULL_PIN_DEFAULT, 0 },
	{ "drive-push-pull", PIN_ID_DRIVE_PUSH_PULL, 0 },
	{ "drive-open-drain", PIN_ID_DRIVE_OPEN_DRAIN, 0 },
	{ "drive-open-source", PIN_ID_DRIVE_OPEN_SOURCE, 0 },
	{ "drive-strength", PIN_ID_DRIVE_STRENGTH, 1 },
	{ "input-enable", PIN_ID_INPUT_ENABLE, 0 },
	{ "input-disable", PIN_ID_INPUT_DISABLE, 0 },
	{ "input-schmitt-enable", PIN_ID_INPUT_SCHMITT_ENABLE, 0 },
	{ "input-schmitt-disable", PIN_ID_INPUT_SCHMITT_DISABLE, 0 },
	{ "input-debounce", PIN_ID_INPUT_DEBOUNCE, 0 },
	{ "power-source", PIN_ID_POWER_SOURCE, 0 },
	{ "slew-rate", PIN_ID_SLEW_RATE, 0},
	{ "low-power-enable", PIN_ID_LOW_POWER_MODE_ENABLE, 0 },
	{ "low-power-disable", PIN_ID_LOW_POWER_MODE_DISABLE, 0 },
	{ "output-low", PIN_ID_OUTPUT_LOW, 0, },
	{ "output-high", PIN_ID_OUTPUT_HIGH, 0, },
	{ "vm-enable", PIN_ID_VM_ENABLE, 0, },
	{ "vm-disable", PIN_ID_VM_DISABLE, 0, },
};

static const struct qcom_tlmm_spec_pin *
qcom_tlmm_pinctrl_search_spin(struct qcom_tlmm_softc *sc, char *pin_name)
{
	int i;

	if (sc->spec_pins == NULL)
		return (NULL);

	for (i = 0; sc->spec_pins[i].name != NULL; i++) {
		if (strcmp(pin_name, sc->spec_pins[i].name) == 0)
			return (&sc->spec_pins[i]);
	}

	return (NULL);
}

static int
qcom_tlmm_pinctrl_config_spin(struct qcom_tlmm_softc *sc,
     char *pin_name, const struct qcom_tlmm_spec_pin *spin,
    struct qcom_tlmm_pinctrl_cfg *cfg)
{
	/* XXX TODO */
	device_printf(sc->dev, "%s: TODO: called; pin_name=%s\n",
	     __func__, pin_name);
	return (0);
}

static const struct qcom_tlmm_gpio_mux *
qcom_tlmm_pinctrl_search_gmux(struct qcom_tlmm_softc *sc, char *pin_name)
{
	int i;

	if (sc->gpio_muxes == NULL)
		return (NULL);

	for (i = 0; sc->gpio_muxes[i].id >= 0; i++) {
		if (strcmp(pin_name, sc->gpio_muxes[i].name) == 0)
			return  (&sc->gpio_muxes[i]);
	}

	return (NULL);
}

static int
qcom_tlmm_pinctrl_gmux_function(const struct qcom_tlmm_gpio_mux *gmux,
    char *fnc_name)
{
	int i;

	for (i = 0; i < 16; i++) { /* XXX size */
		if ((gmux->functions[i] != NULL) &&
		    (strcmp(fnc_name, gmux->functions[i]) == 0))
			return  (i);
	}

	return (-1);
}

static int
qcom_tlmm_pinctrl_read_node(struct qcom_tlmm_softc *sc,
     phandle_t node, struct qcom_tlmm_pinctrl_cfg *cfg, char **pins,
     int *lpins)
{
	int rv, i;

	*lpins = OF_getprop_alloc(node, "pins", (void **)pins);
	if (*lpins <= 0)
		return (ENOENT);

	/* Read function (mux) settings. */
	rv = OF_getprop_alloc(node, "function", (void **)&cfg->function);
	if (rv <= 0)
		cfg->function = NULL;

	/*
	 * Read the rest of the properties.
	 *
	 * Properties that are a flag are simply present with a value of 0.
	 * Properties that have arguments have have_value set to 1, and
	 * we will parse an argument out for it to use.
	 *
	 * Properties that were not found/parsed with have a value of -1
	 * and thus we won't program them into the hardware.
	 */
	for (i = 0; i < PROP_ID_MAX_ID; i++) {
		rv = OF_getencprop(node, prop_names[i].name, &cfg->params[i],
		    sizeof(cfg->params[i]));
		if (prop_names[i].have_value) {
			if (rv == 0) {
				device_printf(sc->dev,
				    "WARNING: Missing value for propety"
				    " \"%s\"\n",
				    prop_names[i].name);
				cfg->params[i] = 0;
			}
		} else {
			/* No value, default to 0 */
			cfg->params[i] = 0;
		}
		if (rv < 0)
			cfg->params[i] = -1;
	}
	return (0);
}

static int
qcom_tlmm_pinctrl_config_gmux(struct qcom_tlmm_softc *sc, char *pin_name,
    const struct qcom_tlmm_gpio_mux *gmux, struct qcom_tlmm_pinctrl_cfg *cfg)
{
	int err = 0, i;

	QCOM_TLMM_DPRINTF(sc, QCOM_TLMM_DEBUG_PINMUX,
	    "%s: called; pin=%s, function %s\n",
	    __func__, pin_name, cfg->function);

	GPIO_LOCK(sc);

	/*
	 * Lookup the function in the configuration table.  Configure it
	 * if required.
	 */
	if (cfg->function != NULL) {
		uint32_t tmp;

		tmp = qcom_tlmm_pinctrl_gmux_function(gmux, cfg->function);
		if (tmp == -1) {
			device_printf(sc->dev,
			    "%s: pin=%s, function=%s, unknown!\n",
			    __func__,
			    pin_name,
			    cfg->function);
			err = EINVAL;
			goto done;
		}

		/*
		 * Program in the given function to the given pin.
		 */
		QCOM_TLMM_DPRINTF(sc, QCOM_TLMM_DEBUG_PINMUX,
		    "%s: pin id=%u, new function=%u\n",
		    __func__,
		    gmux->id,
		    tmp);
		err = qcom_tlmm_ipq4018_hw_pin_set_function(sc, gmux->id,
		    tmp);
		if (err != 0) {
			device_printf(sc->dev,
			    "%s: pin=%d: failed to set function (%d)\n",
			    __func__, gmux->id, err);
			goto done;
		}
	}

	/*
	 * Iterate the set of properties; call the relevant method
	 * if we need to change it.
	 */
	for (i = 0; i < PROP_ID_MAX_ID; i++) {
		if (cfg->params[i] == -1)
			continue;
		QCOM_TLMM_DPRINTF(sc, QCOM_TLMM_DEBUG_PINMUX,
		    "%s: pin_id=%u, param=%d, val=%d\n",
		    __func__,
		    gmux->id,
		    i,
		    cfg->params[i]);
		switch (i) {
		case PIN_ID_BIAS_DISABLE:
			err = qcom_tlmm_ipq4018_hw_pin_set_pupd_config(sc,
			    gmux->id, QCOM_TLMM_PIN_PUPD_CONFIG_DISABLE);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set pupd(DISABLE):"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;
		case PIN_ID_BIAS_PULL_DOWN:
			err = qcom_tlmm_ipq4018_hw_pin_set_pupd_config(sc,
			    gmux->id, QCOM_TLMM_PIN_PUPD_CONFIG_PULL_DOWN);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set pupd(PD):"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;
		case PIN_ID_BIAS_BUS_HOLD:
			err = qcom_tlmm_ipq4018_hw_pin_set_pupd_config(sc,
			    gmux->id, QCOM_TLMM_PIN_PUPD_CONFIG_BUS_HOLD);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set pupd(HOLD):"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;

		case PIN_ID_BIAS_PULL_UP:
			err = qcom_tlmm_ipq4018_hw_pin_set_pupd_config(sc,
			    gmux->id, QCOM_TLMM_PIN_PUPD_CONFIG_PULL_UP);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set pupd(PU):"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;
		case PIN_ID_OUTPUT_LOW:
			err = qcom_tlmm_ipq4018_hw_pin_set_oe_output(sc,
			    gmux->id);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set OE:"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			err = qcom_tlmm_ipq4018_hw_pin_set_output_value(
			    sc, gmux->id, 0);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set output value:"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;
		case PIN_ID_OUTPUT_HIGH:
			err = qcom_tlmm_ipq4018_hw_pin_set_oe_output(sc,
			    gmux->id);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set OE:"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			err = qcom_tlmm_ipq4018_hw_pin_set_output_value(
			    sc, gmux->id, 1);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set output value:"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;
		case PIN_ID_DRIVE_STRENGTH:
			err = qcom_tlmm_ipq4018_hw_pin_set_drive_strength(sc,
			    gmux->id, cfg->params[i]);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set drive"
				    " strength %d (%d)\n",
				    __func__, gmux->id,
				    cfg->params[i], err);
				goto done;
			}
			break;
			case PIN_ID_VM_ENABLE:
			err = qcom_tlmm_ipq4018_hw_pin_set_vm(sc,
			    gmux->id, true);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set VM enable:"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;
		case PIN_ID_VM_DISABLE:
			err = qcom_tlmm_ipq4018_hw_pin_set_vm(sc,
			    gmux->id, false);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set VM disable:"
				    " %d\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;
		case PIN_ID_DRIVE_OPEN_DRAIN:
			err = qcom_tlmm_ipq4018_hw_pin_set_open_drain(sc,
			    gmux->id, true);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set open drain"
				    " (%d)\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;
		case PIN_ID_INPUT_ENABLE:
			/* Configure pin as an input */
			err = qcom_tlmm_ipq4018_hw_pin_set_oe_input(sc,
			    gmux->id);
			if (err != 0) {
				device_printf(sc->dev,
				    "%s: pin=%d: failed to set pin as input"
				    " (%d)\n",
				    __func__, gmux->id, err);
				goto done;
			}
			break;
		case PIN_ID_INPUT_DISABLE:
			/*
			 * the linux-msm GPIO driver treats this as an error;
			 * a pin should be configured as an output instead.
			 */
			err = ENXIO;
			goto done;
			break;
		case PIN_ID_BIAS_HIGH_IMPEDANCE:
		case PIN_ID_INPUT_SCHMITT_ENABLE:
		case PIN_ID_INPUT_SCHMITT_DISABLE:
		case PIN_ID_INPUT_DEBOUNCE:
		case PIN_ID_SLEW_RATE:
		case PIN_ID_LOW_POWER_MODE_ENABLE:
		case PIN_ID_LOW_POWER_MODE_DISABLE:
		case PIN_ID_BIAS_PULL_PIN_DEFAULT:
		case PIN_ID_DRIVE_PUSH_PULL:
		case PIN_ID_DRIVE_OPEN_SOURCE:
		case PIN_ID_POWER_SOURCE:
		default:
			device_printf(sc->dev,
			    "%s: ERROR: unknown/unsupported param: "
			    " pin_id=%u, param=%d, val=%d\n",
			    __func__,
			    gmux->id,
			    i,
			    cfg->params[i]);
			err = ENXIO;
			goto done;

		}
	}
done:
	GPIO_UNLOCK(sc);
	return (0);
}


static int
qcom_tlmm_pinctrl_config_node(struct qcom_tlmm_softc *sc,
    char *pin_name, struct qcom_tlmm_pinctrl_cfg *cfg)
{
	const struct qcom_tlmm_gpio_mux *gmux;
	const struct qcom_tlmm_spec_pin *spin;
	int rv;

	/* Handle GPIO pins */
	gmux = qcom_tlmm_pinctrl_search_gmux(sc, pin_name);

	if (gmux != NULL) {
		rv = qcom_tlmm_pinctrl_config_gmux(sc, pin_name, gmux, cfg);
		return (rv);
	}
	/* Handle special pin groups */
	spin = qcom_tlmm_pinctrl_search_spin(sc, pin_name);
	if (spin != NULL) {
		rv = qcom_tlmm_pinctrl_config_spin(sc, pin_name, spin, cfg);
		return (rv);
	}
	device_printf(sc->dev, "Unknown pin: %s\n", pin_name);
	return (ENXIO);
}

static int
qcom_tlmm_pinctrl_process_node(struct qcom_tlmm_softc *sc,
     phandle_t node)
{
	struct qcom_tlmm_pinctrl_cfg cfg;
	char *pins, *pname;
	int i, len, lpins, rv;

	/*
	 * Read the configuration and list of pins for the given node to
	 * configure.
	 */
	rv = qcom_tlmm_pinctrl_read_node(sc, node, &cfg, &pins, &lpins);
	if (rv != 0)
		return (rv);

	len = 0;
	pname = pins;
	do {
		i = strlen(pname) + 1;
		/*
		 * Configure the given node with the specific configuration.
		 */
		rv = qcom_tlmm_pinctrl_config_node(sc, pname, &cfg);
		if (rv != 0)
			device_printf(sc->dev,
			    "Cannot configure pin: %s: %d\n", pname, rv);

		len += i;
		pname += i;
	} while (len < lpins);

	if (pins != NULL)
		free(pins, M_OFWPROP);
	if (cfg.function != NULL)
		free(cfg.function, M_OFWPROP);

	return (rv);
}

int
qcom_tlmm_pinctrl_configure(device_t dev, phandle_t cfgxref)
{
	struct qcom_tlmm_softc *sc;
	phandle_t node, cfgnode;
	int rv;

	sc = device_get_softc(dev);
	cfgnode = OF_node_from_xref(cfgxref);

	for (node = OF_child(cfgnode); node != 0; node = OF_peer(node)) {
		if (!ofw_bus_node_status_okay(node))
			continue;
		rv = qcom_tlmm_pinctrl_process_node(sc, node);
		if (rv != 0)
		 device_printf(dev, "Pin config failed: %d\n", rv);
	}

	return (0);
}

