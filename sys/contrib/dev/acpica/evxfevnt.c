/******************************************************************************
 *
 * Module Name: evxfevnt - External Interfaces, ACPI event disable/enable
 *              $Revision: 42 $
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, 2000, 2001, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights.  You may have additional license terms from the party that provided
 * you this software, covering your right to use that party's intellectual
 * property rights.
 *
 * 2.2. Intel grants, free of charge, to any person ("Licensee") obtaining a
 * copy of the source code appearing in this file ("Covered Code") an
 * irrevocable, perpetual, worldwide license under Intel's copyrights in the
 * base code distributed originally by Intel ("Original Intel Code") to copy,
 * make derivatives, distribute, use and display any portion of the Covered
 * Code in any form, with the right to sublicense such rights; and
 *
 * 2.3. Intel grants Licensee a non-exclusive and non-transferable patent
 * license (with the right to sublicense), under only those claims of Intel
 * patents that are infringed by the Original Intel Code, to make, use, sell,
 * offer to sell, and import the Covered Code and derivative works thereof
 * solely to the minimum extent necessary to exercise the above copyright
 * license, and in no event shall the patent license extend to any additions
 * to or modifications of the Original Intel Code.  No other license or right
 * is granted directly or by implication, estoppel or otherwise;
 *
 * The above copyright and patent license is granted only if the following
 * conditions are met:
 *
 * 3. Conditions
 *
 * 3.1. Redistribution of Source with Rights to Further Distribute Source.
 * Redistribution of source code of any substantial prton of the Covered
 * Code or modification with rights to further distribute source must include
 * the above Copyright Notice, the above License, this list of Conditions,
 * and the following Disclaimer and Export Compliance provision.  In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change.  Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee.  Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution.  In
 * addition, Licensee may not authorize further sublicense of source of any
 * portion of the Covered Code, and must include terms to the effect that the
 * license from Licensee to its licensee is limited to the intellectual
 * property embodied in the software Licensee provides to its licensee, and
 * not to intellectual property embodied in modifications its licensee may
 * make.
 *
 * 3.3. Redistribution of Executable. Redistribution in executable form of any
 * substantial portion of the Covered Code or modification must reproduce the
 * above Copyright Notice, and the following Disclaimer and Export Compliance
 * provision in the documentation and/or other materials provided with the
 * distribution.
 *
 * 3.4. Intel retains all right, title, and interest in and to the Original
 * Intel Code.
 *
 * 3.5. Neither the name Intel nor any other trademark owned or controlled by
 * Intel shall be used in advertising or otherwise to promote the sale, use or
 * other dealings in products derived from or relating to the Covered Code
 * without prior written authorization from Intel.
 *
 * 4. Disclaimer and Export Compliance
 *
 * 4.1. INTEL MAKES NO WARRANTY OF ANY KIND REGARDING ANY SOFTWARE PROVIDED
 * HERE.  ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT,  ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES.  INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS.  INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES.  THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government.  In the
 * event Licensee exports any such software from the United States or
 * re-exports any such software from a foreign destination, Licensee shall
 * ensure that the distribution and export/re-export of the software is in
 * compliance with all laws, regulations, orders, or other restrictions of the
 * U.S. Export Administration Regulations. Licensee agrees that neither it nor
 * any of its subsidiaries will export/re-export any technical data, process,
 * software, or service, directly or indirectly, to any country for which the
 * United States government or any agency thereof requires an export license,
 * other governmental approval, or letter of assurance, without first obtaining
 * such license, approval or letter.
 *
 *****************************************************************************/


#define __EVXFEVNT_C__

#include "acpi.h"
#include "achware.h"
#include "acnamesp.h"
#include "acevents.h"
#include "amlcode.h"
#include "acinterp.h"

#define _COMPONENT          ACPI_EVENTS
        MODULE_NAME         ("evxfevnt")


/*******************************************************************************
 *
 * FUNCTION:    AcpiEnable
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Transfers the system into ACPI mode.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnable (void)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiEnable");


    /* Make sure we have ACPI tables */

    if (!AcpiGbl_DSDT)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_WARN, "No ACPI tables present!\n"));
        return_ACPI_STATUS (AE_NO_ACPI_TABLES);
    }

    AcpiGbl_OriginalMode = AcpiHwGetMode ();

    if (AcpiGbl_OriginalMode == SYS_MODE_ACPI)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_OK, "Already in ACPI mode.\n"));
    }

    else
    {
        /* Transition to ACPI mode */

        Status = AcpiHwSetMode (SYS_MODE_ACPI);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_FATAL, "Could not transition to ACPI mode.\n"));
            return_ACPI_STATUS (Status);
        }

        ACPI_DEBUG_PRINT ((ACPI_DB_OK, "Transition to ACPI mode successful\n"));
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDisable
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Returns the system to original ACPI/legacy mode, and
 *              uninstalls the SCI interrupt handler.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDisable (void)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiDisable");


    if (AcpiHwGetMode () != AcpiGbl_OriginalMode)
    {
        /* Restore original mode  */
    
        Status = AcpiHwSetMode (AcpiGbl_OriginalMode);
        if (ACPI_FAILURE (Status))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unable to transition to original mode"));
            return_ACPI_STATUS (Status);
        }
    }

    /* Unload the SCI interrupt handler  */

    AcpiEvRemoveSciHandler ();
    AcpiEvRestoreAcpiState ();

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEnableEvent
 *
 * PARAMETERS:  Event           - The fixed event or GPE to be enabled
 *              Type            - The type of event
 *              Flags           - Just enable, or also wake enable?
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Enable an ACPI event (fixed and general purpose)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEnableEvent (
    UINT32                  Event,
    UINT32                  Type,
    UINT32                  Flags)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  RegisterId;


    FUNCTION_TRACE ("AcpiEnableEvent");


    /* The Type must be either Fixed AcpiEvent or GPE */

    switch (Type)
    {

    case ACPI_EVENT_FIXED:

        /* Decode the Fixed AcpiEvent */

        switch (Event)
        {
        case ACPI_EVENT_PMTIMER:
            RegisterId = TMR_EN;
            break;

        case ACPI_EVENT_GLOBAL:
            RegisterId = GBL_EN;
            break;

        case ACPI_EVENT_POWER_BUTTON:
            RegisterId = PWRBTN_EN;
            break;

        case ACPI_EVENT_SLEEP_BUTTON:
            RegisterId = SLPBTN_EN;
            break;

        case ACPI_EVENT_RTC:
            RegisterId = RTC_EN;
            break;

        default:
            return_ACPI_STATUS (AE_BAD_PARAMETER);
            break;
        }

        /*
         * Enable the requested fixed event (by writing a one to the
         * enable register bit)
         */
        AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_LOCK, RegisterId, 1);

        if (1 != AcpiHwRegisterBitAccess(ACPI_READ, ACPI_MTX_LOCK, RegisterId))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Fixed event bit clear when it should be set\n"));
            return_ACPI_STATUS (AE_NO_HARDWARE_RESPONSE);
        }

        break;


    case ACPI_EVENT_GPE:

        /* Ensure that we have a valid GPE number */

        if ((Event > ACPI_GPE_MAX) ||
            (AcpiGbl_GpeValid[Event] == ACPI_GPE_INVALID))
        {
            return_ACPI_STATUS (AE_BAD_PARAMETER);
        }


        /* Enable the requested GPE number */

        if (Flags & ACPI_EVENT_ENABLE)
        {
            AcpiHwEnableGpe (Event);
        }
        if (Flags & ACPI_EVENT_WAKE_ENABLE)
        {
            AcpiHwEnableGpeForWakeup (Event);
        }

        break;


    default:

        Status = AE_BAD_PARAMETER;
    }


    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDisableEvent
 *
 * PARAMETERS:  Event           - The fixed event or GPE to be enabled
 *              Type            - The type of event, fixed or general purpose
 *              Flags           - Wake disable vs. non-wake disable
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Disable an ACPI event (fixed and general purpose)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDisableEvent (
    UINT32                  Event,
    UINT32                  Type,
    UINT32                  Flags)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  RegisterId;


    FUNCTION_TRACE ("AcpiDisableEvent");


    /* The Type must be either Fixed AcpiEvent or GPE */

    switch (Type)
    {

    case ACPI_EVENT_FIXED:

        /* Decode the Fixed AcpiEvent */

        switch (Event)
        {
        case ACPI_EVENT_PMTIMER:
            RegisterId = TMR_EN;
            break;

        case ACPI_EVENT_GLOBAL:
            RegisterId = GBL_EN;
            break;

        case ACPI_EVENT_POWER_BUTTON:
            RegisterId = PWRBTN_EN;
            break;

        case ACPI_EVENT_SLEEP_BUTTON:
            RegisterId = SLPBTN_EN;
            break;

        case ACPI_EVENT_RTC:
            RegisterId = RTC_EN;
            break;

        default:
            return_ACPI_STATUS (AE_BAD_PARAMETER);
            break;
        }

        /*
         * Disable the requested fixed event (by writing a zero to the
         * enable register bit)
         */
        AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_LOCK, RegisterId, 0);

        if (0 != AcpiHwRegisterBitAccess(ACPI_READ, ACPI_MTX_LOCK, RegisterId))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Fixed event bit set when it should be clear,\n"));
            return_ACPI_STATUS (AE_NO_HARDWARE_RESPONSE);
        }

        break;


    case ACPI_EVENT_GPE:

        /* Ensure that we have a valid GPE number */

        if ((Event > ACPI_GPE_MAX) ||
            (AcpiGbl_GpeValid[Event] == ACPI_GPE_INVALID))
        {
            return_ACPI_STATUS (AE_BAD_PARAMETER);
        }

        /* Disable the requested GPE number */

        if (Flags & ACPI_EVENT_DISABLE)
        {
            AcpiHwDisableGpe (Event);
        }
        if (Flags & ACPI_EVENT_WAKE_DISABLE)
        {
            AcpiHwDisableGpeForWakeup (Event);
        }

        break;


    default:
        Status = AE_BAD_PARAMETER;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiClearEvent
 *
 * PARAMETERS:  Event           - The fixed event or GPE to be cleared
 *              Type            - The type of event
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Clear an ACPI event (fixed and general purpose)
 *
 ******************************************************************************/

ACPI_STATUS
AcpiClearEvent (
    UINT32                  Event,
    UINT32                  Type)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  RegisterId;


    FUNCTION_TRACE ("AcpiClearEvent");


    /* The Type must be either Fixed AcpiEvent or GPE */

    switch (Type)
    {

    case ACPI_EVENT_FIXED:

        /* Decode the Fixed AcpiEvent */

        switch (Event)
        {
        case ACPI_EVENT_PMTIMER:
            RegisterId = TMR_STS;
            break;

        case ACPI_EVENT_GLOBAL:
            RegisterId = GBL_STS;
            break;

        case ACPI_EVENT_POWER_BUTTON:
            RegisterId = PWRBTN_STS;
            break;

        case ACPI_EVENT_SLEEP_BUTTON:
            RegisterId = SLPBTN_STS;
            break;

        case ACPI_EVENT_RTC:
            RegisterId = RTC_STS;
            break;

        default:
            return_ACPI_STATUS (AE_BAD_PARAMETER);
            break;
        }

        /*
         * Clear the requested fixed event (By writing a one to the
         * status register bit)
         */
        AcpiHwRegisterBitAccess (ACPI_WRITE, ACPI_MTX_LOCK, RegisterId, 1);
        break;


    case ACPI_EVENT_GPE:

        /* Ensure that we have a valid GPE number */

        if ((Event > ACPI_GPE_MAX) ||
            (AcpiGbl_GpeValid[Event] == ACPI_GPE_INVALID))
        {
            return_ACPI_STATUS (AE_BAD_PARAMETER);
        }


        AcpiHwClearGpe (Event);
        break;


    default:

        Status = AE_BAD_PARAMETER;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiGetEventStatus
 *
 * PARAMETERS:  Event           - The fixed event or GPE
 *              Type            - The type of event
 *              Status          - Where the current status of the event will
 *                                be returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Obtains and returns the current status of the event
 *
 ******************************************************************************/


ACPI_STATUS
AcpiGetEventStatus (
    UINT32                  Event,
    UINT32                  Type,
    ACPI_EVENT_STATUS       *EventStatus)
{
    ACPI_STATUS             Status = AE_OK;
    UINT32                  RegisterId;


    FUNCTION_TRACE ("AcpiGetEventStatus");


    if (!EventStatus)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    /* The Type must be either Fixed AcpiEvent or GPE */

    switch (Type)
    {

    case ACPI_EVENT_FIXED:

        /* Decode the Fixed AcpiEvent */

        switch (Event)
        {
        case ACPI_EVENT_PMTIMER:
            RegisterId = TMR_STS;
            break;

        case ACPI_EVENT_GLOBAL:
            RegisterId = GBL_STS;
            break;

        case ACPI_EVENT_POWER_BUTTON:
            RegisterId = PWRBTN_STS;
            break;

        case ACPI_EVENT_SLEEP_BUTTON:
            RegisterId = SLPBTN_STS;
            break;

        case ACPI_EVENT_RTC:
            RegisterId = RTC_STS;
            break;

        default:
            return_ACPI_STATUS (AE_BAD_PARAMETER);
            break;
        }

        /* Get the status of the requested fixed event */

        *EventStatus = AcpiHwRegisterBitAccess (ACPI_READ, ACPI_MTX_LOCK, RegisterId);
        break;


    case ACPI_EVENT_GPE:

        /* Ensure that we have a valid GPE number */

        if ((Event > ACPI_GPE_MAX) ||
            (AcpiGbl_GpeValid[Event] == ACPI_GPE_INVALID))
        {
            return_ACPI_STATUS (AE_BAD_PARAMETER);
        }


        /* Obtain status on the requested GPE number */

        AcpiHwGetGpeStatus (Event, EventStatus);
        break;


    default:
        Status = AE_BAD_PARAMETER;
    }

    return_ACPI_STATUS (Status);
}

