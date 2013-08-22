/*******************************************************************************
 *
 * Module Name: evsci - System Control Interrupt configuration and
 *                      legacy to ACPI mode state transition functions
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acevents.h>


#define _COMPONENT          ACPI_EVENTS
        ACPI_MODULE_NAME    ("evsci")

#if (!ACPI_REDUCED_HARDWARE) /* Entire module */

/* Local prototypes */

static UINT32 ACPI_SYSTEM_XFACE
AcpiEvSciXruptHandler (
    void                    *Context);


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvSciXruptHandler
 *
 * PARAMETERS:  Context   - Calling Context
 *
 * RETURN:      Status code indicates whether interrupt was handled.
 *
 * DESCRIPTION: Interrupt handler that will figure out what function or
 *              control method to call to deal with a SCI.
 *
 ******************************************************************************/

static UINT32 ACPI_SYSTEM_XFACE
AcpiEvSciXruptHandler (
    void                    *Context)
{
    ACPI_GPE_XRUPT_INFO     *GpeXruptList = Context;
    UINT32                  InterruptHandled = ACPI_INTERRUPT_NOT_HANDLED;


    ACPI_FUNCTION_TRACE (EvSciXruptHandler);


    /*
     * We are guaranteed by the ACPI CA initialization/shutdown code that
     * if this interrupt handler is installed, ACPI is enabled.
     */

    /*
     * Fixed Events:
     * Check for and dispatch any Fixed Events that have occurred
     */
    InterruptHandled |= AcpiEvFixedEventDetect ();

    /*
     * General Purpose Events:
     * Check for and dispatch any GPEs that have occurred
     */
    InterruptHandled |= AcpiEvGpeDetect (GpeXruptList);

    AcpiSciCount++;
    return_UINT32 (InterruptHandled);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvGpeXruptHandler
 *
 * PARAMETERS:  Context   - Calling Context
 *
 * RETURN:      Status code indicates whether interrupt was handled.
 *
 * DESCRIPTION: Handler for GPE Block Device interrupts
 *
 ******************************************************************************/

UINT32 ACPI_SYSTEM_XFACE
AcpiEvGpeXruptHandler (
    void                    *Context)
{
    ACPI_GPE_XRUPT_INFO     *GpeXruptList = Context;
    UINT32                  InterruptHandled = ACPI_INTERRUPT_NOT_HANDLED;


    ACPI_FUNCTION_TRACE (EvGpeXruptHandler);


    /*
     * We are guaranteed by the ACPI CA initialization/shutdown code that
     * if this interrupt handler is installed, ACPI is enabled.
     */

    /* GPEs: Check for and dispatch any GPEs that have occurred */

    InterruptHandled |= AcpiEvGpeDetect (GpeXruptList);

    return_UINT32 (InterruptHandled);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiEvInstallSciHandler
 *
 * PARAMETERS:  none
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Installs SCI handler.
 *
 ******************************************************************************/

UINT32
AcpiEvInstallSciHandler (
    void)
{
    UINT32                  Status = AE_OK;


    ACPI_FUNCTION_TRACE (EvInstallSciHandler);


    Status = AcpiOsInstallInterruptHandler ((UINT32) AcpiGbl_FADT.SciInterrupt,
                AcpiEvSciXruptHandler, AcpiGbl_GpeXruptListHead);
    return_ACPI_STATUS (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiEvRemoveSciHandler
 *
 * PARAMETERS:  none
 *
 * RETURN:      E_OK if handler uninstalled OK, E_ERROR if handler was not
 *              installed to begin with
 *
 * DESCRIPTION: Remove the SCI interrupt handler. No further SCIs will be
 *              taken.
 *
 * Note:  It doesn't seem important to disable all events or set the event
 *        enable registers to their original values. The OS should disable
 *        the SCI interrupt level when the handler is removed, so no more
 *        events will come in.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvRemoveSciHandler (
    void)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE (EvRemoveSciHandler);


    /* Just let the OS remove the handler and disable the level */

    Status = AcpiOsRemoveInterruptHandler ((UINT32) AcpiGbl_FADT.SciInterrupt,
                AcpiEvSciXruptHandler);

    return_ACPI_STATUS (Status);
}

#endif /* !ACPI_REDUCED_HARDWARE */
