/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Andrew Turner
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/gpio/gpiobusvar.h>

#include "pl061.h"

static int
pl061_fdt_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "arm,pl061"))
		return (ENXIO);

	device_set_desc(dev, "Arm PL061 GPIO Controller");

	return (BUS_PROBE_DEFAULT);
}

static int
pl061_fdt_attach(device_t dev)
{
	int error;

	error = pl061_attach(dev);
	if (error != 0)
		return (error);

	if (!intr_pic_register(dev, OF_xref_from_node(ofw_bus_get_node(dev)))) {
		device_printf(dev, "couldn't register PIC\n");
		pl061_detach(dev);
		error = ENXIO;
	}

	return (error);
}

static device_method_t pl061_fdt_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		pl061_fdt_probe),
	DEVMETHOD(device_attach,	pl061_fdt_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gpio, pl061_fdt_driver, pl061_fdt_methods,
    sizeof(struct pl061_softc), pl061_driver);

EARLY_DRIVER_MODULE(pl061, ofwbus, pl061_fdt_driver, NULL, NULL,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_LATE);
MODULE_DEPEND(pl061, gpiobus, 1, 1, 1);
