/*******************************************************************************
 *
 * Module Name: cmutils - common utility procedures
 *              $Revision: 21 $
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


#define __CMUTILS_C__

#include "acpi.h"
#include "acevents.h"
#include "achware.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "amlcode.h"
#include "acdebug.h"


#define _COMPONENT          MISCELLANEOUS
        MODULE_NAME         ("cmutils")


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmValidAcpiName
 *
 * PARAMETERS:  Character           - The character to be examined
 *
 * RETURN:      1 if Character may appear in a name, else 0
 *
 * DESCRIPTION: Check for a valid ACPI name.  Each character must be one of:
 *              1) Upper case alpha
 *              2) numeric
 *              3) underscore
 *
 ******************************************************************************/

BOOLEAN
AcpiCmValidAcpiName (
    UINT32                  Name)
{
    NATIVE_CHAR             *NamePtr = (NATIVE_CHAR *) &Name;
    UINT32                  i;


    for (i = 0; i < ACPI_NAME_SIZE; i++)
    {
        if (!((NamePtr[i] == '_') ||
              (NamePtr[i] >= 'A' && NamePtr[i] <= 'Z') ||
              (NamePtr[i] >= '0' && NamePtr[i] <= '9')))
        {
            return (FALSE);
        }
    }


    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmValidAcpiCharacter
 *
 * PARAMETERS:  Character           - The character to be examined
 *
 * RETURN:      1 if Character may appear in a name, else 0
 *
 * DESCRIPTION: Check for a printable character
 *
 ******************************************************************************/

BOOLEAN
AcpiCmValidAcpiCharacter (
    NATIVE_CHAR             Character)
{

    return ((BOOLEAN)   ((Character == '_') ||
                        (Character >= 'A' && Character <= 'Z') ||
                        (Character >= '0' && Character <= '9')));
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmMutexInitialize
 *
 * PARAMETERS:  None.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create the system mutex objects.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmMutexInitialize (
    void)
{
    UINT32                  i;
    ACPI_STATUS             Status;


    FUNCTION_TRACE ("CmMutexInitialize");


    /*
     * Create each of the predefined mutex objects
     */
    for (i = 0; i < NUM_MTX; i++)
    {
        Status = AcpiCmCreateMutex (i);
        if (ACPI_FAILURE (Status))
        {
            return_ACPI_STATUS (Status);
        }
    }

    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmMutexTerminate
 *
 * PARAMETERS:  None.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Delete all of the system mutex objects.
 *
 ******************************************************************************/

void
AcpiCmMutexTerminate (
    void)
{
    UINT32                  i;


    FUNCTION_TRACE ("CmMutexTerminate");


    /*
     * Delete each predefined mutex object
     */
    for (i = 0; i < NUM_MTX; i++)
    {
        AcpiCmDeleteMutex (i);
    }

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmCreateMutex
 *
 * PARAMETERS:  MutexID         - ID of the mutex to be created
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a mutex object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmCreateMutex (
    ACPI_MUTEX_HANDLE       MutexId)
{
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_U32 ("CmCreateMutex", MutexId);


    if (MutexId > MAX_MTX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    if (!AcpiGbl_AcpiMutexInfo[MutexId].Mutex)
    {
        Status = AcpiOsCreateSemaphore (1, 1,
                                        &AcpiGbl_AcpiMutexInfo[MutexId].Mutex);
        AcpiGbl_AcpiMutexInfo[MutexId].Locked = FALSE;
        AcpiGbl_AcpiMutexInfo[MutexId].UseCount = 0;
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmDeleteMutex
 *
 * PARAMETERS:  MutexID         - ID of the mutex to be deleted
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Delete a mutex object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmDeleteMutex (
    ACPI_MUTEX_HANDLE       MutexId)
{
    ACPI_STATUS             Status;


    FUNCTION_TRACE_U32 ("CmDeleteMutex", MutexId);


    if (MutexId > MAX_MTX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    Status = AcpiOsDeleteSemaphore (AcpiGbl_AcpiMutexInfo[MutexId].Mutex);

    AcpiGbl_AcpiMutexInfo[MutexId].Mutex = NULL;
    AcpiGbl_AcpiMutexInfo[MutexId].Locked = FALSE;

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmAcquireMutex
 *
 * PARAMETERS:  MutexID         - ID of the mutex to be acquired
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Acquire a mutex object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmAcquireMutex (
    ACPI_MUTEX_HANDLE       MutexId)
{
    ACPI_STATUS             Status;


    DEBUG_PRINT (TRACE_MUTEX,
                ("Acquiring Mutex [%s]\n", AcpiCmGetMutexName (MutexId)));

    if (MutexId > MAX_MTX)
    {
        return (AE_BAD_PARAMETER);
    }


    Status = AcpiOsWaitSemaphore (AcpiGbl_AcpiMutexInfo[MutexId].Mutex,
                                    1, WAIT_FOREVER);

    DEBUG_PRINT (TRACE_MUTEX, ("Acquired Mutex  [%s] Status %s\n",
                AcpiCmGetMutexName (MutexId), AcpiCmFormatException (Status)));

    if (ACPI_SUCCESS (Status))
    {
        AcpiGbl_AcpiMutexInfo[MutexId].Locked = TRUE;
        AcpiGbl_AcpiMutexInfo[MutexId].UseCount++;
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmReleaseMutex
 *
 * PARAMETERS:  MutexID         - ID of the mutex to be released
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Release a mutex object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmReleaseMutex (
    ACPI_MUTEX_HANDLE       MutexId)
{
    ACPI_STATUS             Status;


    DEBUG_PRINT (TRACE_MUTEX,
        ("Releasing Mutex [%s]\n", AcpiCmGetMutexName (MutexId)));

    if (MutexId > MAX_MTX)
    {
        return (AE_BAD_PARAMETER);
    }


    AcpiGbl_AcpiMutexInfo[MutexId].Locked = FALSE;  /* Mark before unlocking */

    Status = AcpiOsSignalSemaphore (AcpiGbl_AcpiMutexInfo[MutexId].Mutex, 1);

    if (ACPI_FAILURE (Status))
    {
        DEBUG_PRINT (ACPI_ERROR, ("Error Releasing Mutex [%s], %s\n",
                    AcpiCmGetMutexName (MutexId), AcpiCmFormatException (Status)));
    }
    else
    {
        DEBUG_PRINT (TRACE_MUTEX, ("Released Mutex [%s], %s\n",
                    AcpiCmGetMutexName (MutexId), AcpiCmFormatException (Status)));
    }

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmCreateUpdateStateAndPush
 *
 * PARAMETERS:  *Object         - Object to be added to the new state
 *              Action          - Increment/Decrement
 *              StateList       - List the state will be added to
 *
 * RETURN:      None
 *
 * DESCRIPTION: Create a new state and push it
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmCreateUpdateStateAndPush (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action,
    ACPI_GENERIC_STATE      **StateList)
{
    ACPI_GENERIC_STATE       *State;


    /* Ignore null objects; these are expected */

    if (!Object)
    {
        return (AE_OK);
    }

    State = AcpiCmCreateUpdateState (Object, Action);
    if (!State)
    {
        return (AE_NO_MEMORY);
    }


    AcpiCmPushGenericState (StateList, State);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmPushGenericState
 *
 * PARAMETERS:  ListHead            - Head of the state stack
 *              State               - State object to push
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Push a state object onto a state stack
 *
 ******************************************************************************/

void
AcpiCmPushGenericState (
    ACPI_GENERIC_STATE      **ListHead,
    ACPI_GENERIC_STATE      *State)
{
    FUNCTION_TRACE ("CmPushGenericState");

    /* Push the state object onto the front of the list (stack) */

    State->Common.Next = *ListHead;
    *ListHead = State;

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmPopGenericState
 *
 * PARAMETERS:  ListHead            - Head of the state stack
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Pop a state object from a state stack
 *
 ******************************************************************************/

ACPI_GENERIC_STATE *
AcpiCmPopGenericState (
    ACPI_GENERIC_STATE      **ListHead)
{
    ACPI_GENERIC_STATE      *State;


    FUNCTION_TRACE ("DsPopGenericState");


    /* Remove the state object at the head of the list (stack) */

    State = *ListHead;
    if (State)
    {
        /* Update the list head */

        *ListHead = State->Common.Next;
    }

    return_PTR (State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmCreateGenericState
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a generic state object.  Attempt to obtain one from
 *              the global state cache;  If none available, create a new one.
 *
 ******************************************************************************/

ACPI_GENERIC_STATE *
AcpiCmCreateGenericState (void)
{
    ACPI_GENERIC_STATE      *State;


    AcpiCmAcquireMutex (ACPI_MTX_CACHES);

    AcpiGbl_StateCacheRequests++;

    /* Check the cache first */

    if (AcpiGbl_GenericStateCache)
    {
        /* There is an object available, use it */

        State = AcpiGbl_GenericStateCache;
        AcpiGbl_GenericStateCache = State->Common.Next;
        State->Common.Next = NULL;

        AcpiGbl_StateCacheHits++;
        AcpiGbl_GenericStateCacheDepth--;

        AcpiCmReleaseMutex (ACPI_MTX_CACHES);

        DEBUG_PRINT (TRACE_EXEC, ("CreateGenState: State %p from cache\n", State));
    }

    else
    {
        /* The cache is empty, create a new object */

        AcpiCmReleaseMutex (ACPI_MTX_CACHES);

        State = AcpiCmCallocate (sizeof (ACPI_GENERIC_STATE));
    }

    /* Initialize */

    if (State)
    {
        /* Always zero out the object before init */

        MEMSET (State, 0, sizeof (ACPI_GENERIC_STATE));

        State->Common.DataType = ACPI_DESC_TYPE_STATE;
    }

    return (State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmCreateUpdateState
 *
 * PARAMETERS:  Object              - Initial Object to be installed in the
 *                                    state
 *              Action              - Update action to be performed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create an "Update State" - a flavor of the generic state used
 *              to update reference counts and delete complex objects such
 *              as packages.
 *
 ******************************************************************************/

ACPI_GENERIC_STATE *
AcpiCmCreateUpdateState (
    ACPI_OPERAND_OBJECT     *Object,
    UINT16                  Action)
{
    ACPI_GENERIC_STATE      *State;


    FUNCTION_TRACE_PTR ("CmCreateUpdateState", Object);


    /* Create the generic state object */

    State = AcpiCmCreateGenericState ();
    if (!State)
    {
        return (NULL);
    }

    /* Init fields specific to the update struct */

    State->Update.Object = Object;
    State->Update.Value  = Action;

    return_PTR (State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmCreateControlState
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Create a "Control State" - a flavor of the generic state used
 *              to support nested IF/WHILE constructs in the AML.
 *
 ******************************************************************************/

ACPI_GENERIC_STATE *
AcpiCmCreateControlState (
    void)
{
    ACPI_GENERIC_STATE      *State;


    FUNCTION_TRACE ("CmCreateControlState");

    /* Create the generic state object */

    State = AcpiCmCreateGenericState ();
    if (!State)
    {
        return (NULL);
    }


    /* Init fields specific to the control struct */

    State->Common.State = CONTROL_CONDITIONAL_EXECUTING;

    return_PTR (State);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmDeleteGenericState
 *
 * PARAMETERS:  State               - The state object to be deleted
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Put a state object back into the global state cache.  The object
 *              is not actually freed at this time.
 *
 ******************************************************************************/

void
AcpiCmDeleteGenericState (
    ACPI_GENERIC_STATE      *State)
{
    FUNCTION_TRACE ("CmDeleteGenericState");


    /* If cache is full, just free this state object */

    if (AcpiGbl_GenericStateCacheDepth >= MAX_STATE_CACHE_DEPTH)
    {
        AcpiCmFree (State);
    }

    /* Otherwise put this object back into the cache */

    else
    {
        AcpiCmAcquireMutex (ACPI_MTX_CACHES);

        /* Clear the state */

        MEMSET (State, 0, sizeof (ACPI_GENERIC_STATE));
        State->Common.DataType = ACPI_DESC_TYPE_STATE;

        /* Put the object at the head of the global cache list */

        State->Common.Next = AcpiGbl_GenericStateCache;
        AcpiGbl_GenericStateCache = State;
        AcpiGbl_GenericStateCacheDepth++;


        AcpiCmReleaseMutex (ACPI_MTX_CACHES);
    }
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmDeleteGenericStateCache
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Purge the global state object cache.  Used during subsystem
 *              termination.
 *
 ******************************************************************************/

void
AcpiCmDeleteGenericStateCache (
    void)
{
    ACPI_GENERIC_STATE      *Next;


    FUNCTION_TRACE ("CmDeleteGenericStateCache");


    /* Traverse the global cache list */

    while (AcpiGbl_GenericStateCache)
    {
        /* Delete one cached state object */

        Next = AcpiGbl_GenericStateCache->Common.Next;
        AcpiCmFree (AcpiGbl_GenericStateCache);
        AcpiGbl_GenericStateCache = Next;
        AcpiGbl_GenericStateCacheDepth--;
    }

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiCmResolvePackageReferences
 *
 * PARAMETERS:  ObjDesc         - The Package object on which to resolve refs
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk through a package and turn internal references into values
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmResolvePackageReferences (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    UINT32              Count;
    ACPI_OPERAND_OBJECT *SubObject;

    FUNCTION_TRACE ("AcpiCmResolvePackageReferences");

    if (ObjDesc->Common.Type != ACPI_TYPE_PACKAGE)
    {
        /* Must be a package */

        REPORT_ERROR (("Must resolve Package Refs on a Package\n"));
        return_ACPI_STATUS(AE_ERROR);
    }

    for (Count = 0; Count < ObjDesc->Package.Count; Count++)
    {
        SubObject = ObjDesc->Package.Elements[Count];

        if (SubObject->Common.Type == INTERNAL_TYPE_REFERENCE)
        {
            if (SubObject->Reference.OpCode == AML_ZERO_OP)
            {
                SubObject->Common.Type  = ACPI_TYPE_NUMBER;
                SubObject->Number.Value = 0;
            }
            else if (SubObject->Reference.OpCode == AML_ONE_OP)
            {
                SubObject->Common.Type  = ACPI_TYPE_NUMBER;
                SubObject->Number.Value = 1;
            }
            else if (SubObject->Reference.OpCode == AML_ONES_OP)
            {
                SubObject->Common.Type  = ACPI_TYPE_NUMBER;
                SubObject->Number.Value = ACPI_INTEGER_MAX;
            }
        }
    }

    return_ACPI_STATUS(AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    _ReportError
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print error message from KD table
 *
 ******************************************************************************/

void
_ReportError (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId)
{


    AcpiOsPrintf ("%8s-%04d: *** Error: ", ModuleName, LineNumber);
}


/*******************************************************************************
 *
 * FUNCTION:    _ReportWarning
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print warning message from KD table
 *
 ******************************************************************************/

void
_ReportWarning (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId)
{

    AcpiOsPrintf ("%8s-%04d: *** Warning: ", ModuleName, LineNumber);
}


/*******************************************************************************
 *
 * FUNCTION:    _ReportInfo
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print information message from KD table
 *
 ******************************************************************************/

void
_ReportInfo (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId)
{

    AcpiOsPrintf ("%8s-%04d: *** Info: ", ModuleName, LineNumber);
}


