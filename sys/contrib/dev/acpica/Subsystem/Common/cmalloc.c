/******************************************************************************
 *
 * Module Name: cmalloc - local memory allocation routines
 *              $Revision: 78 $
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

#define __CMALLOC_C__

#include "acpi.h"
#include "acparser.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acglobal.h"

#define _COMPONENT          MISCELLANEOUS
        MODULE_NAME         ("cmalloc")


#ifdef ACPI_DEBUG
/*
 * Most of this code is for tracking memory leaks in the subsystem, and it
 * gets compiled out when the ACPI_DEBUG flag is not set.
 * Every memory allocation is kept track of in a doubly linked list.  Each
 * element contains the caller's component, module name, function name, and
 * line number.  _CmAllocate and _CmCallocate call AcpiCmAddElementToAllocList
 * to add an element to the list; deletion occurs in the bosy of _CmFree.
 */


/*****************************************************************************
 *
 * FUNCTION:    AcpiCmSearchAllocList
 *
 * PARAMETERS:  Address             - Address of allocated memory
 *
 * RETURN:      A list element if found; NULL otherwise.
 *
 * DESCRIPTION: Searches for an element in the global allocation tracking list.
 *
 ****************************************************************************/

ALLOCATION_INFO *
AcpiCmSearchAllocList (
    void                    *Address)
{
    ALLOCATION_INFO         *Element = AcpiGbl_HeadAllocPtr;


    /* Search for the address. */

    while (Element)
    {
        if (Element->Address == Address)
        {
            return (Element);
        }

        Element = Element->Next;
    }

    return (NULL);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiCmAddElementToAllocList
 *
 * PARAMETERS:  Address             - Address of allocated memory
 *              Size                - Size of the allocation
 *              AllocType           - MEM_MALLOC or MEM_CALLOC
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Inserts an element into the global allocation tracking list.
 *
 ****************************************************************************/

ACPI_STATUS
AcpiCmAddElementToAllocList (
    void                    *Address,
    UINT32                  Size,
    UINT8                   AllocType,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    ALLOCATION_INFO         *Element;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_PTR ("CmAddElementToAllocList", Address);


    AcpiCmAcquireMutex (ACPI_MTX_MEMORY);

    /* Keep track of the running total of all allocations. */

    AcpiGbl_CurrentAllocCount++;
    AcpiGbl_RunningAllocCount++;

    if (AcpiGbl_MaxConcurrentAllocCount < AcpiGbl_CurrentAllocCount)
    {
        AcpiGbl_MaxConcurrentAllocCount = AcpiGbl_CurrentAllocCount;
    }

    AcpiGbl_CurrentAllocSize += Size;
    AcpiGbl_RunningAllocSize += Size;

    if (AcpiGbl_MaxConcurrentAllocSize < AcpiGbl_CurrentAllocSize)
    {
        AcpiGbl_MaxConcurrentAllocSize = AcpiGbl_CurrentAllocSize;
    }

    /* If the head pointer is null, create the first element and fill it in. */

    if (NULL == AcpiGbl_HeadAllocPtr)
    {
        AcpiGbl_HeadAllocPtr =
                (ALLOCATION_INFO *) AcpiOsCallocate (sizeof (ALLOCATION_INFO));

        if (!AcpiGbl_HeadAllocPtr)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("Could not allocate memory info block\n"));
            Status = AE_NO_MEMORY;
            goto UnlockAndExit;
        }

        AcpiGbl_TailAllocPtr = AcpiGbl_HeadAllocPtr;
    }

    else
    {
        AcpiGbl_TailAllocPtr->Next =
                (ALLOCATION_INFO *) AcpiOsCallocate (sizeof (ALLOCATION_INFO));
        if (!AcpiGbl_TailAllocPtr->Next)
        {
            DEBUG_PRINT (ACPI_ERROR,
                ("Could not allocate memory info block\n"));
            Status = AE_NO_MEMORY;
            goto UnlockAndExit;
        }

        /* error check */

        AcpiGbl_TailAllocPtr->Next->Previous = AcpiGbl_TailAllocPtr;
        AcpiGbl_TailAllocPtr = AcpiGbl_TailAllocPtr->Next;
    }

    /*
     * Search list for this address to make sure it is not already on the list.
     * This will catch several kinds of problems.
     */

    Element = AcpiCmSearchAllocList (Address);
    if (Element)
    {
        REPORT_ERROR (("CmAddElementToAllocList: Address already present in list!\n"));

        DEBUG_PRINT (ACPI_ERROR, ("Element %p Address %p\n", Element, Address));

        BREAKPOINT3;
    }

    /* Fill in the instance data. */

    AcpiGbl_TailAllocPtr->Address   = Address;
    AcpiGbl_TailAllocPtr->Size      = Size;
    AcpiGbl_TailAllocPtr->AllocType = AllocType;
    AcpiGbl_TailAllocPtr->Component = Component;
    AcpiGbl_TailAllocPtr->Line      = Line;

    STRNCPY (AcpiGbl_TailAllocPtr->Module, Module, MAX_MODULE_NAME);


UnlockAndExit:
    AcpiCmReleaseMutex (ACPI_MTX_MEMORY);
    return_ACPI_STATUS (Status);
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiCmDeleteElementFromAllocList
 *
 * PARAMETERS:  Address             - Address of allocated memory
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:
 *
 * DESCRIPTION: Deletes an element from the global allocation tracking list.
 *
 ****************************************************************************/

void
AcpiCmDeleteElementFromAllocList (
    void                    *Address,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    ALLOCATION_INFO         *Element;
    UINT32                  *DwordPtr;
    UINT32                  DwordLen;
    UINT32                  Size;
    UINT32                  i;


    FUNCTION_TRACE ("CmDeleteElementFromAllocList");

    if (NULL == AcpiGbl_HeadAllocPtr)
    {
        /* Boy we got problems. */

        _REPORT_ERROR (Module, Line, Component,
                ("CmDeleteElementFromAllocList: Empty allocation list, nothing to free!\n"));

        return_VOID;
    }


    AcpiCmAcquireMutex (ACPI_MTX_MEMORY);

    /* Keep track of the amount of memory allocated. */

    Size = 0;
    AcpiGbl_CurrentAllocCount--;

    if (AcpiGbl_HeadAllocPtr == AcpiGbl_TailAllocPtr)
    {
        if (Address != AcpiGbl_HeadAllocPtr->Address)
        {
            _REPORT_ERROR (Module, Line, Component,
                ("CmDeleteElementFromAllocList: Deleting non-allocated memory\n"));

            goto Cleanup;
        }

        Size = AcpiGbl_HeadAllocPtr->Size;

        AcpiOsFree (AcpiGbl_HeadAllocPtr);
        AcpiGbl_HeadAllocPtr = NULL;
        AcpiGbl_TailAllocPtr = NULL;

        DEBUG_PRINT (TRACE_ALLOCATIONS,
            ("_CmFree: Allocation list deleted.  There are no outstanding allocations\n"));

        goto Cleanup;
    }


    /* Search list for this address */

    Element = AcpiCmSearchAllocList (Address);
    if (Element)
    {
        /* cases: head, tail, other */

        if (Element == AcpiGbl_HeadAllocPtr)
        {
            Element->Next->Previous = NULL;
            AcpiGbl_HeadAllocPtr = Element->Next;
        }

        else
        {
            if (Element == AcpiGbl_TailAllocPtr)
            {
                Element->Previous->Next = NULL;
                AcpiGbl_TailAllocPtr = Element->Previous;
            }

            else
            {
                Element->Previous->Next = Element->Next;
                Element->Next->Previous = Element->Previous;
            }
        }


        /* Mark the segment as deleted */

        if (Element->Size >= 4)
        {
            DwordLen = DIV_4 (Element->Size);
            DwordPtr = (UINT32 *) Element->Address;

            for (i = 0; i < DwordLen; i++)
            {
                DwordPtr[i] = 0x00DEAD00;
            }

            /* Set obj type, desc, and ref count fields to all ones */

            DwordPtr[0] = ACPI_UINT32_MAX;
            if (Element->Size >= 8)
            {
                DwordPtr[1] = ACPI_UINT32_MAX;
            }
        }

        Size = Element->Size;

        MEMSET (Element, 0xEA, sizeof (ALLOCATION_INFO));


        if (Size == sizeof (ACPI_OPERAND_OBJECT))
        {
            DEBUG_PRINT (TRACE_ALLOCATIONS, ("CmDelete: Freeing size 0x%X (ACPI_OPERAND_OBJECT)\n", Size));
        }
        else
        {
            DEBUG_PRINT (TRACE_ALLOCATIONS, ("CmDelete: Freeing size 0x%X\n", Size));
        }

        AcpiOsFree (Element);
    }

    else
    {
        _REPORT_ERROR (Module, Line, Component,
                ("_CmFree: Entry not found in list\n"));
        DEBUG_PRINT (ACPI_ERROR,
                ("_CmFree: Entry %p was not found in allocation list\n",
                Address));
        AcpiCmReleaseMutex (ACPI_MTX_MEMORY);
        return_VOID;
    }


Cleanup:

    AcpiGbl_CurrentAllocSize -= Size;
    AcpiCmReleaseMutex (ACPI_MTX_MEMORY);

    return_VOID;
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiCmDumpAllocationInfo
 *
 * PARAMETERS:
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print some info about the outstanding allocations.
 *
 ****************************************************************************/

void
AcpiCmDumpAllocationInfo (
    void)
{
    FUNCTION_TRACE ("CmDumpAllocationInfo");


    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Current allocations",
                    AcpiGbl_CurrentAllocCount,
                    ROUND_UP_TO_1K (AcpiGbl_CurrentAllocSize)));

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Max concurrent allocations",
                    AcpiGbl_MaxConcurrentAllocCount,
                    ROUND_UP_TO_1K (AcpiGbl_MaxConcurrentAllocSize)));

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Current Internal objects",
                    AcpiGbl_CurrentObjectCount,
                    ROUND_UP_TO_1K (AcpiGbl_CurrentObjectSize)));

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Max internal objects",
                    AcpiGbl_MaxConcurrentObjectCount,
                    ROUND_UP_TO_1K (AcpiGbl_MaxConcurrentObjectSize)));

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Current Nodes",
                    AcpiGbl_CurrentNodeCount,
                    ROUND_UP_TO_1K (AcpiGbl_CurrentNodeSize)));

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Max Nodes",
                    AcpiGbl_MaxConcurrentNodeCount,
                    ROUND_UP_TO_1K ((AcpiGbl_MaxConcurrentNodeCount * sizeof (ACPI_NAMESPACE_NODE)))));

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Total (all) internal objects",
                    AcpiGbl_RunningObjectCount,
                    ROUND_UP_TO_1K (AcpiGbl_RunningObjectSize)));

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Total (all) allocations",
                    AcpiGbl_RunningAllocCount,
                    ROUND_UP_TO_1K (AcpiGbl_RunningAllocSize)));

    return_VOID;
}


/*****************************************************************************
 *
 * FUNCTION:    AcpiCmDumpCurrentAllocations
 *
 * PARAMETERS:  Component           - Component(s) to dump info for.
 *              Module              - Module to dump info for.  NULL means all.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print a list of all outstanding allocations.
 *
 ****************************************************************************/

void
AcpiCmDumpCurrentAllocations (
    UINT32                  Component,
    NATIVE_CHAR             *Module)
{
    ALLOCATION_INFO         *Element = AcpiGbl_HeadAllocPtr;
    UINT32                  i;


    FUNCTION_TRACE ("CmDumpCurrentAllocations");


    if (Element == NULL)
    {
        DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                ("No outstanding allocations.\n"));
        return_VOID;
    }


    /*
     * Walk the allocation list.
     */

    AcpiCmAcquireMutex (ACPI_MTX_MEMORY);

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
        ("Outstanding allocations:\n"));

    for (i = 1; ; i++)  /* Just a counter */
    {
        if ((Element->Component & Component) &&
            ((Module == NULL) || (0 == STRCMP (Module, Element->Module))))
        {
            DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                        ("%p Len %04lX %9.9s-%ld",
                        Element->Address, Element->Size, Element->Module,
                        Element->Line));

            /* Most of the elements will be internal objects. */

            switch (((ACPI_OPERAND_OBJECT  *)
                (Element->Address))->Common.DataType)
            {
            case ACPI_DESC_TYPE_INTERNAL:
                DEBUG_PRINT_RAW (TRACE_ALLOCATIONS | TRACE_TABLES,
                        (" ObjType %s",
                        AcpiCmGetTypeName (((ACPI_OPERAND_OBJECT  *)(Element->Address))->Common.Type)));
                break;

            case ACPI_DESC_TYPE_PARSER:
                DEBUG_PRINT_RAW (TRACE_ALLOCATIONS | TRACE_TABLES,
                        (" ParseObj Opcode %04X",
                        ((ACPI_PARSE_OBJECT *)(Element->Address))->Opcode));
                break;

            case ACPI_DESC_TYPE_NAMED:
                DEBUG_PRINT_RAW (TRACE_ALLOCATIONS | TRACE_TABLES,
                        (" Node %4.4s",
                        &((ACPI_NAMESPACE_NODE *)(Element->Address))->Name));
                break;

            case ACPI_DESC_TYPE_STATE:
                DEBUG_PRINT_RAW (TRACE_ALLOCATIONS | TRACE_TABLES,
                        (" StateObj"));
                break;
            }

            DEBUG_PRINT_RAW (TRACE_ALLOCATIONS | TRACE_TABLES, ("\n"));
        }

        if (Element->Next == NULL)
        {
            break;
        }

        Element = Element->Next;
    }

    AcpiCmReleaseMutex (ACPI_MTX_MEMORY);

    DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
        ("Total number of unfreed allocations = %d\n", i));

    return_VOID;
}

#endif  /* Debug routines for memory leak detection */


/*****************************************************************************
 *
 * FUNCTION:    _CmAllocate
 *
 * PARAMETERS:  Size                - Size of the allocation
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: The subsystem's equivalent of malloc.
 *
 ****************************************************************************/

void *
_CmAllocate (
    UINT32                  Size,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    void                    *Address = NULL;
    DEBUG_ONLY_MEMBERS (\
    ACPI_STATUS             Status)


    FUNCTION_TRACE_U32 ("_CmAllocate", Size);


    /* Check for an inadvertent size of zero bytes */

    if (!Size)
    {
        _REPORT_ERROR (Module, Line, Component,
                ("CmAllocate: Attempt to allocate zero bytes\n"));
        Size = 1;
    }

    Address = AcpiOsAllocate (Size);
    if (!Address)
    {
        /* Report allocation error */

        _REPORT_ERROR (Module, Line, Component,
                ("CmAllocate: Could not allocate size 0x%x\n", Size));

        return_VALUE (NULL);
    }

#ifdef ACPI_DEBUG
    Status = AcpiCmAddElementToAllocList (Address, Size, MEM_MALLOC, Component,
                                          Module, Line);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsFree (Address);
        return_PTR (NULL);
    }

    DEBUG_PRINT (TRACE_ALLOCATIONS,
        ("CmAllocate: %p Size 0x%x\n", Address, Size));
#endif

    return_PTR (Address);
}


/*****************************************************************************
 *
 * FUNCTION:    _CmCallocate
 *
 * PARAMETERS:  Size                - Size of the allocation
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      Address of the allocated memory on success, NULL on failure.
 *
 * DESCRIPTION: Subsystem equivalent of calloc.
 *
 ****************************************************************************/

void *
_CmCallocate (
    UINT32                  Size,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    void                    *Address = NULL;
    DEBUG_ONLY_MEMBERS (\
    ACPI_STATUS             Status)


    FUNCTION_TRACE_U32 ("_CmCallocate", Size);


    /* Check for an inadvertent size of zero bytes */

    if (!Size)
    {
        _REPORT_ERROR (Module, Line, Component,
                ("CmCallocate: Attempt to allocate zero bytes\n"));
        return_VALUE (NULL);
    }


    Address = AcpiOsCallocate (Size);

    if (!Address)
    {
        /* Report allocation error */

        _REPORT_ERROR (Module, Line, Component,
                ("CmCallocate: Could not allocate size 0x%x\n", Size));
        return_VALUE (NULL);
    }

#ifdef ACPI_DEBUG
    Status = AcpiCmAddElementToAllocList (Address, Size, MEM_CALLOC, Component,
                                          Module, Line);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsFree (Address);
        return_PTR (NULL);
    }
#endif

    DEBUG_PRINT (TRACE_ALLOCATIONS,
        ("CmCallocate: %p Size 0x%x\n", Address, Size));

    return_PTR (Address);
}


/*****************************************************************************
 *
 * FUNCTION:    _CmFree
 *
 * PARAMETERS:  Address             - Address of the memory to deallocate
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      None
 *
 * DESCRIPTION: Frees the memory at Address
 *
 ****************************************************************************/

void
_CmFree (
    void                    *Address,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    FUNCTION_TRACE_PTR ("_CmFree", Address);


    if (NULL == Address)
    {
        _REPORT_ERROR (Module, Line, Component,
            ("_CmFree: Trying to delete a NULL address\n"));

        return_VOID;
    }

#ifdef ACPI_DEBUG
    AcpiCmDeleteElementFromAllocList (Address, Component, Module, Line);
#endif

    AcpiOsFree (Address);

    DEBUG_PRINT (TRACE_ALLOCATIONS, ("CmFree: %p freed\n", Address));

    return_VOID;
}


