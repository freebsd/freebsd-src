/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Poul-Henning Kamp <phk@FreeBSD.org>
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#include "opt_acpi.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/proc.h>
#include <sys/rman.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>
#include <dev/iicbus/iiconf.h>

#include <dev/iicbus/controller/qcom/geni_iic_var.h>

static int	geniiic_acpi_probe(device_t dev);
static int	geniiic_acpi_attach(device_t dev);
static int	geniiic_acpi_detach(device_t dev);

static char *geniiic_ids[] = {
	"QCOM0C10",
	NULL
};

static int
geniiic_acpi_probe(device_t dev)
{
	int rv;

	if (acpi_disabled("geniiic"))
		return (ENXIO);
	rv = ACPI_ID_PROBE(device_get_parent(dev), dev, geniiic_ids, NULL);
	if (rv > 0)
		return (rv);

	device_set_desc(dev, "Qualcomm GENI I2C Controller");
	return (rv);
}

static int
geniiic_acpi_attach(device_t dev)
{
	geniiic_softc_t	*sc;
	char *str;
	int error;

	sc = device_get_softc(dev);

	sc->dev = dev;
	error = ACPI_ID_PROBE(device_get_parent(dev), dev, geniiic_ids, &str);
	if (error > 0)
		return (error);

	sc->regs_rid = 0;
	sc->regs_res = bus_alloc_resource_any(dev,
	    SYS_RES_MEMORY, &sc->regs_rid, RF_ACTIVE);
	if (sc->regs_res == NULL) {
		device_printf(dev, "unable to map registers\n");
		geniiic_acpi_detach(dev);
		return (ENXIO);
	}
	sc->intr_rid = 0;
	sc->intr_res = bus_alloc_resource_any(dev,
	    SYS_RES_IRQ, &sc->intr_rid, RF_SHAREABLE | RF_ACTIVE);
	if (sc->intr_res == NULL) {
		device_printf(dev, "unable to map interrupt\n");
		geniiic_acpi_detach(dev);
		return (ENXIO);
	}
	sc->platform_attached = true;

	error = geniiic_attach(sc);
	if (error)
		geniiic_acpi_detach(dev);

	return (error);
}

static int
geniiic_acpi_detach(device_t dev)
{
	geniiic_softc_t *sc = device_get_softc(dev);
	int error;

	if (sc->platform_attached) {
		error = geniiic_detach(sc);
		if (error)
			return (error);
		sc->platform_attached = false;
	}

	if (sc->intr_res) {
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->intr_rid, sc->intr_res);
		sc->intr_res = NULL;
	}
	if (sc->regs_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->regs_rid, sc->regs_res);
		sc->regs_res = NULL;
	}

	return (0);
}

static int
geniiic_acpi_suspend(device_t dev)
{
	geniiic_softc_t *sc = device_get_softc(dev);

	return (geniiic_suspend(sc));
}

static int
geniiic_acpi_resume(device_t dev)
{
	geniiic_softc_t *sc  = device_get_softc(dev);

	return (geniiic_resume(sc));
}

static device_method_t geniiic_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, geniiic_acpi_probe),
	DEVMETHOD(device_attach, geniiic_acpi_attach),
	DEVMETHOD(device_detach, geniiic_acpi_detach),
	DEVMETHOD(device_suspend, geniiic_acpi_suspend),
	DEVMETHOD(device_resume, geniiic_acpi_resume),

	/* Bus interface */
	DEVMETHOD(bus_setup_intr, bus_generic_setup_intr),
	DEVMETHOD(bus_teardown_intr, bus_generic_teardown_intr),
	DEVMETHOD(bus_alloc_resource, bus_generic_alloc_resource),
	DEVMETHOD(bus_release_resource, bus_generic_release_resource),
	DEVMETHOD(bus_activate_resource, bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource, bus_generic_deactivate_resource),
	DEVMETHOD(bus_adjust_resource, bus_generic_adjust_resource),

	/* iicbus interface */
	DEVMETHOD(iicbus_transfer, geniiic_transfer),
	DEVMETHOD(iicbus_reset, geniiic_reset),
	DEVMETHOD(iicbus_callback, geniiic_callback),

	DEVMETHOD_END
};

static driver_t geniiic_acpi_driver = {
	"geniiic",
	geniiic_acpi_methods,
	sizeof(struct geniiic_softc),
};

DRIVER_MODULE_ORDERED(geniiic, acpi, geniiic_acpi_driver, 0, 0, SI_ORDER_ANY);
MODULE_DEPEND(geniiic, acpi, 1, 1, 1);
ACPI_PNP_INFO(geniiic_ids);
