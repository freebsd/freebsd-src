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

struct gpio_aei_ctx {
	SLIST_ENTRY(gpio_aei_ctx) next;
	struct resource * intr_res;
	void * intr_cookie;
	ACPI_HANDLE handle;
	gpio_pin_t gpio;
	uint32_t pin;
	int intr_rid;
	enum gpio_aei_type type;
};

struct gpio_aei_softc {
	SLIST_HEAD(, gpio_aei_ctx) aei_ctx;
	ACPI_HANDLE dev_handle;
	device_t dev;
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
	struct gpio_aei_ctx * ctx = arg;

	/* Ask ACPI to run the appropriate _EVT, _Exx or _Lxx method. */
	if (ctx->type == ACPI_AEI_TYPE_EVT)
		acpi_SetInteger(ctx->handle, NULL, ctx->pin);
	else
		AcpiEvaluateObject(ctx->handle, NULL, NULL, NULL);
}

static ACPI_STATUS
gpio_aei_enumerate(ACPI_RESOURCE * res, void * context)
{
	ACPI_RESOURCE_GPIO * gpio_res = &res->Data.Gpio;
	struct gpio_aei_softc * sc = context;
	uint32_t flags, maxpin;
	device_t busdev;
	int err;

	/*
	 * Check that we have a GpioInt object.
	 * Note that according to the spec this
	 * should always be the case.
	 */
	if (res->Type != ACPI_RESOURCE_TYPE_GPIO)
		return (AE_OK);
	if (gpio_res->ConnectionType != ACPI_RESOURCE_GPIO_TYPE_INT)
		return (AE_OK);

	flags = acpi_gpiobus_convflags(gpio_res);
	if (acpi_quirks & ACPI_Q_AEI_NOPULL)
		flags &= ~GPIO_PIN_PULLUP;

	err = GPIO_PIN_MAX(acpi_get_device(sc->dev_handle), &maxpin);
	if (err != 0)
		return (AE_ERROR);

	busdev = GPIO_GET_BUS(acpi_get_device(sc->dev_handle));
	for (int i = 0; i < gpio_res->PinTableLength; i++) {
		struct gpio_aei_ctx * ctx;
		uint32_t pin = gpio_res->PinTable[i];

		if (__predict_false(pin > maxpin)) {
			device_printf(sc->dev,
			    "Invalid pin 0x%x, max: 0x%x (bad ACPI tables?)\n",
			    pin, maxpin);
			continue;
		}

		ctx = malloc(sizeof(struct gpio_aei_ctx), M_DEVBUF, M_WAITOK);
		ctx->type = ACPI_AEI_TYPE_UNKNOWN;
		if (pin <= 255) {
			char objname[5];	/* "_EXX" or "_LXX" */
			sprintf(objname, "_%c%02X",
			    (flags & GPIO_INTR_EDGE_MASK) ? 'E' : 'L', pin);
			if (ACPI_SUCCESS(AcpiGetHandle(sc->dev_handle, objname,
			    &ctx->handle)))
				ctx->type = ACPI_AEI_TYPE_ELX;
		}

		if (ctx->type == ACPI_AEI_TYPE_UNKNOWN) {
			if (ACPI_SUCCESS(AcpiGetHandle(sc->dev_handle, "_EVT",
			    &ctx->handle)))
				ctx->type = ACPI_AEI_TYPE_EVT;
			else {
				device_printf(sc->dev,
				    "AEI Device type is unknown for pin 0x%x\n",
				    pin);

				free(ctx, M_DEVBUF);
				continue;
			}
		}

		err = gpio_pin_get_by_bus_pinnum(busdev, pin, &ctx->gpio);
		if (err != 0) {
			device_printf(sc->dev, "Cannot acquire pin 0x%x\n",
			    pin);

			free(ctx, M_DEVBUF);
			continue;
		}

		err = gpio_pin_setflags(ctx->gpio, flags & ~GPIO_INTR_MASK);
		if (err != 0) {
			device_printf(sc->dev,
			    "Cannot set pin flags for pin 0x%x\n", pin);

			gpio_pin_release(ctx->gpio);
			free(ctx, M_DEVBUF);
			continue;
		}

		ctx->intr_rid = 0;
		ctx->intr_res = gpio_alloc_intr_resource(sc->dev,
		    &ctx->intr_rid, RF_ACTIVE, ctx->gpio,
		    flags & GPIO_INTR_MASK);
		if (ctx->intr_res == NULL) {
			device_printf(sc->dev,
			    "Cannot allocate an IRQ for pin 0x%x\n", pin);

			gpio_pin_release(ctx->gpio);
			free(ctx, M_DEVBUF);
			continue;
		}

		err = bus_setup_intr(sc->dev, ctx->intr_res, INTR_TYPE_MISC |
		    INTR_MPSAFE | INTR_EXCL | INTR_SLEEPABLE, NULL,
		    gpio_aei_intr, ctx, &ctx->intr_cookie);
		if (err != 0) {
			device_printf(sc->dev,
			    "Cannot set up an IRQ for pin 0x%x\n", pin);

			bus_release_resource(sc->dev, ctx->intr_res);
			gpio_pin_release(ctx->gpio);
			free(ctx, M_DEVBUF);
			continue;
		}

		ctx->pin = pin;
		SLIST_INSERT_HEAD(&sc->aei_ctx, ctx, next);
	}

	return (AE_OK);
}

static int
gpio_aei_attach(device_t dev)
{
	struct gpio_aei_softc * sc = device_get_softc(dev);
	ACPI_HANDLE handle;
	ACPI_STATUS status;

	/* This is us. */
	device_set_desc(dev, "ACPI Event Information Device");

	handle = acpi_gpiobus_get_handle(dev);
	status = AcpiGetParent(handle, &sc->dev_handle);
	if (ACPI_FAILURE(status)) {
		device_printf(dev, "Cannot get parent of %s\n",
		    acpi_name(handle));
		return (ENXIO);
	}

	SLIST_INIT(&sc->aei_ctx);
	sc->dev = dev;

	status = AcpiWalkResources(sc->dev_handle, "_AEI",
	    gpio_aei_enumerate, sc);
	if (ACPI_FAILURE(status)) {
		device_printf(dev, "Failed to enumerate AEI resources\n");
		return (ENXIO);
	}

	return (0);
}

static int
gpio_aei_detach(device_t dev)
{
	struct gpio_aei_softc * sc = device_get_softc(dev);
	struct gpio_aei_ctx * ctx, * tctx;

	SLIST_FOREACH_SAFE(ctx, &sc->aei_ctx, next, tctx) {
		bus_teardown_intr(dev, ctx->intr_res, ctx->intr_cookie);
		bus_release_resource(dev, ctx->intr_res);
		gpio_pin_release(ctx->gpio);
		free(ctx, M_DEVBUF);
	}

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
