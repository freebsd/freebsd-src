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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include "qcom_tlmm_pin.h"

#include "qcom_tlmm_ipq4018_reg.h"
#include "qcom_tlmm_ipq4018_hw.h"

#include "gpio_if.h"

static struct gpio_pin *
qcom_tlmm_pin_lookup(struct qcom_tlmm_softc *sc, int pin)
{
	if (pin >= sc->gpio_npins)
		return (NULL);

	return &sc->gpio_pins[pin];
}

static void
qcom_tlmm_pin_configure(struct qcom_tlmm_softc *sc,
    struct gpio_pin *pin, unsigned int flags)
{

	GPIO_LOCK_ASSERT(sc);

	/*
	 * Manage input/output
	 */
	if (flags & (GPIO_PIN_INPUT|GPIO_PIN_OUTPUT)) {
		pin->gp_flags &= ~(GPIO_PIN_INPUT|GPIO_PIN_OUTPUT);
		if (flags & GPIO_PIN_OUTPUT) {
			/*
			 * XXX TODO: read GPIO_PIN_PRESET_LOW /
			 * GPIO_PIN_PRESET_HIGH and if we're a GPIO
			 * function pin here, set the output
			 * pin value before we flip on oe_output.
			 */
			pin->gp_flags |= GPIO_PIN_OUTPUT;
			qcom_tlmm_ipq4018_hw_pin_set_oe_output(sc,
			    pin->gp_pin);
		} else {
			pin->gp_flags |= GPIO_PIN_INPUT;
			qcom_tlmm_ipq4018_hw_pin_set_oe_input(sc,
			    pin->gp_pin);
		}
	}

	/*
	 * Set pull-up / pull-down configuration
	 */
	if (flags & GPIO_PIN_PULLUP) {
		pin->gp_flags |= GPIO_PIN_PULLUP;
		qcom_tlmm_ipq4018_hw_pin_set_pupd_config(sc, pin->gp_pin,
		    QCOM_TLMM_PIN_PUPD_CONFIG_PULL_UP);
	} else if (flags & GPIO_PIN_PULLDOWN) {
		pin->gp_flags |= GPIO_PIN_PULLDOWN;
		qcom_tlmm_ipq4018_hw_pin_set_pupd_config(sc, pin->gp_pin,
		    QCOM_TLMM_PIN_PUPD_CONFIG_PULL_DOWN);
	} else if ((flags & (GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN)) ==
	    (GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN)) {
		pin->gp_flags |= GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN;
		qcom_tlmm_ipq4018_hw_pin_set_pupd_config(sc, pin->gp_pin,
		    QCOM_TLMM_PIN_PUPD_CONFIG_BUS_HOLD);
	} else {
		pin->gp_flags &= ~(GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN);
		qcom_tlmm_ipq4018_hw_pin_set_pupd_config(sc, pin->gp_pin,
		    QCOM_TLMM_PIN_PUPD_CONFIG_DISABLE);
	}
}

device_t
qcom_tlmm_get_bus(device_t dev)
{
	struct qcom_tlmm_softc *sc;

	sc = device_get_softc(dev);

	return (sc->busdev);
}

int
qcom_tlmm_pin_max(device_t dev, int *maxpin)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);

	*maxpin = sc->gpio_npins - 1;
	return (0);
}

int
qcom_tlmm_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	struct gpio_pin *p;

	p = qcom_tlmm_pin_lookup(sc, pin);
	if (p == NULL)
		return (EINVAL);

	GPIO_LOCK(sc);
	*caps = p->gp_caps;
	GPIO_UNLOCK(sc);

	return (0);
}

int
qcom_tlmm_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	uint32_t ret = 0, val;
	bool is_output;
	qcom_tlmm_pin_pupd_config_t pupd_config;

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	*flags = 0;

	GPIO_LOCK(sc);

	/* Lookup function - see what it is, whether we're a GPIO line */
	ret = qcom_tlmm_ipq4018_hw_pin_get_function(sc, pin, &val);
	if (ret != 0)
		goto done;

	/* Lookup input/output state */
	ret = qcom_tlmm_ipq4018_hw_pin_get_oe_state(sc, pin, &is_output);
	if (ret != 0)
		goto done;
	if (is_output)
		*flags |= GPIO_PIN_OUTPUT;
	else
		*flags |= GPIO_PIN_INPUT;

	/* Lookup pull-up / pull-down state */
	ret = qcom_tlmm_ipq4018_hw_pin_get_pupd_config(sc, pin,
	    &pupd_config);
	if (ret != 0)
		goto done;

	switch (pupd_config) {
	case QCOM_TLMM_PIN_PUPD_CONFIG_DISABLE:
		break;
	case QCOM_TLMM_PIN_PUPD_CONFIG_PULL_DOWN:
		*flags |= GPIO_PIN_PULLDOWN;
		break;
	case QCOM_TLMM_PIN_PUPD_CONFIG_PULL_UP:
		*flags |= GPIO_PIN_PULLUP;
		break;
	case QCOM_TLMM_PIN_PUPD_CONFIG_BUS_HOLD:
		*flags |= (GPIO_PIN_PULLUP | GPIO_PIN_PULLDOWN);
		break;
	}

done:
	GPIO_UNLOCK(sc);
	return (ret);
}

int
qcom_tlmm_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	struct gpio_pin *p;

	p = qcom_tlmm_pin_lookup(sc, pin);
	if (p == NULL)
		return (EINVAL);

	GPIO_LOCK(sc);
	memcpy(name, p->gp_name, GPIOMAXNAME);
	GPIO_UNLOCK(sc);

	return (0);
}

int
qcom_tlmm_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	struct gpio_pin *p;

	p = qcom_tlmm_pin_lookup(sc, pin);
	if (p == NULL)
		return (EINVAL);

	GPIO_LOCK(sc);
	qcom_tlmm_pin_configure(sc, p, flags);
	GPIO_UNLOCK(sc);

	return (0);
}

int
qcom_tlmm_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	int ret;

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	ret = qcom_tlmm_ipq4018_hw_pin_set_output_value(sc, pin, value);
	GPIO_UNLOCK(sc);

	return (ret);
}

int
qcom_tlmm_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	int ret;

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	ret = qcom_tlmm_ipq4018_hw_pin_get_input_value(sc, pin, val);
	GPIO_UNLOCK(sc);

	return (ret);
}

int
qcom_tlmm_pin_toggle(device_t dev, uint32_t pin)
{
	struct qcom_tlmm_softc *sc = device_get_softc(dev);
	int ret;

	if (pin >= sc->gpio_npins)
		return (EINVAL);

	GPIO_LOCK(sc);
	ret = qcom_tlmm_ipq4018_hw_pin_toggle_output_value(sc, pin);
	GPIO_UNLOCK(sc);

	return (ret);
}

int
qcom_tlmm_filter(void *arg)
{

	/* TODO: something useful */
	return (FILTER_STRAY);
}

void
qcom_tlmm_intr(void *arg)
{
	struct qcom_tlmm_softc *sc = arg;
	GPIO_LOCK(sc);
	/* TODO: something useful */
	GPIO_UNLOCK(sc);
}

/*
 * ofw bus interface
 */
phandle_t
qcom_tlmm_pin_get_node(device_t dev, device_t bus)
{

	/* We only have one child, the GPIO bus, which needs our own node. */
	return (ofw_bus_get_node(dev));
}

