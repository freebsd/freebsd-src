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

devclass_t acpi_button_devclass;
DRIVER_MODULE(acpi_button, acpi, acpi_button_driver, acpi_button_devclass, 0, 0);

static int
acpi_button_probe(device_t dev)
{
    struct acpi_button_softc	*sc;

    sc = device_get_softc(dev);
    if (acpi_get_type(dev) == ACPI_TYPE_DEVICE) {
	if (acpi_MatchHid(dev, "PNP0C0C")) {
	    device_set_desc(dev, "Control Method Power Button Device");
	    sc->button_type = ACPI_POWER_BUTTON;
	    return(0);
	}
	if (acpi_MatchHid(dev, "PNP0C0E")) {
	    device_set_desc(dev, "Control Method Sleep Button Device");
	    sc->button_type = ACPI_SLEEP_BUTTON;
	    return(0);
	}
	return(ENXIO);
    }
    return(ENXIO);
}

static int
acpi_button_attach(device_t dev)
{
    struct acpi_button_softc	*sc;
    ACPI_STATUS			status;

    sc = device_get_softc(dev);
    sc->button_dev = dev;
    sc->button_handle = acpi_get_handle(dev);

    if ((status = AcpiInstallNotifyHandler(sc->button_handle, ACPI_DEVICE_NOTIFY, 
					   acpi_button_notify_handler, sc)) != AE_OK) {
	device_printf(sc->button_dev, "couldn't install Notify handler - %s\n", acpi_strerror(status));
	return(ENXIO);
    }
    return(0);
}

static void
acpi_button_notify_pressed_for_sleep(void *arg)
{
    struct acpi_button_softc	*sc;
    struct acpi_softc		*acpi_sc;

    sc = (struct acpi_button_softc *)arg;
    acpi_sc = acpi_device_get_parent_softc(sc->button_dev);
    if (acpi_sc == NULL) {
	return;
    }

    switch (sc->button_type) {
    case ACPI_POWER_BUTTON:
	acpi_eventhandler_power_button_for_sleep((void *)acpi_sc);
	break;
    case ACPI_SLEEP_BUTTON:
	acpi_eventhandler_sleep_button_for_sleep((void *)acpi_sc);
	break;
    default:
	return;		/* unknown button type */
    }
}

static void
acpi_button_notify_pressed_for_wakeup(void *arg)
{
    struct acpi_button_softc	*sc;
    struct acpi_softc		*acpi_sc;

    sc = (struct acpi_button_softc *)arg;
    acpi_sc = acpi_device_get_parent_softc(sc->button_dev);
    if (acpi_sc == NULL) {
	return;
    }

    switch (sc->button_type) {
    case ACPI_POWER_BUTTON:
	acpi_eventhandler_power_button_for_wakeup((void *)acpi_sc);
	break;
    case ACPI_SLEEP_BUTTON:
	acpi_eventhandler_sleep_button_for_wakeup((void *)acpi_sc);
	break;
    default:
	return;		/* unknown button type */
    }
}

/* XXX maybe not here */
#define ACPI_NOTIFY_BUTTON_PRESSED_FOR_SLEEP	0x80
#define ACPI_NOTIFY_BUTTON_PRESSED_FOR_WAKEUP	0x02

static void 
acpi_button_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_button_softc	*sc = (struct acpi_button_softc *)context;

    switch (notify) {
    case ACPI_NOTIFY_BUTTON_PRESSED_FOR_SLEEP:
	AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_button_notify_pressed_for_sleep, sc);
	device_printf(sc->button_dev, "pressed for sleep, button type: %d\n", sc->button_type);
	break;   
    case ACPI_NOTIFY_BUTTON_PRESSED_FOR_WAKEUP:
	AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_button_notify_pressed_for_wakeup, sc);
	device_printf(sc->button_dev, "pressed for wakeup, button type: %d\n", sc->button_type);
	break;   
    default:
	return;		/* unknown notification value */
    }
}


