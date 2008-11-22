/*******************************************************************************
 *
 * Module Name: dbexec - debugger control method execution
 *              $Revision: 62 $
 *
 ******************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2004, Intel Corp.
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


#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acdebug.h>
#include <contrib/dev/acpica/acnamesp.h>

#ifdef ACPI_DEBUGGER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbexec")


static ACPI_DB_METHOD_INFO  AcpiGbl_DbMethodInfo;


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
    ACPI_DB_METHOD_INFO     *Info,
    ACPI_BUFFER             *ReturnObj)
{
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        ParamObjects;
    ACPI_OBJECT             Params[ACPI_METHOD_NUM_ARGS];
    UINT32                  i;


    if (AcpiGbl_DbOutputToFile && !AcpiDbgLevel)
    {
        AcpiOsPrintf ("Warning: debug output is not enabled!\n");
    }

    /* Are there arguments to the method? */

    if (Info->Args && Info->Args[0])
    {
        for (i = 0; Info->Args[i] && i < ACPI_METHOD_NUM_ARGS; i++)
        {
            Params[i].Type              = ACPI_TYPE_INTEGER;
            Params[i].Integer.Value     = ACPI_STRTOUL (Info->Args[i], NULL, 16);
        }

        ParamObjects.Pointer        = Params;
        ParamObjects.Count          = i;
    }
    else
    {
        /* Setup default parameters */

        Params[0].Type              = ACPI_TYPE_INTEGER;
        Params[0].Integer.Value     = 0x01020304;

        Params[1].Type              = ACPI_TYPE_STRING;
        Params[1].String.Length     = 12;
        Params[1].String.Pointer    = "AML Debugger";

        ParamObjects.Pointer        = Params;
        ParamObjects.Count          = 2;
    }

    /* Prepare for a return object of arbitrary size */

    ReturnObj->Pointer           = AcpiGbl_DbBuffer;
    ReturnObj->Length            = ACPI_DEBUG_BUFFER_SIZE;

    /* Do the actual method execution */

    AcpiGbl_MethodExecuting = TRUE;
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
    ACPI_DB_METHOD_INFO     *Info)
{

    /* Catenate the current scope to the supplied name */

    Info->Pathname[0] = 0;
    if ((Info->Name[0] != '\\') &&
        (Info->Name[0] != '/'))
    {
        ACPI_STRCAT (Info->Pathname, AcpiGbl_DbScopeBuf);
    }

    ACPI_STRCAT (Info->Pathname, Info->Name);
    AcpiDbPrepNamestring (Info->Pathname);

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("Executing %s\n", Info->Pathname);

    if (Info->Flags & EX_SINGLE_STEP)
    {
        AcpiGbl_CmSingleStep = TRUE;
        AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
    }

    else
    {
        /* No single step, allow redirection to a file */

        AcpiDbSetOutputDestination (ACPI_DB_REDIRECTABLE_OUTPUT);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbGetOutstandingAllocations
 *
 * PARAMETERS:  None
 *
 * RETURN:      Current global allocation count minus cache entries
 *
 * DESCRIPTION: Determine the current number of "outstanding" allocations --
 *              those allocations that have not been freed and also are not
 *              in one of the various object caches.
 *
 ******************************************************************************/

UINT32
AcpiDbGetOutstandingAllocations (
    void)
{
    UINT32                  Outstanding = 0;

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
    UINT32                  i;


    for (i = ACPI_MEM_LIST_FIRST_CACHE_LIST; i < ACPI_NUM_MEM_LISTS; i++)
    {
        Outstanding += (AcpiGbl_MemoryLists[i].TotalAllocated -
                        AcpiGbl_MemoryLists[i].TotalFreed -
                        AcpiGbl_MemoryLists[i].CacheDepth);
    }
#endif

    return (Outstanding);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbExecutionWalk
 *
 * PARAMETERS:  WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute a control method.  Name is relative to the current
 *              scope.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiDbExecutionWalk (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_BUFFER             ReturnObj;
    ACPI_STATUS             Status;


    ObjDesc = AcpiNsGetAttachedObject (Node);
    if (ObjDesc->Method.ParamCount)
    {
        return (AE_OK);
    }

    ReturnObj.Pointer = NULL;
    ReturnObj.Length = ACPI_ALLOCATE_BUFFER;

    AcpiNsPrintNodePathname (Node, "Execute");

    /* Do the actual method execution */

    AcpiOsPrintf ("\n");
    AcpiGbl_MethodExecuting = TRUE;

    Status = AcpiEvaluateObject (Node, NULL, NULL, &ReturnObj);

    AcpiOsPrintf ("[%4.4s] returned %s\n", AcpiUtGetNodeName (Node),
            AcpiFormatException (Status));
    AcpiGbl_MethodExecuting = FALSE;

    return (AE_OK);
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
    char                    *Name,
    char                    **Args,
    UINT32                  Flags)
{
    ACPI_STATUS             Status;
    ACPI_BUFFER             ReturnObj;


#ifdef ACPI_DEBUG_OUTPUT
    UINT32                  PreviousAllocations;
    UINT32                  Allocations;


    /* Memory allocation tracking */

    PreviousAllocations = AcpiDbGetOutstandingAllocations ();
#endif

    if (*Name == '*')
    {
        (void) AcpiWalkNamespace (ACPI_TYPE_METHOD, ACPI_ROOT_OBJECT, ACPI_UINT32_MAX,
                    AcpiDbExecutionWalk, NULL, NULL);
        return;
    }
    else
    {
        AcpiGbl_DbMethodInfo.Name = Name;
        AcpiGbl_DbMethodInfo.Args = Args;
        AcpiGbl_DbMethodInfo.Flags = Flags;

        ReturnObj.Pointer = NULL;
        ReturnObj.Length = ACPI_ALLOCATE_BUFFER;

        AcpiDbExecuteSetup (&AcpiGbl_DbMethodInfo);
        Status = AcpiDbExecuteMethod (&AcpiGbl_DbMethodInfo, &ReturnObj);
    }

    /*
     * Allow any handlers in separate threads to complete.
     * (Such as Notify handlers invoked from AML executed above).
     */
    AcpiOsSleep (10);


#ifdef ACPI_DEBUG_OUTPUT

    /* Memory allocation tracking */

    Allocations = AcpiDbGetOutstandingAllocations () - PreviousAllocations;

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);

    if (Allocations > 0)
    {
        AcpiOsPrintf ("Outstanding: 0x%X allocations after execution\n",
                        Allocations);
    }
#endif

    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Execution of %s failed with status %s\n",
            AcpiGbl_DbMethodInfo.Pathname, AcpiFormatException (Status));
    }

    else
    {
        /* Display a return object, if any */

        if (ReturnObj.Length)
        {
            AcpiOsPrintf ("Execution of %s returned object %p Buflen %X\n",
                AcpiGbl_DbMethodInfo.Pathname, ReturnObj.Pointer,
                (UINT32) ReturnObj.Length);
            AcpiDbDumpObject (ReturnObj.Pointer, 1);
        }
        else
        {
            AcpiOsPrintf ("No return object from execution of %s\n",
                AcpiGbl_DbMethodInfo.Pathname);
        }
    }

    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
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

void ACPI_SYSTEM_XFACE
AcpiDbMethodThread (
    void                    *Context)
{
    ACPI_STATUS             Status;
    ACPI_DB_METHOD_INFO     *Info = Context;
    UINT32                  i;
    ACPI_BUFFER             ReturnObj;


    for (i = 0; i < Info->NumLoops; i++)
    {
#if 0
       if (i == 0xEFDC)
        {
            AcpiDbgLevel = 0x00FFFFFF;
        }
#endif

        Status = AcpiDbExecuteMethod (Info, &ReturnObj);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s During execution of %s at iteration %X\n",
                AcpiFormatException (Status), Info->Pathname, i);
            if (Status == AE_ABORT_METHOD)
            {
                break;
            }
        }

        if ((i % 100) == 0)
        {
            AcpiOsPrintf ("%d executions\n", i);
        }

#if 0
        if (ReturnObj.Length)
        {
            AcpiOsPrintf ("Execution of %s returned object %p Buflen %X\n",
                Info->Pathname, ReturnObj.Pointer, (UINT32) ReturnObj.Length);
            AcpiDbDumpObject (ReturnObj.Pointer, 1);
        }
#endif
    }

    /* Signal our completion */

    Status = AcpiOsSignalSemaphore (Info->ThreadGate, 1);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not signal debugger semaphore\n");
    }
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
    char                    *NumThreadsArg,
    char                    *NumLoopsArg,
    char                    *MethodNameArg)
{
    ACPI_STATUS             Status;
    UINT32                  NumThreads;
    UINT32                  NumLoops;
    UINT32                  i;
    ACPI_HANDLE             ThreadGate;


    /* Get the arguments */

    NumThreads = ACPI_STRTOUL (NumThreadsArg, NULL, 0);
    NumLoops   = ACPI_STRTOUL (NumLoopsArg, NULL, 0);

    if (!NumThreads || !NumLoops)
    {
        AcpiOsPrintf ("Bad argument: Threads %X, Loops %X\n", NumThreads, NumLoops);
        return;
    }

    /* Create the synchronization semaphore */

    Status = AcpiOsCreateSemaphore (1, 0, &ThreadGate);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not create semaphore, %s\n", AcpiFormatException (Status));
        return;
    }

    /* Setup the context to be passed to each thread */

    AcpiGbl_DbMethodInfo.Name = MethodNameArg;
    AcpiGbl_DbMethodInfo.Args = NULL;
    AcpiGbl_DbMethodInfo.Flags = 0;
    AcpiGbl_DbMethodInfo.NumLoops = NumLoops;
    AcpiGbl_DbMethodInfo.ThreadGate = ThreadGate;

    AcpiDbExecuteSetup (&AcpiGbl_DbMethodInfo);

    /* Create the threads */

    AcpiOsPrintf ("Creating %X threads to execute %X times each\n", NumThreads, NumLoops);

    for (i = 0; i < (NumThreads); i++)
    {
        Status = AcpiOsQueueForExecution (OSD_PRIORITY_MED, AcpiDbMethodThread, &AcpiGbl_DbMethodInfo);
        if (ACPI_FAILURE (Status))
        {
            break;
        }
    }

    /* Wait for all threads to complete */

    i = NumThreads;
    while (i)   /* Brain damage for OSD implementations that only support wait of 1 unit */
    {
        Status = AcpiOsWaitSemaphore (ThreadGate, 1, ACPI_WAIT_FOREVER);
        i--;
    }

    /* Cleanup and exit */

    (void) AcpiOsDeleteSemaphore (ThreadGate);

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("All threads (%X) have completed\n", NumThreads);
    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);
}


#endif /* ACPI_DEBUGGER */


