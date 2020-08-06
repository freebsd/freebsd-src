/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2012 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * Copyright (c) 2012-2015 Luiz Otavio O Souza <loos@FreeBSD.org>
 * All rights reserved.
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
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/sx.h>
#include <sys/proc.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/ofw/ofw_bus.h>

#include <arm/broadcom/bcm2835/bcm2835_firmware.h>

#include "gpio_if.h"

#define	RPI_FW_GPIO_PINS		8
#define	RPI_FW_GPIO_BASE		128
#define	RPI_FW_GPIO_DEFAULT_CAPS	(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT)

struct rpi_fw_gpio_softc {
	device_t		sc_busdev;
	device_t		sc_firmware;
	struct sx		sc_sx;
	struct gpio_pin		sc_gpio_pins[RPI_FW_GPIO_PINS];
	uint8_t			sc_gpio_state;
};

#define	RPI_FW_GPIO_LOCK(_sc)	sx_xlock(&(_sc)->sc_sx)
#define	RPI_FW_GPIO_UNLOCK(_sc)	sx_xunlock(&(_sc)->sc_sx)

static struct ofw_compat_data compat_data[] = {
	{"raspberrypi,firmware-gpio",	1},
	{NULL,				0}
};

static int
rpi_fw_gpio_pin_configure(struct rpi_fw_gpio_softc *sc, struct gpio_pin *pin,
    unsigned int flags)
{
	union msg_get_gpio_config old_cfg;
	union msg_set_gpio_config new_cfg;
	int rv;

	bzero(&old_cfg, sizeof(old_cfg));
	bzero(&new_cfg, sizeof(new_cfg));
	old_cfg.req.gpio = RPI_FW_GPIO_BASE + pin->gp_pin;

	RPI_FW_GPIO_LOCK(sc);
	rv = bcm2835_firmware_property(sc->sc_firmware,
	    BCM2835_FIRMWARE_TAG_GET_GPIO_CONFIG, &old_cfg, sizeof(old_cfg));
	if (rv == 0 && old_cfg.resp.gpio != 0)
		rv = EIO;
	if (rv != 0)
		goto fail;

	new_cfg.req.gpio = RPI_FW_GPIO_BASE + pin->gp_pin;
	if (flags & GPIO_PIN_INPUT) {
		new_cfg.req.dir = BCM2835_FIRMWARE_GPIO_IN;
		new_cfg.req.state = 0;
		pin->gp_flags = GPIO_PIN_INPUT;
	} else if (flags & GPIO_PIN_OUTPUT) {
		new_cfg.req.dir = BCM2835_FIRMWARE_GPIO_OUT;
		if (flags & (GPIO_PIN_PRESET_HIGH | GPIO_PIN_PRESET_LOW)) {
			if (flags & GPIO_PIN_PRESET_HIGH) {
				new_cfg.req.state = 1;
				sc->sc_gpio_state |= (1 << pin->gp_pin);
			} else {
				new_cfg.req.state = 0;
				sc->sc_gpio_state &= ~(1 << pin->gp_pin);
			}
		} else {
			if ((sc->sc_gpio_state & (1 << pin->gp_pin)) != 0) {
				new_cfg.req.state = 1;
			} else {
				new_cfg.req.state = 0;
			}
		}
		pin->gp_flags = GPIO_PIN_OUTPUT;
	} else {
		new_cfg.req.dir = old_cfg.resp.dir;
		/* Use the old state to decide high/low */
		if ((sc->sc_gpio_state & (1 << pin->gp_pin)) != 0)
			new_cfg.req.state = 1;
		else
			new_cfg.req.state = 0;
	}
	new_cfg.req.pol = old_cfg.resp.pol;
	new_cfg.req.term_en = 0;
	new_cfg.req.term_pull_up = 0;

	rv = bcm2835_firmware_property(sc->sc_firmware,
	    BCM2835_FIRMWARE_TAG_SET_GPIO_CONFIG, &new_cfg, sizeof(new_cfg));

fail:
	RPI_FW_GPIO_UNLOCK(sc);

	return (rv);
}

static device_t
rpi_fw_gpio_get_bus(device_t dev)
{
	struct rpi_fw_gpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->sc_busdev);
}

static int
rpi_fw_gpio_pin_max(device_t dev, int *maxpin)
{

	*maxpin = RPI_FW_GPIO_PINS - 1;
	return (0);
}

static int
rpi_fw_gpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct rpi_fw_gpio_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < RPI_FW_GPIO_PINS; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= RPI_FW_GPIO_PINS)
		return (EINVAL);

	*caps = RPI_FW_GPIO_DEFAULT_CAPS;
	return (0);
}

static int
rpi_fw_gpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct rpi_fw_gpio_softc *sc = device_get_softc(dev);
	int i;

	for (i = 0; i < RPI_FW_GPIO_PINS; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= RPI_FW_GPIO_PINS)
		return (EINVAL);

	RPI_FW_GPIO_LOCK(sc);
	*flags = sc->sc_gpio_pins[i].gp_flags;
	RPI_FW_GPIO_UNLOCK(sc);

	return (0);
}

static int
rpi_fw_gpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct rpi_fw_gpio_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < RPI_FW_GPIO_PINS; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= RPI_FW_GPIO_PINS)
		return (EINVAL);

	RPI_FW_GPIO_LOCK(sc);
	memcpy(name, sc->sc_gpio_pins[i].gp_name, GPIOMAXNAME);
	RPI_FW_GPIO_UNLOCK(sc);

	return (0);
}

static int
rpi_fw_gpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct rpi_fw_gpio_softc *sc;
	int i;

	sc = device_get_softc(dev);
	for (i = 0; i < RPI_FW_GPIO_PINS; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}

	if (i >= RPI_FW_GPIO_PINS)
		return (EINVAL);

	return (rpi_fw_gpio_pin_configure(sc, &sc->sc_gpio_pins[i], flags));
}

static int
rpi_fw_gpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct rpi_fw_gpio_softc *sc;
	union msg_set_gpio_state state;
	int i, rv;

	sc = device_get_softc(dev);
	for (i = 0; i < RPI_FW_GPIO_PINS; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}
	if (i >= RPI_FW_GPIO_PINS)
		return (EINVAL);

	state.req.gpio = RPI_FW_GPIO_BASE + pin;
	state.req.state = value;

	RPI_FW_GPIO_LOCK(sc);
	rv = bcm2835_firmware_property(sc->sc_firmware,
	    BCM2835_FIRMWARE_TAG_SET_GPIO_STATE, &state, sizeof(state));
	/* The firmware sets gpio to 0 on success */
	if (rv == 0 && state.resp.gpio != 0)
		rv = EINVAL;
	if (rv == 0) {
		sc->sc_gpio_pins[i].gp_flags &= ~(GPIO_PIN_PRESET_HIGH |
		    GPIO_PIN_PRESET_LOW);
		if (value)
			sc->sc_gpio_state |= (1 << i);
		else
			sc->sc_gpio_state &= ~(1 << i);
	}
	RPI_FW_GPIO_UNLOCK(sc);

	return (rv);
}

static int
rpi_fw_gpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct rpi_fw_gpio_softc *sc;
	union msg_get_gpio_state state;
	int i, rv;

	sc = device_get_softc(dev);
	for (i = 0; i < RPI_FW_GPIO_PINS; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}
	if (i >= RPI_FW_GPIO_PINS)
		return (EINVAL);

	bzero(&state, sizeof(state));
	state.req.gpio = RPI_FW_GPIO_BASE + pin;

	RPI_FW_GPIO_LOCK(sc);
	rv = bcm2835_firmware_property(sc->sc_firmware,
	    BCM2835_FIRMWARE_TAG_GET_GPIO_STATE, &state, sizeof(state));
	RPI_FW_GPIO_UNLOCK(sc);

	/* The firmware sets gpio to 0 on success */
	if (rv == 0 && state.resp.gpio != 0)
		rv = EINVAL;
	if (rv == 0)
		*val = !state.resp.state;

	return (rv);
}

static int
rpi_fw_gpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct rpi_fw_gpio_softc *sc;
	union msg_get_gpio_state old_state;
	union msg_set_gpio_state new_state;
	int i, rv;

	sc = device_get_softc(dev);
	for (i = 0; i < RPI_FW_GPIO_PINS; i++) {
		if (sc->sc_gpio_pins[i].gp_pin == pin)
			break;
	}
	if (i >= RPI_FW_GPIO_PINS)
		return (EINVAL);

	bzero(&old_state, sizeof(old_state));
	bzero(&new_state, sizeof(new_state));

	old_state.req.gpio = RPI_FW_GPIO_BASE + pin;
	new_state.req.gpio = RPI_FW_GPIO_BASE + pin;

	RPI_FW_GPIO_LOCK(sc);
	rv = bcm2835_firmware_property(sc->sc_firmware,
	    BCM2835_FIRMWARE_TAG_GET_GPIO_STATE, &old_state, sizeof(old_state));
	/* The firmware sets gpio to 0 on success */
	if (rv == 0 && old_state.resp.gpio == 0) {
		/* Set the new state to invert the GPIO */
		new_state.req.state = !old_state.resp.state;
		rv = bcm2835_firmware_property(sc->sc_firmware,
		    BCM2835_FIRMWARE_TAG_SET_GPIO_STATE, &new_state,
		    sizeof(new_state));
	}
	if (rv == 0 && (old_state.resp.gpio != 0 || new_state.resp.gpio != 0))
		rv = EINVAL;
	RPI_FW_GPIO_UNLOCK(sc);

	return (rv);
}

static int
rpi_fw_gpio_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Raspberry Pi Firmware GPIO controller");
	return (BUS_PROBE_DEFAULT);
}

static int
rpi_fw_gpio_attach(device_t dev)
{
	union msg_get_gpio_config cfg;
	struct rpi_fw_gpio_softc *sc;
	char *names;
	phandle_t gpio;
	int i, nelems, elm_pos, rv;

	sc = device_get_softc(dev);
	sc->sc_firmware = device_get_parent(dev);
	sx_init(&sc->sc_sx, "Raspberry Pi firmware gpio");
	/* Find our node. */
	gpio = ofw_bus_get_node(dev);
	if (!OF_hasprop(gpio, "gpio-controller"))
		/* This is not a GPIO controller. */
		goto fail;

	nelems = OF_getprop_alloc(gpio, "gpio-line-names", (void **)&names);
	if (nelems <= 0)
		names = NULL;
	elm_pos = 0;
	for (i = 0; i < RPI_FW_GPIO_PINS; i++) {
		/* Set the current pin name */
		if (names != NULL && elm_pos < nelems &&
		    names[elm_pos] != '\0') {
			snprintf(sc->sc_gpio_pins[i].gp_name, GPIOMAXNAME,
			    "%s", names + elm_pos);
			/* Find the next pin name */
			elm_pos += strlen(names + elm_pos) + 1;
		} else {
			snprintf(sc->sc_gpio_pins[i].gp_name, GPIOMAXNAME,
			    "pin %d", i);
		}

		sc->sc_gpio_pins[i].gp_pin = i;
		sc->sc_gpio_pins[i].gp_caps = RPI_FW_GPIO_DEFAULT_CAPS;

		bzero(&cfg, sizeof(cfg));
		cfg.req.gpio = RPI_FW_GPIO_BASE + i;
		rv = bcm2835_firmware_property(sc->sc_firmware,
		    BCM2835_FIRMWARE_TAG_GET_GPIO_CONFIG, &cfg, sizeof(cfg));
		if (rv == 0 && cfg.resp.gpio == 0) {
			if (cfg.resp.dir == BCM2835_FIRMWARE_GPIO_IN)
				sc->sc_gpio_pins[i].gp_flags = GPIO_PIN_INPUT;
			else
				sc->sc_gpio_pins[i].gp_flags = GPIO_PIN_OUTPUT;
		} else {
			sc->sc_gpio_pins[i].gp_flags = GPIO_PIN_INPUT;
		}
	}
	free(names, M_OFWPROP);
	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL)
		goto fail;

	return (0);

fail:
	sx_destroy(&sc->sc_sx);

	return (ENXIO);
}

static int
rpi_fw_gpio_detach(device_t dev)
{

	return (EBUSY);
}

static device_method_t rpi_fw_gpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rpi_fw_gpio_probe),
	DEVMETHOD(device_attach,	rpi_fw_gpio_attach),
	DEVMETHOD(device_detach,	rpi_fw_gpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus,		rpi_fw_gpio_get_bus),
	DEVMETHOD(gpio_pin_max,		rpi_fw_gpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	rpi_fw_gpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	rpi_fw_gpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps,	rpi_fw_gpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags,	rpi_fw_gpio_pin_setflags),
	DEVMETHOD(gpio_pin_get,		rpi_fw_gpio_pin_get),
	DEVMETHOD(gpio_pin_set,		rpi_fw_gpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	rpi_fw_gpio_pin_toggle),

	DEVMETHOD_END
};

static devclass_t rpi_fw_gpio_devclass;

static driver_t rpi_fw_gpio_driver = {
	"gpio",
	rpi_fw_gpio_methods,
	sizeof(struct rpi_fw_gpio_softc),
};

EARLY_DRIVER_MODULE(rpi_fw_gpio, bcm2835_firmware, rpi_fw_gpio_driver,
    rpi_fw_gpio_devclass, 0, 0, BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
