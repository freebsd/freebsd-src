/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
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
 *
 *	$FreeBSD$
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include "acpi.h"

#include <dev/acpica/acpivar.h>

/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_SYSTEM
MODULE_NAME("TIMER")

#define ACPITIMER_MAGIC 0x524d4954	/* "TIMR" */

struct acpi_timer_softc {
    device_t	tm_dev;
};

static void	acpi_timer_identify(driver_t *driver, device_t parent);
static int	acpi_timer_probe(device_t dev);
static int	acpi_timer_attach(device_t dev);

static device_method_t acpi_timer_methods[] = {
    /* Device interface */
    DEVMETHOD(device_identify,	acpi_timer_identify),
    DEVMETHOD(device_probe,	acpi_timer_probe),
    DEVMETHOD(device_attach,	acpi_timer_attach),

    {0, 0}
};

static driver_t acpi_timer_driver = {
    "acpi_timer",
    acpi_timer_methods,
    sizeof(struct acpi_timer_softc),
};

devclass_t acpi_timer_devclass;
DRIVER_MODULE(acpi_timer, acpi, acpi_timer_driver, acpi_timer_devclass, 0, 0);

static void
acpi_timer_identify(driver_t *driver, device_t parent)
{
    device_t			dev;
    char			desc[40];

    FUNCTION_TRACE(__func__);

    if (acpi_disabled("timer"))
	return_VOID;

    if (AcpiGbl_FADT == NULL)
	return_VOID;
    
    if ((dev = BUS_ADD_CHILD(parent, 0, "acpi_timer", 0)) == NULL) {
	device_printf(parent, "could not add acpi_timer0\n");
	return_VOID;
    }
    if (acpi_set_magic(dev, ACPITIMER_MAGIC)) {
	device_printf(dev, "could not set magic\n");
	return_VOID;
    }

    sprintf(desc, "%d-bit timer at 3.579545MHz", AcpiGbl_FADT->TmrValExt ? 32 : 24);
    device_set_desc_copy(dev, desc);

    return_VOID;
}

static int
acpi_timer_probe(device_t dev)
{
    if (acpi_get_magic(dev) == ACPITIMER_MAGIC)
	return(0);
    return(ENXIO);
}

static int
acpi_timer_attach(device_t dev)
{
    struct acpi_timer_softc		*sc;

    FUNCTION_TRACE(__func__);

    sc = device_get_softc(dev);
    sc->tm_dev = dev;

    return_VALUE(0);
}
