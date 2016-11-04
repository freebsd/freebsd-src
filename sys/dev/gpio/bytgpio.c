/*-
 * Copyright (c) 2016 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/gpio/gpiobusvar.h>

#include "gpio_if.h"

/**
 *	Macros for driver mutex locking
 */
#define	BYTGPIO_LOCK(_sc)		mtx_lock_spin(&(_sc)->sc_mtx)
#define	BYTGPIO_UNLOCK(_sc)		mtx_unlock_spin(&(_sc)->sc_mtx)
#define	BYTGPIO_LOCK_INIT(_sc)		\
	mtx_init(&_sc->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
	    "bytgpio", MTX_SPIN)
#define	BYTGPIO_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx)
#define	BYTGPIO_ASSERT_LOCKED(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)
#define	BYTGPIO_ASSERT_UNLOCKED(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_NOTOWNED)

struct bytgpio_softc {
	ACPI_HANDLE		sc_handle;
	device_t		sc_dev;
	device_t		sc_busdev;
	struct mtx		sc_mtx;
	int			sc_mem_rid;
	struct resource		*sc_mem_res;
	int			sc_npins;
	const char*		sc_bank_prefix;
	const int		*sc_pinpad_map;
};

static int	bytgpio_probe(device_t dev);
static int	bytgpio_attach(device_t dev);

#define SCORE_UID		1
#define SCORE_BANK_PREFIX	"GPIO_S0_SC"
const int bytgpio_score_pins[] = {
	85, 89, 93, 96, 99, 102, 98, 101, 34, 37, 36, 38, 39, 35, 40,
	84, 62, 61, 64, 59, 54, 56, 60, 55, 63, 57, 51, 50, 53, 47,
	52, 49, 48, 43, 46, 41, 45, 42, 58, 44, 95, 105, 70, 68, 67,
	66, 69, 71, 65, 72, 86, 90, 88, 92, 103, 77, 79, 83, 78, 81,
	80, 82, 13, 12, 15, 14, 17, 18, 19, 16, 2, 1, 0, 4, 6, 7, 9,
	8, 33, 32, 31, 30, 29, 27, 25, 28, 26, 23, 21, 20, 24, 22, 5,
	3, 10, 11, 106, 87, 91, 104, 97, 100
};
#define SCORE_PINS	nitems(bytgpio_score_pins)

#define NCORE_UID		2
#define NCORE_BANK_PREFIX	"GPIO_S0_NC"
const int bytgpio_ncore_pins[] = {
	19, 18, 17, 20, 21, 22, 24, 25, 23, 16, 14, 15, 12, 26, 27,
	1, 4, 8, 11, 0, 3, 6, 10, 13, 2, 5, 9, 7
};
#define	NCORE_PINS	nitems(bytgpio_ncore_pins)

#define SUS_UID		3
#define SUS_BANK_PREFIX	"GPIO_S5_"
const int bytgpio_sus_pins[] = {
        29, 33, 30, 31, 32, 34, 36, 35, 38, 37, 18, 7, 11, 20, 17, 1,
	8, 10, 19, 12, 0, 2, 23, 39, 28, 27, 22, 21, 24, 25, 26, 51,
	56, 54, 49, 55, 48, 57, 50, 58, 52, 53, 59, 40
};
#define	SUS_PINS	nitems(bytgpio_sus_pins)

#define	BYGPIO_PIN_REGISTER(sc, pin, reg)	((sc)->sc_pinpad_map[(pin)] * 16 + (reg))
#define	BYTGPIO_PCONF0		0x0000
#define	BYTGPIO_PAD_VAL		0x0008
#define		BYTGPIO_PAD_VAL_LEVEL		(1 << 0)	
#define		BYTGPIO_PAD_VAL_I_OUTPUT_ENABLED	(1 << 1)
#define		BYTGPIO_PAD_VAL_I_INPUT_ENABLED	(1 << 2)
#define		BYTGPIO_PAD_VAL_DIR_MASK		(3 << 1)

static inline uint32_t
bytgpio_read_4(struct bytgpio_softc *sc, bus_size_t off)
{
	return (bus_read_4(sc->sc_mem_res, off));
}

static inline void
bytgpio_write_4(struct bytgpio_softc *sc, bus_size_t off,
    uint32_t val)
{
	bus_write_4(sc->sc_mem_res, off, val);
}

static device_t
bytgpio_get_bus(device_t dev)
{
	struct bytgpio_softc *sc;

	sc = device_get_softc(dev);

	return (sc->sc_busdev);
}

static int
bytgpio_pin_max(device_t dev, int *maxpin)
{
	struct bytgpio_softc *sc;

	sc = device_get_softc(dev);

	*maxpin = sc->sc_npins - 1;

	return (0);
}

static int
bytgpio_valid_pin(struct bytgpio_softc *sc, int pin)
{

	if (pin >= sc->sc_npins || sc->sc_mem_res == NULL)
		return (EINVAL);

	return (0);
}

static int
bytgpio_pin_getcaps(device_t dev, uint32_t pin, uint32_t *caps)
{
	struct bytgpio_softc *sc;

	sc = device_get_softc(dev);
	if (bytgpio_valid_pin(sc, pin) != 0)
		return (EINVAL);

	*caps = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;

	return (0);
}

static int
bytgpio_pin_getflags(device_t dev, uint32_t pin, uint32_t *flags)
{
	struct bytgpio_softc *sc;
	uint32_t reg, val;

	sc = device_get_softc(dev);
	if (bytgpio_valid_pin(sc, pin) != 0)
		return (EINVAL);

	/* Get the current pin state */
	BYTGPIO_LOCK(sc);
	reg = BYGPIO_PIN_REGISTER(sc, pin, BYTGPIO_PAD_VAL);
	val = bytgpio_read_4(sc, reg);
	*flags = 0;
	if ((val & BYTGPIO_PAD_VAL_I_OUTPUT_ENABLED) == 0)
		*flags |= GPIO_PIN_OUTPUT;
	/*
	 * this bit can be cleared to read current output value
	 * sou output bit takes precedense
	 */
	else if ((val & BYTGPIO_PAD_VAL_I_INPUT_ENABLED) == 0)
		*flags |= GPIO_PIN_INPUT;
	BYTGPIO_UNLOCK(sc);

	return (0);
}

static int
bytgpio_pin_setflags(device_t dev, uint32_t pin, uint32_t flags)
{
	struct bytgpio_softc *sc;
	uint32_t reg, val;
	uint32_t allowed;

	sc = device_get_softc(dev);
	if (bytgpio_valid_pin(sc, pin) != 0)
		return (EINVAL);

	allowed = GPIO_PIN_INPUT | GPIO_PIN_OUTPUT;

	/* 
	 * Only directtion flag allowed
	 */
	if (flags & ~allowed)
		return (EINVAL);

	/* 
	 * Not both directions simultaneously
	 */
	if ((flags & allowed) == allowed)
		return (EINVAL);

	/* Set the GPIO mode and state */
	BYTGPIO_LOCK(sc);
	reg = BYGPIO_PIN_REGISTER(sc, pin, BYTGPIO_PAD_VAL);
	val = bytgpio_read_4(sc, reg);
	val = val | BYTGPIO_PAD_VAL_DIR_MASK;
	if (flags & GPIO_PIN_INPUT)
		val = val & ~BYTGPIO_PAD_VAL_I_INPUT_ENABLED;
	if (flags & GPIO_PIN_OUTPUT)
		val = val & ~BYTGPIO_PAD_VAL_I_OUTPUT_ENABLED;
	bytgpio_write_4(sc, reg, val);
	BYTGPIO_UNLOCK(sc);

	return (0);
}

static int
bytgpio_pin_getname(device_t dev, uint32_t pin, char *name)
{
	struct bytgpio_softc *sc;

	sc = device_get_softc(dev);
	if (bytgpio_valid_pin(sc, pin) != 0)
		return (EINVAL);

	/* Set a very simple name */
	snprintf(name, GPIOMAXNAME, "%s%u", sc->sc_bank_prefix, pin);
	name[GPIOMAXNAME - 1] = '\0';

	return (0);
}

static int
bytgpio_pin_set(device_t dev, uint32_t pin, unsigned int value)
{
	struct bytgpio_softc *sc;
	uint32_t reg, val;

	sc = device_get_softc(dev);
	if (bytgpio_valid_pin(sc, pin) != 0)
		return (EINVAL);

	BYTGPIO_LOCK(sc);
	reg = BYGPIO_PIN_REGISTER(sc, pin, BYTGPIO_PAD_VAL);
	val = bytgpio_read_4(sc, reg);
	if (value == GPIO_PIN_LOW)
		val = val & ~BYTGPIO_PAD_VAL_LEVEL;
	else
		val = val | BYTGPIO_PAD_VAL_LEVEL;
	bytgpio_write_4(sc, reg, val);
	BYTGPIO_UNLOCK(sc);

	return (0);
}

static int
bytgpio_pin_get(device_t dev, uint32_t pin, unsigned int *value)
{
	struct bytgpio_softc *sc;
	uint32_t reg, val;

	sc = device_get_softc(dev);
	if (bytgpio_valid_pin(sc, pin) != 0)
		return (EINVAL);

	BYTGPIO_LOCK(sc);
	reg = BYGPIO_PIN_REGISTER(sc, pin, BYTGPIO_PAD_VAL);
	/*
	 * Enable input to read current value
	 */
	val = bytgpio_read_4(sc, reg);
	val = val & ~BYTGPIO_PAD_VAL_I_INPUT_ENABLED;
	bytgpio_write_4(sc, reg, val);
	/*
	 * And read actual value
	 */
	val = bytgpio_read_4(sc, reg);
	if (val & BYTGPIO_PAD_VAL_LEVEL)
		*value = GPIO_PIN_HIGH;
	else
		*value = GPIO_PIN_LOW;
	BYTGPIO_UNLOCK(sc);

	return (0);
}

static int
bytgpio_pin_toggle(device_t dev, uint32_t pin)
{
	struct bytgpio_softc *sc;
	uint32_t reg, val;

	sc = device_get_softc(dev);
	if (bytgpio_valid_pin(sc, pin) != 0)
		return (EINVAL);

	/* Toggle the pin */
	BYTGPIO_LOCK(sc);
	reg = BYGPIO_PIN_REGISTER(sc, pin, BYTGPIO_PAD_VAL);
	val = bytgpio_read_4(sc, reg);
	val = val ^ BYTGPIO_PAD_VAL_LEVEL;
	bytgpio_write_4(sc, reg, val);
	BYTGPIO_UNLOCK(sc);

	return (0);
}

static int
bytgpio_probe(device_t dev)
{
	static char *gpio_ids[] = { "INT33FC", NULL };

	if (acpi_disabled("gpio") ||
	    ACPI_ID_PROBE(device_get_parent(dev), dev, gpio_ids) == NULL)
	return (ENXIO);

	device_set_desc(dev, "Intel Baytrail GPIO Controller");
	return (0);
}

static int
bytgpio_attach(device_t dev)
{
	struct bytgpio_softc	*sc;
	ACPI_STATUS status;
	int uid;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;
	sc->sc_handle = acpi_get_handle(dev);
	status = acpi_GetInteger(sc->sc_handle, "_UID", &uid);
	if (ACPI_FAILURE(status)) {
		device_printf(dev, "failed to read _UID\n");
		return (ENXIO);
	}

	switch (uid) {
	case SCORE_UID:
		sc->sc_npins = SCORE_PINS;
		sc->sc_bank_prefix = SCORE_BANK_PREFIX;
		sc->sc_pinpad_map = bytgpio_score_pins;
		break;
	case NCORE_UID:
		sc->sc_npins = NCORE_PINS;
		sc->sc_bank_prefix = NCORE_BANK_PREFIX;
		sc->sc_pinpad_map = bytgpio_ncore_pins;
		break;
	case SUS_UID:
		sc->sc_npins = SUS_PINS;
		sc->sc_bank_prefix = SUS_BANK_PREFIX;
		sc->sc_pinpad_map = bytgpio_sus_pins;
		break;
	default:
		device_printf(dev, "invalid _UID value: %d\n", uid);
	}

	sc->sc_mem_rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(sc->sc_dev,
	    SYS_RES_MEMORY, &sc->sc_mem_rid, RF_ACTIVE);
	if (sc->sc_mem_res == NULL) {
		device_printf(dev, "can't allocate resource\n");
		goto error;
	}

	BYTGPIO_LOCK_INIT(sc);

	sc->sc_busdev = gpiobus_attach_bus(dev);
	if (sc->sc_busdev == NULL) {
		BYTGPIO_LOCK_DESTROY(sc);
		bus_release_resource(dev, SYS_RES_MEMORY,
		    sc->sc_mem_rid, sc->sc_mem_res);
		return (ENXIO);
	}

	return (0);

error:
	return (ENXIO);
}

static device_method_t bytgpio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, bytgpio_probe),
	DEVMETHOD(device_attach, bytgpio_attach),

	/* GPIO protocol */
	DEVMETHOD(gpio_get_bus, bytgpio_get_bus),
	DEVMETHOD(gpio_pin_max, bytgpio_pin_max),
	DEVMETHOD(gpio_pin_getname, bytgpio_pin_getname),
	DEVMETHOD(gpio_pin_getflags, bytgpio_pin_getflags),
	DEVMETHOD(gpio_pin_getcaps, bytgpio_pin_getcaps),
	DEVMETHOD(gpio_pin_setflags, bytgpio_pin_setflags),
	DEVMETHOD(gpio_pin_get, bytgpio_pin_get),
	DEVMETHOD(gpio_pin_set, bytgpio_pin_set),
	DEVMETHOD(gpio_pin_toggle, bytgpio_pin_toggle),

	DEVMETHOD_END
};

static driver_t bytgpio_driver = {
	"gpio",
	bytgpio_methods,
	sizeof(struct bytgpio_softc),
};

static devclass_t bytgpio_devclass;
DRIVER_MODULE(bytgpio, acpi, bytgpio_driver, bytgpio_devclass, 0, 0);
MODULE_DEPEND(bytgpio, acpi, 1, 1, 1);
MODULE_DEPEND(bytgpio, gpio, 1, 1, 1);
