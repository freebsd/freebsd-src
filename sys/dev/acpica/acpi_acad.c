/*-
 * Copyright (c) 2000 Takanori Watanabe
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
 * $FreeBSD$
 */

#include "opt_acpi.h"
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/ioccom.h>
#include <sys/malloc.h>
#include <sys/conf.h>

#include  "acpi.h"
#include <dev/acpica/acpivar.h>
#include <dev/acpica/acpiio.h>
 
/*
 * Hooks for the ACPI CA debugging infrastructure
 */
#define _COMPONENT	ACPI_AC_ADAPTER
MODULE_NAME("AC_ADAPTER")

#define ACPI_DEVICE_CHECK_PNP		0x00
#define ACPI_DEVICE_CHECK_EXISTENCE	0x01
#define ACPI_POWERSOURCE_STAT_CHANGE	0x80

static void acpi_acad_get_status(void * );
static void acpi_acad_notify_handler(ACPI_HANDLE , UINT32 ,void *);
static int acpi_acad_probe(device_t);
static int acpi_acad_attach(device_t);
static int acpi_acad_ioctl(u_long, caddr_t, void *);
static int acpi_acad_sysctl(SYSCTL_HANDLER_ARGS);

struct  acpi_acad_softc {
	int status;
};

static void
acpi_acad_get_status(void *context)
{
	int newstatus;
	device_t dev = context;
	struct acpi_acad_softc *sc = device_get_softc(dev);
	ACPI_HANDLE h = acpi_get_handle(dev);

	if (acpi_EvaluateInteger(h, "_PSR", &newstatus) != AE_OK) {
		sc->status = -1;
		return;
	}

	if (sc->status != newstatus) {
		sc->status = newstatus;
		/* set system power profile based on AC adapter status */
		powerprofile_set_state(sc->status ? POWERPROFILE_PERFORMANCE : POWERPROFILE_ECONOMY);
		ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
		    "%s Line\n",(sc->status) ? "On" : "Off");
	}
}

static void
acpi_acad_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
	device_t dev = context;

	ACPI_VPRINT(dev, acpi_device_get_parent_softc(dev),
	    "Notify %d\n", notify);

	switch (notify) {
	case ACPI_DEVICE_CHECK_PNP:
	case ACPI_DEVICE_CHECK_EXISTENCE:
	case ACPI_POWERSOURCE_STAT_CHANGE:
		/*Temporally. It is better to notify policy manager*/
		AcpiOsQueueForExecution(OSD_PRIORITY_LO,
		    acpi_acad_get_status,context);
		break;
	default:
		break;
	}
}

static int
acpi_acad_probe(device_t dev)
{

    if ((acpi_get_type(dev) == ACPI_TYPE_DEVICE) &&
	acpi_MatchHid(dev, "ACPI0003")) {
      
      /*
       * Set device description 
       */
	device_set_desc(dev, "AC adapter");
	return(0);
    }
    return(ENXIO);
}

static int
acpi_acad_attach(device_t dev)
{
	int	 error;
	struct acpi_acad_softc	*sc;
	struct acpi_softc	*acpi_sc;

	ACPI_HANDLE handle = acpi_get_handle(dev);
	AcpiInstallNotifyHandler(handle, 
				 ACPI_DEVICE_NOTIFY,
				 acpi_acad_notify_handler, dev);
	/*Installing system notify is not so good*/
	AcpiInstallNotifyHandler(handle, 
				 ACPI_SYSTEM_NOTIFY,
				 acpi_acad_notify_handler, dev);

	if ((sc = device_get_softc(dev)) == NULL) {
		return (ENXIO);
	}
	if ((error = acpi_register_ioctl(ACPIIO_ACAD_GET_STATUS,
			acpi_acad_ioctl, dev)) != 0) {
		return (error);
	}

	if (device_get_unit(dev) == 0) {
		acpi_sc = acpi_device_get_parent_softc(dev);
		SYSCTL_ADD_PROC(&acpi_sc->acpi_sysctl_ctx,
			SYSCTL_CHILDREN(acpi_sc->acpi_sysctl_tree),
			OID_AUTO, "acline", CTLTYPE_INT | CTLFLAG_RD,
			&sc->status, 0, acpi_acad_sysctl, "I", "");
	}

	/* Get initial status after whole system is up. */
	sc->status = -1;
	AcpiOsQueueForExecution(OSD_PRIORITY_LO, acpi_acad_get_status, dev);

	return(0);
}

static device_method_t acpi_acad_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,	acpi_acad_probe),
    DEVMETHOD(device_attach,	acpi_acad_attach),

    {0, 0}
};

static driver_t acpi_acad_driver = {
    "acpi_acad",
    acpi_acad_methods,
    sizeof(struct acpi_acad_softc),
};

static devclass_t acpi_acad_devclass;
DRIVER_MODULE(acpi_acad,acpi,acpi_acad_driver,acpi_acad_devclass,0,0);

static int
acpi_acad_ioctl(u_long cmd, caddr_t addr, void *arg)
{
	device_t		 dev;
	struct acpi_acad_softc	*sc;

	dev = (device_t)arg;
	if ((sc = device_get_softc(dev)) == NULL) {
		return(ENXIO);
	}

	switch (cmd) {
	case ACPIIO_ACAD_GET_STATUS:
		acpi_acad_get_status(dev);
		*(int *)addr = sc->status;
		break;
	}

	return(0);
}

static int
acpi_acad_sysctl(SYSCTL_HANDLER_ARGS)
{
	int     val;
	int     error;

	if (acpi_acad_get_acline(&val)) {
		return (ENXIO);
	}

	val = *(u_int *)oidp->oid_arg1;
	error = sysctl_handle_int(oidp, &val, 0, req);
	return (error);
}

/*
 * Public interfaces.
 */

int
acpi_acad_get_acline(int *status)
{
	device_t		 dev;
	struct acpi_acad_softc	*sc;

	if ((dev = devclass_get_device(acpi_acad_devclass, 0)) == NULL) {
		return (ENXIO);
	}

	if ((sc = device_get_softc(dev)) == NULL) {
		return (ENXIO);
	}

	acpi_acad_get_status(dev);
	*status = sc->status;

	return (0);
}

