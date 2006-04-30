/*******************************************************************************
 *
 * Module Name: nseval - Object evaluation interfaces -- includes control
 *                       method lookup and execution.
 *              $Revision: 129 $
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

#define __NSEVAL_C__

#include <contrib/dev/acpica/acpi.h>
#include <contrib/dev/acpica/acparser.h>
#include <contrib/dev/acpica/acinterp.h>
#include <contrib/dev/acpica/acnamesp.h>


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nseval")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsEvaluateRelative
 *
 * PARAMETERS:  Pathname            - Name of method to execute, If NULL, the
 *                                    handle is the object to execute
 *              Info                - Method info block
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find and execute the requested method using the handle as a
 *              scope
 *
 * MUTEX:       Locks Namespace
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsEvaluateRelative (
    char                    *Pathname,
    ACPI_PARAMETER_INFO     *Info)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node = NULL;
    ACPI_GENERIC_STATE      *ScopeInfo;
    char                    *InternalPath = NULL;


    ACPI_FUNCTION_TRACE ("NsEvaluateRelative");


    /*
     * Must have a valid object handle
     */
    if (!Info || !Info->Node)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Build an internal name string for the method */

    Status = AcpiNsInternalizeName (Pathname, &InternalPath);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    ScopeInfo = AcpiUtCreateGenericState ();
    if (!ScopeInfo)
    {
        goto Cleanup1;
    }

    /* Get the prefix handle and Node */

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    Info->Node = AcpiNsMapHandleToNode (Info->Node);
    if (!Info->Node)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
        Status = AE_BAD_PARAMETER;
        goto Cleanup;
    }

    /* Lookup the name in the namespace */

    ScopeInfo->Scope.Node = Info->Node;
    Status = AcpiNsLookup (ScopeInfo, InternalPath, ACPI_TYPE_ANY,
                            ACPI_IMODE_EXECUTE, ACPI_NS_NO_UPSEARCH, NULL,
                            &Node);

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);

    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Object [%s] not found [%s]\n",
            Pathname, AcpiFormatException (Status)));
        goto Cleanup;
    }

    /*
     * Now that we have a handle to the object, we can attempt to evaluate it.
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "%s [%p] Value %p\n",
        Pathname, Node, AcpiNsGetAttachedObject (Node)));

    Info->Node = Node;
    Status = AcpiNsEvaluateByHandle (Info);

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "*** Completed eval of object %s ***\n",
        Pathname));

Cleanup:
    AcpiUtDeleteGenericState (ScopeInfo);

Cleanup1:
    ACPI_MEM_FREE (InternalPath);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsEvaluateByName
 *
 * PARAMETERS:  Pathname            - Fully qualified pathname to the object
 *              Info                - Contains:
 *                  ReturnObject    - Where to put method's return value (if
 *                                    any).  If NULL, no value is returned.
 *                  Params          - List of parameters to pass to the method,
 *                                    terminated by NULL.  Params itself may be
 *                                    NULL if no parameters are being passed.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Find and execute the requested method passing the given
 *              parameters
 *
 * MUTEX:       Locks Namespace
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsEvaluateByName (
    char                    *Pathname,
    ACPI_PARAMETER_INFO     *Info)
{
    ACPI_STATUS             Status;
    char                    *InternalPath = NULL;


    ACPI_FUNCTION_TRACE ("NsEvaluateByName");


    /* Build an internal name string for the method */

    Status = AcpiNsInternalizeName (Pathname, &InternalPath);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        goto Cleanup;
    }

    /* Lookup the name in the namespace */

    Status = AcpiNsLookup (NULL, InternalPath, ACPI_TYPE_ANY,
                            ACPI_IMODE_EXECUTE, ACPI_NS_NO_UPSEARCH, NULL,
                            &Info->Node);

    (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);

    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_NAMES,
            "Object at [%s] was not found, status=%.4X\n",
            Pathname, Status));
        goto Cleanup;
    }

    /*
     * Now that we have a handle to the object, we can attempt to evaluate it.
     */
    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "%s [%p] Value %p\n",
        Pathname, Info->Node, AcpiNsGetAttachedObject (Info->Node)));

    Status = AcpiNsEvaluateByHandle (Info);

    ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "*** Completed eval of object %s ***\n",
        Pathname));


Cleanup:

    /* Cleanup */

    if (InternalPath)
    {
        ACPI_MEM_FREE (InternalPath);
    }

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsEvaluateByHandle
 *
 * PARAMETERS:  Handle              - Method Node to execute
 *              Params              - List of parameters to pass to the method,
 *                                    terminated by NULL.  Params itself may be
 *                                    NULL if no parameters are being passed.
 *              ParamType           - Type of Parameter list
 *              ReturnObject        - Where to put method's return value (if
 *                                    any).  If NULL, no value is returned.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute the requested method passing the given parameters
 *
 * MUTEX:       Locks Namespace
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsEvaluateByHandle (
    ACPI_PARAMETER_INFO     *Info)
{
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("NsEvaluateByHandle");


    /* Check if namespace has been initialized */

    if (!AcpiGbl_RootNode)
    {
        return_ACPI_STATUS (AE_NO_NAMESPACE);
    }

    /* Parameter Validation */

    if (!Info)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Initialize the return value to an invalid object */

    Info->ReturnObject = NULL;

    /* Get the prefix handle and Node */

    Status = AcpiUtAcquireMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Info->Node = AcpiNsMapHandleToNode (Info->Node);
    if (!Info->Node)
    {
        (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /*
     * For a method alias, we must grab the actual method node so that proper
     * scoping context will be established before execution.
     */
    if (AcpiNsGetType (Info->Node) == ACPI_TYPE_LOCAL_METHOD_ALIAS)
    {
        Info->Node = ACPI_CAST_PTR (ACPI_NAMESPACE_NODE, Info->Node->Object);
    }

    /*
     * Two major cases here:
     * 1) The object is an actual control method -- execute it.
     * 2) The object is not a method -- just return it's current value
     *
     * In both cases, the namespace is unlocked by the AcpiNs* procedure
     */
    if (AcpiNsGetType (Info->Node) == ACPI_TYPE_METHOD)
    {
        /*
         * Case 1) We have an actual control method to execute
         */
        Status = AcpiNsExecuteControlMethod (Info);
    }
    else
    {
        /*
         * Case 2) Object is NOT a method, just return its current value
         */
        Status = AcpiNsGetObjectValue (Info);
    }

    /*
     * Check if there is a return value on the stack that must be dealt with
     */
    if (Status == AE_CTRL_RETURN_VALUE)
    {
        /* Map AE_CTRL_RETURN_VALUE to AE_OK, we are done with it */

        Status = AE_OK;
    }

    /*
     * Namespace was unlocked by the handling AcpiNs* function, so we
     * just return
     */
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsExecuteControlMethod
 *
 * PARAMETERS:  Info            - Method info block (w/params)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Execute the requested method passing the given parameters
 *
 * MUTEX:       Assumes namespace is locked
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsExecuteControlMethod (
    ACPI_PARAMETER_INFO     *Info)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ObjDesc;


    ACPI_FUNCTION_TRACE ("NsExecuteControlMethod");


    /* Verify that there is a method associated with this object */

    ObjDesc = AcpiNsGetAttachedObject (Info->Node);
    if (!ObjDesc)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "No attached method object\n"));

        (void) AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
        return_ACPI_STATUS (AE_NULL_OBJECT);
    }

    ACPI_DUMP_PATHNAME (Info->Node, "Execute Method:",
        ACPI_LV_INFO, _COMPONENT);

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Method at AML address %p Length %X\n",
        ObjDesc->Method.AmlStart + 1, ObjDesc->Method.AmlLength - 1));

    /*
     * Unlock the namespace before execution.  This allows namespace access
     * via the external Acpi* interfaces while a method is being executed.
     * However, any namespace deletion must acquire both the namespace and
     * interpreter locks to ensure that no thread is using the portion of the
     * namespace that is being deleted.
     */
    Status = AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Execute the method via the interpreter.  The interpreter is locked
     * here before calling into the AML parser
     */
    Status = AcpiExEnterInterpreter ();
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiPsxExecute (Info);
    AcpiExExitInterpreter ();

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetObjectValue
 *
 * PARAMETERS:  Info            - Method info block (w/params)
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Return the current value of the object
 *
 * MUTEX:       Assumes namespace is locked, leaves namespace unlocked
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsGetObjectValue (
    ACPI_PARAMETER_INFO     *Info)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_NAMESPACE_NODE     *ResolvedNode = Info->Node;


    ACPI_FUNCTION_TRACE ("NsGetObjectValue");


    /*
     * Objects require additional resolution steps (e.g., the Node may be a
     * field that must be read, etc.) -- we can't just grab the object out of
     * the node.
     */

    /*
     * Use ResolveNodeToValue() to get the associated value.  This call always
     * deletes ObjDesc (allocated above).
     *
     * NOTE: we can get away with passing in NULL for a walk state because
     * ObjDesc is guaranteed to not be a reference to either a method local or
     * a method argument (because this interface can only be called from the
     * AcpiEvaluate external interface, never called from a running method.)
     *
     * Even though we do not directly invoke the interpreter for this, we must
     * enter it because we could access an opregion. The opregion access code
     * assumes that the interpreter is locked.
     *
     * We must release the namespace lock before entering the intepreter.
     */
    Status = AcpiUtReleaseMutex (ACPI_MTX_NAMESPACE);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    Status = AcpiExEnterInterpreter ();
    if (ACPI_SUCCESS (Status))
    {
        Status = AcpiExResolveNodeToValue (&ResolvedNode, NULL);
        /*
         * If AcpiExResolveNodeToValue() succeeded, the return value was placed
         * in ResolvedNode.
         */
        AcpiExExitInterpreter ();

        if (ACPI_SUCCESS (Status))
        {
            Status = AE_CTRL_RETURN_VALUE;
            Info->ReturnObject = ACPI_CAST_PTR
                                    (ACPI_OPERAND_OBJECT, ResolvedNode);
            ACPI_DEBUG_PRINT ((ACPI_DB_NAMES, "Returning object %p [%s]\n",
                Info->ReturnObject,
                AcpiUtGetObjectTypeName (Info->ReturnObject)));
        }
    }

    /* Namespace is unlocked */

    return_ACPI_STATUS (Status);
}

