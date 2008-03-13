/*-
 * Copyright (c) 2001 Mitsuru IWASAKI
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <contrib/dev/acpica/acpi.h>
#include <dev/acpica/acpivar.h>

#include <machine/nexusvar.h>

static int intr_model = ACPI_INTR_PIC;

int
acpi_machdep_init(device_t dev)
{
	struct	acpi_softc *sc;

	sc = devclass_get_softc(devclass_find("acpi"), 0);
	acpi_install_wakeup_handler(sc);

	if (intr_model != ACPI_INTR_PIC)
		acpi_SetIntrModel(intr_model);

	return (0);
}

void
acpi_SetDefaultIntrModel(int model)
{

	intr_model = model;
}

int
acpi_machdep_quirks(int *quirks)
{
	return (0);
}

void
acpi_cpu_c1()
{
	__asm __volatile("sti; hlt");
}

/*
 * ACPI nexus(4) driver.
 */
static int
nexus_acpi_probe(device_t dev)
{
	int error;

	error = acpi_identify();
	if (error)
		return (error);

	return (BUS_PROBE_DEFAULT);
}

static int
nexus_acpi_attach(device_t dev)
{

	nexus_init_resources();
	bus_generic_probe(dev);
	if (BUS_ADD_CHILD(dev, 10, "acpi", 0) == NULL)
		panic("failed to add acpi0 device");

	return (bus_generic_attach(dev));
}

static device_method_t nexus_acpi_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nexus_acpi_probe),
	DEVMETHOD(device_attach,	nexus_acpi_attach),

	{ 0, 0 }
};

DEFINE_CLASS_1(nexus, nexus_acpi_driver, nexus_acpi_methods, 1, nexus_driver);
static devclass_t nexus_devclass;

DRIVER_MODULE(nexus_acpi, root, nexus_acpi_driver, nexus_devclass, 0, 0);
