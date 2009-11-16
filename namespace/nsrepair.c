/******************************************************************************
 *
 * Module Name: nsrepair - Repair for objects returned by predefined methods
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2009, Intel Corp.
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

#define __NSREPAIR_C__

#include "acpi.h"
#include "accommon.h"
#include "acnamesp.h"
#include "acinterp.h"
#include "acpredef.h"

#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsrepair")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsRepairObject
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ExpectedBtypes      - Object types expected
 *              PackageIndex        - Index of object within parent package (if
 *                                    applicable - ACPI_NOT_PACKAGE_ELEMENT
 *                                    otherwise)
 *              ReturnObjectPtr     - Pointer to the object returned from the
 *                                    evaluation of a method or object
 *
 * RETURN:      Status. AE_OK if repair was successful.
 *
 * DESCRIPTION: Attempt to repair/convert a return object of a type that was
 *              not expected.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsRepairObject (
    ACPI_PREDEFINED_DATA    *Data,
    UINT32                  ExpectedBtypes,
    UINT32                  PackageIndex,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr)
{
    ACPI_OPERAND_OBJECT     *ReturnObject = *ReturnObjectPtr;
    ACPI_OPERAND_OBJECT     *NewObject;
    ACPI_SIZE               Length;
    ACPI_STATUS             Status;


    /*
     * At this point, we know that the type of the returned object was not
     * one of the expected types for this predefined name. Attempt to
     * repair the object. Only a limited number of repairs are possible.
     */
    switch (ReturnObject->Common.Type)
    {
    case ACPI_TYPE_BUFFER:

        /* Does the method/object legally return a string? */

        if (!(ExpectedBtypes & ACPI_RTYPE_STRING))
        {
            return (AE_AML_OPERAND_TYPE);
        }

        /*
         * Have a Buffer, expected a String, convert. Use a ToString
         * conversion, no transform performed on the buffer data. The best
         * example of this is the _BIF method, where the string data from
         * the battery is often (incorrectly) returned as buffer object(s).
         */
        Length = 0;
        while ((Length < ReturnObject->Buffer.Length) &&
                (ReturnObject->Buffer.Pointer[Length]))
        {
            Length++;
        }

        /* Allocate a new string object */

        NewObject = AcpiUtCreateStringObject (Length);
        if (!NewObject)
        {
            return (AE_NO_MEMORY);
        }

        /*
         * Copy the raw buffer data with no transform. String is already NULL
         * terminated at Length+1.
         */
        ACPI_MEMCPY (NewObject->String.Pointer,
            ReturnObject->Buffer.Pointer, Length);
        break;


    case ACPI_TYPE_INTEGER:

        /* 1) Does the method/object legally return a buffer? */

        if (ExpectedBtypes & ACPI_RTYPE_BUFFER)
        {
            /*
             * Convert the Integer to a packed-byte buffer. _MAT needs
             * this sometimes, if a read has been performed on a Field
             * object that is less than or equal to the global integer
             * size (32 or 64 bits).
             */
            Status = AcpiExConvertToBuffer (ReturnObject, &NewObject);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }

        /* 2) Does the method/object legally return a string? */

        else if (ExpectedBtypes & ACPI_RTYPE_STRING)
        {
            /*
             * The only supported Integer-to-String conversion is to convert
             * an integer of value 0 to a NULL string. The last element of
             * _BIF and _BIX packages occasionally need this fix.
             */
            if (ReturnObject->Integer.Value != 0)
            {
                return (AE_AML_OPERAND_TYPE);
            }

            /* Allocate a new NULL string object */

            NewObject = AcpiUtCreateStringObject (0);
            if (!NewObject)
            {
                return (AE_NO_MEMORY);
            }
        }
        else
        {
            return (AE_AML_OPERAND_TYPE);
        }
        break;


    default:

        /* We cannot repair this object */

        return (AE_AML_OPERAND_TYPE);
    }

    /* Object was successfully repaired */

    /*
     * If the original object is a package element, we need to:
     * 1. Set the reference count of the new object to match the
     *    reference count of the old object.
     * 2. Decrement the reference count of the original object.
     */
    if (PackageIndex != ACPI_NOT_PACKAGE_ELEMENT)
    {
        NewObject->Common.ReferenceCount =
            ReturnObject->Common.ReferenceCount;

        if (ReturnObject->Common.ReferenceCount > 1)
        {
            ReturnObject->Common.ReferenceCount--;
        }

        ACPI_INFO_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
            "Converted %s to expected %s at index %u",
            AcpiUtGetObjectTypeName (ReturnObject),
            AcpiUtGetObjectTypeName (NewObject), PackageIndex));
    }
    else
    {
        ACPI_INFO_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
            "Converted %s to expected %s",
            AcpiUtGetObjectTypeName (ReturnObject),
            AcpiUtGetObjectTypeName (NewObject)));
    }

    /* Delete old object, install the new return object */

    AcpiUtRemoveReference (ReturnObject);
    *ReturnObjectPtr = NewObject;
    Data->Flags |= ACPI_OBJECT_REPAIRED;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsRepairPackageList
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ObjDescPtr          - Pointer to the object to repair. The new
 *                                    package object is returned here,
 *                                    overwriting the old object.
 *
 * RETURN:      Status, new object in *ObjDescPtr
 *
 * DESCRIPTION: Repair a common problem with objects that are defined to return
 *              a variable-length Package of Packages. If the variable-length
 *              is one, some BIOS code mistakenly simply declares a single
 *              Package instead of a Package with one sub-Package. This
 *              function attempts to repair this error by wrapping a Package
 *              object around the original Package, creating the correct
 *              Package with one sub-Package.
 *
 *              Names that can be repaired in this manner include:
 *              _ALR, _CSD, _HPX, _MLS, _PRT, _PSS, _TRT, TSS
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsRepairPackageList (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ObjDescPtr)
{
    ACPI_OPERAND_OBJECT     *PkgObjDesc;


    /*
     * Create the new outer package and populate it. The new package will
     * have a single element, the lone subpackage.
     */
    PkgObjDesc = AcpiUtCreatePackageObject (1);
    if (!PkgObjDesc)
    {
        return (AE_NO_MEMORY);
    }

    PkgObjDesc->Package.Elements[0] = *ObjDescPtr;

    /* Return the new object in the object pointer */

    *ObjDescPtr = PkgObjDesc;
    Data->Flags |= ACPI_OBJECT_REPAIRED;

    ACPI_INFO_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
        "Repaired Incorrectly formed Package"));

    return (AE_OK);
}
