/*-
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <sys/gpio.h>
#include <dev/gpio/gpiobusvar.h>
#include "gpio_if.h"
#include "gpiobus_if.h"

static void gpiobus_print_pins(struct gpiobus_ivar *);
static int gpiobus_parse_pins(struct gpiobus_softc *, device_t, int);
static int gpiobus_probe(device_t);
static int gpiobus_attach(device_t);
static int gpiobus_detach(device_t);
static int gpiobus_suspend(device_t);
static int gpiobus_resume(device_t);
static int gpiobus_print_child(device_t, device_t);
static int gpiobus_child_location_str(device_t, device_t, char *, size_t);
static int gpiobus_child_pnpinfo_str(device_t, device_t, char *, size_t);
static device_t gpiobus_add_child(device_t, u_int, const char *, int);
static void gpiobus_hinted_child(device_t, const char *, int);

/*
 * GPIOBUS interface
 */
static void gpiobus_lock_bus(device_t);
static void gpiobus_unlock_bus(device_t);
static void gpiobus_acquire_bus(device_t, device_t);
static void gpiobus_release_bus(device_t, device_t);
static int gpiobus_pin_setflags(device_t, device_t, uint32_t, uint32_t);
static int gpiobus_pin_getflags(device_t, device_t, uint32_t, uint32_t*);
static int gpiobus_pin_getcaps(device_t, device_t, uint32_t, uint32_t*);
static int gpiobus_pin_set(device_t, device_t, uint32_t, unsigned int);
static int gpiobus_pin_get(device_t, device_t, uint32_t, unsigned int*);
static int gpiobus_pin_toggle(device_t, device_t, uint32_t);

#define	GPIOBUS_LOCK(_sc) mtx_lock(&(_sc)->sc_mtx)
#define	GPIOBUS_UNLOCK(_sc) mtx_unlock(&(_sc)->sc_mtx)
#define	GPIOBUS_LOCK_INIT(_sc) \
	mtx_init(&_sc->sc_mtx, device_get_nameunit(_sc->sc_dev), \
	    "gpiobus", MTX_DEF)
#define	GPIOBUS_LOCK_DESTROY(_sc) mtx_destroy(&_sc->sc_mtx);
#define	GPIOBUS_ASSERT_LOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_OWNED);
#define	GPIOBUS_ASSERT_UNLOCKED(_sc) mtx_assert(&_sc->sc_mtx, MA_NOTOWNED);


static void
gpiobus_print_pins(struct gpiobus_ivar *devi)
{
	int range_start, range_stop, need_coma;
	int i;

	if (devi->npins == 0)
		return;

	need_coma = 0;
	range_start = range_stop = devi->pins[0];
	for (i = 1; i < devi->npins; i++) {
		if (devi->pins[i] != (range_stop + 1)) {
			if (need_coma)
				printf(",");
			if (range_start != range_stop)
				printf("%d-%d", range_start, range_stop);
			else
				printf("%d", range_start);

			range_start = range_stop = devi->pins[i];
			need_coma = 1;
		}
		else
			range_stop++;
	}

	if (need_coma)
		printf(",");
	if (range_start != range_stop)
		printf("%d-%d", range_start, range_stop);
	else
		printf("%d", range_start);
}

static int
gpiobus_parse_pins(struct gpiobus_softc *sc, device_t child, int mask)
{
	struct gpiobus_ivar *devi = GPIOBUS_IVAR(child);
	int i, npins;

	npins = 0;
	for (i = 0; i < 32; i++) {
		if (mask & (1 << i))
			npins++;
	}

	if (npins == 0) {
		device_printf(child, "empty pin mask\n");
		return (EINVAL);
	}

	devi->npins = npins;
	devi->pins = malloc(sizeof(uint32_t) * devi->npins, M_DEVBUF, 
	    M_NOWAIT | M_ZERO);

	if (!devi->pins)
		return (ENOMEM);

	npins = 0;
	for (i = 0; i < 32; i++) {

		if ((mask & (1 << i)) == 0)
			continue;

		if (i >= sc->sc_npins) {
			device_printf(child, 
			    "invalid pin %d, max: %d\n", i, sc->sc_npins - 1);
			free(devi->pins, M_DEVBUF);
			return (EINVAL);
		}

		devi->pins[npins++] = i;
		/*
		 * Mark pin as mapped and give warning if it's already mapped
		 */
		if (sc->sc_pins_mapped[i]) {
			device_printf(child, 
			    "warning: pin %d is already mapped\n", i);
			free(devi->pins, M_DEVBUF);
			return (EINVAL);
		}
		sc->sc_pins_mapped[i] = 1;
	}

	return (0);
}

static int
gpiobus_probe(device_t dev)
{
	device_set_desc(dev, "GPIO bus");

	return (BUS_PROBE_GENERIC);
}

static int
gpiobus_attach(device_t dev)
{
	struct gpiobus_softc *sc = GPIOBUS_SOFTC(dev);
	int res;

	sc->sc_busdev = dev;
	sc->sc_dev = device_get_parent(dev);
	res = GPIO_PIN_MAX(sc->sc_dev, &sc->sc_npins);
	if (res)
		return (ENXIO);

	KASSERT(sc->sc_npins != 0, ("GPIO device with no pins"));

	/*
	 * Increase to get number of pins
	 */
	sc->sc_npins++;

	sc->sc_pins_mapped = malloc(sizeof(int) * sc->sc_npins, M_DEVBUF, 
	    M_NOWAIT | M_ZERO);

	if (!sc->sc_pins_mapped)
		return (ENOMEM);

	/* init bus lock */
	GPIOBUS_LOCK_INIT(sc);

	/*
	 * Get parent's pins and mark them as unmapped
	 */
	bus_generic_probe(dev);
	bus_enumerate_hinted_children(dev);

	return (bus_generic_attach(dev));
}

/*
 * Since this is not a self-enumerating bus, and since we always add
 * children in attach, we have to always delete children here.
 */
static int
gpiobus_detach(device_t dev)
{
	struct gpiobus_softc *sc;
	struct gpiobus_ivar *devi;
	device_t *devlist;
	int i, err, ndevs;

	sc = GPIOBUS_SOFTC(dev);
	KASSERT(mtx_initialized(&sc->sc_mtx),
	    ("gpiobus mutex not initialized"));
	GPIOBUS_LOCK_DESTROY(sc);

	if ((err = bus_generic_detach(dev)) != 0)
		return (err);

	if ((err = device_get_children(dev, &devlist, &ndevs)) != 0)
		return (err);
	for (i = 0; i < ndevs; i++) {
		device_delete_child(dev, devlist[i]);
		devi = GPIOBUS_IVAR(devlist[i]);
		if (devi->pins) {
			free(devi->pins, M_DEVBUF);
			devi->pins = NULL;
		}
	}
	free(devlist, M_TEMP);

	if (sc->sc_pins_mapped) {
		free(sc->sc_pins_mapped, M_DEVBUF);
		sc->sc_pins_mapped = NULL;
	}

	return (0);
}

static int
gpiobus_suspend(device_t dev)
{

	return (bus_generic_suspend(dev));
}

static int
gpiobus_resume(device_t dev)
{

	return (bus_generic_resume(dev));
}

static int
gpiobus_print_child(device_t dev, device_t child)
{
	struct gpiobus_ivar *devi = GPIOBUS_IVAR(child);
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf(" at pin(s) ");
	gpiobus_print_pins(devi);
	retval += bus_print_child_footer(dev, child);

	return (retval);
}

static int
gpiobus_child_location_str(device_t bus, device_t child, char *buf,
    size_t buflen)
{

	snprintf(buf, buflen, "pins=?");
	return (0);
}

static int
gpiobus_child_pnpinfo_str(device_t bus, device_t child, char *buf,
    size_t buflen)
{

	*buf = '\0';
	return (0);
}

static device_t
gpiobus_add_child(device_t dev, u_int order, const char *name, int unit)
{
	device_t child;
	struct gpiobus_ivar *devi;

	child = device_add_child_ordered(dev, order, name, unit);
	if (child == NULL) 
		return (child);
	devi = malloc(sizeof(struct gpiobus_ivar), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (devi == NULL) {
		device_delete_child(dev, child);
		return (0);
	}
	device_set_ivars(child, devi);
	return (child);
}

static void
gpiobus_hinted_child(device_t bus, const char *dname, int dunit)
{
	struct gpiobus_softc *sc = GPIOBUS_SOFTC(bus);
	struct gpiobus_ivar *devi;
	device_t child;
	int pins;


	child = BUS_ADD_CHILD(bus, 0, dname, dunit);
	devi = GPIOBUS_IVAR(child);
	resource_int_value(dname, dunit, "pins", &pins);
	if (gpiobus_parse_pins(sc, child, pins))
		device_delete_child(bus, child);
}

static void
gpiobus_lock_bus(device_t busdev)
{
	struct gpiobus_softc *sc;

	sc = device_get_softc(busdev);
	GPIOBUS_ASSERT_UNLOCKED(sc);
	GPIOBUS_LOCK(sc);
}

static void
gpiobus_unlock_bus(device_t busdev)
{
	struct gpiobus_softc *sc;

	sc = device_get_softc(busdev);
	GPIOBUS_ASSERT_LOCKED(sc);
	GPIOBUS_UNLOCK(sc);
}

static void
gpiobus_acquire_bus(device_t busdev, device_t child)
{
	struct gpiobus_softc *sc;

	sc = device_get_softc(busdev);
	GPIOBUS_ASSERT_LOCKED(sc);

	if (sc->sc_owner)
		panic("gpiobus: cannot serialize the access to device.");
	sc->sc_owner = child;
}

static void
gpiobus_release_bus(device_t busdev, device_t child)
{
	struct gpiobus_softc *sc;

	sc = device_get_softc(busdev);
	GPIOBUS_ASSERT_LOCKED(sc);

	if (!sc->sc_owner)
		panic("gpiobus: releasing unowned bus.");
	if (sc->sc_owner != child)
		panic("gpiobus: you don't own the bus. game over.");

	sc->sc_owner = NULL;
}

static int
gpiobus_pin_setflags(device_t dev, device_t child, uint32_t pin, 
    uint32_t flags)
{
	struct gpiobus_softc *sc = GPIOBUS_SOFTC(dev);
	struct gpiobus_ivar *devi = GPIOBUS_IVAR(child);

	if (pin >= devi->npins)
		return (EINVAL);

	return GPIO_PIN_SETFLAGS(sc->sc_dev, devi->pins[pin], flags);
}

static int
gpiobus_pin_getflags(device_t dev, device_t child, uint32_t pin, 
    uint32_t *flags)
{
	struct gpiobus_softc *sc = GPIOBUS_SOFTC(dev);
	struct gpiobus_ivar *devi = GPIOBUS_IVAR(child);

	if (pin >= devi->npins)
		return (EINVAL);

	return GPIO_PIN_GETFLAGS(sc->sc_dev, devi->pins[pin], flags);
}

static int
gpiobus_pin_getcaps(device_t dev, device_t child, uint32_t pin, 
    uint32_t *caps)
{
	struct gpiobus_softc *sc = GPIOBUS_SOFTC(dev);
	struct gpiobus_ivar *devi = GPIOBUS_IVAR(child);

	if (pin >= devi->npins)
		return (EINVAL);

	return GPIO_PIN_GETCAPS(sc->sc_dev, devi->pins[pin], caps);
}

static int
gpiobus_pin_set(device_t dev, device_t child, uint32_t pin, 
    unsigned int value)
{
	struct gpiobus_softc *sc = GPIOBUS_SOFTC(dev);
	struct gpiobus_ivar *devi = GPIOBUS_IVAR(child);

	if (pin >= devi->npins)
		return (EINVAL);

	return GPIO_PIN_SET(sc->sc_dev, devi->pins[pin], value);
}

static int
gpiobus_pin_get(device_t dev, device_t child, uint32_t pin, 
    unsigned int *value)
{
	struct gpiobus_softc *sc = GPIOBUS_SOFTC(dev);
	struct gpiobus_ivar *devi = GPIOBUS_IVAR(child);

	if (pin >= devi->npins)
		return (EINVAL);

	return GPIO_PIN_GET(sc->sc_dev, devi->pins[pin], value);
}

static int
gpiobus_pin_toggle(device_t dev, device_t child, uint32_t pin)
{
	struct gpiobus_softc *sc = GPIOBUS_SOFTC(dev);
	struct gpiobus_ivar *devi = GPIOBUS_IVAR(child);

	if (pin >= devi->npins)
		return (EINVAL);

	return GPIO_PIN_TOGGLE(sc->sc_dev, devi->pins[pin]);
}

static device_method_t gpiobus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		gpiobus_probe),
	DEVMETHOD(device_attach,	gpiobus_attach),
	DEVMETHOD(device_detach,	gpiobus_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	gpiobus_suspend),
	DEVMETHOD(device_resume,	gpiobus_resume),

	/* Bus interface */
	DEVMETHOD(bus_add_child,	gpiobus_add_child),
	DEVMETHOD(bus_print_child,	gpiobus_print_child),
	DEVMETHOD(bus_child_pnpinfo_str, gpiobus_child_pnpinfo_str),
	DEVMETHOD(bus_child_location_str, gpiobus_child_location_str),
	DEVMETHOD(bus_hinted_child,	gpiobus_hinted_child),

	/* GPIO protocol */
	DEVMETHOD(gpiobus_lock_bus,	gpiobus_lock_bus),
	DEVMETHOD(gpiobus_unlock_bus,	gpiobus_unlock_bus),
	DEVMETHOD(gpiobus_acquire_bus,	gpiobus_acquire_bus),
	DEVMETHOD(gpiobus_release_bus,	gpiobus_release_bus),
	DEVMETHOD(gpiobus_pin_getflags,	gpiobus_pin_getflags),
	DEVMETHOD(gpiobus_pin_getcaps,	gpiobus_pin_getcaps),
	DEVMETHOD(gpiobus_pin_setflags,	gpiobus_pin_setflags),
	DEVMETHOD(gpiobus_pin_get,	gpiobus_pin_get),
	DEVMETHOD(gpiobus_pin_set,	gpiobus_pin_set),
	DEVMETHOD(gpiobus_pin_toggle,	gpiobus_pin_toggle),

	DEVMETHOD_END
};

driver_t gpiobus_driver = {
	"gpiobus",
	gpiobus_methods,
	sizeof(struct gpiobus_softc)
};

devclass_t	gpiobus_devclass;

DRIVER_MODULE(gpiobus, gpio, gpiobus_driver, gpiobus_devclass, 0, 0);
MODULE_VERSION(gpiobus, 1);
