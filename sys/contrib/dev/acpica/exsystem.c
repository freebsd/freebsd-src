
/******************************************************************************
 *
 * Module Name: exsystem - Interface to OS services
 *              $Revision: 62 $
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

#define __EXSYSTEM_C__

#include "acpi.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "achware.h"
#include "acevents.h"

#define _COMPONENT          ACPI_EXECUTER
        MODULE_NAME         ("exsystem")


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemWaitSemaphore
 *
 * PARAMETERS:  Semaphore           - OSD semaphore to wait on
 *              Timeout             - Max time to wait
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Implements a semaphore wait with a check to see if the
 *              semaphore is available immediately.  If it is not, the
 *              interpreter is released.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExSystemWaitSemaphore (
    ACPI_HANDLE             Semaphore,
    UINT32                  Timeout)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("AcpiExSystemWaitSemaphore");

    Status = AcpiOsWaitSemaphore (Semaphore, 1, 0);
    if (ACPI_SUCCESS (Status))
    {
        return_ACPI_STATUS (Status);
    }

    if (Status == AE_TIME)
    {
        /* We must wait, so unlock the interpreter */

        AcpiExExitInterpreter ();

        Status = AcpiOsWaitSemaphore (Semaphore, 1, Timeout);

        DEBUG_PRINT (TRACE_EXEC, ("*** Thread awake after blocking, %s\n",
            AcpiUtFormatException (Status)));

        /* Reacquire the interpreter */

        Status = AcpiExEnterInterpreter ();
        if (ACPI_SUCCESS (Status))
        {
            /* Restore the timeout exception */

            Status = AE_TIME;
        }
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemDoStall
 *
 * PARAMETERS:  HowLong             - The amount of time to stall
 *
 * RETURN:      None
 *
 * DESCRIPTION: Suspend running thread for specified amount of time.
 *
 ******************************************************************************/

void
AcpiExSystemDoStall (
    UINT32                  HowLong)
{

    if (HowLong > 1000) /* 1 millisecond */
    {
        /* Since this thread will sleep, we must release the interpreter */

        AcpiExExitInterpreter ();

        AcpiOsSleepUsec (HowLong);

        /* And now we must get the interpreter again */

        AcpiExEnterInterpreter ();
    }

    else
    {
        AcpiOsSleepUsec (HowLong);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemDoSuspend
 *
 * PARAMETERS:  HowLong             - The amount of time to suspend
 *
 * RETURN:      None
 *
 * DESCRIPTION: Suspend running thread for specified amount of time.
 *
 ******************************************************************************/

void
AcpiExSystemDoSuspend (
    UINT32                  HowLong)
{
    /* Since this thread will sleep, we must release the interpreter */

    AcpiExExitInterpreter ();

    AcpiOsSleep ((UINT16) (HowLong / (UINT32) 1000),
                 (UINT16) (HowLong % (UINT32) 1000));

    /* And now we must get the interpreter again */

    AcpiExEnterInterpreter ();
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemAcquireMutex
 *
 * PARAMETERS:  *TimeDesc           - The 'time to delay' object descriptor
 *              *ObjDesc            - The object descriptor for this op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Provides an access point to perform synchronization operations
 *              within the AML.  This function will cause a lock to be generated
 *              for the Mutex pointed to by ObjDesc.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExSystemAcquireMutex (
    ACPI_OPERAND_OBJECT     *TimeDesc,
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_PTR ("AcpiExSystemAcquireMutex", ObjDesc);

    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Support for the _GL_ Mutex object -- go get the global lock
     */

    if (ObjDesc->Mutex.Semaphore == AcpiGbl_GlobalLockSemaphore)
    {
        Status = AcpiEvAcquireGlobalLock ();
        return_ACPI_STATUS (Status);
    }

    Status = AcpiExSystemWaitSemaphore (ObjDesc->Mutex.Semaphore,
                                         (UINT32) TimeDesc->Integer.Value);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemReleaseMutex
 *
 * PARAMETERS:  *ObjDesc            - The object descriptor for this op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Provides an access point to perform synchronization operations
 *              within the AML.  This operation is a request to release a
 *              previously acquired Mutex.  If the Mutex variable is set then
 *              it will be decremented.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExSystemReleaseMutex (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiExSystemReleaseMutex");


    if (!ObjDesc)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * Support for the _GL_ Mutex object -- release the global lock
     */
    if (ObjDesc->Mutex.Semaphore == AcpiGbl_GlobalLockSemaphore)
    {
        AcpiEvReleaseGlobalLock ();
        return_ACPI_STATUS (AE_OK);
    }

    Status = AcpiOsSignalSemaphore (ObjDesc->Mutex.Semaphore, 1);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemSignalEvent
 *
 * PARAMETERS:  *ObjDesc            - The object descriptor for this op
 *
 * RETURN:      AE_OK
 *
 * DESCRIPTION: Provides an access point to perform synchronization operations
 *              within the AML.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExSystemSignalEvent (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiExSystemSignalEvent");


    if (ObjDesc)
    {
        Status = AcpiOsSignalSemaphore (ObjDesc->Event.Semaphore, 1);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemWaitEvent
 *
 * PARAMETERS:  *TimeDesc           - The 'time to delay' object descriptor
 *              *ObjDesc            - The object descriptor for this op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Provides an access point to perform synchronization operations
 *              within the AML.  This operation is a request to wait for an
 *              event.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExSystemWaitEvent (
    ACPI_OPERAND_OBJECT     *TimeDesc,
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE ("AcpiExSystemWaitEvent");


    if (ObjDesc)
    {
        Status = AcpiExSystemWaitSemaphore (ObjDesc->Event.Semaphore,
                                             (UINT32) TimeDesc->Integer.Value);
    }


    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiExSystemResetEvent
 *
 * PARAMETERS:  *ObjDesc            - The object descriptor for this op
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Reset an event to a known state.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiExSystemResetEvent (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    ACPI_STATUS             Status = AE_OK;
    void                    *TempSemaphore;


    /*
     * We are going to simply delete the existing semaphore and
     * create a new one!
     */

    Status = AcpiOsCreateSemaphore (ACPI_NO_UNIT_LIMIT, 0, &TempSemaphore);
    if (ACPI_SUCCESS (Status))
    {
        AcpiOsDeleteSemaphore (ObjDesc->Event.Semaphore);
        ObjDesc->Event.Semaphore = TempSemaphore;
    }

    return (Status);
}

