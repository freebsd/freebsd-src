/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Colin Percival
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

#include <sys/types.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include "gpiobus_if.h"

#include <contrib/dev/acpica/include/acpi.h>
#include <dev/acpica/acpivar.h>

#include <dev/gpio/gpiobusvar.h>
#include <dev/gpio/acpi_gpiobusvar.h>

enum gpio_aei_type {
	ACPI_AEI_TYPE_UNKNOWN,
	ACPI_AEI_TYPE_ELX,
	ACPI_AEI_TYPE_EVT
};

struct gpio_aei_softc {
	ACPI_HANDLE handle;
	enum gpio_aei_type type;
	int pin;
	struct resource * intr_res;
	int intr_rid;
	void * intr_cookie;
};

static int
gpio_aei_probe(device_t dev)
{

	/* We only match when gpiobus explicitly requested gpio_aei. */
	return (BUS_PROBE_NOWILDCARD);
}

static void
gpio_aei_intr(void * arg)
{
	struct gpio_aei_softc * sc = arg;

	/* Ask ACPI to run the appropriate _EVT, _Exx or _Lxx method. */
	if (sc->type == ACPI_AEI_TYPE_EVT)
		acpi_SetInteger(sc->handle, NULL, sc->pin);
	else
		AcpiEvaluateObject(sc->handle, NULL, NULL, NULL);
}

static int
gpio_aei_attach(device_t dev)
{
	struct gpio_aei_softc * sc = device_get_softc(dev);
	gpio_pin_t pin;
	ACPI_HANDLE handle;
	int err;

	/* This is us. */
	device_set_desc(dev, "ACPI Event Information Device");

	/* Store parameters needed by gpio_aei_intr. */
	handle = acpi_gpiobus_get_handle(dev);
	if (gpio_pin_get_by_acpi_index(dev, 0, &pin) != 0) {
		device_printf(dev, "Unable to get the input pin\n");
		return (ENXIO);
	}

	sc->type = ACPI_AEI_TYPE_UNKNOWN;
	sc->pin = pin->pin;
	if (pin->pin <= 255) {
		char objname[5];	/* "_EXX" or "_LXX" */
		sprintf(objname, "_%c%02X",
		    (pin->flags & GPIO_INTR_EDGE_MASK) ? 'E' : 'L', pin->pin);
		if (ACPI_SUCCESS(AcpiGetHandle(handle, objname, &sc->handle)))
			sc->type = ACPI_AEI_TYPE_ELX;
	}
	if (sc->type == ACPI_AEI_TYPE_UNKNOWN) {
		if (ACPI_SUCCESS(AcpiGetHandle(handle, "_EVT", &sc->handle)))
			sc->type = ACPI_AEI_TYPE_EVT;
	}

	if (sc->type == ACPI_AEI_TYPE_UNKNOWN) {
		device_printf(dev, "ACPI Event Information Device type is unknown");
		return (ENOTSUP);
	}

	/* Set up the interrupt. */
	if ((sc->intr_res = gpio_alloc_intr_resource(dev, &sc->intr_rid,
	    RF_ACTIVE, pin, pin->flags & GPIO_INTR_MASK)) == NULL) {
		device_printf(dev, "Cannot allocate an IRQ\n");
		return (ENOTSUP);
	}
	err = bus_setup_intr(dev, sc->intr_res, INTR_TYPE_MISC | INTR_MPSAFE,
	    NULL, gpio_aei_intr, sc, &sc->intr_cookie);
	if (err != 0) {
		device_printf(dev, "Cannot set up IRQ\n");
		bus_release_resource(dev, SYS_RES_IRQ, sc->intr_rid,
		    sc->intr_res);
		return (err);
	}

	return (0);
}

static int
gpio_aei_detach(device_t dev)
{
	struct gpio_aei_softc * sc = device_get_softc(dev);

	bus_teardown_intr(dev, sc->intr_res, sc->intr_cookie);
	bus_release_resource(dev, SYS_RES_IRQ, sc->intr_rid, sc->intr_res);
	return (0);
}

static device_method_t gpio_aei_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		gpio_aei_probe),
	DEVMETHOD(device_attach,	gpio_aei_attach),
	DEVMETHOD(device_detach,	gpio_aei_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_0(gpio_aei, gpio_aei_driver, gpio_aei_methods, sizeof(struct gpio_aei_softc));
DRIVER_MODULE(gpio_aei, gpiobus, gpio_aei_driver, NULL, NULL);
MODULE_DEPEND(gpio_aei, acpi_gpiobus, 1, 1, 1);
