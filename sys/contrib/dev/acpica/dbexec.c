/*******************************************************************************
 *
 * Module Name: dbexec - debugger control method execution
 *              $Revision: 16 $
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
#include "acparser.h"
#include "acdispat.h"
#include "amlcode.h"
#include "acnamesp.h"
#include "acparser.h"
#include "acevents.h"
#include "acinterp.h"
#include "acdebug.h"
#include "actables.h"

#ifdef ENABLE_DEBUGGER

#define _COMPONENT          DEBUGGER
        MODULE_NAME         ("dbexec")


typedef struct dbmethodinfo
{
    ACPI_HANDLE             ThreadGate;
    NATIVE_CHAR             *Name;
    NATIVE_CHAR             **Args;
    UINT32                  Flags;
    UINT32                  NumLoops;
    NATIVE_CHAR             Pathname[128];

} DB_METHOD_INFO;


DB_METHOD_INFO              Info;


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbExecuteMethod
 *
 * PARAMETERS:  Info            - Valid info segment
 *              ReturnObj       - Where to put return object
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a control method.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbExecuteMethod (
    DB_METHOD_INFO          *Info,
    ACPI_BUFFER             *ReturnObj)
{
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        ParamObjects;
    ACPI_OBJECT             Params[MTH_NUM_ARGS];
    UINT32                  i;


    if (OutputToFile && !AcpiDbgLevel)
    {
        AcpiOsPrintf ("Warning: debug output is not enabled!\n");
    }

    /* Are there arguments to the method? */

    if (Info->Args && Info->Args[0])
    {
        for (i = 0; Info->Args[i] && i < MTH_NUM_ARGS; i++)
        {
            Params[i].Type              = ACPI_TYPE_NUMBER;
            Params[i].Number.Value      = STRTOUL (Info->Args[i], NULL, 16);
        }

        ParamObjects.Pointer        = Params;
        ParamObjects.Count          = i;
    }

    else
    {
        /* Setup default parameters */

        Params[0].Type              = ACPI_TYPE_NUMBER;
        Params[0].Number.Value      = 0x01020304;

        Params[1].Type              = ACPI_TYPE_STRING;
        Params[1].String.Length     = 12;
        Params[1].String.Pointer    = "AML Debugger";

        ParamObjects.Pointer        = Params;
        ParamObjects.Count          = 2;
    }

    /* Prepare for a return object of arbitrary size */

    ReturnObj->Pointer           = Buffer;
    ReturnObj->Length            = BUFFER_SIZE;


    /* Do the actual method execution */

    Status = AcpiEvaluateObject (NULL, Info->Pathname, &ParamObjects, ReturnObj);

    AcpiGbl_CmSingleStep = FALSE;
    AcpiGbl_MethodExecuting = FALSE;

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbExecuteSetup
 *
 * PARAMETERS:  Info            - Valid method info
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Setup info segment prior to method execution
 *
 ******************************************************************************/

void
AcpiDbExecuteSetup (
    DB_METHOD_INFO          *Info)
{

    /* Catenate the current scope to the supplied name */

    Info->Pathname[0] = 0;
    if ((Info->Name[0] != '\\') &&
        (Info->Name[0] != '/'))
    {
        STRCAT (Info->Pathname, ScopeBuf);
    }

    STRCAT (Info->Pathname, Info->Name);
    AcpiDbPrepNamestring (Info->Pathname);

    AcpiDbSetOutputDestination (DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("Executing %s\n", Info->Pathname);

    if (Info->Flags & EX_SINGLE_STEP)
    {
        AcpiGbl_CmSingleStep = TRUE;
        AcpiDbSetOutputDestination (DB_CONSOLE_OUTPUT);
    }

    else
    {
        /* No single step, allow redirection to a file */

        AcpiDbSetOutputDestination (DB_REDIRECTABLE_OUTPUT);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbExecute
 *
 * PARAMETERS:  Name                - Name of method to execute
 *              Args                - Parameters to the method
 *              Flags               - single step/no single step
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a control method.  Name is relative to the current
 *              scope.
 *
 ******************************************************************************/

void
AcpiDbExecute (
    NATIVE_CHAR             *Name,
    NATIVE_CHAR             **Args,
    UINT32                  Flags)
{
    ACPI_STATUS             Status;
    UINT32                  PreviousAllocations;
    UINT32                  PreviousSize;
    UINT32                  Allocations;
    UINT32                  Size;
    ACPI_BUFFER             ReturnObj;


    /* Memory allocation tracking */

    PreviousAllocations = AcpiGbl_CurrentAllocCount;
    PreviousSize = AcpiGbl_CurrentAllocSize;


    Info.Name = Name;
    Info.Args = Args;
    Info.Flags = Flags;

    AcpiDbExecuteSetup (&Info);
    Status = AcpiDbExecuteMethod (&Info, &ReturnObj);


    /* Memory allocation tracking */

    Allocations = AcpiGbl_CurrentAllocCount - PreviousAllocations;
    Size = AcpiGbl_CurrentAllocSize - PreviousSize;

    AcpiDbSetOutputDestination (DB_DUPLICATE_OUTPUT);

    if (Allocations > 0)
    {
        AcpiOsPrintf ("Outstanding: %ld allocations of total size %ld after execution\n",
                        Allocations, Size);
    }


    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Execution of %s failed with status %s\n", Info.Pathname, AcpiCmFormatException (Status));
    }

    else
    {
        /* Display a return object, if any */

        if (ReturnObj.Length)
        {
            AcpiOsPrintf ("Execution of %s returned object %p\n", Info.Pathname, ReturnObj.Pointer);
            AcpiDbDumpObject (ReturnObj.Pointer, 1);
        }
    }

    AcpiDbSetOutputDestination (DB_CONSOLE_OUTPUT);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbMethodThread
 *
 * PARAMETERS:  Context             - Execution info segment
 *
 * RETURN:      None
 *
 * DESCRIPTION: Debugger execute thread.  Waits for a command line, then
 *              simply dispatches it.
 *
 ******************************************************************************/

void
AcpiDbMethodThread (
    void                    *Context)
{
    ACPI_STATUS             Status;
    DB_METHOD_INFO          *Info = Context;
    UINT32                  i;
    ACPI_BUFFER             ReturnObj;


    for (i = 0; i < Info->NumLoops; i++)
    {
        Status = AcpiDbExecuteMethod (Info, &ReturnObj);
        if (ACPI_SUCCESS (Status))
        {
            if (ReturnObj.Length)
            {
                AcpiOsPrintf ("Execution of %s returned object %p\n", Info->Pathname, ReturnObj.Pointer);
                AcpiDbDumpObject (ReturnObj.Pointer, 1);
            }
        }
    }


    /* Signal our completion */

    AcpiOsSignalSemaphore (Info->ThreadGate, 1);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbCreateExecutionThreads
 *
 * PARAMETERS:  NumThreadsArg           - Number of threads to create
 *              NumLoopsArg             - Loop count for the thread(s)
 *              MethodNameArg           - Control method to execute
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create threads to execute method(s)
 *
 ******************************************************************************/

void
AcpiDbCreateExecutionThreads (
    NATIVE_CHAR             *NumThreadsArg,
    NATIVE_CHAR             *NumLoopsArg,
    NATIVE_CHAR             *MethodNameArg)
{
    ACPI_STATUS             Status;
    UINT32                  NumThreads;
    UINT32                  NumLoops;
    UINT32                  i;
    ACPI_HANDLE             ThreadGate;


    /* Get the arguments */

    NumThreads = STRTOUL (NumThreadsArg, NULL, 0);
    NumLoops = STRTOUL (NumLoopsArg, NULL, 0);

    if (!NumThreads || !NumLoops)
    {
        AcpiOsPrintf ("Bad argument: Threads %d, Loops %d\n", NumThreads, NumLoops);
        return;
    }


    /* Create the synchronization semaphore */

    Status = AcpiOsCreateSemaphore (1, 0, &ThreadGate);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not create semaphore, %s\n", AcpiCmFormatException (Status));
        return;
    }

    /* Setup the context to be passed to each thread */

    Info.Name = MethodNameArg;
    Info.Args = NULL;
    Info.Flags = 0;
    Info.NumLoops = NumLoops;
    Info.ThreadGate = ThreadGate;

    AcpiDbExecuteSetup (&Info);


    /* Create the threads */

    AcpiOsPrintf ("Creating %d threads to execute %d times each\n", NumThreads, NumLoops);

    for (i = 0; i < (NumThreads); i++)
    {
        AcpiOsQueueForExecution (OSD_PRIORITY_MED, AcpiDbMethodThread, &Info);
    }


    /* Wait for all threads to complete */

    i = NumThreads;
    while (i)   /* Brain damage for OSD implementations that only support wait of 1 unit */
    {
        Status = AcpiOsWaitSemaphore (ThreadGate, 1, WAIT_FOREVER);
        i--;
    }

    /* Cleanup and exit */

    AcpiOsDeleteSemaphore (ThreadGate);

    AcpiDbSetOutputDestination (DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("All threads (%d) have completed\n", NumThreads);
    AcpiDbSetOutputDestination (DB_CONSOLE_OUTPUT);
}


#endif /* ENABLE_DEBUGGER */


