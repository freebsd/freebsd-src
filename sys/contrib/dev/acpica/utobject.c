/******************************************************************************
 *
 * Module Name: utobject - ACPI object create/delete/size/cache routines
 *              $Revision: 61 $
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

#define __UTOBJECT_C__

#include "acpi.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "actables.h"
#include "amlcode.h"


#define _COMPONENT          ACPI_UTILITIES
        MODULE_NAME         ("utobject")


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCreateInternalObjectDbg
 *
 * PARAMETERS:  Address             - Address of the memory to deallocate
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *              Type                - ACPI Type of the new object
 *
 * RETURN:      Object              - The new object.  Null on failure
 *
 * DESCRIPTION: Create and initialize a new internal object.
 *
 * NOTE:        We always allocate the worst-case object descriptor because
 *              these objects are cached, and we want them to be
 *              one-size-satisifies-any-request.  This in itself may not be
 *              the most memory efficient, but the efficiency of the object
 *              cache should more than make up for this!
 *
 ******************************************************************************/

ACPI_OPERAND_OBJECT  *
AcpiUtCreateInternalObjectDbg (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    ACPI_OBJECT_TYPE8       Type)
{
    ACPI_OPERAND_OBJECT     *Object;
    ACPI_OPERAND_OBJECT     *SecondObject;


    FUNCTION_TRACE_STR ("UtCreateInternalObjectDbg", AcpiUtGetTypeName (Type));


    /* Allocate the raw object descriptor */

    Object = AcpiUtAllocateObjectDescDbg (ModuleName, LineNumber, ComponentId);
    if (!Object)
    {
        return_PTR (NULL);
    }

    switch (Type)
    {
    case ACPI_TYPE_REGION:
    case ACPI_TYPE_BUFFER_FIELD:
        
        /* These types require a secondary object */

        SecondObject = AcpiUtAllocateObjectDescDbg (ModuleName, LineNumber, ComponentId);
        if (!SecondObject)
        {
            AcpiUtDeleteObjectDesc (Object);
            return_PTR (NULL);
        }

        SecondObject->Common.Type = INTERNAL_TYPE_EXTRA;
        SecondObject->Common.ReferenceCount = 1;

        /* Link the second object to the first */

        Object->Common.NextObject = SecondObject;
        break;
    }

    /* Save the object type in the object descriptor */

    Object->Common.Type = Type;

    /* Init the reference count */

    Object->Common.ReferenceCount = 1;

    /* Any per-type initialization should go here */

    return_PTR (Object);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidInternalObject
 *
 * PARAMETERS:  Operand             - Object to be validated
 *
 * RETURN:      Validate a pointer to be an ACPI_OPERAND_OBJECT
 *
 ******************************************************************************/

BOOLEAN
AcpiUtValidInternalObject (
    void                    *Object)
{

    PROC_NAME ("UtValidInternalObject");


    /* Check for a null pointer */

    if (!Object)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
            "**** Null Object Ptr\n"));
        return (FALSE);
    }

    /* Check the descriptor type field */

    if (!VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_INTERNAL))
    {
        /* Not an ACPI internal object, do some further checking */

        if (VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_NAMED))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                "**** Obj %p is a named obj, not ACPI obj\n", Object));
        }

        else if (VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_PARSER))
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                "**** Obj %p is a parser obj, not ACPI obj\n", Object));
        }

        else
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_INFO,
                "**** Obj %p is of unknown type\n", Object));
        }

        return (FALSE);
    }


    /* The object appears to be a valid ACPI_OPERAND_OBJECT  */

    return (TRUE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAllocateObjectDescDbg
 *
 * PARAMETERS:  ModuleName          - Caller's module name (for error output)
 *              LineNumber          - Caller's line number (for error output)
 *              ComponentId         - Caller's component ID (for error output)
 *              Message             - Error message to use on failure
 *
 * RETURN:      Pointer to newly allocated object descriptor.  Null on error
 *
 * DESCRIPTION: Allocate a new object descriptor.  Gracefully handle
 *              error conditions.
 *
 ******************************************************************************/

void *
AcpiUtAllocateObjectDescDbg (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId)
{
    ACPI_OPERAND_OBJECT     *Object;


    FUNCTION_TRACE ("UtAllocateObjectDescDbg");


    Object = AcpiUtAcquireFromCache (ACPI_MEM_LIST_OPERAND);
    if (!Object)
    {
        _REPORT_ERROR (ModuleName, LineNumber, ComponentId,
                        ("Could not allocate an object descriptor\n"));

        return_PTR (NULL);
    }


    /* Mark the descriptor type */

    Object->Common.DataType = ACPI_DESC_TYPE_INTERNAL;

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "%p Size %X\n",
            Object, sizeof (ACPI_OPERAND_OBJECT)));

    return_PTR (Object);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDeleteObjectDesc
 *
 * PARAMETERS:  Object          - Acpi internal object to be deleted
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Free an ACPI object descriptor or add it to the object cache
 *
 ******************************************************************************/

void
AcpiUtDeleteObjectDesc (
    ACPI_OPERAND_OBJECT     *Object)
{
    FUNCTION_TRACE_PTR ("UtDeleteObjectDesc", Object);


    /* Object must be an ACPI_OPERAND_OBJECT  */

    if (Object->Common.DataType != ACPI_DESC_TYPE_INTERNAL)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "Obj %p is not an ACPI object\n", Object));
        return_VOID;
    }

    AcpiUtReleaseToCache (ACPI_MEM_LIST_OPERAND, Object);

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDeleteObjectCache
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
AcpiUtDeleteObjectCache (
    void)
{
    FUNCTION_TRACE ("UtDeleteObjectCache");


    AcpiUtDeleteGenericCache (ACPI_MEM_LIST_OPERAND);
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetSimpleObjectSize
 *
 * PARAMETERS:  *InternalObject     - Pointer to the object we are examining
 *              *RetLength          - Where the length is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to determine the space required to
 *              contain a simple object for return to an API user.
 *
 *              The length includes the object structure plus any additional
 *              needed space.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtGetSimpleObjectSize (
    ACPI_OPERAND_OBJECT     *InternalObject,
    UINT32                  *ObjLength)
{
    UINT32                  Length;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_PTR ("UtGetSimpleObjectSize", InternalObject);


    /* Handle a null object (Could be a uninitialized package element -- which is legal) */

    if (!InternalObject)
    {
        *ObjLength = 0;
        return_ACPI_STATUS (AE_OK);
    }


    /* Start with the length of the Acpi object */

    Length = sizeof (ACPI_OBJECT);

    if (VALID_DESCRIPTOR_TYPE (InternalObject, ACPI_DESC_TYPE_NAMED))
    {
        /* Object is a named object (reference), just return the length */

        *ObjLength = (UINT32) ROUND_UP_TO_NATIVE_WORD (Length);
        return_ACPI_STATUS (Status);
    }


    /*
     * The final length depends on the object type
     * Strings and Buffers are packed right up against the parent object and
     * must be accessed bytewise or there may be alignment problems on
     * certain processors
     */

    switch (InternalObject->Common.Type)
    {

    case ACPI_TYPE_STRING:

        Length += InternalObject->String.Length + 1;
        break;


    case ACPI_TYPE_BUFFER:

        Length += InternalObject->Buffer.Length;
        break;


    case ACPI_TYPE_INTEGER:
    case ACPI_TYPE_PROCESSOR:
    case ACPI_TYPE_POWER:

        /*
         * No extra data for these types
         */
        break;


    case INTERNAL_TYPE_REFERENCE:

        /*
         * The only type that should be here is internal opcode NAMEPATH_OP -- since
         * this means an object reference
         */
        if (InternalObject->Reference.Opcode != AML_INT_NAMEPATH_OP)
        {
            ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
                "Unsupported Reference opcode=%X in object %p\n",
                InternalObject->Reference.Opcode, InternalObject));
            Status = AE_TYPE;
        }

        else
        {
            /*
             * Get the actual length of the full pathname to this object.
             * The reference will be converted to the pathname to the object
             */
            Length += ROUND_UP_TO_NATIVE_WORD (AcpiNsGetPathnameLength (InternalObject->Reference.Node));
        }
        break;


    default:

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Unsupported type=%X in object %p\n",
            InternalObject->Common.Type, InternalObject));
        Status = AE_TYPE;
        break;
    }


    /*
     * Account for the space required by the object rounded up to the next
     * multiple of the machine word size.  This keeps each object aligned
     * on a machine word boundary. (preventing alignment faults on some
     * machines.)
     */
    *ObjLength = (UINT32) ROUND_UP_TO_NATIVE_WORD (Length);

    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetElementLength
 *
 * PARAMETERS:  ACPI_PKG_CALLBACK
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: Get the length of one package element.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtGetElementLength (
    UINT8                   ObjectType,
    ACPI_OPERAND_OBJECT     *SourceObject,
    ACPI_GENERIC_STATE      *State,
    void                    *Context)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_PKG_INFO           *Info = (ACPI_PKG_INFO *) Context;
    UINT32                  ObjectSpace;


    switch (ObjectType)
    {
    case 0:

        /*
         * Simple object - just get the size (Null object/entry is handled
         * here also) and sum it into the running package length
         */
        Status = AcpiUtGetSimpleObjectSize (SourceObject, &ObjectSpace);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        Info->Length += ObjectSpace;
        break;


    case 1:
        /* Package - nothing much to do here, let the walk handle it */

        Info->NumPackages++;
        State->Pkg.ThisTargetObj = NULL;
        break;

    default:
        return (AE_BAD_PARAMETER);
    }


    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetPackageObjectSize
 *
 * PARAMETERS:  *InternalObject     - Pointer to the object we are examining
 *              *RetLength          - Where the length is returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to determine the space required to
 *              contain a package object for return to an API user.
 *
 *              This is moderately complex since a package contains other
 *              objects including packages.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtGetPackageObjectSize (
    ACPI_OPERAND_OBJECT     *InternalObject,
    UINT32                  *ObjLength)
{
    ACPI_STATUS             Status;
    ACPI_PKG_INFO           Info;


    FUNCTION_TRACE_PTR ("UtGetPackageObjectSize", InternalObject);


    Info.Length      = 0;
    Info.ObjectSpace = 0;
    Info.NumPackages = 1;

    Status = AcpiUtWalkPackageTree (InternalObject, NULL,
                            AcpiUtGetElementLength, &Info);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * We have handled all of the objects in all levels of the package.
     * just add the length of the package objects themselves.
     * Round up to the next machine word.
     */
    Info.Length += ROUND_UP_TO_NATIVE_WORD (sizeof (ACPI_OBJECT)) *
                    Info.NumPackages;

    /* Return the total package length */

    *ObjLength = Info.Length;
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtGetObjectSize
 *
 * PARAMETERS:  *InternalObject     - Pointer to the object we are examining
 *              *RetLength          - Where the length will be returned
 *
 * RETURN:      Status
 *
 * DESCRIPTION: This function is called to determine the space required to
 *              contain an object for return to an API user.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtGetObjectSize(
    ACPI_OPERAND_OBJECT     *InternalObject,
    UINT32                  *ObjLength)
{
    ACPI_STATUS             Status;


    FUNCTION_ENTRY ();


    if ((VALID_DESCRIPTOR_TYPE (InternalObject, ACPI_DESC_TYPE_INTERNAL)) &&
        (IS_THIS_OBJECT_TYPE (InternalObject, ACPI_TYPE_PACKAGE)))
    {
        Status = AcpiUtGetPackageObjectSize (InternalObject, ObjLength);
    }

    else
    {
        Status = AcpiUtGetSimpleObjectSize (InternalObject, ObjLength);
    }

    return (Status);
}


