/******************************************************************************
 *
 * Module Name: cmobject - ACPI object create/delete/size/cache routines
 *              $Revision: 30 $
 *
 *****************************************************************************/

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

#define __CMOBJECT_C__

#include "acpi.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "actables.h"
#include "amlcode.h"


#define _COMPONENT          MISCELLANEOUS
        MODULE_NAME         ("cmobject")


/******************************************************************************
 *
 * FUNCTION:    _CmCreateInternalObject
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
 * NOTE:
 *      We always allocate the worst-case object descriptor because these
 *      objects are cached, and we want them to be one-size-satisifies-any-request.
 *      This in itself may not be the most memory efficient, but the efficiency
 *      of the object cache should more than make up for this!
 *
 ******************************************************************************/

ACPI_OPERAND_OBJECT  *
_CmCreateInternalObject (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId,
    OBJECT_TYPE_INTERNAL    Type)
{
    ACPI_OPERAND_OBJECT     *Object;


    FUNCTION_TRACE_STR ("CmCreateInternalObject", AcpiCmGetTypeName (Type));


    /* Allocate the raw object descriptor */

    Object = _CmAllocateObjectDesc (ModuleName, LineNumber, ComponentId);
    if (!Object)
    {
        /* Allocation failure */

        return_VALUE (NULL);
    }

    /* Save the object type in the object descriptor */

    Object->Common.Type = Type;

    /* Init the reference count */

    Object->Common.ReferenceCount = 1;

    /* Any per-type initialization should go here */


    return_PTR (Object);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiCmValidInternalObject
 *
 * PARAMETERS:  Operand             - Object to be validated
 *
 * RETURN:      Validate a pointer to be an ACPI_OPERAND_OBJECT
 *
 *****************************************************************************/

BOOLEAN
AcpiCmValidInternalObject (
    void                    *Object)
{

    /* Check for a null pointer */

    if (!Object)
    {
        DEBUG_PRINT (ACPI_INFO,
            ("CmValidInternalObject: **** Null Object Ptr\n"));
        return (FALSE);
    }

    /* Check for a pointer within one of the ACPI tables */

    if (AcpiTbSystemTablePointer (Object))
    {
        DEBUG_PRINT (ACPI_INFO,
            ("CmValidInternalObject: **** Object %p is a Pcode Ptr\n", Object));
        return (FALSE);
    }

    /* Check the descriptor type field */

    if (!VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_INTERNAL))
    {
        /* Not an ACPI internal object, do some further checking */

        if (VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_NAMED))
        {
            DEBUG_PRINT (ACPI_INFO,
                ("CmValidInternalObject: **** Obj %p is a named obj, not ACPI obj\n",
                Object));
        }

        else if (VALID_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_PARSER))
        {
            DEBUG_PRINT (ACPI_INFO,
                ("CmValidInternalObject: **** Obj %p is a parser obj, not ACPI obj\n",
                Object));
        }

        else
        {
            DEBUG_PRINT (ACPI_INFO,
                ("CmValidInternalObject: **** Obj %p is of unknown type\n",
                Object));
        }

        return (FALSE);
    }


    /* The object appears to be a valid ACPI_OPERAND_OBJECT  */

    return (TRUE);
}


/*****************************************************************************
 *
 * FUNCTION:    _CmAllocateObjectDesc
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
 ****************************************************************************/

void *
_CmAllocateObjectDesc (
    NATIVE_CHAR             *ModuleName,
    UINT32                  LineNumber,
    UINT32                  ComponentId)
{
    ACPI_OPERAND_OBJECT     *Object;


    FUNCTION_TRACE ("_AllocateObjectDesc");


    AcpiCmAcquireMutex (ACPI_MTX_CACHES);

    AcpiGbl_ObjectCacheRequests++;

    /* Check the cache first */

    if (AcpiGbl_ObjectCache)
    {
        /* There is an object available, use it */

        Object = AcpiGbl_ObjectCache;
        AcpiGbl_ObjectCache = Object->Cache.Next;
        Object->Cache.Next = NULL;

        AcpiGbl_ObjectCacheHits++;
        AcpiGbl_ObjectCacheDepth--;

        AcpiCmReleaseMutex (ACPI_MTX_CACHES);
    }

    else
    {
        /* The cache is empty, create a new object */

        AcpiCmReleaseMutex (ACPI_MTX_CACHES);

        /* Attempt to allocate new descriptor */

        Object = _CmCallocate (sizeof (ACPI_OPERAND_OBJECT), ComponentId,
                                    ModuleName, LineNumber);
        if (!Object)
        {
            /* Allocation failed */

            _REPORT_ERROR (ModuleName, LineNumber, ComponentId,
                            ("Could not allocate an object descriptor\n"));

            return_PTR (NULL);
        }

        /* Memory allocation metrics - compiled out in non debug mode. */

        INCREMENT_OBJECT_METRICS (sizeof (ACPI_OPERAND_OBJECT));
    }

    /* Mark the descriptor type */

    Object->Common.DataType = ACPI_DESC_TYPE_INTERNAL;

    DEBUG_PRINT (TRACE_ALLOCATIONS, ("AllocateObjectDesc: %p Size 0x%x\n",
                    Object, sizeof (ACPI_OPERAND_OBJECT)));

    return_PTR (Object);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiCmDeleteObjectDesc
 *
 * PARAMETERS:  Object          - Acpi internal object to be deleted
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Free an ACPI object descriptor or add it to the object cache
 *
 ****************************************************************************/

void
AcpiCmDeleteObjectDesc (
    ACPI_OPERAND_OBJECT     *Object)
{

    FUNCTION_TRACE_PTR ("AcpiCmDeleteObjectDesc", Object);


    /* Make sure that the object isn't already in the cache */

    if (Object->Common.DataType == (ACPI_DESC_TYPE_INTERNAL | ACPI_CACHED_OBJECT))
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("CmDeleteObjectDesc: Obj %p is already in the object cache\n",
            Object));
        return_VOID;
    }

    /* Object must be an ACPI_OPERAND_OBJECT  */

    if (Object->Common.DataType != ACPI_DESC_TYPE_INTERNAL)
    {
        DEBUG_PRINT (ACPI_ERROR,
            ("CmDeleteObjectDesc: Obj %p is not an ACPI object\n", Object));
        return_VOID;
    }


    /* If cache is full, just free this object */

    if (AcpiGbl_ObjectCacheDepth >= MAX_OBJECT_CACHE_DEPTH)
    {
        /*
         * Memory allocation metrics.  Call the macro here since we only
         * care about dynamically allocated objects.
         */
        DECREMENT_OBJECT_METRICS (sizeof (ACPI_OPERAND_OBJECT));

        AcpiCmFree (Object);
        return_VOID;
    }

    AcpiCmAcquireMutex (ACPI_MTX_CACHES);

    /* Clear the entire object.  This is important! */

    MEMSET (Object, 0, sizeof (ACPI_OPERAND_OBJECT));
    Object->Common.DataType = ACPI_DESC_TYPE_INTERNAL | ACPI_CACHED_OBJECT;

    /* Put the object at the head of the global cache list */

    Object->Cache.Next = AcpiGbl_ObjectCache;
    AcpiGbl_ObjectCache = Object;
    AcpiGbl_ObjectCacheDepth++;


    AcpiCmReleaseMutex (ACPI_MTX_CACHES);
    return_VOID;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiCmDeleteObjectCache
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
AcpiCmDeleteObjectCache (
    void)
{
    ACPI_OPERAND_OBJECT     *Next;


    FUNCTION_TRACE ("CmDeleteObjectCache");


    /* Traverse the global cache list */

    while (AcpiGbl_ObjectCache)
    {
        /* Delete one cached state object */

        Next = AcpiGbl_ObjectCache->Cache.Next;
        AcpiGbl_ObjectCache->Cache.Next = NULL;

        /*
         * Memory allocation metrics.  Call the macro here since we only
         * care about dynamically allocated objects.
         */
        DECREMENT_OBJECT_METRICS (sizeof (ACPI_OPERAND_OBJECT));

        AcpiCmFree (AcpiGbl_ObjectCache);
        AcpiGbl_ObjectCache = Next;
        AcpiGbl_ObjectCacheDepth--;
    }

    return_VOID;
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiCmInitStaticObject
 *
 * PARAMETERS:  ObjDesc             - Pointer to a "static" object - on stack
 *                                    or in the data segment.
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Initialize a static object.  Sets flags to disallow dynamic
 *              deletion of the object.
 *
 ****************************************************************************/

void
AcpiCmInitStaticObject (
    ACPI_OPERAND_OBJECT     *ObjDesc)
{

    FUNCTION_TRACE_PTR ("CmInitStaticObject", ObjDesc);


    if (!ObjDesc)
    {
        return_VOID;
    }


    /*
     * Clear the entire descriptor
     */
    MEMSET ((void *) ObjDesc, 0, sizeof (ACPI_OPERAND_OBJECT));


    /*
     * Initialize the header fields
     * 1) This is an ACPI_OPERAND_OBJECT  descriptor
     * 2) The size is the full object (worst case)
     * 3) The flags field indicates static allocation
     * 4) Reference count starts at one (not really necessary since the
     *    object can't be deleted, but keeps everything sane)
     */

    ObjDesc->Common.DataType        = ACPI_DESC_TYPE_INTERNAL;
    ObjDesc->Common.Flags           = AOPOBJ_STATIC_ALLOCATION;
    ObjDesc->Common.ReferenceCount  = 1;

    return_VOID;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiCmGetSimpleObjectSize
 *
 * PARAMETERS:  *InternalObj    - Pointer to the object we are examining
 *              *RetLength      - Where the length is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to determine the space required to
 *              contain a simple object for return to an API user.
 *
 *              The length includes the object structure plus any additional
 *              needed space.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmGetSimpleObjectSize (
    ACPI_OPERAND_OBJECT     *InternalObj,
    UINT32                  *ObjLength)
{
    UINT32                  Length;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_PTR ("CmGetSimpleObjectSize", InternalObj);


    /* Handle a null object (Could be a uninitialized package element -- which is legal) */

    if (!InternalObj)
    {
        *ObjLength = 0;
        return_ACPI_STATUS (AE_OK);
    }


    /* Start with the length of the Acpi object */

    Length = sizeof (ACPI_OBJECT);

    if (VALID_DESCRIPTOR_TYPE (InternalObj, ACPI_DESC_TYPE_NAMED))
    {
        /* Object is a named object (reference), just return the length */

        *ObjLength = (UINT32) ROUND_UP_TO_NATIVE_WORD (Length);
        return_ACPI_STATUS (Status);
    }


    /*
     * The final length depends on the object type
     * Strings and Buffers are packed right up against the parent object and
     * must be accessed bytewise or there may be alignment problems.
     *
     * TBD:[Investigate] do strings and buffers require alignment also?
     */

    switch (InternalObj->Common.Type)
    {

    case ACPI_TYPE_STRING:

        Length += InternalObj->String.Length;
        break;


    case ACPI_TYPE_BUFFER:

        Length += InternalObj->Buffer.Length;
        break;


    case ACPI_TYPE_NUMBER:
    case ACPI_TYPE_PROCESSOR:
    case ACPI_TYPE_POWER:

        /*
         * No extra data for these types
         */
        break;


    case INTERNAL_TYPE_REFERENCE:

        /*
         * The only type that should be here is opcode AML_NAMEPATH_OP -- since
         * this means an object reference
         */
        if (InternalObj->Reference.OpCode != AML_NAMEPATH_OP)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("CmGetSimpleObjectSize: Unsupported Reference opcode=0x%X in object %p\n",
                InternalObj->Reference.OpCode, InternalObj));
            Status = AE_TYPE;
        }
        break;


    default:

        DEBUG_PRINT (ACPI_ERROR,
            ("CmGetSimpleObjectSize: Unsupported type=0x%X in object %p\n",
            InternalObj->Common.Type, InternalObj));
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


/******************************************************************************
 *
 * FUNCTION:    AcpiCmGetPackageObjectSize
 *
 * PARAMETERS:  *InternalObj    - Pointer to the object we are examining
 *              *RetLength      - Where the length is returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to determine the space required to contain
 *              a package object for return to an API user.
 *
 *              This is moderately complex since a package contains other objects
 *              including packages.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmGetPackageObjectSize (
    ACPI_OPERAND_OBJECT     *InternalObj,
    UINT32                  *ObjLength)
{

    ACPI_OPERAND_OBJECT     *ThisInternalObj;
    ACPI_OPERAND_OBJECT     *ParentObj[MAX_PACKAGE_DEPTH] = { 0,0,0,0,0 };
    ACPI_OPERAND_OBJECT     *ThisParent;
    UINT32                  ThisIndex;
    UINT32                  Index[MAX_PACKAGE_DEPTH] = { 0,0,0,0,0 };
    UINT32                  Length = 0;
    UINT32                  ObjectSpace;
    UINT32                  CurrentDepth = 0;
    UINT32                  PackageCount = 1;
    ACPI_STATUS             Status;


    FUNCTION_TRACE_PTR ("CmGetPackageObjectSize", InternalObj);


    ParentObj[0] = InternalObj;

    while (1)
    {
        ThisParent      = ParentObj[CurrentDepth];
        ThisIndex       = Index[CurrentDepth];
        ThisInternalObj = ThisParent->Package.Elements[ThisIndex];


        /*
         * Check for 1) An uninitialized package element.  It is completely
         *              legal to declare a package and leave it uninitialized
         *           2) Any type other than a package.  Packages are handled
         *              below.
         */

        if ((!ThisInternalObj) ||
            (!IS_THIS_OBJECT_TYPE (ThisInternalObj, ACPI_TYPE_PACKAGE)))
        {
            /*
             * Simple object - just get the size (Null object/entry handled
             *  also)
             */

            Status =
                AcpiCmGetSimpleObjectSize (ThisInternalObj, &ObjectSpace);

            if (ACPI_FAILURE (Status))
            {
                return_ACPI_STATUS (Status);
            }

            Length += ObjectSpace;

            Index[CurrentDepth]++;
            while (Index[CurrentDepth] >=
                ParentObj[CurrentDepth]->Package.Count)
            {
                /*
                 * We've handled all of the objects at
                 * this level,  This means that we have
                 * just completed a package.  That package
                 * may have contained one or more packages
                 * itself.
                 */
                if (CurrentDepth == 0)
                {
                    /*
                     * We have handled all of the objects
                     * in the top level package just add the
                     * length of the package objects and
                     * get out. Round up to the next machine
                     * word.
                     */
                    Length +=
                        ROUND_UP_TO_NATIVE_WORD (
                                sizeof (ACPI_OBJECT)) *
                                PackageCount;

                    *ObjLength = Length;

                    return_ACPI_STATUS (AE_OK);
                }

                /*
                 * Go back up a level and move the index
                 * past the just completed package object.
                 */
                CurrentDepth--;
                Index[CurrentDepth]++;
            }
        }

        else
        {
            /*
             * This object is a package
             * -- go one level deeper
             */
            PackageCount++;
            if (CurrentDepth < MAX_PACKAGE_DEPTH-1)
            {
                CurrentDepth++;
                ParentObj[CurrentDepth] = ThisInternalObj;
                Index[CurrentDepth]     = 0;
            }

            else
            {
                /*
                 * Too many nested levels of packages for us
                 * to handle
                 */

                DEBUG_PRINT (ACPI_ERROR,
                    ("CmGetPackageObjectSize: Pkg nested too deep (max %d)\n",
                    MAX_PACKAGE_DEPTH));
                return_ACPI_STATUS (AE_LIMIT);
            }
        }
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiCmGetObjectSize
 *
 * PARAMETERS:  *InternalObj    - Pointer to the object we are examining
 *              *RetLength      - Where the length will be returned
 *
 * RETURN:      Status          - the status of the call
 *
 * DESCRIPTION: This function is called to determine the space required to
 *              contain an object for return to an API user.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiCmGetObjectSize(
    ACPI_OPERAND_OBJECT     *InternalObj,
    UINT32                  *ObjLength)
{
    ACPI_STATUS             Status;


    if ((VALID_DESCRIPTOR_TYPE (InternalObj, ACPI_DESC_TYPE_INTERNAL)) &&
        (IS_THIS_OBJECT_TYPE (InternalObj, ACPI_TYPE_PACKAGE)))
    {
        Status =
            AcpiCmGetPackageObjectSize (InternalObj, ObjLength);
    }

    else
    {
        Status =
            AcpiCmGetSimpleObjectSize (InternalObj, ObjLength);
    }

    return (Status);
}


