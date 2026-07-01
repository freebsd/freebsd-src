/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Beckhoff Automation GmbH & Co. KG
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/gpio/gpiobusvar.h>

#include "intelgpio.h"

#include "gpio_if.h"
#include "opt_acpi.h"

#define INTELGPIO_LOCK(_sc)   mtx_lock_spin(&(_sc)->sc_mtx)
#define INTELGPIO_UNLOCK(_sc) mtx_unlock_spin(&(_sc)->sc_mtx)
#define INTELGPIO_LOCK_INIT(_sc)                                     \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
	    "intelgpio", MTX_SPIN)
#define INTELGPIO_LOCK_DESTROY(_sc) mtx_destroy(&(_sc)->sc_mtx)

static int
intelgpio_gpio_to_pad(struct intelgpio_softc *sc, uint32_t pin,
    struct resource **res, bus_size_t *pad_off,
    const struct intelgpio_padgroup **pgp)
{
	const struct intelgpio_platform *plat = sc->sc_plat;
	const struct intelgpio_community *com;
	const struct intelgpio_padgroup *pg;
	int i, j, offset, local_pad;

	for (i = 0; i < plat->ncommunities; i++) {
		com = &plat->communities[i];
		for (j = 0; j < com->ngroups; j++) {
			pg = &com->groups[j];
			if (pg->gpio_base == INTELGPIO_GPIO_NOMAP)
				continue;
			if (pin >= pg->gpio_base &&
			    pin < pg->gpio_base + pg->npads) {
				if (res != NULL)
					*res = sc->sc_mem_res[i];
				if (pad_off != NULL) {
					offset = pin - pg->gpio_base;
					local_pad = (pg->first_pad + offset) -
					    com->groups[0].first_pad;
					*pad_off = sc->sc_padbar[i] +
					    (local_pad * INTELGPIO_PAD_SIZE);
				}
				if (pgp != NULL)
					*pgp = pg;
				return (0);
			}
		}
	}
	return (-1);
}

static device_t
intelgpio_get_bus(device_t dev)
{
	struct intelgpio_softc *sc;

	sc = device_get_softc(dev);
	return (sc->sc_busdev);
}

static int
intelgpio_pin_max(device_t dev, int *maxpin)
{
	struct intelgpio_softc *sc;
	const struct intelgpio_platform *plat;
	const struct intelgpio_community *com;
	const struct intelgpio_padgroup *pg;
	int i, j;

	if (maxpin == NULL)
		return (EINVAL);

	sc = device_get_softc(dev);
	plat = sc->sc_plat;

	/* Find the last group with a valid gpio_base, skipping NOMAP */
	*maxpin = 0;
	for (i = 0; i < plat->ncommunities; i++) {
		com = &plat->communities[i];
		for (j = 0; j < com->ngroups; j++) {
			pg = &com->groups[j];
			if (pg->gpio_base == INTELGPIO_GPIO_NOMAP)
				continue;
			*maxpin = pg->gpio_base + (pg->npads - 1);
		}
	}
	return (0);
}

static int
intelgpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct intelgpio_softc *sc;
	const struct intelgpio_padgroup *pg;
	int offset;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, NULL, NULL, &pg) != 0)
		return (EINVAL);

	offset = pin - pg->gpio_base;
	snprintf(name, GPIOMAXNAME, "%s%d", pg->name, offset);
	return (0);
}

static int
intelgpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct intelgpio_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * The getcaps function is expected to return the capabilities
	 * of the pin. Validate that the pin number maps to an existing
	 * pad first.
	 */
	if (intelgpio_gpio_to_pad(sc, pin, NULL, NULL, NULL) != 0)
		return (EINVAL);

	*caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;
	return (0);
}

static int
intelgpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	*flags = 0;

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	INTELGPIO_UNLOCK(sc);

	/*
	 * If the pad is not in GPIO mode (i.e. configured for a native
	 * function like PCIe, UART, I2C, etc.), report no flags. The pin
	 * cannot be used as GPIO without first switching it via setflags.
	 */
	if ((val & INTELGPIO_PADCFG0_PMODE_MASK) !=
	    INTELGPIO_PADCFG0_PMODE_GPIO)
		return (0);

	/*
	 * Report OUTPUT only if TX is explicitly enabled. Default to INPUT
	 * otherwise: a non-driving pin is considered an input from the user's
	 * perspective.
	 */
	if (!(val & INTELGPIO_PADCFG0_GPIOTXDIS))
		*flags = GPIO_PIN_OUTPUT;
	else
		*flags = GPIO_PIN_INPUT;

	return (0);
}

static int
intelgpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	/*
	 * We currently only support input and output, not both simultaneously
	 */
	if (flags & ~(GPIO_PIN_INPUT | GPIO_PIN_OUTPUT))
		return (EINVAL);
	if ((flags & GPIO_PIN_INPUT) && (flags & GPIO_PIN_OUTPUT))
		return (EINVAL);

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);

	/*
	 * Switch the pad to GPIO mode. Intel pads can operate in different
	 * modes (native function 1-15 or GPIO). Since setflags is only called
	 * when userspace or a consumer explicitly wants to use the pin as a
	 * GPIO, we force the pad into GPIO mode here so the direction bits
	 * (TXDIS/RXDIS) below take effect.
	 */
	val &= ~INTELGPIO_PADCFG0_PMODE_MASK;
	val |= INTELGPIO_PADCFG0_PMODE_GPIO;

	if (flags & GPIO_PIN_INPUT)
		val &= ~INTELGPIO_PADCFG0_GPIORXDIS;
	else
		val |= INTELGPIO_PADCFG0_GPIORXDIS;

	if (flags & GPIO_PIN_OUTPUT)
		val &= ~INTELGPIO_PADCFG0_GPIOTXDIS;
	else
		val |= INTELGPIO_PADCFG0_GPIOTXDIS;

	bus_write_4(res, pad_off, val);
	INTELGPIO_UNLOCK(sc);

	return (0);
}

static int
intelgpio_pin_get(device_t dev, uint32_t pin, unsigned int *value)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	INTELGPIO_UNLOCK(sc);

	*value = (val & INTELGPIO_PADCFG0_GPIORXSTATE) ? GPIO_PIN_HIGH :
							 GPIO_PIN_LOW;

	return (0);
}

static int
intelgpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	if (value == GPIO_PIN_LOW)
		val &= ~INTELGPIO_PADCFG0_GPIOTXSTATE;
	else
		val |= INTELGPIO_PADCFG0_GPIOTXSTATE;
	bus_write_4(res, pad_off, val);
	INTELGPIO_UNLOCK(sc);

	return (0);
}

static int
intelgpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct intelgpio_softc *sc;
	struct resource *res;
	bus_size_t pad_off;
	uint32_t val;

	sc = device_get_softc(dev);
	if (intelgpio_gpio_to_pad(sc, pin, &res, &pad_off, NULL) != 0)
		return (EINVAL);
	if (res == NULL)
		return (EINVAL);

	INTELGPIO_LOCK(sc);
	val = bus_read_4(res, pad_off);
	val ^= INTELGPIO_PADCFG0_GPIOTXSTATE;
	bus_write_4(res, pad_off, val);
	INTELGPIO_UNLOCK(sc);

	return (0);
}

int
intelgpio_probe(device_t dev, const struct intelgpio_platform *plat)
{
	int rv;

	if (acpi_disabled("intelgpio"))
		return (ENXIO);

	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, plat->hids, NULL);
	if (rv <= 0)
		device_set_desc(dev, plat->desc);
	return (rv);
}

static int
intelgpio_detach(device_t dev)
{
	struct intelgpio_softc *sc;
	int i;

	sc = device_get_softc(dev);

	if (sc->sc_busdev != NULL)
		gpiobus_detach_bus(dev);

	for (i = 0; i < sc->sc_plat->ncommunities; i++) {
		if (sc->sc_mem_res[i] != NULL)
			bus_release_resource(dev, SYS_RES_MEMORY, i,
			    sc->sc_mem_res[i]);
	}

	INTELGPIO_LOCK_DESTROY(sc);
	return (0);
}

int
intelgpio_attach(device_t dev, const struct intelgpio_platform *plat)
{
	struct intelgpio_softc *sc;
	bool have_res;
	int i, rid;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_handle = acpi_get_handle(dev);
	sc->sc_plat = plat;

	KASSERT(plat->ncommunities <= INTELGPIO_MAX_COMMUNITIES,
	    ("too many communities: %d > %d", plat->ncommunities,
		INTELGPIO_MAX_COMMUNITIES));

	INTELGPIO_LOCK_INIT(sc);

	have_res = false;
	for (i = 0; i < plat->ncommunities; i++) {
		rid = i;
		sc->sc_mem_res[i] = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
		    &rid, RF_ACTIVE);
		if (sc->sc_mem_res[i] == NULL) {
			device_printf(dev,
			    "failed to allocate memory resource "
			    "for community %d\n",
			    i);
			continue;
		}
		sc->sc_padbar[i] = bus_read_4(sc->sc_mem_res[i],
		    INTELGPIO_PADBAR_REG);
		have_res = true;
	}

	if (!have_res) {
		device_printf(dev, "no memory resources found\n");
		INTELGPIO_LOCK_DESTROY(sc);
		return (ENXIO);
	}

	sc->sc_busdev = gpiobus_add_bus(dev);
	if (sc->sc_busdev == NULL) {
		device_printf(dev, "failed to add gpiobus\n");
		intelgpio_detach(dev);
		return (ENXIO);
	}

	bus_attach_children(dev);

	return (0);
}


static device_method_t intelgpio_methods[] = {
	DEVMETHOD(device_detach, intelgpio_detach),

	DEVMETHOD(gpio_get_bus, intelgpio_get_bus),
	DEVMETHOD(gpio_pin_max, intelgpio_pin_max),
	DEVMETHOD(gpio_pin_getname, intelgpio_pin_getname),
	DEVMETHOD(gpio_pin_getcaps, intelgpio_pin_getcaps),
	DEVMETHOD(gpio_pin_getflags, intelgpio_pin_getflags),
	DEVMETHOD(gpio_pin_setflags, intelgpio_pin_setflags),
	DEVMETHOD(gpio_pin_get, intelgpio_pin_get),
	DEVMETHOD(gpio_pin_set, intelgpio_pin_set),
	DEVMETHOD(gpio_pin_toggle, intelgpio_pin_toggle),

	DEVMETHOD_END
};

DEFINE_CLASS_0(intelgpio, intelgpio_driver, intelgpio_methods,
    sizeof(struct intelgpio_softc));
