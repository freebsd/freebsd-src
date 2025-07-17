/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Amazon.com, Inc. or its affiliates.
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

#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/intr.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/gpio/gpiobusvar.h>

#include "pl061.h"

static char *gpio_ids[] = { "ARMH0061", NULL };

static int
pl061_acpi_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("gpio"))
		return (ENXIO);

	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, gpio_ids, NULL);

	if (rv <= 0)
		device_set_desc(dev, "Arm PL061 GPIO Controller");

	return (rv);
}

static int
pl061_acpi_attach(device_t dev)
{
	struct pl061_softc *sc;

	sc = device_get_softc(dev);
	sc->sc_xref = ACPI_GPIO_XREF;

	return (pl061_attach(dev));
}

static device_method_t pl061_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pl061_acpi_probe),
	DEVMETHOD(device_attach,	pl061_acpi_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gpio, pl061_acpi_driver, pl061_acpi_methods,
    sizeof(struct pl061_softc), pl061_driver);

EARLY_DRIVER_MODULE(pl061, acpi, pl061_acpi_driver, NULL, NULL,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
MODULE_DEPEND(pl061, acpi, 1, 1, 1);
MODULE_DEPEND(pl061, gpiobus, 1, 1, 1);
