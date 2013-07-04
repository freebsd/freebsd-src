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
#include <sys/reboot.h>

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


#define	WIIGPIO_PINBANK(_p)	((_p) / (WIIGPIO_NPINS / 2))
#define	WIIGPIO_PINMASK(_p)	(1 << ((_p) % (WIIGPIO_NPINS / 2)))
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
static void	wiigpio_shutdown(void *, int);

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
wiigpio_read(struct wiigpio_softc *sc, int n)
{

	return (bus_space_read_4(sc->sc_bt, sc->sc_bh, n * 0x20));
}

static __inline void
wiigpio_write(struct wiigpio_softc *sc, int n, uint32_t reg)
{

	bus_space_write_4(sc->sc_bt, sc->sc_bh, n * 0x20, reg);
}

static __inline uint32_t
wiigpio_dir_read(struct wiigpio_softc *sc, int n)
{

	return (bus_space_read_4(sc->sc_bt, sc->sc_bh, n * 0x20 + 4));
}

static __inline void
wiigpio_dir_write(struct wiigpio_softc *sc, int n, uint32_t reg)
{

	bus_space_write_4(sc->sc_bt, sc->sc_bh, n * 0x20 + 4, reg);
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
	uint32_t d;

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
#ifdef WIIGPIO_DEBUG
	device_printf(dev, "dir bank0=0x%08x bank1=0x%08x\n",
	    wiigpio_dir_read(sc, 0), wiigpio_dir_read(sc, 1));
	device_printf(dev, "val bank0=0x%08x bank1=0x%08x\n",
	    wiigpio_read(sc, 0), wiigpio_read(sc, 1));
#endif
	for (i = 0; i < WIIGPIO_NPINS; i++) {
		sc->sc_pins[i].gp_caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;
		sc->sc_pins[i].gp_pin = i;
		d = wiigpio_dir_read(sc, WIIGPIO_PINBANK(i));
		if (d & WIIGPIO_PINMASK(i))
			sc->sc_pins[i].gp_flags = GPIO_PIN_OUTPUT;
		else
			sc->sc_pins[i].gp_flags = GPIO_PIN_INPUT;
		snprintf(sc->sc_pins[i].gp_name, GPIOMAXNAME, "PIN %d", i);
#ifdef WIIGPIO_DEBUG
		device_printf(dev, "PIN %d state %d flag %s\n", i,
		    wiigpio_read(sc, WIIGPIO_PINBANK(i)) >> 
			(i % (WIIGPIO_NPINS / 2)) & 1,
		    sc->sc_pins[i].gp_flags == GPIO_PIN_INPUT ?
		    "GPIO_PIN_INPUT" : "GPIO_PIN_OUTPUT");
#endif
	}
	device_add_child(dev, "gpioc", device_get_unit(dev));
	device_add_child(dev, "gpiobus", device_get_unit(dev));
	/*
	 * We will be responsible for powering off the system.
	 */
	EVENTHANDLER_REGISTER(shutdown_final, wiigpio_shutdown, dev,
	    SHUTDOWN_PRI_LAST);

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
	reg = wiigpio_read(sc, WIIGPIO_PINBANK(pin));
	*val = !!(reg & WIIGPIO_PINMASK(pin));
	WIIGPIO_UNLOCK(sc);

	return (0);
}

static int
wiigpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct wiigpio_softc *sc;
	uint32_t reg, pinbank, pinmask;

	if (pin >= WIIGPIO_NPINS)
		return (EINVAL);
	sc = device_get_softc(dev);
	pinbank = WIIGPIO_PINBANK(pin);
	pinmask = WIIGPIO_PINMASK(pin);
	WIIGPIO_LOCK(sc);
	reg = wiigpio_read(sc, pinbank) & ~pinmask;
	if (value)
		reg |= pinmask;
	wiigpio_write(sc, pinbank, reg);
	WIIGPIO_UNLOCK(sc);

	return (0);
}

static int
wiigpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct wiigpio_softc *sc;
	uint32_t val, pinbank, pinmask;

	if (pin >= WIIGPIO_NPINS)
		return (EINVAL);
	sc = device_get_softc(dev);
	pinbank = WIIGPIO_PINBANK(pin);
	pinmask = WIIGPIO_PINMASK(pin);
	WIIGPIO_LOCK(sc);
	val = wiigpio_read(sc, pinbank);
	if (val & pinmask)
		wiigpio_write(sc, pinbank, val & ~pinmask);
	else
		wiigpio_write(sc, pinbank, val | pinmask);
	WIIGPIO_UNLOCK(sc);

	return (0);
}

static int
wiigpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct wiigpio_softc *sc;
	uint32_t reg, pinbank, pinmask;

	if (pin >= WIIGPIO_NPINS)
		return (EINVAL);
	if ((flags & ~(GPIO_PIN_OUTPUT|GPIO_PIN_INPUT)) != 0)
		return (EINVAL);
	if ((flags & (GPIO_PIN_OUTPUT|GPIO_PIN_INPUT)) == 
	    (GPIO_PIN_OUTPUT|GPIO_PIN_INPUT))
		return (EINVAL);
	sc = device_get_softc(dev);
	pinbank = WIIGPIO_PINBANK(pin);
	pinmask = WIIGPIO_PINMASK(pin);
	WIIGPIO_LOCK(sc);
	reg = wiigpio_dir_read(sc, WIIGPIO_PINBANK(pin));
	if (flags & GPIO_PIN_OUTPUT)
		wiigpio_dir_write(sc, pinbank, reg | pinmask);
	else
		wiigpio_dir_write(sc, pinbank, reg & ~pinmask);
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

static void
wiigpio_shutdown(void *xdev, int howto)
{
	device_t dev;

	if (!(howto & RB_POWEROFF))
		return;
	dev = (device_t)xdev;
	wiigpio_pin_setflags(dev, WIIGPIO_POWEROFF_PIN, GPIO_PIN_OUTPUT);
	wiigpio_pin_set(dev, WIIGPIO_POWEROFF_PIN, 1);
}
