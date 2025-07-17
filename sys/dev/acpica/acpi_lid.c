/*-
 * Copyright (c) 2000 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2000 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
 * Copyright (c) 2000 Michael Smith <msmith@freebd.org>
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
 */

#include <sys/cdefs.h>
#include "opt_acpi.h"
#include "opt_evdev.h"
#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/proc.h>

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>

#include <dev/acpica/acpivar.h>

#ifdef EVDEV_SUPPORT
#include <dev/evdev/input.h>
#include <dev/evdev/evdev.h>
#endif

/* Hooks for the ACPI CA debugging infrastructure */
#define _COMPONENT	ACPI_BUTTON
ACPI_MODULE_NAME("LID")

struct acpi_lid_softc {
    device_t	lid_dev;
    ACPI_HANDLE	lid_handle;
    int		lid_status;	/* open or closed */
#ifdef EVDEV_SUPPORT
    struct evdev_dev *lid_evdev;
#endif
};

ACPI_HANDLE acpi_lid_handle;

ACPI_SERIAL_DECL(lid, "ACPI lid");

static int	acpi_lid_probe(device_t dev);
static int	acpi_lid_attach(device_t dev);
static int	acpi_lid_suspend(device_t dev);
static int	acpi_lid_resume(device_t dev);
static void	acpi_lid_notify_status_changed(void *arg);
static void 	acpi_lid_notify_handler(ACPI_HANDLE h, UINT32 notify,
					void *context);

static device_method_t acpi_lid_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_lid_probe),
    DEVMETHOD(device_attach,	acpi_lid_attach),
    DEVMETHOD(device_suspend,	acpi_lid_suspend),
    DEVMETHOD(device_resume,	acpi_lid_resume),

    DEVMETHOD_END
};

static driver_t acpi_lid_driver = {
    "acpi_lid",
    acpi_lid_methods,
    sizeof(struct acpi_lid_softc),
};

DRIVER_MODULE(acpi_lid, acpi, acpi_lid_driver, 0, 0);
MODULE_DEPEND(acpi_lid, acpi, 1, 1, 1);

static int
acpi_lid_status_update(struct acpi_lid_softc *sc)
{
	ACPI_STATUS status;
	int lid_status;

	ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

	/*
	 * Evaluate _LID and check the return value, update lid status.
	 *	Zero:		The lid is closed
	 *	Non-zero:	The lid is open
	 */
	status = acpi_GetInteger(sc->lid_handle, "_LID", &lid_status);
	if (ACPI_FAILURE(status))
		lid_status = 1;		/* assume lid is opened */
	else
		lid_status = (lid_status != 0); /* range check value */

	if (sc->lid_status == lid_status)
		return (EALREADY);
	sc->lid_status = lid_status;

	/* Send notification via devd */
	acpi_UserNotify("Lid", sc->lid_handle, sc->lid_status);

#ifdef EVDEV_SUPPORT
	/* Notify evdev about lid status */
	evdev_push_sw(sc->lid_evdev, SW_LID, lid_status ? 0 : 1);
	evdev_sync(sc->lid_evdev);
#endif
	return (0);
}

static int
acpi_lid_probe(device_t dev)
{
    static char *lid_ids[] = { "PNP0C0D", NULL };
    int rv;

    if (acpi_disabled("lid"))
	return (ENXIO);
    rv = ACPI_ID_PROBE(device_get_parent(dev), dev, lid_ids, NULL);
    if (rv <= 0)
	device_set_desc(dev, "Control Method Lid Switch");
    return (rv);
}

static int
acpi_lid_attach(device_t dev)
{
    struct acpi_prw_data	prw;
    struct acpi_lid_softc	*sc;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = device_get_softc(dev);
    sc->lid_dev = dev;
    acpi_lid_handle = sc->lid_handle = acpi_get_handle(dev);
    sc->lid_status = -1;

#ifdef EVDEV_SUPPORT
    /* Register evdev device before initial status update */
    sc->lid_evdev = evdev_alloc();
    evdev_set_name(sc->lid_evdev, device_get_desc(dev));
    evdev_set_phys(sc->lid_evdev, device_get_nameunit(dev));
    evdev_set_id(sc->lid_evdev, BUS_HOST, 0, 0, 1);
    evdev_support_event(sc->lid_evdev, EV_SYN);
    evdev_support_event(sc->lid_evdev, EV_SW);
    evdev_support_sw(sc->lid_evdev, SW_LID);

    if (evdev_register(sc->lid_evdev))
        return (ENXIO);
#endif

    /*
     * If a system does not get lid events, it may make sense to change
     * the type to ACPI_ALL_NOTIFY.  Some systems generate both a wake and
     * runtime notify in that case though.
     */
    AcpiInstallNotifyHandler(sc->lid_handle, ACPI_DEVICE_NOTIFY,
			     acpi_lid_notify_handler, sc);

    /* Enable the GPE for wake/runtime. */
    acpi_wake_set_enable(dev, 1);
    if (acpi_parse_prw(sc->lid_handle, &prw) == 0)
	AcpiEnableGpe(prw.gpe_handle, prw.gpe_bit);

    /* Get the initial lid status */
    acpi_lid_status_update(sc);

    /*
     * Export the lid status
     */
    SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	"state", CTLFLAG_RD, &sc->lid_status, 0,
	"Device state (0 = closed, 1 = open)");

    return (0);
}

static int
acpi_lid_suspend(device_t dev)
{
    return (0);
}

static int
acpi_lid_resume(device_t dev)
{
    struct acpi_lid_softc	*sc;

    sc = device_get_softc(dev);

    /* Update lid status, if any */
    acpi_lid_status_update(sc);

    return (0);
}

static void
acpi_lid_notify_status_changed(void *arg)
{
    struct acpi_lid_softc	*sc;
    struct acpi_softc		*acpi_sc;

    ACPI_FUNCTION_TRACE((char *)(uintptr_t)__func__);

    sc = (struct acpi_lid_softc *)arg;
    ACPI_SERIAL_BEGIN(lid);

    /* Update lid status, if any */
    if (acpi_lid_status_update(sc) != 0)
	goto out;

    acpi_sc = acpi_device_get_parent_softc(sc->lid_dev);
    if (acpi_sc == NULL)
	goto out;

    ACPI_VPRINT(sc->lid_dev, acpi_sc, "Lid %s\n",
		sc->lid_status ? "opened" : "closed");

    if (sc->lid_status == 0)
	EVENTHANDLER_INVOKE(acpi_sleep_event, acpi_sc->acpi_lid_switch_sx);
    else
	EVENTHANDLER_INVOKE(acpi_wakeup_event, acpi_sc->acpi_lid_switch_sx);

out:
    ACPI_SERIAL_END(lid);
    return_VOID;
}

/* XXX maybe not here */
#define	ACPI_NOTIFY_STATUS_CHANGED	0x80

static void 
acpi_lid_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
    struct acpi_lid_softc	*sc;

    ACPI_FUNCTION_TRACE_U32((char *)(uintptr_t)__func__, notify);

    sc = (struct acpi_lid_softc *)context;
    switch (notify) {
    case ACPI_NOTIFY_STATUS_CHANGED:
	AcpiOsExecute(OSL_NOTIFY_HANDLER,
		      acpi_lid_notify_status_changed, sc);
	break;
    default:
	device_printf(sc->lid_dev, "unknown notify %#x\n", notify);
	break;
    }

    return_VOID;
}
