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

#define TZ_KELVTOC(x)	(((x) - 2732) / 10), (((x) - 2732) % 10)

struct acpi_tz_softc {
    device_t	tz_dev;
    ACPI_HANDLE	tz_handle;
};

static int	acpi_tz_probe(device_t dev);
static int	acpi_tz_attach(device_t dev);

static device_method_t acpi_tz_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_tz_probe),
    DEVMETHOD(device_attach,	acpi_tz_attach),

    {0, 0}
};

static driver_t acpi_tz_driver = {
    "acpi_tz",
    acpi_tz_methods,
    sizeof(struct acpi_tz_softc),
};

devclass_t acpi_tz_devclass;
DRIVER_MODULE(acpi_tz, acpi, acpi_tz_driver, acpi_tz_devclass, 0, 0);

static int
acpi_tz_probe(device_t dev)
{
    if (acpi_get_type(dev) == ACPI_TYPE_THERMAL) {
	device_set_desc(dev, "thermal zone");
	return(0);
    }
    return(ENXIO);
}

static int
acpi_tz_attach(device_t dev)
{
    struct acpi_tz_softc	*sc;
    UINT32			param[4];
    ACPI_BUFFER			buf;
    ACPI_STATUS			status;

    sc = device_get_softc(dev);
    sc->tz_dev = dev;
    sc->tz_handle = acpi_get_handle(dev);

    buf.Pointer = &param[0];
    buf.Length = sizeof(param);
    if ((status = AcpiEvaluateObject(sc->tz_handle, "_TMP", NULL, &buf)) != AE_OK) {
	device_printf(sc->tz_dev, "can't fetch temperature - %s\n", acpi_strerror(status));
	return(ENXIO);
    }
    if (param[0] != ACPI_TYPE_NUMBER) {
	device_printf(sc->tz_dev, "%s._TMP does not evaluate to ACPI_TYPE_NUMBER\n", 
		      acpi_name(sc->tz_handle));
	return(ENXIO);
    }
    device_printf(sc->tz_dev, "current temperature %d.%dC\n", TZ_KELVTOC(param[1]));
    
    return(0);
}
