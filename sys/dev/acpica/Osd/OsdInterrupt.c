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

/*
 * 6.5 : Interrupt handling
 */

#include "acpi.h"

#include <sys/bus.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
 
#include <dev/acpica/acpivar.h>

#define _COMPONENT	ACPI_OS_SERVICES
MODULE_NAME("INTERRUPT")

static void		InterruptWrapper(void *arg);
static OSD_HANDLER	InterruptHandler;

/*
 * XXX this does not correctly free resources in the case of partically successful
 * attachment.
 */
ACPI_STATUS
AcpiOsInstallInterruptHandler(UINT32 InterruptNumber, OSD_HANDLER ServiceRoutine, void *Context)
{
    struct acpi_softc	*sc;

    FUNCTION_TRACE(__func__);

    if ((sc = devclass_get_softc(devclass_find("acpi"), 0)) == NULL)
	panic("can't find ACPI device to register interrupt");
    if (sc->acpi_dev == NULL)
	panic("acpi softc has invalid device");

    if ((InterruptNumber < 0) || (InterruptNumber > 255))
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    if (ServiceRoutine == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    if (InterruptHandler != NULL && InterruptHandler != ServiceRoutine) {
	device_printf(sc->acpi_dev, "can't register more than one ACPI interrupt\n");
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    }
    InterruptHandler = ServiceRoutine;

    /*
     * This isn't strictly true, as we ought to be able to handle > 1 interrupt.  The ACPI
     * spec doesn't call for this though.
     */
    if (sc->acpi_irq != NULL) {
	device_printf(sc->acpi_dev, "attempt to register more than one interrupt handler\n");
	return_ACPI_STATUS(AE_ALREADY_EXISTS);
    }
    sc->acpi_irq_rid = 0;
    bus_set_resource(sc->acpi_dev, SYS_RES_IRQ, 0, InterruptNumber, 1);
    if ((sc->acpi_irq = bus_alloc_resource(sc->acpi_dev, SYS_RES_IRQ, &sc->acpi_irq_rid, 0, ~0, 1, 
					   RF_SHAREABLE | RF_ACTIVE)) == NULL) {
	device_printf(sc->acpi_dev, "could not allocate SCI interrupt\n");
	return_ACPI_STATUS(AE_ALREADY_EXISTS);
    }
    if (bus_setup_intr(sc->acpi_dev, sc->acpi_irq, INTR_TYPE_MISC, (driver_intr_t *)InterruptWrapper,
		       Context, &sc->acpi_irq_handle)) {
	device_printf(sc->acpi_dev, "could not set up SCI interrupt\n");
	return_ACPI_STATUS(AE_ALREADY_EXISTS);
    }
	
    return_ACPI_STATUS(AE_OK);
}

ACPI_STATUS
AcpiOsRemoveInterruptHandler (UINT32 InterruptNumber, OSD_HANDLER ServiceRoutine)
{
    struct acpi_softc	*sc;

    FUNCTION_TRACE(__func__);

    if ((InterruptNumber < 0) || (InterruptNumber > 255))
	return_ACPI_STATUS(AE_BAD_PARAMETER);
    if (ServiceRoutine == NULL)
	return_ACPI_STATUS(AE_BAD_PARAMETER);

    if ((sc = devclass_get_softc(devclass_find("acpi"), 0)) == NULL)
	panic("can't find ACPI device to deregister interrupt");

    if (sc->acpi_irq == NULL)
	return_ACPI_STATUS(AE_NOT_EXIST);

    bus_teardown_intr(sc->acpi_dev, sc->acpi_irq, sc->acpi_irq_handle);
    bus_release_resource(sc->acpi_dev, SYS_RES_IRQ, 0, sc->acpi_irq);
    bus_delete_resource(sc->acpi_dev, SYS_RES_IRQ, 0);

    sc->acpi_irq = NULL;

    return_ACPI_STATUS(AE_OK);
}

/*
 * Interrupt handler wrapper.
 */
static void
InterruptWrapper(void *arg)
{
    ACPI_LOCK;
    InterruptHandler(arg);
    ACPI_UNLOCK;
}
