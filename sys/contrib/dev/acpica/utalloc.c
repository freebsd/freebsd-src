/******************************************************************************
 *
 * Module Name: utalloc - local cache and memory allocation routines
 *              $Revision: 138 $
 *
 *****************************************************************************/

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

#define __UTALLOC_C__

#include "acpi.h"

#define _COMPONENT          ACPI_UTILITIES
        ACPI_MODULE_NAME    ("utalloc")


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


    ACPI_FUNCTION_ENTRY ();


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
        if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_CACHES)))
        {
            return;
        }

        /* Mark the object as cached */

        ACPI_MEMSET (Object, 0xCA, CacheInfo->ObjectSize);
        ACPI_SET_DESCRIPTOR_TYPE (Object, ACPI_DESC_TYPE_CACHED);

        /* Put the object at the head of the cache list */

        * (ACPI_CAST_INDIRECT_PTR (char, &(((char *) Object)[CacheInfo->LinkOffset]))) = CacheInfo->ListHead;
        CacheInfo->ListHead = Object;
        CacheInfo->CacheDepth++;

        (void) AcpiUtReleaseMutex (ACPI_MTX_CACHES);
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


    ACPI_FUNCTION_NAME ("UtAcquireFromCache");


    CacheInfo = &AcpiGbl_MemoryLists[ListId];
    if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_CACHES)))
    {
        return (NULL);
    }

    ACPI_MEM_TRACKING (CacheInfo->CacheRequests++);

    /* Check the cache first */

    if (CacheInfo->ListHead)
    {
        /* There is an object available, use it */

        Object = CacheInfo->ListHead;
        CacheInfo->ListHead = *(ACPI_CAST_INDIRECT_PTR (char, &(((char *) Object)[CacheInfo->LinkOffset])));

        ACPI_MEM_TRACKING (CacheInfo->CacheHits++);
        CacheInfo->CacheDepth--;

#ifdef ACPI_DBG_TRACK_ALLOCATIONS
        ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "Object %p from %s\n",
            Object, AcpiGbl_MemoryLists[ListId].ListName));
#endif

        if (ACPI_FAILURE (AcpiUtReleaseMutex (ACPI_MTX_CACHES)))
        {
            return (NULL);
        }

        /* Clear (zero) the previously used Object */

        ACPI_MEMSET (Object, 0, CacheInfo->ObjectSize);
    }

    else
    {
        /* The cache is empty, create a new object */

        /* Avoid deadlock with ACPI_MEM_CALLOCATE */

        if (ACPI_FAILURE (AcpiUtReleaseMutex (ACPI_MTX_CACHES)))
        {
            return (NULL);
        }

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


    ACPI_FUNCTION_ENTRY ();


    CacheInfo = &AcpiGbl_MemoryLists[ListId];
    while (CacheInfo->ListHead)
    {
        /* Delete one cached state object */

        Next = *(ACPI_CAST_INDIRECT_PTR (char, &(((char *) CacheInfo->ListHead)[CacheInfo->LinkOffset])));
        ACPI_MEM_FREE (CacheInfo->ListHead);

        CacheInfo->ListHead = Next;
        CacheInfo->CacheDepth--;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtValidateBuffer
 *
 * PARAMETERS:  Buffer              - Buffer descriptor to be validated
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Perform parameter validation checks on an ACPI_BUFFER
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtValidateBuffer (
    ACPI_BUFFER             *Buffer)
{

    /* Obviously, the structure pointer must be valid */

    if (!Buffer)
    {
        return (AE_BAD_PARAMETER);
    }

    /* Special semantics for the length */

    if ((Buffer->Length == ACPI_NO_BUFFER)              ||
        (Buffer->Length == ACPI_ALLOCATE_BUFFER)        ||
        (Buffer->Length == ACPI_ALLOCATE_LOCAL_BUFFER))
    {
        return (AE_OK);
    }

    /* Length is valid, the buffer pointer must be also */

    if (!Buffer->Pointer)
    {
        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtInitializeBuffer
 *
 * PARAMETERS:  Buffer              - Buffer to be validated
 *              RequiredLength      - Length needed
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Validate that the buffer is of the required length or
 *              allocate a new buffer.  Returned buffer is always zeroed.
 *
 ******************************************************************************/

ACPI_STATUS
AcpiUtInitializeBuffer (
    ACPI_BUFFER             *Buffer,
    ACPI_SIZE               RequiredLength)
{
    ACPI_STATUS             Status = AE_OK;


    switch (Buffer->Length)
    {
    case ACPI_NO_BUFFER:

        /* Set the exception and returned the required length */

        Status = AE_BUFFER_OVERFLOW;
        break;


    case ACPI_ALLOCATE_BUFFER:

        /* Allocate a new buffer */

        Buffer->Pointer = AcpiOsAllocate (RequiredLength);
        if (!Buffer->Pointer)
        {
            return (AE_NO_MEMORY);
        }

        /* Clear the buffer */

        ACPI_MEMSET (Buffer->Pointer, 0, RequiredLength);
        break;


    case ACPI_ALLOCATE_LOCAL_BUFFER:

        /* Allocate a new buffer with local interface to allow tracking */

        Buffer->Pointer = ACPI_MEM_CALLOCATE (RequiredLength);
        if (!Buffer->Pointer)
        {
            return (AE_NO_MEMORY);
        }
        break;


    default:

        /* Existing buffer: Validate the size of the buffer */

        if (Buffer->Length < RequiredLength)
        {
            Status = AE_BUFFER_OVERFLOW;
            break;
        }

        /* Clear the buffer */

        ACPI_MEMSET (Buffer->Pointer, 0, RequiredLength);
        break;
    }

    Buffer->Length = RequiredLength;
    return (Status);
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
    ACPI_SIZE               Size,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line)
{
    void                    *Allocation;


    ACPI_FUNCTION_TRACE_U32 ("UtAllocate", Size);


    /* Check for an inadvertent size of zero bytes */

    if (!Size)
    {
        _ACPI_REPORT_ERROR (Module, Line, Component,
                ("UtAllocate: Attempt to allocate zero bytes\n"));
        Size = 1;
    }

    Allocation = AcpiOsAllocate (Size);
    if (!Allocation)
    {
        /* Report allocation error */

        _ACPI_REPORT_ERROR (Module, Line, Component,
                ("UtAllocate: Could not allocate size %X\n", (UINT32) Size));

        return_PTR (NULL);
    }

    return_PTR (Allocation);
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
    ACPI_SIZE               Size,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line)
{
    void                    *Allocation;


    ACPI_FUNCTION_TRACE_U32 ("UtCallocate", Size);


    /* Check for an inadvertent size of zero bytes */

    if (!Size)
    {
        _ACPI_REPORT_ERROR (Module, Line, Component,
                ("UtCallocate: Attempt to allocate zero bytes\n"));
        return_PTR (NULL);
    }

    Allocation = AcpiOsAllocate (Size);
    if (!Allocation)
    {
        /* Report allocation error */

        _ACPI_REPORT_ERROR (Module, Line, Component,
                ("UtCallocate: Could not allocate size %X\n", (UINT32) Size));
        return_PTR (NULL);
    }

    /* Clear the memory block */

    ACPI_MEMSET (Allocation, 0, Size);
    return_PTR (Allocation);
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
 * FUNCTION:    AcpiUtAllocateAndTrack
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
AcpiUtAllocateAndTrack (
    ACPI_SIZE               Size,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line)
{
    ACPI_DEBUG_MEM_BLOCK    *Allocation;
    ACPI_STATUS             Status;


    Allocation = AcpiUtAllocate (Size + sizeof (ACPI_DEBUG_MEM_HEADER), Component,
                                Module, Line);
    if (!Allocation)
    {
        return (NULL);
    }

    Status = AcpiUtTrackAllocation (ACPI_MEM_LIST_GLOBAL, Allocation, Size,
                    ACPI_MEM_MALLOC, Component, Module, Line);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsFree (Allocation);
        return (NULL);
    }

    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].TotalAllocated++;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].CurrentTotalSize += (UINT32) Size;

    return ((void *) &Allocation->UserSpace);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtCallocateAndTrack
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
AcpiUtCallocateAndTrack (
    ACPI_SIZE               Size,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line)
{
    ACPI_DEBUG_MEM_BLOCK    *Allocation;
    ACPI_STATUS             Status;


    Allocation = AcpiUtCallocate (Size + sizeof (ACPI_DEBUG_MEM_HEADER), Component,
                                Module, Line);
    if (!Allocation)
    {
        /* Report allocation error */

        _ACPI_REPORT_ERROR (Module, Line, Component,
                ("UtCallocate: Could not allocate size %X\n", (UINT32) Size));
        return (NULL);
    }

    Status = AcpiUtTrackAllocation (ACPI_MEM_LIST_GLOBAL, Allocation, Size,
                        ACPI_MEM_CALLOC, Component, Module, Line);
    if (ACPI_FAILURE (Status))
    {
        AcpiOsFree (Allocation);
        return (NULL);
    }

    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].TotalAllocated++;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].CurrentTotalSize += (UINT32) Size;

    return ((void *) &Allocation->UserSpace);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtFreeAndTrack
 *
 * PARAMETERS:  Allocation          - Address of the memory to deallocate
 *              Component           - Component type of caller
 *              Module              - Source file name of caller
 *              Line                - Line number of caller
 *
 * RETURN:      None
 *
 * DESCRIPTION: Frees the memory at Allocation
 *
 ******************************************************************************/

void
AcpiUtFreeAndTrack (
    void                    *Allocation,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line)
{
    ACPI_DEBUG_MEM_BLOCK    *DebugBlock;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE_PTR ("UtFree", Allocation);


    if (NULL == Allocation)
    {
        _ACPI_REPORT_ERROR (Module, Line, Component,
            ("AcpiUtFree: Attempt to delete a NULL address\n"));

        return_VOID;
    }

    DebugBlock = ACPI_CAST_PTR (ACPI_DEBUG_MEM_BLOCK,
                    (((char *) Allocation) - sizeof (ACPI_DEBUG_MEM_HEADER)));

    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].TotalFreed++;
    AcpiGbl_MemoryLists[ACPI_MEM_LIST_GLOBAL].CurrentTotalSize -= DebugBlock->Size;

    Status = AcpiUtRemoveAllocation (ACPI_MEM_LIST_GLOBAL, DebugBlock,
                    Component, Module, Line);
    if (ACPI_FAILURE (Status))
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Could not free memory, %s\n",
            AcpiFormatException (Status)));
    }

    AcpiOsFree (DebugBlock);

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "%p freed\n", Allocation));

    return_VOID;
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtFindAllocation
 *
 * PARAMETERS:  ListId                  - Memory list to search
 *              Allocation              - Address of allocated memory
 *
 * RETURN:      A list element if found; NULL otherwise.
 *
 * DESCRIPTION: Searches for an element in the global allocation tracking list.
 *
 ******************************************************************************/

ACPI_DEBUG_MEM_BLOCK *
AcpiUtFindAllocation (
    UINT32                  ListId,
    void                    *Allocation)
{
    ACPI_DEBUG_MEM_BLOCK    *Element;


    ACPI_FUNCTION_ENTRY ();


    if (ListId > ACPI_MEM_LIST_MAX)
    {
        return (NULL);
    }

    Element = AcpiGbl_MemoryLists[ListId].ListHead;

    /* Search for the address. */

    while (Element)
    {
        if (Element == Allocation)
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
 * PARAMETERS:  ListId              - Memory list to search
 *              Allocation          - Address of allocated memory
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
    ACPI_DEBUG_MEM_BLOCK    *Allocation,
    ACPI_SIZE               Size,
    UINT8                   AllocType,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line)
{
    ACPI_MEMORY_LIST        *MemList;
    ACPI_DEBUG_MEM_BLOCK    *Element;
    ACPI_STATUS             Status = AE_OK;


    ACPI_FUNCTION_TRACE_PTR ("UtTrackAllocation", Allocation);


    if (ListId > ACPI_MEM_LIST_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    MemList = &AcpiGbl_MemoryLists[ListId];
    Status = AcpiUtAcquireMutex (ACPI_MTX_MEMORY);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /*
     * Search list for this address to make sure it is not already on the list.
     * This will catch several kinds of problems.
     */

    Element = AcpiUtFindAllocation (ListId, Allocation);
    if (Element)
    {
        ACPI_REPORT_ERROR (("UtTrackAllocation: Allocation already present in list! (%p)\n",
            Allocation));

        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR, "Element %p Address %p\n", Element, Allocation));

        goto UnlockAndExit;
    }

    /* Fill in the instance data. */

    Allocation->Size      = (UINT32) Size;
    Allocation->AllocType = AllocType;
    Allocation->Component = Component;
    Allocation->Line      = Line;

    ACPI_STRNCPY (Allocation->Module, Module, ACPI_MAX_MODULE_NAME);
    Allocation->Module[ACPI_MAX_MODULE_NAME-1] = 0;

    /* Insert at list head */

    if (MemList->ListHead)
    {
        ((ACPI_DEBUG_MEM_BLOCK *)(MemList->ListHead))->Previous = Allocation;
    }

    Allocation->Next = MemList->ListHead;
    Allocation->Previous = NULL;

    MemList->ListHead = Allocation;


UnlockAndExit:
    Status = AcpiUtReleaseMutex (ACPI_MTX_MEMORY);
    return_ACPI_STATUS (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiUtRemoveAllocation
 *
 * PARAMETERS:  ListId              - Memory list to search
 *              Allocation          - Address of allocated memory
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
    ACPI_DEBUG_MEM_BLOCK    *Allocation,
    UINT32                  Component,
    char                    *Module,
    UINT32                  Line)
{
    ACPI_MEMORY_LIST        *MemList;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_TRACE ("UtRemoveAllocation");


    if (ListId > ACPI_MEM_LIST_MAX)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    MemList = &AcpiGbl_MemoryLists[ListId];
    if (NULL == MemList->ListHead)
    {
        /* No allocations! */

        _ACPI_REPORT_ERROR (Module, Line, Component,
                ("UtRemoveAllocation: Empty allocation list, nothing to free!\n"));

        return_ACPI_STATUS (AE_OK);
    }

    Status = AcpiUtAcquireMutex (ACPI_MTX_MEMORY);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Unlink */

    if (Allocation->Previous)
    {
        (Allocation->Previous)->Next = Allocation->Next;
    }
    else
    {
        MemList->ListHead = Allocation->Next;
    }

    if (Allocation->Next)
    {
        (Allocation->Next)->Previous = Allocation->Previous;
    }

    /* Mark the segment as deleted */

    ACPI_MEMSET (&Allocation->UserSpace, 0xEA, Allocation->Size);

    ACPI_DEBUG_PRINT ((ACPI_DB_ALLOCATIONS, "Freeing size 0%X\n", Allocation->Size));

    Status = AcpiUtReleaseMutex (ACPI_MTX_MEMORY);
    return_ACPI_STATUS (Status);
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

    ACPI_FUNCTION_TRACE ("UtDumpAllocationInfo");

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
    char                    *Module)
{
    ACPI_DEBUG_MEM_BLOCK    *Element;
    ACPI_DESCRIPTOR         *Descriptor;
    UINT32                  NumOutstanding = 0;


    ACPI_FUNCTION_TRACE ("UtDumpAllocations");


    /*
     * Walk the allocation list.
     */
    if (ACPI_FAILURE (AcpiUtAcquireMutex (ACPI_MTX_MEMORY)))
    {
        return;
    }

    Element = AcpiGbl_MemoryLists[0].ListHead;
    while (Element)
    {
        if ((Element->Component & Component) &&
            ((Module == NULL) || (0 == ACPI_STRCMP (Module, Element->Module))))
        {
            /* Ignore allocated objects that are in a cache */

            Descriptor = ACPI_CAST_PTR (ACPI_DESCRIPTOR, &Element->UserSpace);
            if (Descriptor->DescriptorId != ACPI_DESC_TYPE_CACHED)
            {
                AcpiOsPrintf ("%p Len %04X %9.9s-%d [%s] ",
                            Descriptor, Element->Size, Element->Module,
                            Element->Line, AcpiUtGetDescriptorName (Descriptor));

                /* Most of the elements will be Operand objects. */

                switch (ACPI_GET_DESCRIPTOR_TYPE (Descriptor))
                {
                case ACPI_DESC_TYPE_OPERAND:
                    AcpiOsPrintf ("%12.12s R%hd",
                            AcpiUtGetTypeName (Descriptor->Object.Common.Type),
                            Descriptor->Object.Common.ReferenceCount);
                    break;

                case ACPI_DESC_TYPE_PARSER:
                    AcpiOsPrintf ("AmlOpcode %04hX",
                            Descriptor->Op.Asl.AmlOpcode);
                    break;

                case ACPI_DESC_TYPE_NAMED:
                    AcpiOsPrintf ("%4.4s",
                            AcpiUtGetNodeName (&Descriptor->Node));
                    break;

                default:
                    break;
                }

                AcpiOsPrintf ( "\n");
                NumOutstanding++;
            }
        }
        Element = Element->Next;
    }

    (void) AcpiUtReleaseMutex (ACPI_MTX_MEMORY);

    /* Print summary */

    if (!NumOutstanding)
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "No outstanding allocations.\n"));
    }
    else
    {
        ACPI_DEBUG_PRINT ((ACPI_DB_ERROR,
            "%d(%X) Outstanding allocations\n",
            NumOutstanding, NumOutstanding));
    }

    return_VOID;
}


#endif  /* #ifdef ACPI_DBG_TRACK_ALLOCATIONS */

