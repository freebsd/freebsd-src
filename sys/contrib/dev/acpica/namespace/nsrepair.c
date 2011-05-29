/******************************************************************************
 *
 * Module Name: nsrepair - Repair for objects returned by predefined methods
 *
 *****************************************************************************/

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

#define __NSREPAIR_C__

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acnamesp.h>
#include <contrib/dev/acpica/include/acinterp.h>
#include <contrib/dev/acpica/include/acpredef.h>

#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsrepair")


/*******************************************************************************
 *
 * This module attempts to repair or convert objects returned by the
 * predefined methods to an object type that is expected, as per the ACPI
 * specification. The need for this code is dictated by the many machines that
 * return incorrect types for the standard predefined methods. Performing these
 * conversions here, in one place, eliminates the need for individual ACPI
 * device drivers to do the same. Note: Most of these conversions are different
 * than the internal object conversion routines used for implicit object
 * conversion.
 *
 * The following conversions can be performed as necessary:
 *
 * Integer -> String
 * Integer -> Buffer
 * String  -> Integer
 * String  -> Buffer
 * Buffer  -> Integer
 * Buffer  -> String
 * Buffer  -> Package of Integers
 * Package -> Package of one Package
 *
 * Additional possible repairs:
 *
 * Required package elements that are NULL replaced by Integer/String/Buffer
 * Incorrect standalone package wrapped with required outer package
 *
 ******************************************************************************/


/* Local prototypes */

static ACPI_STATUS
AcpiNsConvertToInteger (
    ACPI_OPERAND_OBJECT     *OriginalObject,
    ACPI_OPERAND_OBJECT     **ReturnObject);

static ACPI_STATUS
AcpiNsConvertToString (
    ACPI_OPERAND_OBJECT     *OriginalObject,
    ACPI_OPERAND_OBJECT     **ReturnObject);

static ACPI_STATUS
AcpiNsConvertToBuffer (
    ACPI_OPERAND_OBJECT     *OriginalObject,
    ACPI_OPERAND_OBJECT     **ReturnObject);

static ACPI_STATUS
AcpiNsConvertToPackage (
    ACPI_OPERAND_OBJECT     *OriginalObject,
    ACPI_OPERAND_OBJECT     **ReturnObject);


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
    ACPI_STATUS             Status;


    ACPI_FUNCTION_NAME (NsRepairObject);


    /*
     * At this point, we know that the type of the returned object was not
     * one of the expected types for this predefined name. Attempt to
     * repair the object by converting it to one of the expected object
     * types for this predefined name.
     */
    if (ExpectedBtypes & ACPI_RTYPE_INTEGER)
    {
        Status = AcpiNsConvertToInteger (ReturnObject, &NewObject);
        if (ACPI_SUCCESS (Status))
        {
            goto ObjectRepaired;
        }
    }
    if (ExpectedBtypes & ACPI_RTYPE_STRING)
    {
        Status = AcpiNsConvertToString (ReturnObject, &NewObject);
        if (ACPI_SUCCESS (Status))
        {
            goto ObjectRepaired;
        }
    }
    if (ExpectedBtypes & ACPI_RTYPE_BUFFER)
    {
        Status = AcpiNsConvertToBuffer (ReturnObject, &NewObject);
        if (ACPI_SUCCESS (Status))
        {
            goto ObjectRepaired;
        }
    }
    if (ExpectedBtypes & ACPI_RTYPE_PACKAGE)
    {
        Status = AcpiNsConvertToPackage (ReturnObject, &NewObject);
        if (ACPI_SUCCESS (Status))
        {
            goto ObjectRepaired;
        }
    }

    /* We cannot repair this object */

    return (AE_AML_OPERAND_TYPE);


ObjectRepaired:

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

        ACPI_DEBUG_PRINT ((ACPI_DB_REPAIR,
            "%s: Converted %s to expected %s at index %u\n",
            Data->Pathname, AcpiUtGetObjectTypeName (ReturnObject),
            AcpiUtGetObjectTypeName (NewObject), PackageIndex));
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_REPAIR,
            "%s: Converted %s to expected %s\n",
            Data->Pathname, AcpiUtGetObjectTypeName (ReturnObject),
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
 * FUNCTION:    AcpiNsConvertToInteger
 *
 * PARAMETERS:  OriginalObject      - Object to be converted
 *              ReturnObject        - Where the new converted object is returned
 *
 * RETURN:      Status. AE_OK if conversion was successful.
 *
 * DESCRIPTION: Attempt to convert a String/Buffer object to an Integer.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsConvertToInteger (
    ACPI_OPERAND_OBJECT     *OriginalObject,
    ACPI_OPERAND_OBJECT     **ReturnObject)
{
    ACPI_OPERAND_OBJECT     *NewObject;
    ACPI_STATUS             Status;
    UINT64                  Value = 0;
    UINT32                  i;


    switch (OriginalObject->Common.Type)
    {
    case ACPI_TYPE_STRING:

        /* String-to-Integer conversion */

        Status = AcpiUtStrtoul64 (OriginalObject->String.Pointer,
                    ACPI_ANY_BASE, &Value);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        break;

    case ACPI_TYPE_BUFFER:

        /* Buffer-to-Integer conversion. Max buffer size is 64 bits. */

        if (OriginalObject->Buffer.Length > 8)
        {
            return (AE_AML_OPERAND_TYPE);
        }

        /* Extract each buffer byte to create the integer */

        for (i = 0; i < OriginalObject->Buffer.Length; i++)
        {
            Value |= ((UINT64) OriginalObject->Buffer.Pointer[i] << (i * 8));
        }
        break;

    default:
        return (AE_AML_OPERAND_TYPE);
    }

    NewObject = AcpiUtCreateIntegerObject (Value);
    if (!NewObject)
    {
        return (AE_NO_MEMORY);
    }

    *ReturnObject = NewObject;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsConvertToString
 *
 * PARAMETERS:  OriginalObject      - Object to be converted
 *              ReturnObject        - Where the new converted object is returned
 *
 * RETURN:      Status. AE_OK if conversion was successful.
 *
 * DESCRIPTION: Attempt to convert a Integer/Buffer object to a String.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsConvertToString (
    ACPI_OPERAND_OBJECT     *OriginalObject,
    ACPI_OPERAND_OBJECT     **ReturnObject)
{
    ACPI_OPERAND_OBJECT     *NewObject;
    ACPI_SIZE               Length;
    ACPI_STATUS             Status;


    switch (OriginalObject->Common.Type)
    {
    case ACPI_TYPE_INTEGER:
        /*
         * Integer-to-String conversion. Commonly, convert
         * an integer of value 0 to a NULL string. The last element of
         * _BIF and _BIX packages occasionally need this fix.
         */
        if (OriginalObject->Integer.Value == 0)
        {
            /* Allocate a new NULL string object */

            NewObject = AcpiUtCreateStringObject (0);
            if (!NewObject)
            {
                return (AE_NO_MEMORY);
            }
        }
        else
        {
            Status = AcpiExConvertToString (OriginalObject, &NewObject,
                        ACPI_IMPLICIT_CONVERT_HEX);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
        }
        break;

    case ACPI_TYPE_BUFFER:
        /*
         * Buffer-to-String conversion. Use a ToString
         * conversion, no transform performed on the buffer data. The best
         * example of this is the _BIF method, where the string data from
         * the battery is often (incorrectly) returned as buffer object(s).
         */
        Length = 0;
        while ((Length < OriginalObject->Buffer.Length) &&
                (OriginalObject->Buffer.Pointer[Length]))
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
            OriginalObject->Buffer.Pointer, Length);
        break;

    default:
        return (AE_AML_OPERAND_TYPE);
    }

    *ReturnObject = NewObject;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsConvertToBuffer
 *
 * PARAMETERS:  OriginalObject      - Object to be converted
 *              ReturnObject        - Where the new converted object is returned
 *
 * RETURN:      Status. AE_OK if conversion was successful.
 *
 * DESCRIPTION: Attempt to convert a Integer/String/Package object to a Buffer.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsConvertToBuffer (
    ACPI_OPERAND_OBJECT     *OriginalObject,
    ACPI_OPERAND_OBJECT     **ReturnObject)
{
    ACPI_OPERAND_OBJECT     *NewObject;
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     **Elements;
    UINT32                  *DwordBuffer;
    UINT32                  Count;
    UINT32                  i;


    switch (OriginalObject->Common.Type)
    {
    case ACPI_TYPE_INTEGER:
        /*
         * Integer-to-Buffer conversion.
         * Convert the Integer to a packed-byte buffer. _MAT and other
         * objects need this sometimes, if a read has been performed on a
         * Field object that is less than or equal to the global integer
         * size (32 or 64 bits).
         */
        Status = AcpiExConvertToBuffer (OriginalObject, &NewObject);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }
        break;

    case ACPI_TYPE_STRING:

        /* String-to-Buffer conversion. Simple data copy */

        NewObject = AcpiUtCreateBufferObject (OriginalObject->String.Length);
        if (!NewObject)
        {
            return (AE_NO_MEMORY);
        }

        ACPI_MEMCPY (NewObject->Buffer.Pointer,
            OriginalObject->String.Pointer, OriginalObject->String.Length);
        break;

    case ACPI_TYPE_PACKAGE:
        /*
         * This case is often seen for predefined names that must return a
         * Buffer object with multiple DWORD integers within. For example,
         * _FDE and _GTM. The Package can be converted to a Buffer.
         */

        /* All elements of the Package must be integers */

        Elements = OriginalObject->Package.Elements;
        Count = OriginalObject->Package.Count;

        for (i = 0; i < Count; i++)
        {
            if ((!*Elements) ||
                ((*Elements)->Common.Type != ACPI_TYPE_INTEGER))
            {
                return (AE_AML_OPERAND_TYPE);
            }
            Elements++;
        }

        /* Create the new buffer object to replace the Package */

        NewObject = AcpiUtCreateBufferObject (ACPI_MUL_4 (Count));
        if (!NewObject)
        {
            return (AE_NO_MEMORY);
        }

        /* Copy the package elements (integers) to the buffer as DWORDs */

        Elements = OriginalObject->Package.Elements;
        DwordBuffer = ACPI_CAST_PTR (UINT32, NewObject->Buffer.Pointer);

        for (i = 0; i < Count; i++)
        {
            *DwordBuffer = (UINT32) (*Elements)->Integer.Value;
            DwordBuffer++;
            Elements++;
        }
        break;

    default:
        return (AE_AML_OPERAND_TYPE);
    }

    *ReturnObject = NewObject;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsConvertToPackage
 *
 * PARAMETERS:  OriginalObject      - Object to be converted
 *              ReturnObject        - Where the new converted object is returned
 *
 * RETURN:      Status. AE_OK if conversion was successful.
 *
 * DESCRIPTION: Attempt to convert a Buffer object to a Package. Each byte of
 *              the buffer is converted to a single integer package element.
 *
 ******************************************************************************/

static ACPI_STATUS
AcpiNsConvertToPackage (
    ACPI_OPERAND_OBJECT     *OriginalObject,
    ACPI_OPERAND_OBJECT     **ReturnObject)
{
    ACPI_OPERAND_OBJECT     *NewObject;
    ACPI_OPERAND_OBJECT     **Elements;
    UINT32                  Length;
    UINT8                   *Buffer;


    switch (OriginalObject->Common.Type)
    {
    case ACPI_TYPE_BUFFER:

        /* Buffer-to-Package conversion */

        Length = OriginalObject->Buffer.Length;
        NewObject = AcpiUtCreatePackageObject (Length);
        if (!NewObject)
        {
            return (AE_NO_MEMORY);
        }

        /* Convert each buffer byte to an integer package element */

        Elements = NewObject->Package.Elements;
        Buffer = OriginalObject->Buffer.Pointer;

        while (Length--)
        {
            *Elements = AcpiUtCreateIntegerObject ((UINT64) *Buffer);
            if (!*Elements)
            {
                AcpiUtRemoveReference (NewObject);
                return (AE_NO_MEMORY);
            }
            Elements++;
            Buffer++;
        }
        break;

    default:
        return (AE_AML_OPERAND_TYPE);
    }

    *ReturnObject = NewObject;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsRepairNullElement
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
 * DESCRIPTION: Attempt to repair a NULL element of a returned Package object.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsRepairNullElement (
    ACPI_PREDEFINED_DATA    *Data,
    UINT32                  ExpectedBtypes,
    UINT32                  PackageIndex,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr)
{
    ACPI_OPERAND_OBJECT     *ReturnObject = *ReturnObjectPtr;
    ACPI_OPERAND_OBJECT     *NewObject;


    ACPI_FUNCTION_NAME (NsRepairNullElement);


    /* No repair needed if return object is non-NULL */

    if (ReturnObject)
    {
        return (AE_OK);
    }

    /*
     * Attempt to repair a NULL element of a Package object. This applies to
     * predefined names that return a fixed-length package and each element
     * is required. It does not apply to variable-length packages where NULL
     * elements are allowed, especially at the end of the package.
     */
    if (ExpectedBtypes & ACPI_RTYPE_INTEGER)
    {
        /* Need an Integer - create a zero-value integer */

        NewObject = AcpiUtCreateIntegerObject ((UINT64) 0);
    }
    else if (ExpectedBtypes & ACPI_RTYPE_STRING)
    {
        /* Need a String - create a NULL string */

        NewObject = AcpiUtCreateStringObject (0);
    }
    else if (ExpectedBtypes & ACPI_RTYPE_BUFFER)
    {
        /* Need a Buffer - create a zero-length buffer */

        NewObject = AcpiUtCreateBufferObject (0);
    }
    else
    {
        /* Error for all other expected types */

        return (AE_AML_OPERAND_TYPE);
    }

    if (!NewObject)
    {
        return (AE_NO_MEMORY);
    }

    /* Set the reference count according to the parent Package object */

    NewObject->Common.ReferenceCount = Data->ParentPackage->Common.ReferenceCount;

    ACPI_DEBUG_PRINT ((ACPI_DB_REPAIR,
        "%s: Converted NULL package element to expected %s at index %u\n",
         Data->Pathname, AcpiUtGetObjectTypeName (NewObject), PackageIndex));

    *ReturnObjectPtr = NewObject;
    Data->Flags |= ACPI_OBJECT_REPAIRED;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsRemoveNullElements
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              PackageType         - An AcpiReturnPackageTypes value
 *              ObjDesc             - A Package object
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Remove all NULL package elements from packages that contain
 *              a variable number of sub-packages. For these types of
 *              packages, NULL elements can be safely removed.
 *
 *****************************************************************************/

void
AcpiNsRemoveNullElements (
    ACPI_PREDEFINED_DATA    *Data,
    UINT8                   PackageType,
    ACPI_OPERAND_OBJECT     *ObjDesc)
{
    ACPI_OPERAND_OBJECT     **Source;
    ACPI_OPERAND_OBJECT     **Dest;
    UINT32                  Count;
    UINT32                  NewCount;
    UINT32                  i;


    ACPI_FUNCTION_NAME (NsRemoveNullElements);


    /*
     * We can safely remove all NULL elements from these package types:
     * PTYPE1_VAR packages contain a variable number of simple data types.
     * PTYPE2 packages contain a variable number of sub-packages.
     */
    switch (PackageType)
    {
    case ACPI_PTYPE1_VAR:
    case ACPI_PTYPE2:
    case ACPI_PTYPE2_COUNT:
    case ACPI_PTYPE2_PKG_COUNT:
    case ACPI_PTYPE2_FIXED:
    case ACPI_PTYPE2_MIN:
    case ACPI_PTYPE2_REV_FIXED:
        break;

    default:
    case ACPI_PTYPE1_FIXED:
    case ACPI_PTYPE1_OPTION:
        return;
    }

    Count = ObjDesc->Package.Count;
    NewCount = Count;

    Source = ObjDesc->Package.Elements;
    Dest = Source;

    /* Examine all elements of the package object, remove nulls */

    for (i = 0; i < Count; i++)
    {
        if (!*Source)
        {
            NewCount--;
        }
        else
        {
            *Dest = *Source;
            Dest++;
        }
        Source++;
    }

    /* Update parent package if any null elements were removed */

    if (NewCount < Count)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_REPAIR,
            "%s: Found and removed %u NULL elements\n",
            Data->Pathname, (Count - NewCount)));

        /* NULL terminate list and update the package count */

        *Dest = NULL;
        ObjDesc->Package.Count = NewCount;
    }
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


    ACPI_FUNCTION_NAME (NsRepairPackageList);


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

    ACPI_DEBUG_PRINT ((ACPI_DB_REPAIR,
        "%s: Repaired incorrectly formed Package\n", Data->Pathname));

    return (AE_OK);
}
