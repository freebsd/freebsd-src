/*-
 * Copyright (C) 2012 Margarida Gouveia
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/rman.h>
#include <sys/gpio.h>

#include <machine/bus.h>
#include <machine/platform.h>
#include <machine/intr_machdep.h>
#include <machine/resource.h>

#include <powerpc/wii/wii_gpioreg.h>

#include "gpio_if.h"

struct wiigpio_softc {
	device_t		 sc_dev;
	struct resource		*sc_rres;
	bus_space_tag_t		 sc_bt;
	bus_space_handle_t	 sc_bh;
	int			 sc_rrid;
	struct mtx		 sc_mtx;
	struct gpio_pin		 sc_pins[WIIGPIO_NPINS];
};

#define WIIGPIO_LOCK(sc)	mtx_lock(&(sc)->sc_mtx)
#define WIIGPIO_UNLOCK(sc)	mtx_unlock(&(sc)->sc_mtx)

static int	wiigpio_probe(device_t);
static int	wiigpio_attach(device_t);
static int	wiigpio_detach(device_t);
static int	wiigpio_pin_max(device_t, int *);
static int	wiigpio_pin_getname(device_t, uint32_t, char *);
static int	wiigpio_pin_getflags(device_t, uint32_t, uint32_t *);
static int	wiigpio_pin_setflags(device_t, uint32_t, uint32_t);
static int	wiigpio_pin_getcaps(device_t, uint32_t, uint32_t *);
static int	wiigpio_pin_get(device_t, uint32_t, unsigned int *);
static int	wiigpio_pin_set(device_t, uint32_t, unsigned int);
static int	wiigpio_pin_toggle(device_t, uint32_t);

static device_method_t wiigpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		wiigpio_probe),
	DEVMETHOD(device_attach,	wiigpio_attach),
	DEVMETHOD(device_detach,	wiigpio_detach),

	/* GPIO protocol */
	DEVMETHOD(gpio_pin_max,		wiigpio_pin_max),
	DEVMETHOD(gpio_pin_getname,	wiigpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags,	wiigpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags,	wiigpio_pin_setflags),
	DEVMETHOD(gpio_pin_getcaps,	wiigpio_pin_getcaps),
	DEVMETHOD(gpio_pin_get,		wiigpio_pin_get),
	DEVMETHOD(gpio_pin_set,		wiigpio_pin_set),
	DEVMETHOD(gpio_pin_toggle,	wiigpio_pin_toggle),

        DEVMETHOD_END
};

static driver_t wiigpio_driver = {
	"wiigpio",
	wiigpio_methods,
	sizeof(struct wiigpio_softc)
};

static devclass_t wiigpio_devclass;

DRIVER_MODULE(wiigpio, wiibus, wiigpio_driver, wiigpio_devclass, 0, 0);

static __inline uint32_t
wiigpio_read(struct wiigpio_softc *sc)
{

	return (bus_space_read_4(sc->sc_bt, sc->sc_bh, 0));
}

static __inline void
wiigpio_write(struct wiigpio_softc *sc, uint32_t reg)
{

	bus_space_write_4(sc->sc_bt, sc->sc_bh, 0, reg);
}

static int
wiigpio_probe(device_t dev)
{
        device_set_desc(dev, "Nintendo Wii GPIO");

        return (BUS_PROBE_NOWILDCARD);
}

static int
wiigpio_attach(device_t dev)
{
	struct wiigpio_softc *sc;
	int i;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->sc_rrid, RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		device_printf(dev, "could not alloc mem resource\n");
		return (ENXIO);
	}
	sc->sc_bt = rman_get_bustag(sc->sc_rres);
	sc->sc_bh = rman_get_bushandle(sc->sc_rres);

	mtx_init(&sc->sc_mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	for (i = 0; i < WIIGPIO_NPINS; i++) {
		sc->sc_pins[i].gp_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;
		sc->sc_pins[i].gp_pin = i;
		sc->sc_pins[i].gp_flags = 0;
		snprintf(sc->sc_pins[i].gp_name, GPIOMAXNAME, "PIN %d", i);
	}

	device_add_child(dev, "gpioc", device_get_unit(dev));
	device_add_child(dev, "gpiobus", device_get_unit(dev));

	return (bus_generic_attach(dev));
}

static int
wiigpio_detach(device_t dev)
{
	struct wiigpio_softc *sc;

	sc = device_get_softc(dev);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rrid, sc->sc_rres);
	mtx_destroy(&sc->sc_mtx);

	return (0);
}

static int
wiigpio_pin_max(device_t dev, int *maxpin)
{
	
	*maxpin = WIIGPIO_NPINS - 1;

	return (0);
}

static int
wiigpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct wiigpio_softc *sc;

	if (pin >= WIIGPIO_NPINS)
		return (EINVAL);
	sc = device_get_softc(dev);
	*caps = sc->sc_pins[pin].gp_caps;

	return (0);
}

static int
wiigpio_pin_get(device_t dev, uint32_t pin, unsigned int *val)
{
	struct wiigpio_softc *sc;
	uint32_t reg;

	if (pin >= WIIGPIO_NPINS)
		return (EINVAL);
	sc = device_get_softc(dev);
	WIIGPIO_LOCK(sc);
	reg = wiigpio_read(sc);
	*val = !!(reg & (1 << pin));
	WIIGPIO_UNLOCK(sc);

	return (0);
}

static int
wiigpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct wiigpio_softc *sc;
	uint32_t reg, pinmask = 1 << pin;

	if (pin >= WIIGPIO_NPINS)
		return (EINVAL);
	sc = device_get_softc(dev);
	WIIGPIO_LOCK(sc);
	reg = wiigpio_read(sc) & ~pinmask;
	if (value)
		reg |= pinmask;
	wiigpio_write(sc, reg);
	WIIGPIO_UNLOCK(sc);

	return (0);
}

static int
wiigpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct wiigpio_softc *sc;
	uint32_t val, pinmask = 1 << pin;

	sc = device_get_softc(dev);
	WIIGPIO_LOCK(sc);
	val = wiigpio_read(sc) & pinmask;
	if (val)
		wiigpio_write(sc, wiigpio_read(sc) & ~pinmask);
	else
		wiigpio_write(sc, wiigpio_read(sc) | pinmask);
	WIIGPIO_UNLOCK(sc);

	return (0);
}

static int
wiigpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct wiigpio_softc *sc;

	if (pin >= WIIGPIO_NPINS)
		return (EINVAL);
	sc = device_get_softc(dev);
	WIIGPIO_LOCK(sc);
	sc->sc_pins[pin].gp_flags = flags;
	WIIGPIO_UNLOCK(sc);

	return (0);
}

static int
wiigpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct wiigpio_softc *sc;

	if (pin >= WIIGPIO_NPINS)
		return (EINVAL);
	sc = device_get_softc(dev);
	WIIGPIO_LOCK(sc);
	*flags = sc->sc_pins[pin].gp_flags;
	WIIGPIO_UNLOCK(sc);

	return (0);
}

static int
wiigpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct wiigpio_softc *sc;

	if (pin >= WIIGPIO_NPINS)
		return (EINVAL);
	sc = device_get_softc(dev);
	WIIGPIO_LOCK(sc);
	memcpy(name, sc->sc_pins[pin].gp_name, GPIOMAXNAME);
	WIIGPIO_UNLOCK(sc);

	return (0);
}
