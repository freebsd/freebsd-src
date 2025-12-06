/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009 Oleksandr Tymoshenko <gonzo@freebsd.org>
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
 */

#include <sys/cdefs.h>
#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/led/led.h>

#include "gpiobus_if.h"

/*
 * Only one pin for led
 */
#define	GPIOLED_PIN	0

#define	GPIOLED_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	GPIOLED_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	GPIOLED_LOCK_INIT(_sc)		mtx_init(&(_sc)->sc_mtx,	\
    device_get_nameunit((_sc)->sc_dev), "gpioled", MTX_DEF)
#define	GPIOLED_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx)

struct gpioled_softc
{
	device_t	sc_dev;
	device_t	sc_busdev;
	struct mtx	sc_mtx;
	struct cdev	*sc_leddev;
	int	sc_softinvert;
};

static void gpioled_control(void *, int);
static int gpioled_probe(device_t);
static int gpioled_attach(device_t);
static int gpioled_detach(device_t);

static void
gpioled_control(void *priv, int onoff)
{
	struct gpioled_softc *sc;

	sc = (struct gpioled_softc *)priv;
	if (sc->sc_softinvert)
		onoff = !onoff;
	GPIOLED_LOCK(sc);
	GPIOBUS_PIN_SET(sc->sc_busdev, sc->sc_dev, GPIOLED_PIN,
	    onoff ? GPIO_PIN_HIGH : GPIO_PIN_LOW);
	GPIOLED_UNLOCK(sc);
}

static int
gpioled_probe(device_t dev)
{
	device_set_desc(dev, "GPIO led");

	return (BUS_PROBE_DEFAULT);
}

static int
gpioled_inv(device_t dev, uint32_t *pin_flags)
{
	struct gpioled_softc *sc;
	int invert;
	uint32_t pin_caps;

	sc = device_get_softc(dev);

	if (resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "invert", &invert))
		invert = 0;

	if (GPIOBUS_PIN_GETCAPS(sc->sc_busdev, sc->sc_dev, GPIOLED_PIN,
	    &pin_caps) != 0) {
		if (bootverbose)
			device_printf(sc->sc_dev, "unable to get pin caps\n");
		return (-1);
	}
	if (pin_caps & GPIO_PIN_INVOUT)
		*pin_flags &= ~GPIO_PIN_INVOUT;
	sc->sc_softinvert = 0;
	if (invert) {
		const char *invmode;

		if (resource_string_value(device_get_name(dev),
		    device_get_unit(dev), "invmode", &invmode))
			invmode = NULL;

		if (invmode) {
			if (!strcmp(invmode, "sw"))
				sc->sc_softinvert = 1;
			else if (!strcmp(invmode, "hw")) {
				if (pin_caps & GPIO_PIN_INVOUT)
					*pin_flags |= GPIO_PIN_INVOUT;
				else {
					device_printf(sc->sc_dev, "hardware pin inversion not supported\n");
					return (-1);
				}
			} else {
				if (strcmp(invmode, "auto") != 0)
					device_printf(sc->sc_dev, "invalid pin inversion mode\n");
				invmode = NULL;
			}
		}
		/*
		 * auto inversion mode: use hardware support if available, else fallback to
		 * software emulation.
		 */
		if (invmode == NULL) {
			if (pin_caps & GPIO_PIN_INVOUT)
				*pin_flags |= GPIO_PIN_INVOUT;
			else
				sc->sc_softinvert = 1;
		}
	}
	MPASS(!invert ||
	    (((*pin_flags & GPIO_PIN_INVOUT) != 0) && !sc->sc_softinvert) ||
	    (((*pin_flags & GPIO_PIN_INVOUT) == 0) && sc->sc_softinvert));
	return (invert);
}

static int
gpioled_attach(device_t dev)
{
	struct gpioled_softc *sc;
	int state;
	const char *name;
	uint32_t pin_flags;
	int invert;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_busdev = device_get_parent(dev);
	GPIOLED_LOCK_INIT(sc);

	if (resource_string_value(device_get_name(dev),
	    device_get_unit(dev), "name", &name))
		name = NULL;

	if (resource_int_value(device_get_name(dev),
	    device_get_unit(dev), "state", &state))
		state = 0;

	pin_flags = GPIO_PIN_OUTPUT;
	invert = gpioled_inv(dev, &pin_flags);
	if (invert < 0)
		return (ENXIO);
	device_printf(sc->sc_dev, "state %d invert %s\n",
	    state, (invert ? (sc->sc_softinvert ? "sw" : "hw") : "no"));
	if (GPIOBUS_PIN_SETFLAGS(sc->sc_busdev, sc->sc_dev, GPIOLED_PIN,
	    pin_flags) != 0) {
		if (bootverbose)
			device_printf(sc->sc_dev, "unable to set pin flags, %#x\n", pin_flags);
		return (ENXIO);
	}

	sc->sc_leddev = led_create_state(gpioled_control, sc, name ? name :
	    device_get_nameunit(dev), state);

	return (0);
}

static int
gpioled_detach(device_t dev)
{
	struct gpioled_softc *sc;

	sc = device_get_softc(dev);
	if (sc->sc_leddev) {
		led_destroy(sc->sc_leddev);
		sc->sc_leddev = NULL;
	}
	GPIOLED_LOCK_DESTROY(sc);
	return (0);
}

static device_method_t gpioled_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gpioled_probe),
	DEVMETHOD(device_attach,	gpioled_attach),
	DEVMETHOD(device_detach,	gpioled_detach),

	DEVMETHOD_END
};

static driver_t gpioled_driver = {
	"gpioled",
	gpioled_methods,
	sizeof(struct gpioled_softc),
};

DRIVER_MODULE(gpioled, gpiobus, gpioled_driver, 0, 0);
MODULE_DEPEND(gpioled, gpiobus, 1, 1, 1);
MODULE_VERSION(gpioled, 1);
