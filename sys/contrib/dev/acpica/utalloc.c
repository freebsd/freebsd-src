/******************************************************************************
 *
 * Module Name: utalloc - local cache and memory allocation routines
 *              $Revision: 106 $
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

#define __UTALLOC_C__

#include "acpi.h"
#include "acparser.h"
#include "acinterp.h"
#include "acnamesp.h"
#include "acglobal.h"

#define _COMPONENT          ACPI_UTILITIES
        MODULE_NAME         ("utalloc")


/******************************************************************************
 *
 * FUNCTION:    AcpiUtReleaseToCache
 *
 * PARAMETERS:  ListId              - Memory list/cache ID
 *              Object              - The object to be released
 *
 * RETURN:      None
 *
 * DESCRIPTION: Release an object to the specified cache.  If cache is full,
 *              the object is deleted.
 *
 ******************************************************************************/

void
AcpiUtReleaseToCache (
    UINT32                  ListId,
    void                    *Object)
{
    ACPI_MEMORY_LIST        *CacheInfo;


    FUNCTION_ENTRY ();


    /* If walk cache is full, just free this wallkstate object */

    CacheInfo = &AcpiGbl_MemoryLists[ListId];
    if (CacheInfo->CacheDepth >= CacheInfo->MaxCacheDepth)
    {
        ACPI_MEM_FREE (Object);
        ACPI_MEM_TRACKING (CacheInfo->TotalFreed++);
    }

    /* Otherwise put this object back into the cache */

    else
    {
        AcpiUtAcquireMutex (ACPI_MTX_CACHES);

        /* Mark the object as cached */

        MEMSET (Object, 0xCA, CacheInfo->ObjectSize);
        ((ACPI_OPERAND_OBJECT *) Object)->Common.DataType = ACPI_CACHED_OBJECT;

        /* Put the object at the head of the cache list */

        * (char **) (((char *) Object) + CacheInfo->LinkOffset) = CacheInfo->ListHead;
        CacheInfo->ListHead = Object;
        CacheInfo->CacheDepth++;

        AcpiUtReleaseMutex (ACPI_MTX_CACHES);
    }
}


/******************************************************************************
 *
 * FUNCTION:    AcpiUtAcquireFromCache
 *
 * PARAMETERS:  ListId              - Memory list ID
 *
 * RETURN:      A requested object.  NULL if the object could not be
 *              allocated.
 *
 * DESCRIPTION: Get an object from the specified cache.  If cache is empty,
 *              the object is allocated.
 *
 ******************************************************************************/

void *
AcpiUtAcquireFromCache (
    UINT32                  ListId)
{
    ACPI_MEMORY_LIST        *CacheInfo;
    void                    *Object;


    PROC_NAME ("UtAcquireFromCache");


    CacheInfo = &AcpiGbl_MemoryLists[ListId];
    AcpiUtAcquireMutex (ACPI_MTX_CACHES);
    ACPI_MEM_TRACKING (CacheInfo->CacheRequests++);

    /* Check the cache first */

    if (CacheInfo->ListHead)
    {
        /* There is an object available, use it */

        Object = CacheInfo->ListHead;
        CacheInfo->ListHead = * (char **) (((char *) Object) + CacheInfo->LinkOffset);

        ACPI_MEM_TRACKING (CacheInfo->CacheHits++);
        CacheInfo->CacheDepth--;

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Object %p from %s\n",
            Object, AcpiGbl_MemoryLists[ListId].ListName));
#endif

        AcpiUtReleaseMutex (ACPI_MTX_CACHES);

        /* Clear (zero) the previously used Object */

        MEMSET (Object, 0, CacheInfo->ObjectSize);
    }

    else
    {
        /* The cache is empty, create a new object */

        /* Avoid deadlock with ACPI_MEM_CALLOCATE */

        AcpiUtReleaseMutex (ACPI_MTX_CACHES);

        Object = ACPI_MEM_CALLOCATE (CacheInfo->ObjectSize);
        ACPI_MEM_TRACKING (CacheInfo->TotalAllocated++);
    }

    return (Object);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiUtDeleteGenericCache
 *
 * PARAMETERS:  ListId          - Memory list ID
 *
 * RETURN:      None
 *
 * DESCRIPTION: Free all objects within the requested cache.
 *
 ******************************************************************************/

void
AcpiUtDeleteGenericCache (
    UINT32                  ListId)
{
    ACPI_MEMORY_LIST        *CacheInfo;
    char                    *Next;


    FUNCTION_ENTRY ();


    CacheInfo = &AcpiGbl_MemoryLists[ListId];
    while (CacheInfo->ListHead)
    {
        /* Delete one cached state object */

        Next = * (char **) (((char *) CacheInfo->ListHead) + CacheInfo->LinkOffset);
        ACPI_MEM_FREE (CacheInfo->ListHead);

        CacheInfo->ListHead = Next;
        CacheInfo->CacheDepth--;
    }
}


#ifdef ACPI_DBG_TRACK_ALLOCATIONS


/*
 * These procedures are used for tracking memory leaks in the subsystem, and
 * they get compiled out when the ACPI_DBG_TRACK_ALLOCATIONS is not set.
 *
 * Each memory allocation is tracked via a doubly linked list.  Each
 * element contains the caller's component, module name, function name, and
 * line number.  AcpiUtAllocate and AcpiUtCallocate call
 * AcpiUtTrackAllocation to add an element to the list; deletion
 * occurs in the body of AcpiUtFree.
 */


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtFindAllocation
 *
 * PARAMETERS:  Address             - Address of allocated memory
 *
 * RETURN:      A list element if found; NULL otherwise.
 *
 * DESCRIPTION: Searches for an element in the global allocation tracking list.
 *
 ******************************************************************************/

ACPI_DEBUG_MEM_BLOCK *
AcpiUtFindAllocation (
    UINT32                  ListId,
    void                    *Address)
{
    ACPI_DEBUG_MEM_BLOCK    *Element;


    FUNCTION_ENTRY ();


    if (ListId > ACPI_MEM_LIST_MAX)
    {
        return (NULL);
    }

    Element = AcpiGbl_MemoryLists[ListId].ListHead;

    /* Search for the address. */

    while (Element)
    {
        if (Element == Address)
        {
            return (Element);
        }

        Element = Element->Next;
    }

    return (NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtTrackAllocation
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
 ******************************************************************************/

ACPI_STATUS
AcpiUtTrackAllocation (
    UINT32                  ListId,
    ACPI_DEBUG_MEM_BLOCK    *Address,
    UINT32                  Size,
    UINT8                   AllocType,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    ACPI_MEMORY_LIST        *MemList;
    ACPI_DEBUG_MEM_BLOCK    *Element;
    ACPI_STATUS             Status = AE_OK;


    FUNCTION_TRACE_PTR ("UtTrackAllocation", Address);


    if (ListId > ACPI_MEM_LIST_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    MemList = &AcpiGbl_MemoryLists[ListId];
    AcpiUtAcquireMutex (ACPI_MTX_MEMORY);

    /*
     * Search list for this address to make sure it is not already on the list.
     * This will catch several kinds of problems.
     */

    Element = AcpiUtFindAllocation (ListId, Address);
    if (Element)
    {
        REPORT_ERROR (("UtTrackAllocation: Address already present in list! (%p)\n",
            Address));

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Element %p Address %p\n", Element, Address));

        goto UnlockAndExit;
    }

    /* Fill in the instance data. */

    Address->Size      = Size;
    Address->AllocType = AllocType;
    Address->Component = Component;
    Address->Line      = Line;

    STRNCPY (Address->Module, Module, MAX_MODULE_NAME);

    /* Insert at list head */

    if (MemList->ListHead)
    {
        ((ACPI_DEBUG_MEM_BLOCK *)(MemList->ListHead))->Previous = Address;
    }

    Address->Next = MemList->ListHead;
    Address->Previous = NULL;

    MemList->ListHead = Address;


UnlockAndExit:
    AcpiUtReleaseMutex (ACPI_MTX_MEMORY);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtRemoveAllocation
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
 ******************************************************************************/

ACPI_STATUS
AcpiUtRemoveAllocation (
    UINT32                  ListId,
    ACPI_DEBUG_MEM_BLOCK    *Address,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    ACPI_MEMORY_LIST        *MemList;


    FUNCTION_TRACE ("UtRemoveAllocation");


    if (ListId > ACPI_MEM_LIST_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    MemList = &AcpiGbl_MemoryLists[ListId];
    if (NULL == MemList->ListHead)
    {
        /* No allocations! */

        _REPORT_ERROR (Module, Line, Component,
                ("UtRemoveAllocation: Empty allocation list, nothing to free!\n"));

        return_ACPI_STATUS (AE_OK);
    }


    AcpiUtAcquireMutex (ACPI_MTX_MEMORY);

    /* Unlink */

    if (Address->Previous)
    {
        (Address->Previous)->Next = Address->Next;
    }
    else
    {
        MemList->ListHead = Address->Next;
    }

    if (Address->Next)
    {
        (Address->Next)->Previous = Address->Previous;
    }


    /* Mark the segment as deleted */

    MEMSET (&Address->UserSpace, 0xEA, Address->Size);

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "Freeing size %X\n", Address->Size));

    AcpiUtReleaseMutex (ACPI_MTX_MEMORY);
    return_ACPI_STATUS (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDumpAllocationInfo
 *
 * PARAMETERS:
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print some info about the outstanding allocations.
 *
 ******************************************************************************/

void
AcpiUtDumpAllocationInfo (
    void)
{
/*
    ACPI_MEMORY_LIST        *MemList;
*/

    FUNCTION_TRACE ("UtDumpAllocationInfo");

/*
    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Current allocations",
                    MemList->CurrentCount,
                    ROUND_UP_TO_1K (MemList->CurrentSize)));

    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Max concurrent allocations",
                    MemList->MaxConcurrentCount,
                    ROUND_UP_TO_1K (MemList->MaxConcurrentSize)));


    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Total (all) internal objects",
                    RunningObjectCount,
                    ROUND_UP_TO_1K (RunningObjectSize)));

    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Total (all) allocations",
                    RunningAllocCount,
                    ROUND_UP_TO_1K (RunningAllocSize)));


    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Current Nodes",
                    AcpiGbl_CurrentNodeCount,
                    ROUND_UP_TO_1K (AcpiGbl_CurrentNodeSize)));

    ACPI_DEBUG_PRINT (TRACE_ALLOCATIONS | TRACE_TABLES,
                    ("%30s: %4d (%3d Kb)\n", "Max Nodes",
                    AcpiGbl_MaxConcurrentNodeCount,
                    ROUND_UP_TO_1K ((AcpiGbl_MaxConcurrentNodeCount * sizeof (ACPI_NAMESPACE_NODE)))));
*/
    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtDumpAllocations
 *
 * PARAMETERS:  Component           - Component(s) to dump info for.
 *              Module              - Module to dump info for.  NULL means all.
 *
 * RETURN:      None
 *
 * DESCRIPTION: Print a list of all outstanding allocations.
 *
 ******************************************************************************/

void
AcpiUtDumpAllocations (
    UINT32                  Component,
    NATIVE_CHAR             *Module)
{
    ACPI_DEBUG_MEM_BLOCK    *Element;
    UINT32                  i;


    FUNCTION_TRACE ("UtDumpAllocations");


    Element = AcpiGbl_MemoryLists[0].ListHead;
    if (Element == NULL)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_OK,
                "No outstanding allocations.\n"));
        return_VOID;
    }


    /*
     * Walk the allocation list.
     */
    AcpiUtAcquireMutex (ACPI_MTX_MEMORY);

    ACPI_DEBUG_PRINT ((ACPI_DB_OK,
        "Outstanding allocations:\n"));

    for (i = 1; ; i++)  /* Just a counter */
    {
        if ((Element->Component & Component) &&
            ((Module == NULL) || (0 == STRCMP (Module, Element->Module))))
        {
            if (((ACPI_OPERAND_OBJECT  *)(&Element->UserSpace))->Common.Type != ACPI_CACHED_OBJECT)
            {
                ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            "%p Len %04X %9.9s-%d",
                            &Element->UserSpace, Element->Size, Element->Module,
                            Element->Line));

                /* Most of the elements will be internal objects. */

                switch (((ACPI_OPERAND_OBJECT  *)
                    (&Element->UserSpace))->Common.DataType)
                {
                case ACPI_DESC_TYPE_INTERNAL:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " ObjType %12.12s R%d",
                            AcpiUtGetTypeName (((ACPI_OPERAND_OBJECT *)(&Element->UserSpace))->Common.Type),
                            ((ACPI_OPERAND_OBJECT *)(&Element->UserSpace))->Common.ReferenceCount));
                    break;

                case ACPI_DESC_TYPE_PARSER:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " ParseObj Opcode %04X",
                            ((ACPI_PARSE_OBJECT *)(&Element->UserSpace))->Opcode));
                    break;

                case ACPI_DESC_TYPE_NAMED:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " Node %4.4s",
                            (char*)&((ACPI_NAMESPACE_NODE *)(&Element->UserSpace))->Name));
                    break;

                case ACPI_DESC_TYPE_STATE:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " Untyped StateObj"));
                    break;

                case ACPI_DESC_TYPE_STATE_UPDATE:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " UPDATE StateObj"));
                    break;

                case ACPI_DESC_TYPE_STATE_PACKAGE:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " PACKAGE StateObj"));
                    break;

                case ACPI_DESC_TYPE_STATE_CONTROL:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " CONTROL StateObj"));
                    break;

                case ACPI_DESC_TYPE_STATE_RPSCOPE:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " ROOT-PARSE-SCOPE StateObj"));
                    break;

                case ACPI_DESC_TYPE_STATE_PSCOPE:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " PARSE-SCOPE StateObj"));
                    break;

                case ACPI_DESC_TYPE_STATE_WSCOPE:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " WALK-SCOPE StateObj"));
                    break;

                case ACPI_DESC_TYPE_STATE_RESULT:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " RESULT StateObj"));
                    break;

                case ACPI_DESC_TYPE_STATE_NOTIFY:
                    ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK,
                            " NOTIFY StateObj"));
                    break;
                }

                ACPI_DEBUG_PRINT_RAW ((ACPI_DB_OK, "\n"));
            }
        }

        if (Element->Next == NULL)
        {
            break;
        }

        Element = Element->Next;
    }

    AcpiUtReleaseMutex (ACPI_MTX_MEMORY);

    ACPI_DEBUG_PRINT ((ACPI_DB_OK,
        "Total number of unfreed allocations = %d(%X)\n", i,i));


    return_VOID;

}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtAllocate
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
 ******************************************************************************/

void *
AcpiUtAllocate (
    UINT32                  Size,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    ACPI_DEBUG_MEM_BLOCK    *Address;
    ACPI_STATUS             Status;


    FUNCTION_TRACE_U32 ("UtAllocate", Size);


    /* Check for an inadvertent size of zero bytes */

    if (!Size)
    {
        _REPORT_ERROR (Module, Line, Component,
                ("UtAllocate: Attempt to allocate zero bytes\n"));
        Size = 1;
    }

    Address = AcpiOsAllocate (Size + sizeof (ACPI_DEBUG_MEM_BLOCK));
    if (!Address)
    {
        /* Report allocation error */

        _REPORT_ERROR (Module, Line, Component,
                ("UtAllocate: Could not allocate size %X\n", Size));

        return_PTR (NULL);
    }

    Status = AcpiUtTrackAllocation (ACPI_MEM_LIST_GLOBAL, Address, Size,
                    MEM_MALLOC, Component, Module, Line);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsFree (Address);
        return_PTR (NULL);
    }

    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].TotalAllocated++;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].CurrentTotalSize += Size;

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "%p Size %X\n", Address, Size));

    return_PTR ((void *) &Address->UserSpace);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCallocate
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
 ******************************************************************************/

void *
AcpiUtCallocate (
    UINT32                  Size,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    ACPI_DEBUG_MEM_BLOCK    *Address;
    ACPI_STATUS             Status;


    FUNCTION_TRACE_U32 ("UtCallocate", Size);


    /* Check for an inadvertent size of zero bytes */

    if (!Size)
    {
        _REPORT_ERROR (Module, Line, Component,
                ("UtCallocate: Attempt to allocate zero bytes\n"));
        return_PTR (NULL);
    }


    Address = AcpiOsCallocate (Size + sizeof (ACPI_DEBUG_MEM_BLOCK));
    if (!Address)
    {
        /* Report allocation error */

        _REPORT_ERROR (Module, Line, Component,
                ("UtCallocate: Could not allocate size %X\n", Size));
        return_PTR (NULL);
    }

    Status = AcpiUtTrackAllocation (ACPI_MEM_LIST_GLOBAL, Address, Size,
                        MEM_CALLOC, Component, Module, Line);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsFree (Address);
        return_PTR (NULL);
    }

    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].TotalAllocated++;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].CurrentTotalSize += Size;

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "%p Size %X\n", Address, Size));
    return_PTR ((void *) &Address->UserSpace);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtFree
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
 ******************************************************************************/

void
AcpiUtFree (
    void                    *Address,
    UINT32                  Component,
    NATIVE_CHAR             *Module,
    UINT32                  Line)
{
    ACPI_DEBUG_MEM_BLOCK    *DebugBlock;


    FUNCTION_TRACE_PTR ("UtFree", Address);


    if (NULL == Address)
    {
        _REPORT_ERROR (Module, Line, Component,
            ("AcpiUtFree: Trying to delete a NULL address\n"));

        return_VOID;
    }

    DebugBlock = (ACPI_DEBUG_MEM_BLOCK *)
                    (((char *) Address) - sizeof (ACPI_DEBUG_MEM_HEADER));

    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].TotalFreed++;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].CurrentTotalSize -= DebugBlock->Size;

    AcpiUtRemoveAllocation (ACPI_MEM_LIST_GLOBAL, DebugBlock,
            Component, Module, Line);
    AcpiOsFree (DebugBlock);

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "%p freed\n", Address));

    return_VOID;
}

#endif  /* #ifdef ACPI_DBG_TRACK_ALLOCATIONS */

