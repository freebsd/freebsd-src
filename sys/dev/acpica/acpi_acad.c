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

struct  acpi_acad_softc {
	int status;
};

static void
acpi_acad_get_status(void *context)
{
	device_t dev = context;
	struct acpi_acad_softc *sc = device_get_softc(dev);
	ACPI_HANDLE h = acpi_get_handle(dev);

	if (acpi_EvaluateInteger(h, "_PSR", &sc->status) != AE_OK)
		return;
	device_printf(dev,"%s\n",(sc->status) ? "On Line" : "Off Line");
}

static void
acpi_acad_notify_handler(ACPI_HANDLE h, UINT32 notify, void *context)
{
	device_t dev = context;

	device_printf(dev, "Notify %d\n", notify);
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

	ACPI_HANDLE handle = acpi_get_handle(dev);
	AcpiInstallNotifyHandler(handle, 
				 ACPI_DEVICE_NOTIFY,
				 acpi_acad_notify_handler, dev);
	/*Installing system notify is not so good*/
	AcpiInstallNotifyHandler(handle, 
				 ACPI_SYSTEM_NOTIFY,
				 acpi_acad_notify_handler, dev);

	acpi_acad_get_status((void *)dev);

	error = acpi_register_ioctl(ACPIIO_ACAD_GET_STATUS, acpi_acad_ioctl,
	    device_get_softc(dev));
	if (error)
		return (error);

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
	struct	 acpi_acad_softc *sc;

	sc = (struct acpi_acad_softc *)arg;
	if (sc == NULL) {
		return(ENXIO);
	}

	switch (cmd) {
	case ACPIIO_ACAD_GET_STATUS:
		*(int *)addr = sc->status;
		break;
	}

	return(0);
}
