/*******************************************************************************
 *
 * Module Name: evsci - System Control Interrupt configuration and
 *                      legacy to ACPI mode state transition functions
 *              $Revision: 60 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999, Intel Corp.  All rights
 * reserved.
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
 * Redistribution of source code of any substantial portion of the Covered
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

#include "acpi.h"
#include "acnamesp.h"
#include "achware.h"
#include "acevents.h"


#define _COMPONENT          EVENT_HANDLING
        MODULE_NAME         ("evsci")


/*
 * Elements correspond to counts for TMR, NOT_USED, GBL, PWR_BTN, SLP_BTN, RTC,
 * and GENERAL respectively.  These counts are modified by the ACPI interrupt
 * handler.
 *
 * TBD: [Investigate] Note that GENERAL should probably be split out into
 * one element for each bit in the GPE registers
 */


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvSciHandler
 *
 * PARAMETERS:  Context   - Calling Context
 *
 * RETURN:      Status code indicates whether interrupt was handled.
 *
 * DESCRIPTION: Interrupt handler that will figure out what function or
 *              control method to call to deal with a SCI.  Installed
 *              using BU interrupt support.
 *
 ******************************************************************************/

UINT32
AcpiEvSciHandler (void *Context)
{
    UINT32                  InterruptHandled = INTERRUPT_NOT_HANDLED;


    FUNCTION_TRACE("EvSciHandler");


    /*
     * Make sure that ACPI is enabled by checking SCI_EN.  Note that we are
     * required to treat the SCI interrupt as sharable, level, active low.
     */
    if (!AcpiHwRegisterAccess (ACPI_READ, ACPI_MTX_DO_NOT_LOCK, SCI_EN))
    {
        /* ACPI is not enabled;  this interrupt cannot be for us */

        return_VALUE (INTERRUPT_NOT_HANDLED);
    }

    /*
     * Fixed AcpiEvents:
     * -------------
     * Check for and dispatch any Fixed AcpiEvents that have occurred
     */
    InterruptHandled |= AcpiEvFixedEventDetect ();

    /*
     * GPEs:
     * -----
     * Check for and dispatch any GPEs that have occurred
     */
    InterruptHandled |= AcpiEvGpeDetect ();

    return_VALUE (InterruptHandled);
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
AcpiEvInstallSciHandler (void)
{
    UINT32                  Except = AE_OK;


    FUNCTION_TRACE ("EvInstallSciHandler");


    Except = AcpiOsInstallInterruptHandler ((UINT32) AcpiGbl_FACP->SciInt,
                                            AcpiEvSciHandler,
                                            NULL);

    return_ACPI_STATUS (Except);
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
 * DESCRIPTION: Restores original status of all fixed event enable bits and
 *              removes SCI handler.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiEvRemoveSciHandler (void)
{
    FUNCTION_TRACE ("EvRemoveSciHandler");

#if 0
    /* TBD:[Investigate] Figure this out!!  Disable all events first ???  */

    if (OriginalFixedEnableBitStatus ^ 1 << AcpiEventIndex (TMR_FIXED_EVENT))
    {
        AcpiEventDisableEvent (TMR_FIXED_EVENT);
    }

    if (OriginalFixedEnableBitStatus ^ 1 << AcpiEventIndex (GBL_FIXED_EVENT))
    {
        AcpiEventDisableEvent (GBL_FIXED_EVENT);
    }

    if (OriginalFixedEnableBitStatus ^ 1 << AcpiEventIndex (PWR_BTN_FIXED_EVENT))
    {
        AcpiEventDisableEvent (PWR_BTN_FIXED_EVENT);
    }

    if (OriginalFixedEnableBitStatus ^ 1 << AcpiEventIndex (SLP_BTN_FIXED_EVENT))
    {
        AcpiEventDisableEvent (SLP_BTN_FIXED_EVENT);
    }

    if (OriginalFixedEnableBitStatus ^ 1 << AcpiEventIndex (RTC_FIXED_EVENT))
    {
        AcpiEventDisableEvent (RTC_FIXED_EVENT);
    }

    OriginalFixedEnableBitStatus = 0;

#endif

    AcpiOsRemoveInterruptHandler ((UINT32) AcpiGbl_FACP->SciInt,
                                    AcpiEvSciHandler);

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvSciCount
 *
 * PARAMETERS:  Event       Event that generated an SCI.
 *
 * RETURN:      Number of SCI's for requested event since last time
 *              SciOccured() was called for this event.
 *
 * DESCRIPTION: Checks to see if SCI has been generated from requested source
 *              since the last time this function was called.
 *
 ******************************************************************************/

#ifdef ACPI_DEBUG

UINT32
AcpiEvSciCount (
    UINT32                  Event)
{
    UINT32                  Count;

    FUNCTION_TRACE ("EvSciCount");

    /*
     * Elements correspond to counts for TMR, NOT_USED, GBL,
     * PWR_BTN, SLP_BTN, RTC, and GENERAL respectively.
     */

    if (Event >= NUM_FIXED_EVENTS)
    {
        Count = (UINT32) -1;
    }
    else
    {
        Count = AcpiGbl_EventCount[Event];
    }

    return_VALUE (Count);
}

#endif


/*******************************************************************************
 *
 * FUNCTION:    AcpiEvRestoreAcpiState
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: Restore the original ACPI state of the machine
 *
 ******************************************************************************/

void
AcpiEvRestoreAcpiState (void)
{
    UINT32                  Index;


    FUNCTION_TRACE ("EvRestoreAcpiState");


    /* Restore the state of the chipset enable bits. */

    if (AcpiGbl_RestoreAcpiChipset == TRUE)
    {
        /* Restore the fixed events */

        if (AcpiOsIn16 (AcpiGbl_FACP->Pm1aEvtBlk + 2) !=
            AcpiGbl_Pm1EnableRegisterSave)
        {
            AcpiOsOut16 ((AcpiGbl_FACP->Pm1aEvtBlk + 2),
                          AcpiGbl_Pm1EnableRegisterSave);
        }

        if (AcpiGbl_FACP->Pm1bEvtBlk)
        {
            if (AcpiOsIn16 (AcpiGbl_FACP->Pm1bEvtBlk + 2) !=
                AcpiGbl_Pm1EnableRegisterSave)
            {
                AcpiOsOut16 ((AcpiGbl_FACP->Pm1bEvtBlk + 2),
                              AcpiGbl_Pm1EnableRegisterSave);
            }
        }


        /* Ensure that all status bits are clear */

        AcpiHwClearAcpiStatus ();


        /* Now restore the GPEs */

        for (Index = 0; Index < DIV_2 (AcpiGbl_FACP->Gpe0BlkLen); Index++)
        {
            if (AcpiOsIn8 (AcpiGbl_FACP->Gpe0Blk +
                DIV_2 (AcpiGbl_FACP->Gpe0BlkLen)) !=
                AcpiGbl_Gpe0EnableRegisterSave[Index])
            {
                AcpiOsOut8 ((AcpiGbl_FACP->Gpe0Blk +
                             DIV_2 (AcpiGbl_FACP->Gpe0BlkLen)),
                             AcpiGbl_Gpe0EnableRegisterSave[Index]);
            }
        }

        if (AcpiGbl_FACP->Gpe1Blk && AcpiGbl_FACP->Gpe1BlkLen)
        {
            for (Index = 0; Index < DIV_2 (AcpiGbl_FACP->Gpe1BlkLen); Index++)
            {
                if (AcpiOsIn8 (AcpiGbl_FACP->Gpe1Blk +
                    DIV_2 (AcpiGbl_FACP->Gpe1BlkLen)) !=
                    AcpiGbl_Gpe1EnableRegisterSave[Index])
                {
                    AcpiOsOut8 ((AcpiGbl_FACP->Gpe1Blk +
                                 DIV_2 (AcpiGbl_FACP->Gpe1BlkLen)),
                                 AcpiGbl_Gpe1EnableRegisterSave[Index]);
                }
            }
        }

        if (AcpiHwGetMode() != AcpiGbl_OriginalMode)
        {
            AcpiHwSetMode (AcpiGbl_OriginalMode);
        }
    }

    return_VOID;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiEvTerminate
 *
 * PARAMETERS:  none
 *
 * RETURN:      none
 *
 * DESCRIPTION: free memory allocated for table storage.
 *
 ******************************************************************************/

void
AcpiEvTerminate (void)
{

    FUNCTION_TRACE ("EvTerminate");


    /*
     * Free global tables, etc.
     */

    if (AcpiGbl_GpeRegisters)
    {
        AcpiCmFree (AcpiGbl_GpeRegisters);
    }

    if (AcpiGbl_GpeInfo)
    {
        AcpiCmFree (AcpiGbl_GpeInfo);
    }

    return_VOID;
}


