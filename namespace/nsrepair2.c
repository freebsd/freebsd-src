/******************************************************************************
 *
 * Module Name: nsrepair2 - Repair for objects returned by specific
 *                          predefined methods
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

#define __NSREPAIR2_C__

#include "acpi.h"
#include "accommon.h"
#include "acnamesp.h"

#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsrepair2")


/*
 * Information structure and handler for ACPI predefined names that can
 * be repaired on a per-name basis.
 */
typedef
ACPI_STATUS (*ACPI_REPAIR_FUNCTION) (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr);

typedef struct acpi_repair_info
{
    char                    Name[ACPI_NAME_SIZE];
    ACPI_REPAIR_FUNCTION    RepairFunction;

} ACPI_REPAIR_INFO;


/* Local prototypes */

static const ACPI_REPAIR_INFO *
AcpiNsMatchRepairableName (
    ACPI_NAMESPACE_NODE     *Node);

static ACPI_STATUS
AcpiNsRepair_ALR (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr);

static ACPI_STATUS
AcpiNsRepair_CID (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr);

static ACPI_STATUS
AcpiNsRepair_FDE (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr);

static ACPI_STATUS
AcpiNsRepair_HID (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr);

static ACPI_STATUS
AcpiNsRepair_PSS (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr);

static ACPI_STATUS
AcpiNsRepair_TSS (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr);

static ACPI_STATUS
AcpiNsCheckSortedList (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     *ReturnObject,
    UINT32                  ExpectedCount,
    UINT32                  SortIndex,
    UINT8                   SortDirection,
    char                    *SortKeyName);

static void
AcpiNsSortList (
    ACPI_OPERAND_OBJECT     **Elements,
    UINT32                  Count,
    UINT32                  Index,
    UINT8                   SortDirection);

/* Values for SortDirection above */

#define ACPI_SORT_ASCENDING     0
#define ACPI_SORT_DESCENDING    1


/*
 * This table contains the names of the predefined methods for which we can
 * perform more complex repairs.
 *
 * As necessary:
 *
 * _ALR: Sort the list ascending by AmbientIlluminance
 * _CID: Strings: uppercase all, remove any leading asterisk
 * _FDE: Convert Buffer of BYTEs to a Buffer of DWORDs
 * _GTM: Convert Buffer of BYTEs to a Buffer of DWORDs
 * _HID: Strings: uppercase all, remove any leading asterisk
 * _PSS: Sort the list descending by Power
 * _TSS: Sort the list descending by Power
 *
 * Names that must be packages, but cannot be sorted:
 *
 * _BCL: Values are tied to the Package index where they appear, and cannot
 * be moved or sorted. These index values are used for _BQC and _BCM.
 * However, we can fix the case where a buffer is returned, by converting
 * it to a Package of integers.
 */
static const ACPI_REPAIR_INFO       AcpiNsRepairableNames[] =
{
    {"_ALR", AcpiNsRepair_ALR},
    {"_CID", AcpiNsRepair_CID},
    {"_FDE", AcpiNsRepair_FDE},
    {"_GTM", AcpiNsRepair_FDE},     /* _GTM has same repair as _FDE */
    {"_HID", AcpiNsRepair_HID},
    {"_PSS", AcpiNsRepair_PSS},
    {"_TSS", AcpiNsRepair_TSS},
    {{0,0,0,0}, NULL}               /* Table terminator */
};


#define ACPI_FDE_FIELD_COUNT        5
#define ACPI_FDE_BYTE_BUFFER_SIZE   5
#define ACPI_FDE_DWORD_BUFFER_SIZE  (ACPI_FDE_FIELD_COUNT * sizeof (UINT32))


/******************************************************************************
 *
 * FUNCTION:    AcpiNsComplexRepairs
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              Node                - Namespace node for the method/object
 *              ValidateStatus      - Original status of earlier validation
 *              ReturnObjectPtr     - Pointer to the object returned from the
 *                                    evaluation of a method or object
 *
 * RETURN:      Status. AE_OK if repair was successful. If name is not
 *              matched, ValidateStatus is returned.
 *
 * DESCRIPTION: Attempt to repair/convert a return object of a type that was
 *              not expected.
 *
 *****************************************************************************/

ACPI_STATUS
AcpiNsComplexRepairs (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_STATUS             ValidateStatus,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr)
{
    const ACPI_REPAIR_INFO  *Predefined;
    ACPI_STATUS             Status;


    /* Check if this name is in the list of repairable names */

    Predefined = AcpiNsMatchRepairableName (Node);
    if (!Predefined)
    {
        return (ValidateStatus);
    }

    Status = Predefined->RepairFunction (Data, ReturnObjectPtr);
    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsMatchRepairableName
 *
 * PARAMETERS:  Node                - Namespace node for the method/object
 *
 * RETURN:      Pointer to entry in repair table. NULL indicates not found.
 *
 * DESCRIPTION: Check an object name against the repairable object list.
 *
 *****************************************************************************/

static const ACPI_REPAIR_INFO *
AcpiNsMatchRepairableName (
    ACPI_NAMESPACE_NODE     *Node)
{
    const ACPI_REPAIR_INFO  *ThisName;


    /* Search info table for a repairable predefined method/object name */

    ThisName = AcpiNsRepairableNames;
    while (ThisName->RepairFunction)
    {
        if (ACPI_COMPARE_NAME (Node->Name.Ascii, ThisName->Name))
        {
            return (ThisName);
        }
        ThisName++;
    }

    return (NULL); /* Not found */
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsRepair_ALR
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ReturnObjectPtr     - Pointer to the object returned from the
 *                                    evaluation of a method or object
 *
 * RETURN:      Status. AE_OK if object is OK or was repaired successfully
 *
 * DESCRIPTION: Repair for the _ALR object. If necessary, sort the object list
 *              ascending by the ambient illuminance values.
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiNsRepair_ALR (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr)
{
    ACPI_OPERAND_OBJECT     *ReturnObject = *ReturnObjectPtr;
    ACPI_STATUS             Status;


    Status = AcpiNsCheckSortedList (Data, ReturnObject, 2, 1,
                ACPI_SORT_ASCENDING, "AmbientIlluminance");

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsRepair_FDE
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ReturnObjectPtr     - Pointer to the object returned from the
 *                                    evaluation of a method or object
 *
 * RETURN:      Status. AE_OK if object is OK or was repaired successfully
 *
 * DESCRIPTION: Repair for the _FDE and _GTM objects. The expected return
 *              value is a Buffer of 5 DWORDs. This function repairs a common
 *              problem where the return value is a Buffer of BYTEs, not
 *              DWORDs.
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiNsRepair_FDE (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr)
{
    ACPI_OPERAND_OBJECT     *ReturnObject = *ReturnObjectPtr;
    ACPI_OPERAND_OBJECT     *BufferObject;
    UINT8                   *ByteBuffer;
    UINT32                  *DwordBuffer;
    UINT32                  i;


    ACPI_FUNCTION_NAME (NsRepair_FDE);


    switch (ReturnObject->Common.Type)
    {
    case ACPI_TYPE_BUFFER:

        /* This is the expected type. Length should be (at least) 5 DWORDs */

        if (ReturnObject->Buffer.Length >= ACPI_FDE_DWORD_BUFFER_SIZE)
        {
            return (AE_OK);
        }

        /* We can only repair if we have exactly 5 BYTEs */

        if (ReturnObject->Buffer.Length != ACPI_FDE_BYTE_BUFFER_SIZE)
        {
            ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
                "Incorrect return buffer length %u, expected %u",
                ReturnObject->Buffer.Length, ACPI_FDE_DWORD_BUFFER_SIZE));

            return (AE_AML_OPERAND_TYPE);
        }

        /* Create the new (larger) buffer object */

        BufferObject = AcpiUtCreateBufferObject (ACPI_FDE_DWORD_BUFFER_SIZE);
        if (!BufferObject)
        {
            return (AE_NO_MEMORY);
        }

        /* Expand each byte to a DWORD */

        ByteBuffer = ReturnObject->Buffer.Pointer;
        DwordBuffer = ACPI_CAST_PTR (UINT32, BufferObject->Buffer.Pointer);

        for (i = 0; i < ACPI_FDE_FIELD_COUNT; i++)
        {
            *DwordBuffer = (UINT32) *ByteBuffer;
            DwordBuffer++;
            ByteBuffer++;
        }

        ACPI_DEBUG_PRINT ((ACPI_DB_REPAIR,
            "%s Expanded Byte Buffer to expected DWord Buffer\n",
            Data->Pathname));
        break;

    default:
        return (AE_AML_OPERAND_TYPE);
    }

    /* Delete the original return object, return the new buffer object */

    AcpiUtRemoveReference (ReturnObject);
    *ReturnObjectPtr = BufferObject;

    Data->Flags |= ACPI_OBJECT_REPAIRED;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsRepair_CID
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ReturnObjectPtr     - Pointer to the object returned from the
 *                                    evaluation of a method or object
 *
 * RETURN:      Status. AE_OK if object is OK or was repaired successfully
 *
 * DESCRIPTION: Repair for the _CID object. If a string, ensure that all
 *              letters are uppercase and that there is no leading asterisk.
 *              If a Package, ensure same for all string elements.
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiNsRepair_CID (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr)
{
    ACPI_STATUS             Status;
    ACPI_OPERAND_OBJECT     *ReturnObject = *ReturnObjectPtr;
    ACPI_OPERAND_OBJECT     **ElementPtr;
    ACPI_OPERAND_OBJECT     *OriginalElement;
    UINT16                  OriginalRefCount;
    UINT32                  i;


    /* Check for _CID as a simple string */

    if (ReturnObject->Common.Type == ACPI_TYPE_STRING)
    {
        Status = AcpiNsRepair_HID (Data, ReturnObjectPtr);
        return (Status);
    }

    /* Exit if not a Package */

    if (ReturnObject->Common.Type != ACPI_TYPE_PACKAGE)
    {
        return (AE_OK);
    }

    /* Examine each element of the _CID package */

    ElementPtr = ReturnObject->Package.Elements;
    for (i = 0; i < ReturnObject->Package.Count; i++)
    {
        OriginalElement = *ElementPtr;
        OriginalRefCount = OriginalElement->Common.ReferenceCount;

        Status = AcpiNsRepair_HID (Data, ElementPtr);
        if (ACPI_FAILURE (Status))
        {
            return (Status);
        }

        /* Take care with reference counts */

        if (OriginalElement != *ElementPtr)
        {
            /* Element was replaced */

            (*ElementPtr)->Common.ReferenceCount =
                OriginalRefCount;

            AcpiUtRemoveReference (OriginalElement);
        }

        ElementPtr++;
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsRepair_HID
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ReturnObjectPtr     - Pointer to the object returned from the
 *                                    evaluation of a method or object
 *
 * RETURN:      Status. AE_OK if object is OK or was repaired successfully
 *
 * DESCRIPTION: Repair for the _HID object. If a string, ensure that all
 *              letters are uppercase and that there is no leading asterisk.
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiNsRepair_HID (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr)
{
    ACPI_OPERAND_OBJECT     *ReturnObject = *ReturnObjectPtr;
    ACPI_OPERAND_OBJECT     *NewString;
    char                    *Source;
    char                    *Dest;


    ACPI_FUNCTION_NAME (NsRepair_HID);


    /* We only care about string _HID objects (not integers) */

    if (ReturnObject->Common.Type != ACPI_TYPE_STRING)
    {
        return (AE_OK);
    }

    if (ReturnObject->String.Length == 0)
    {
        ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
            "Invalid zero-length _HID or _CID string"));

        /* Return AE_OK anyway, let driver handle it */

        Data->Flags |= ACPI_OBJECT_REPAIRED;
        return (AE_OK);
    }

    /* It is simplest to always create a new string object */

    NewString = AcpiUtCreateStringObject (ReturnObject->String.Length);
    if (!NewString)
    {
        return (AE_NO_MEMORY);
    }

    /*
     * Remove a leading asterisk if present. For some unknown reason, there
     * are many machines in the field that contains IDs like this.
     *
     * Examples: "*PNP0C03", "*ACPI0003"
     */
    Source = ReturnObject->String.Pointer;
    if (*Source == '*')
    {
        Source++;
        NewString->String.Length--;

        ACPI_DEBUG_PRINT ((ACPI_DB_REPAIR,
            "%s: Removed invalid leading asterisk\n", Data->Pathname));
    }

    /*
     * Copy and uppercase the string. From the ACPI specification:
     *
     * A valid PNP ID must be of the form "AAA####" where A is an uppercase
     * letter and # is a hex digit. A valid ACPI ID must be of the form
     * "ACPI####" where # is a hex digit.
     */
    for (Dest = NewString->String.Pointer; *Source; Dest++, Source++)
    {
        *Dest = (char) ACPI_TOUPPER (*Source);
    }

    AcpiUtRemoveReference (ReturnObject);
    *ReturnObjectPtr = NewString;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsRepair_TSS
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ReturnObjectPtr     - Pointer to the object returned from the
 *                                    evaluation of a method or object
 *
 * RETURN:      Status. AE_OK if object is OK or was repaired successfully
 *
 * DESCRIPTION: Repair for the _TSS object. If necessary, sort the object list
 *              descending by the power dissipation values.
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiNsRepair_TSS (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr)
{
    ACPI_OPERAND_OBJECT     *ReturnObject = *ReturnObjectPtr;
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;


    /*
     * We can only sort the _TSS return package if there is no _PSS in the
     * same scope. This is because if _PSS is present, the ACPI specification
     * dictates that the _TSS Power Dissipation field is to be ignored, and
     * therefore some BIOSs leave garbage values in the _TSS Power field(s).
     * In this case, it is best to just return the _TSS package as-is.
     * (May, 2011)
     */
    Status = AcpiNsGetNode (Data->Node, "^_PSS", ACPI_NS_NO_UPSEARCH, &Node);
    if (ACPI_SUCCESS (Status))
    {
        return (AE_OK);
    }

    Status = AcpiNsCheckSortedList (Data, ReturnObject, 5, 1,
                ACPI_SORT_DESCENDING, "PowerDissipation");

    return (Status);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsRepair_PSS
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ReturnObjectPtr     - Pointer to the object returned from the
 *                                    evaluation of a method or object
 *
 * RETURN:      Status. AE_OK if object is OK or was repaired successfully
 *
 * DESCRIPTION: Repair for the _PSS object. If necessary, sort the object list
 *              by the CPU frequencies. Check that the power dissipation values
 *              are all proportional to CPU frequency (i.e., sorting by
 *              frequency should be the same as sorting by power.)
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiNsRepair_PSS (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     **ReturnObjectPtr)
{
    ACPI_OPERAND_OBJECT     *ReturnObject = *ReturnObjectPtr;
    ACPI_OPERAND_OBJECT     **OuterElements;
    UINT32                  OuterElementCount;
    ACPI_OPERAND_OBJECT     **Elements;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  PreviousValue;
    ACPI_STATUS             Status;
    UINT32                  i;


    /*
     * Entries (sub-packages) in the _PSS Package must be sorted by power
     * dissipation, in descending order. If it appears that the list is
     * incorrectly sorted, sort it. We sort by CpuFrequency, since this
     * should be proportional to the power.
     */
    Status =AcpiNsCheckSortedList (Data, ReturnObject, 6, 0,
                ACPI_SORT_DESCENDING, "CpuFrequency");
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    /*
     * We now know the list is correctly sorted by CPU frequency. Check if
     * the power dissipation values are proportional.
     */
    PreviousValue = ACPI_UINT32_MAX;
    OuterElements = ReturnObject->Package.Elements;
    OuterElementCount = ReturnObject->Package.Count;

    for (i = 0; i < OuterElementCount; i++)
    {
        Elements = (*OuterElements)->Package.Elements;
        ObjDesc = Elements[1]; /* Index1 = PowerDissipation */

        if ((UINT32) ObjDesc->Integer.Value > PreviousValue)
        {
            ACPI_WARN_PREDEFINED ((AE_INFO, Data->Pathname, Data->NodeFlags,
                "SubPackage[%u,%u] - suspicious power dissipation values",
                i-1, i));
        }

        PreviousValue = (UINT32) ObjDesc->Integer.Value;
        OuterElements++;
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsCheckSortedList
 *
 * PARAMETERS:  Data                - Pointer to validation data structure
 *              ReturnObject        - Pointer to the top-level returned object
 *              ExpectedCount       - Minimum length of each sub-package
 *              SortIndex           - Sub-package entry to sort on
 *              SortDirection       - Ascending or descending
 *              SortKeyName         - Name of the SortIndex field
 *
 * RETURN:      Status. AE_OK if the list is valid and is sorted correctly or
 *              has been repaired by sorting the list.
 *
 * DESCRIPTION: Check if the package list is valid and sorted correctly by the
 *              SortIndex. If not, then sort the list.
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiNsCheckSortedList (
    ACPI_PREDEFINED_DATA    *Data,
    ACPI_OPERAND_OBJECT     *ReturnObject,
    UINT32                  ExpectedCount,
    UINT32                  SortIndex,
    UINT8                   SortDirection,
    char                    *SortKeyName)
{
    UINT32                  OuterElementCount;
    ACPI_OPERAND_OBJECT     **OuterElements;
    ACPI_OPERAND_OBJECT     **Elements;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    UINT32                  i;
    UINT32                  PreviousValue;


    ACPI_FUNCTION_NAME (NsCheckSortedList);


    /* The top-level object must be a package */

    if (ReturnObject->Common.Type != ACPI_TYPE_PACKAGE)
    {
        return (AE_AML_OPERAND_TYPE);
    }

    /*
     * NOTE: assumes list of sub-packages contains no NULL elements.
     * Any NULL elements should have been removed by earlier call
     * to AcpiNsRemoveNullElements.
     */
    OuterElements = ReturnObject->Package.Elements;
    OuterElementCount = ReturnObject->Package.Count;
    if (!OuterElementCount)
    {
        return (AE_AML_PACKAGE_LIMIT);
    }

    PreviousValue = 0;
    if (SortDirection == ACPI_SORT_DESCENDING)
    {
        PreviousValue = ACPI_UINT32_MAX;
    }

    /* Examine each subpackage */

    for (i = 0; i < OuterElementCount; i++)
    {
        /* Each element of the top-level package must also be a package */

        if ((*OuterElements)->Common.Type != ACPI_TYPE_PACKAGE)
        {
            return (AE_AML_OPERAND_TYPE);
        }

        /* Each sub-package must have the minimum length */

        if ((*OuterElements)->Package.Count < ExpectedCount)
        {
            return (AE_AML_PACKAGE_LIMIT);
        }

        Elements = (*OuterElements)->Package.Elements;
        ObjDesc = Elements[SortIndex];

        if (ObjDesc->Common.Type != ACPI_TYPE_INTEGER)
        {
            return (AE_AML_OPERAND_TYPE);
        }

        /*
         * The list must be sorted in the specified order. If we detect a
         * discrepancy, sort the entire list.
         */
        if (((SortDirection == ACPI_SORT_ASCENDING) &&
                (ObjDesc->Integer.Value < PreviousValue)) ||
            ((SortDirection == ACPI_SORT_DESCENDING) &&
                (ObjDesc->Integer.Value > PreviousValue)))
        {
            AcpiNsSortList (ReturnObject->Package.Elements,
                OuterElementCount, SortIndex, SortDirection);

            Data->Flags |= ACPI_OBJECT_REPAIRED;

            ACPI_DEBUG_PRINT ((ACPI_DB_REPAIR,
                "%s: Repaired unsorted list - now sorted by %s\n",
                Data->Pathname, SortKeyName));
            return (AE_OK);
        }

        PreviousValue = (UINT32) ObjDesc->Integer.Value;
        OuterElements++;
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiNsSortList
 *
 * PARAMETERS:  Elements            - Package object element list
 *              Count               - Element count for above
 *              Index               - Sort by which package element
 *              SortDirection       - Ascending or Descending sort
 *
 * RETURN:      None
 *
 * DESCRIPTION: Sort the objects that are in a package element list.
 *
 * NOTE: Assumes that all NULL elements have been removed from the package,
 *       and that all elements have been verified to be of type Integer.
 *
 *****************************************************************************/

static void
AcpiNsSortList (
    ACPI_OPERAND_OBJECT     **Elements,
    UINT32                  Count,
    UINT32                  Index,
    UINT8                   SortDirection)
{
    ACPI_OPERAND_OBJECT     *ObjDesc1;
    ACPI_OPERAND_OBJECT     *ObjDesc2;
    ACPI_OPERAND_OBJECT     *TempObj;
    UINT32                  i;
    UINT32                  j;


    /* Simple bubble sort */

    for (i = 1; i < Count; i++)
    {
        for (j = (Count - 1); j >= i; j--)
        {
            ObjDesc1 = Elements[j-1]->Package.Elements[Index];
            ObjDesc2 = Elements[j]->Package.Elements[Index];

            if (((SortDirection == ACPI_SORT_ASCENDING) &&
                    (ObjDesc1->Integer.Value > ObjDesc2->Integer.Value)) ||

                ((SortDirection == ACPI_SORT_DESCENDING) &&
                    (ObjDesc1->Integer.Value < ObjDesc2->Integer.Value)))
            {
                TempObj = Elements[j-1];
                Elements[j-1] = Elements[j];
                Elements[j] = TempObj;
            }
        }
    }
}
