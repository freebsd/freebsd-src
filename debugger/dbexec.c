/*******************************************************************************
 *
 * Module Name: dbexec - debugger control method execution
 *
 ******************************************************************************/

/*
 * Copyright (C) 2000 - 2011, Intel Corp.
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


#include "acpi.h"
#include "accommon.h"
#include "acdebug.h"
#include "acnamesp.h"

#ifdef ACPI_DEBUGGER

#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dbexec")


static ACPI_DB_METHOD_INFO  AcpiGbl_DbMethodInfo;

/* Local prototypes */

static ACPI_STATUS
AcpiDbExecuteMethod (
    ACPI_DB_METHOD_INFO     *Info,
    ACPI_BUFFER             *ReturnObj);

static void
AcpiDbExecuteSetup (
    ACPI_DB_METHOD_INFO     *Info);

static UINT32
AcpiDbGetOutstandingAllocations (
    void);

static void ACPI_SYSTEM_XFACE
AcpiDbMethodThread (
    void                    *Context);

static ACPI_STATUS
AcpiDbExecutionWalk (
    ACPI_HANDLE             ObjHandle,
    UINT32                  NestingLevel,
    void                    *Context,
    void                    **ReturnValue);


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

static ACPI_STATUS
AcpiDbExecuteMethod (
    ACPI_DB_METHOD_INFO     *Info,
    ACPI_BUFFER             *ReturnObj)
{
    ACPI_STATUS             Status;
    ACPI_OBJECT_LIST        ParamObjects;
    ACPI_OBJECT             Params[ACPI_METHOD_NUM_ARGS];
    ACPI_HANDLE             Handle;
    UINT32                  i;
    ACPI_DEVICE_INFO        *ObjInfo;


    ACPI_FUNCTION_TRACE (DbExecuteMethod);


    if (AcpiGbl_DbOutputToFile && !AcpiDbgLevel)
    {
        AcpiOsPrintf ("Warning: debug output is not enabled!\n");
    }

    /* Get the NS node, determines existence also */

    Status = AcpiGetHandle (NULL, Info->Pathname, &Handle);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Get the object info for number of method parameters */

    Status = AcpiGetObjectInfo (Handle, &ObjInfo);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    ParamObjects.Pointer = NULL;
    ParamObjects.Count   = 0;

    if (ObjInfo->Type == ACPI_TYPE_METHOD)
    {
        /* Are there arguments to the method? */

        if (Info->Args && Info->Args[0])
        {
            for (i = 0; Info->Args[i] &&
                (i < ACPI_METHOD_NUM_ARGS) &&
                (i < ObjInfo->ParamCount);
                i++)
            {
                Params[i].Type          = ACPI_TYPE_INTEGER;
                Params[i].Integer.Value = ACPI_STRTOUL (Info->Args[i], NULL, 16);
            }

            ParamObjects.Pointer = Params;
            ParamObjects.Count   = i;
        }
        else
        {
            /* Setup default parameters */

            for (i = 0; i < ObjInfo->ParamCount; i++)
            {
                switch (i)
                {
                case 0:

                    Params[0].Type           = ACPI_TYPE_INTEGER;
                    Params[0].Integer.Value  = 0x01020304;
                    break;

                case 1:

                    Params[1].Type           = ACPI_TYPE_STRING;
                    Params[1].String.Length  = 12;
                    Params[1].String.Pointer = "AML Debugger";
                    break;

                default:

                    Params[i].Type           = ACPI_TYPE_INTEGER;
                    Params[i].Integer.Value  = i * (UINT64) 0x1000;
                    break;
                }
            }

            ParamObjects.Pointer     = Params;
            ParamObjects.Count       = ObjInfo->ParamCount;
        }
    }

    ACPI_FREE (ObjInfo);

    /* Prepare for a return object of arbitrary size */

    ReturnObj->Pointer = AcpiGbl_DbBuffer;
    ReturnObj->Length  = ACPI_DEBUG_BUFFER_SIZE;

    /* Do the actual method execution */

    AcpiGbl_MethodExecuting = TRUE;
    Status = AcpiEvaluateObject (NULL,
                Info->Pathname, &ParamObjects, ReturnObj);

    AcpiGbl_CmSingleStep = FALSE;
    AcpiGbl_MethodExecuting = FALSE;

    if (ACPI_FAILURE (Status))
    {
        ACPI_EXCEPTION ((AE_INFO, Status,
        "while executing %s from debugger", Info->Pathname));

        if (Status == AE_BUFFER_OVERFLOW)
        {
            ACPI_ERROR ((AE_INFO,
            "Possible overflow of internal debugger buffer (size 0x%X needed 0x%X)",
                ACPI_DEBUG_BUFFER_SIZE, (UINT32) ReturnObj->Length));
        }
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDbExecuteSetup
 *
 * PARAMETERS:  Info            - Valid method info
 *
 * RETURN:      None
 *
 * DESCRIPTION: Setup info segment prior to method execution
 *
 ******************************************************************************/

static void
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


#ifdef ACPI_DBG_TRACK_ALLOCATIONS
UINT32
AcpiDbGetCacheInfo (
    ACPI_MEMORY_LIST        *Cache)
{

    return (Cache->TotalAllocated - Cache->TotalFreed - Cache->CurrentDepth);
}
#endif

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

static UINT32
AcpiDbGetOutstandingAllocations (
    void)
{
    UINT32                  Outstanding = 0;

#ifdef ACPI_DBG_TRACK_ALLOCATIONS

    Outstanding += AcpiDbGetCacheInfo (AcpiGbl_StateCache);
    Outstanding += AcpiDbGetCacheInfo (AcpiGbl_PsNodeCache);
    Outstanding += AcpiDbGetCacheInfo (AcpiGbl_PsNodeExtCache);
    Outstanding += AcpiDbGetCacheInfo (AcpiGbl_OperandCache);
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

static ACPI_STATUS
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
 * RETURN:      None
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
    char                    *NameString;


#ifdef ACPI_DEBUG_OUTPUT
    UINT32                  PreviousAllocations;
    UINT32                  Allocations;


    /* Memory allocation tracking */

    PreviousAllocations = AcpiDbGetOutstandingAllocations ();
#endif

    if (*Name == '*')
    {
        (void) AcpiWalkNamespace (ACPI_TYPE_METHOD, ACPI_ROOT_OBJECT,
                    ACPI_UINT32_MAX, AcpiDbExecutionWalk, NULL, NULL, NULL);
        return;
    }
    else
    {
        NameString = ACPI_ALLOCATE (ACPI_STRLEN (Name) + 1);
        if (!NameString)
        {
            return;
        }

        ACPI_MEMSET (&AcpiGbl_DbMethodInfo, 0, sizeof (ACPI_DB_METHOD_INFO));

        ACPI_STRCPY (NameString, Name);
        AcpiUtStrupr (NameString);
        AcpiGbl_DbMethodInfo.Name = NameString;
        AcpiGbl_DbMethodInfo.Args = Args;
        AcpiGbl_DbMethodInfo.Flags = Flags;

        ReturnObj.Pointer = NULL;
        ReturnObj.Length = ACPI_ALLOCATE_BUFFER;

        AcpiDbExecuteSetup (&AcpiGbl_DbMethodInfo);
        Status = AcpiDbExecuteMethod (&AcpiGbl_DbMethodInfo, &ReturnObj);
        ACPI_FREE (NameString);
    }

    /*
     * Allow any handlers in separate threads to complete.
     * (Such as Notify handlers invoked from AML executed above).
     */
    AcpiOsSleep ((UINT64) 10);


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
            AcpiDbDumpExternalObject (ReturnObj.Pointer, 1);
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

static void ACPI_SYSTEM_XFACE
AcpiDbMethodThread (
    void                    *Context)
{
    ACPI_STATUS             Status;
    ACPI_DB_METHOD_INFO     *Info = Context;
    ACPI_DB_METHOD_INFO     LocalInfo;
    UINT32                  i;
    UINT8                   Allow;
    ACPI_BUFFER             ReturnObj;


    /*
     * AcpiGbl_DbMethodInfo.Arguments will be passed as method arguments.
     * Prevent AcpiGbl_DbMethodInfo from being modified by multiple threads
     * concurrently.
     *
     * Note: The arguments we are passing are used by the ASL test suite
     * (aslts). Do not change them without updating the tests.
     */
    (void) AcpiOsWaitSemaphore (Info->InfoGate, 1, ACPI_WAIT_FOREVER);

    if (Info->InitArgs)
    {
        AcpiDbUInt32ToHexString (Info->NumCreated, Info->IndexOfThreadStr);
        AcpiDbUInt32ToHexString ((UINT32) AcpiOsGetThreadId (), Info->IdOfThreadStr);
    }

    if (Info->Threads && (Info->NumCreated < Info->NumThreads))
    {
        Info->Threads[Info->NumCreated++] = AcpiOsGetThreadId();
    }

    LocalInfo = *Info;
    LocalInfo.Args = LocalInfo.Arguments;
    LocalInfo.Arguments[0] = LocalInfo.NumThreadsStr;
    LocalInfo.Arguments[1] = LocalInfo.IdOfThreadStr;
    LocalInfo.Arguments[2] = LocalInfo.IndexOfThreadStr;
    LocalInfo.Arguments[3] = NULL;

    (void) AcpiOsSignalSemaphore (Info->InfoGate, 1);

    for (i = 0; i < Info->NumLoops; i++)
    {
        Status = AcpiDbExecuteMethod (&LocalInfo, &ReturnObj);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("%s During execution of %s at iteration %X\n",
                AcpiFormatException (Status), Info->Pathname, i);
            if (Status == AE_ABORT_METHOD)
            {
                break;
            }
        }

#if 0
        if ((i % 100) == 0)
        {
            AcpiOsPrintf ("%u executions, Thread 0x%x\n", i, AcpiOsGetThreadId ());
        }

        if (ReturnObj.Length)
        {
            AcpiOsPrintf ("Execution of %s returned object %p Buflen %X\n",
                Info->Pathname, ReturnObj.Pointer, (UINT32) ReturnObj.Length);
            AcpiDbDumpExternalObject (ReturnObj.Pointer, 1);
        }
#endif
    }

    /* Signal our completion */

    Allow = 0;
    (void) AcpiOsWaitSemaphore (Info->ThreadCompleteGate, 1, ACPI_WAIT_FOREVER);
    Info->NumCompleted++;

    if (Info->NumCompleted == Info->NumThreads)
    {
        /* Do signal for main thread once only */
        Allow = 1;
    }

    (void) AcpiOsSignalSemaphore (Info->ThreadCompleteGate, 1);

    if (Allow)
    {
        Status = AcpiOsSignalSemaphore (Info->MainThreadGate, 1);
        if (ACPI_FAILURE (Status))
        {
            AcpiOsPrintf ("Could not signal debugger thread sync semaphore, %s\n",
                AcpiFormatException (Status));
        }
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
    UINT32                  Size;
    ACPI_MUTEX              MainThreadGate;
    ACPI_MUTEX              ThreadCompleteGate;
    ACPI_MUTEX              InfoGate;


    /* Get the arguments */

    NumThreads = ACPI_STRTOUL (NumThreadsArg, NULL, 0);
    NumLoops   = ACPI_STRTOUL (NumLoopsArg, NULL, 0);

    if (!NumThreads || !NumLoops)
    {
        AcpiOsPrintf ("Bad argument: Threads %X, Loops %X\n",
            NumThreads, NumLoops);
        return;
    }

    /*
     * Create the semaphore for synchronization of
     * the created threads with the main thread.
     */
    Status = AcpiOsCreateSemaphore (1, 0, &MainThreadGate);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not create semaphore for synchronization with the main thread, %s\n",
            AcpiFormatException (Status));
        return;
    }

    /*
     * Create the semaphore for synchronization
     * between the created threads.
     */
    Status = AcpiOsCreateSemaphore (1, 1, &ThreadCompleteGate);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not create semaphore for synchronization between the created threads, %s\n",
            AcpiFormatException (Status));
        (void) AcpiOsDeleteSemaphore (MainThreadGate);
        return;
    }

    Status = AcpiOsCreateSemaphore (1, 1, &InfoGate);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsPrintf ("Could not create semaphore for synchronization of AcpiGbl_DbMethodInfo, %s\n",
            AcpiFormatException (Status));
        (void) AcpiOsDeleteSemaphore (ThreadCompleteGate);
        (void) AcpiOsDeleteSemaphore (MainThreadGate);
        return;
    }

    ACPI_MEMSET (&AcpiGbl_DbMethodInfo, 0, sizeof (ACPI_DB_METHOD_INFO));

    /* Array to store IDs of threads */

    AcpiGbl_DbMethodInfo.NumThreads = NumThreads;
    Size = sizeof (ACPI_THREAD_ID) * AcpiGbl_DbMethodInfo.NumThreads;
    AcpiGbl_DbMethodInfo.Threads = AcpiOsAllocate (Size);
    if (AcpiGbl_DbMethodInfo.Threads == NULL)
    {
        AcpiOsPrintf ("No memory for thread IDs array\n");
        (void) AcpiOsDeleteSemaphore (MainThreadGate);
        (void) AcpiOsDeleteSemaphore (ThreadCompleteGate);
        (void) AcpiOsDeleteSemaphore (InfoGate);
        return;
    }
    ACPI_MEMSET (AcpiGbl_DbMethodInfo.Threads, 0, Size);

    /* Setup the context to be passed to each thread */

    AcpiGbl_DbMethodInfo.Name = MethodNameArg;
    AcpiGbl_DbMethodInfo.Flags = 0;
    AcpiGbl_DbMethodInfo.NumLoops = NumLoops;
    AcpiGbl_DbMethodInfo.MainThreadGate = MainThreadGate;
    AcpiGbl_DbMethodInfo.ThreadCompleteGate = ThreadCompleteGate;
    AcpiGbl_DbMethodInfo.InfoGate = InfoGate;

    /* Init arguments to be passed to method */

    AcpiGbl_DbMethodInfo.InitArgs = 1;
    AcpiGbl_DbMethodInfo.Args = AcpiGbl_DbMethodInfo.Arguments;
    AcpiGbl_DbMethodInfo.Arguments[0] = AcpiGbl_DbMethodInfo.NumThreadsStr;
    AcpiGbl_DbMethodInfo.Arguments[1] = AcpiGbl_DbMethodInfo.IdOfThreadStr;
    AcpiGbl_DbMethodInfo.Arguments[2] = AcpiGbl_DbMethodInfo.IndexOfThreadStr;
    AcpiGbl_DbMethodInfo.Arguments[3] = NULL;
    AcpiDbUInt32ToHexString (NumThreads, AcpiGbl_DbMethodInfo.NumThreadsStr);

    AcpiDbExecuteSetup (&AcpiGbl_DbMethodInfo);

    /* Create the threads */

    AcpiOsPrintf ("Creating %X threads to execute %X times each\n",
        NumThreads, NumLoops);

    for (i = 0; i < (NumThreads); i++)
    {
        Status = AcpiOsExecute (OSL_DEBUGGER_THREAD, AcpiDbMethodThread,
            &AcpiGbl_DbMethodInfo);
        if (ACPI_FAILURE (Status))
        {
            break;
        }
    }

    /* Wait for all threads to complete */

    (void) AcpiOsWaitSemaphore (MainThreadGate, 1, ACPI_WAIT_FOREVER);

    AcpiDbSetOutputDestination (ACPI_DB_DUPLICATE_OUTPUT);
    AcpiOsPrintf ("All threads (%X) have completed\n", NumThreads);
    AcpiDbSetOutputDestination (ACPI_DB_CONSOLE_OUTPUT);

    /* Cleanup and exit */

    (void) AcpiOsDeleteSemaphore (MainThreadGate);
    (void) AcpiOsDeleteSemaphore (ThreadCompleteGate);
    (void) AcpiOsDeleteSemaphore (InfoGate);

    AcpiOsFree (AcpiGbl_DbMethodInfo.Threads);
    AcpiGbl_DbMethodInfo.Threads = NULL;
}

#endif /* ACPI_DEBUGGER */


