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
#define _COMPONENT	ACPI_THERMAL_ZONE
MODULE_NAME("THERMAL")

#define TZ_ZEROC	2732
#define TZ_KELVTOC(x)	(((x) - TZ_ZEROC) / 10), (((x) - TZ_ZEROC) % 10)

struct acpi_tz_softc {
    device_t	tz_dev;
    ACPI_HANDLE	tz_handle;
};

static int	acpi_tz_probe(device_t dev);
static int	acpi_tz_attach(device_t dev);
static void	acpi_tz_check_tripping_point(void *context);
static void	acpi_tz_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context);

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

    FUNCTION_TRACE(__func__);

    if ((acpi_get_type(dev) == ACPI_TYPE_THERMAL) &&
	!acpi_disabled("thermal")) {
	device_set_desc(dev, "thermal zone");
	return_VALUE(0);
    }
    return_VALUE(ENXIO);
}

static int
acpi_tz_attach(device_t dev)
{
    struct acpi_tz_softc	*sc;
    ACPI_STATUS			status;

    FUNCTION_TRACE(__func__);

    sc = device_get_softc(dev);
    sc->tz_dev = dev;
    sc->tz_handle = acpi_get_handle(dev);

    AcpiInstallNotifyHandler(sc->tz_handle, ACPI_DEVICE_NOTIFY, 
			     acpi_tz_notify_handler, dev);

    /*
     * Don't bother evaluating/printing the temperature at this point;
     * on many systems it'll be bogus until the EC is running.
     */
    return_VALUE(0);
}

static void
acpi_tz_check_tripping_point(void *context)
{
    struct acpi_tz_softc	*sc;
    device_t			dev = context;
    ACPI_STATUS			status;
    int				tp;

    FUNCTION_TRACE(__func__);

    sc = device_get_softc(dev);
    if ((status =  acpi_EvaluateInteger(sc->tz_handle, "_TMP", &tp)) != AE_OK) {
	device_printf(dev, "can't evaluate _TMP method - %s\n", acpi_strerror(status));
	return_VOID;
    }
    
    device_printf(dev,"%d.%dC\n", TZ_KELVTOC(tp));
    return_VOID;
}

#define ACPI_TZ_STATUS_CHANGE 0x80
#define ACPI_TZ_TRIPPOINT_CHANGE 0x81
static void
acpi_tz_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    FUNCTION_TRACE(__func__);

    switch(notify){
    case ACPI_TZ_STATUS_CHANGE:
    case ACPI_TZ_TRIPPOINT_CHANGE:
	/*Check trip point*/
	AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_tz_check_tripping_point, context);
	break;	
    }
    return_VOID;
}






