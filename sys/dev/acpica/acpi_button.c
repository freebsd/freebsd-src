/*-
 * Copyright (c) 2000 Mitsaru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2000 Michael Smith <msmith@freebsd.org>
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
#define _COMPONENT	ACPI_BUTTON
ACPI_MODULE_NAME("BUTTON")

struct acpi_button_softc {
    device_t	button_dev;
    ACPI_HANDLE	button_handle;
#define ACPI_POWER_BUTTON	0
#define ACPI_SLEEP_BUTTON	1
    boolean_t	button_type;	/* Power or Sleep Button */
};

static int	acpi_button_probe(device_t dev);
static int	acpi_button_attach(device_t dev);
static void 	acpi_button_notify_handler(ACPI_HANDLE h,UINT32 notify, void *context);
static void	acpi_button_notify_pressed_for_sleep(void *arg);
static void	acpi_button_notify_pressed_for_wakeup(void *arg);

static device_method_t acpi_button_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_button_probe),
    DEVMETHOD(device_attach,	acpi_button_attach),

    {0, 0}
};

static driver_t acpi_button_driver = {
    "acpi_button",
    acpi_button_methods,
    sizeof(struct acpi_button_softc),
};

static devclass_t acpi_button_devclass;
DRIVER_MODULE(acpi_button, acpi, acpi_button_driver, acpi_button_devclass, 0, 0);

static int
acpi_button_probe(device_t dev)
{
    struct acpi_button_softc	*sc;

    sc = device_get_softc(dev);
    if (acpi_get_type(dev) == ACPI_TYPE_DEVICE) {
	if (!acpi_disabled("button")) {
	    if (acpi_MatchHid(dev, "PNP0C0C")) {
		device_set_desc(dev, "Power Button");
		sc->button_type = ACPI_POWER_BUTTON;
		return(0);
	    }
	    if (acpi_MatchHid(dev, "PNP0C0E")) {
		device_set_desc(dev, "Sleep Button");
		sc->button_type = ACPI_SLEEP_BUTTON;
		return(0);
	    }
	}
    }
    return(ENXIO);
}

static int
acpi_button_attach(device_t dev)
{
    struct acpi_button_softc	*sc;
    ACPI_STATUS			status;

    ACPI_FUNCTION_TRACE(__func__);

    sc = device_get_softc(dev);
    sc->button_dev = dev;
    sc->button_handle = acpi_get_handle(dev);

    if (ACPI_FAILURE(status = AcpiInstallNotifyHandler(sc->button_handle, ACPI_DEVICE_NOTIFY, 
					   acpi_button_notify_handler, sc))) {
	device_printf(sc->button_dev, "couldn't install Notify handler - %s\n", AcpiFormatException(status));
	return_VALUE(ENXIO);
    }
    return_VALUE(0);
}

static void
acpi_button_notify_pressed_for_sleep(void *arg)
{
    struct acpi_button_softc	*sc;
    struct acpi_softc		*acpi_sc;

    ACPI_FUNCTION_TRACE(__func__);

    sc = (struct acpi_button_softc *)arg;
    acpi_sc = acpi_device_get_parent_softc(sc->button_dev);
    if (acpi_sc == NULL) {
	return_VOID;
    }

    switch (sc->button_type) {
    case ACPI_POWER_BUTTON:
	ACPI_VPRINT(sc->button_dev, acpi_sc,
	    "power button pressed\n", sc->button_type);
	acpi_eventhandler_power_button_for_sleep((void *)acpi_sc);
	break;
    case ACPI_SLEEP_BUTTON:
	ACPI_VPRINT(sc->button_dev, acpi_sc,
	    "sleep button pressed\n", sc->button_type);
	acpi_eventhandler_sleep_button_for_sleep((void *)acpi_sc);
	break;
    default:
	break;		/* unknown button type */
    }
    return_VOID;
}

static void
acpi_button_notify_pressed_for_wakeup(void *arg)
{
    struct acpi_button_softc	*sc;
    struct acpi_softc		*acpi_sc;

    ACPI_FUNCTION_TRACE(__func__);

    sc = (struct acpi_button_softc *)arg;
    acpi_sc = acpi_device_get_parent_softc(sc->button_dev);
    if (acpi_sc == NULL) {
	return_VOID;
    }

    switch (sc->button_type) {
    case ACPI_POWER_BUTTON:
	ACPI_VPRINT(sc->button_dev, acpi_sc,
	    "wakeup by power button\n", sc->button_type);
	acpi_eventhandler_power_button_for_wakeup((void *)acpi_sc);
	break;
    case ACPI_SLEEP_BUTTON:
	ACPI_VPRINT(sc->button_dev, acpi_sc,
	    "wakeup by sleep button\n", sc->button_type);
	acpi_eventhandler_sleep_button_for_wakeup((void *)acpi_sc);
	break;
    default:
	break;		/* unknown button type */
    }
    return_VOID;
}

/* XXX maybe not here */
#define ACPI_NOTIFY_BUTTON_PRESSED_FOR_SLEEP	0x80
#define ACPI_NOTIFY_BUTTON_PRESSED_FOR_WAKEUP	0x02

static void 
acpi_button_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_button_softc	*sc = (struct acpi_button_softc *)context;

    ACPI_FUNCTION_TRACE_U32(__func__, notify);

    switch (notify) {
    case ACPI_NOTIFY_BUTTON_PRESSED_FOR_SLEEP:
	AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_button_notify_pressed_for_sleep, sc);
	break;   
    case ACPI_NOTIFY_BUTTON_PRESSED_FOR_WAKEUP:
	AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_button_notify_pressed_for_wakeup, sc);
	break;   
    default:
	break;		/* unknown notification value */
    }
    return_VOID;
}


