/******************************************************************************
 *
 * Module Name: aslcache -- Local cache support for iASL
 *
 *****************************************************************************/

/******************************************************************************
 *
 * 1. Copyright Notice
 *
 * Some or all of this work - Copyright (c) 1999 - 2018, Intel Corp.
 * All rights reserved.
 *
 * 2. License
 *
 * 2.1. This is your license from Intel Corp. under its intellectual property
 * rights. You may have additional license terms from the party that provided
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
 * to or modifications of the Original Intel Code. No other license or right
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
 * and the following Disclaimer and Export Compliance provision. In addition,
 * Licensee must cause all Covered Code to which Licensee contributes to
 * contain a file documenting the changes Licensee made to create that Covered
 * Code and the date of any change. Licensee must include in that file the
 * documentation of any changes made by any predecessor Licensee. Licensee
 * must include a prominent statement that the modification is derived,
 * directly or indirectly, from Original Intel Code.
 *
 * 3.2. Redistribution of Source with no Rights to Further Distribute Source.
 * Redistribution of source code of any substantial portion of the Covered
 * Code or modification without rights to further distribute source must
 * include the following Disclaimer and Export Compliance provision in the
 * documentation and/or other materials provided with distribution. In
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
 * HERE. ANY SOFTWARE ORIGINATING FROM INTEL OR DERIVED FROM INTEL SOFTWARE
 * IS PROVIDED "AS IS," AND INTEL WILL NOT PROVIDE ANY SUPPORT, ASSISTANCE,
 * INSTALLATION, TRAINING OR OTHER SERVICES. INTEL WILL NOT PROVIDE ANY
 * UPDATES, ENHANCEMENTS OR EXTENSIONS. INTEL SPECIFICALLY DISCLAIMS ANY
 * IMPLIED WARRANTIES OF MERCHANTABILITY, NONINFRINGEMENT AND FITNESS FOR A
 * PARTICULAR PURPOSE.
 *
 * 4.2. IN NO EVENT SHALL INTEL HAVE ANY LIABILITY TO LICENSEE, ITS LICENSEES
 * OR ANY OTHER THIRD PARTY, FOR ANY LOST PROFITS, LOST DATA, LOSS OF USE OR
 * COSTS OF PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES, OR FOR ANY INDIRECT,
 * SPECIAL OR CONSEQUENTIAL DAMAGES ARISING OUT OF THIS AGREEMENT, UNDER ANY
 * CAUSE OF ACTION OR THEORY OF LIABILITY, AND IRRESPECTIVE OF WHETHER INTEL
 * HAS ADVANCE NOTICE OF THE POSSIBILITY OF SUCH DAMAGES. THESE LIMITATIONS
 * SHALL APPLY NOTWITHSTANDING THE FAILURE OF THE ESSENTIAL PURPOSE OF ANY
 * LIMITED REMEDY.
 *
 * 4.3. Licensee shall not export, either directly or indirectly, any of this
 * software or system incorporating such software without first obtaining any
 * required license or other approval from the U. S. Department of Commerce or
 * any other agency or department of the United States Government. In the
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
 *****************************************************************************
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * following license:
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Alternatively, you may choose to be licensed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 *****************************************************************************/

#include "aslcompiler.h"

/*
 * Local caches. The caches are fully deleted after the compilation/disassembly
 * of each individual input file. Thus, individual allocations from the cache
 * memory do not need to be freed or even released back into the cache.
 *
 * See aslallocate.c for standard heap allocations.
 */


/*******************************************************************************
 *
 * FUNCTION:    UtLocalCacheCalloc
 *
 * PARAMETERS:  Length              - Size of buffer requested
 *
 * RETURN:      Pointer to the buffer. Aborts compiler on allocation failure
 *
 * DESCRIPTION: Allocate a string buffer. Bypass the local
 *              dynamic memory manager for performance reasons (This has a
 *              major impact on the speed of the compiler.)
 *
 ******************************************************************************/

char *
UtLocalCacheCalloc (
    UINT32                  Length)
{
    char                    *Buffer;
    ASL_CACHE_INFO          *Cache;
    UINT32                  CacheSize = ASL_STRING_CACHE_SIZE;


    if (Length > CacheSize)
    {
        CacheSize = Length;

        if (Gbl_StringCacheList)
        {
            Cache = UtLocalCalloc (sizeof (Cache->Next) + CacheSize);

            /* Link new cache buffer just following head of list */

            Cache->Next = Gbl_StringCacheList->Next;
            Gbl_StringCacheList->Next = Cache;

            /* Leave cache management pointers alone as they pertain to head */

            Gbl_StringCount++;
            Gbl_StringSize += Length;

            return (Cache->Buffer);
        }
    }

    if ((Gbl_StringCacheNext + Length) >= Gbl_StringCacheLast)
    {
        /* Allocate a new buffer */

        Cache = UtLocalCalloc (sizeof (Cache->Next) + CacheSize);

        /* Link new cache buffer to head of list */

        Cache->Next = Gbl_StringCacheList;
        Gbl_StringCacheList = Cache;

        /* Setup cache management pointers */

        Gbl_StringCacheNext = Cache->Buffer;
        Gbl_StringCacheLast = Gbl_StringCacheNext + CacheSize;
    }

    Gbl_StringCount++;
    Gbl_StringSize += Length;

    Buffer = Gbl_StringCacheNext;
    Gbl_StringCacheNext += Length;
    return (Buffer);
}


/*******************************************************************************
 *
 * FUNCTION:    UtParseOpCacheCalloc
 *
 * PARAMETERS:  None
 *
 * RETURN:      New parse op. Aborts on allocation failure
 *
 * DESCRIPTION: Allocate a new parse op for the parse tree. Bypass the local
 *              dynamic memory manager for performance reasons (This has a
 *              major impact on the speed of the compiler.)
 *
 ******************************************************************************/

ACPI_PARSE_OBJECT *
UtParseOpCacheCalloc (
    void)
{
    ASL_CACHE_INFO          *Cache;


    if (Gbl_ParseOpCacheNext >= Gbl_ParseOpCacheLast)
    {
        /* Allocate a new buffer */

        Cache = UtLocalCalloc (sizeof (Cache->Next) +
            (sizeof (ACPI_PARSE_OBJECT) * ASL_PARSEOP_CACHE_SIZE));

        /* Link new cache buffer to head of list */

        Cache->Next = Gbl_ParseOpCacheList;
        Gbl_ParseOpCacheList = Cache;

        /* Setup cache management pointers */

        Gbl_ParseOpCacheNext = ACPI_CAST_PTR (ACPI_PARSE_OBJECT, Cache->Buffer);
        Gbl_ParseOpCacheLast = Gbl_ParseOpCacheNext + ASL_PARSEOP_CACHE_SIZE;
    }

    Gbl_ParseOpCount++;
    return (Gbl_ParseOpCacheNext++);
}


/*******************************************************************************
 *
 * FUNCTION:    UtSubtableCacheCalloc - Data Table compiler
 *
 * PARAMETERS:  None
 *
 * RETURN:      Pointer to the buffer. Aborts on allocation failure
 *
 * DESCRIPTION: Allocate a subtable object buffer. Bypass the local
 *              dynamic memory manager for performance reasons (This has a
 *              major impact on the speed of the compiler.)
 *
 ******************************************************************************/

DT_SUBTABLE *
UtSubtableCacheCalloc (
    void)
{
    ASL_CACHE_INFO          *Cache;


    if (Gbl_SubtableCacheNext >= Gbl_SubtableCacheLast)
    {
        /* Allocate a new buffer */

        Cache = UtLocalCalloc (sizeof (Cache->Next) +
            (sizeof (DT_SUBTABLE) * ASL_SUBTABLE_CACHE_SIZE));

        /* Link new cache buffer to head of list */

        Cache->Next = Gbl_SubtableCacheList;
        Gbl_SubtableCacheList = Cache;

        /* Setup cache management pointers */

        Gbl_SubtableCacheNext = ACPI_CAST_PTR (DT_SUBTABLE, Cache->Buffer);
        Gbl_SubtableCacheLast = Gbl_SubtableCacheNext + ASL_SUBTABLE_CACHE_SIZE;
    }

    Gbl_SubtableCount++;
    return (Gbl_SubtableCacheNext++);
}


/*******************************************************************************
 *
 * FUNCTION:    UtFieldCacheCalloc - Data Table compiler
 *
 * PARAMETERS:  None
 *
 * RETURN:      Pointer to the buffer. Aborts on allocation failure
 *
 * DESCRIPTION: Allocate a field object buffer. Bypass the local
 *              dynamic memory manager for performance reasons (This has a
 *              major impact on the speed of the compiler.)
 *
 ******************************************************************************/

DT_FIELD *
UtFieldCacheCalloc (
    void)
{
    ASL_CACHE_INFO          *Cache;


    if (Gbl_FieldCacheNext >= Gbl_FieldCacheLast)
    {
        /* Allocate a new buffer */

        Cache = UtLocalCalloc (sizeof (Cache->Next) +
            (sizeof (DT_FIELD) * ASL_FIELD_CACHE_SIZE));

        /* Link new cache buffer to head of list */

        Cache->Next = Gbl_FieldCacheList;
        Gbl_FieldCacheList = Cache;

        /* Setup cache management pointers */

        Gbl_FieldCacheNext = ACPI_CAST_PTR (DT_FIELD, Cache->Buffer);
        Gbl_FieldCacheLast = Gbl_FieldCacheNext + ASL_FIELD_CACHE_SIZE;
    }

    Gbl_FieldCount++;
    return (Gbl_FieldCacheNext++);
}


/*******************************************************************************
 *
 * FUNCTION:    UtDeleteLocalCaches
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Delete all local cache buffer blocks
 *
 ******************************************************************************/

void
UtDeleteLocalCaches (
    void)
{
    UINT32                  BufferCount;
    ASL_CACHE_INFO          *Next;


    /*
     * Generic cache, arbitrary size allocations
     */
    BufferCount = 0;
    while (Gbl_StringCacheList)
    {
        Next = Gbl_StringCacheList->Next;
        ACPI_FREE (Gbl_StringCacheList);
        Gbl_StringCacheList = Next;
        BufferCount++;
    }

    DbgPrint (ASL_DEBUG_OUTPUT,
        "%u Strings (%u bytes), Buffer size: %u bytes, %u Buffers\n",
        Gbl_StringCount, Gbl_StringSize, ASL_STRING_CACHE_SIZE, BufferCount);

    /* Reset cache globals */

    Gbl_StringSize = 0;
    Gbl_StringCount = 0;
    Gbl_StringCacheNext = NULL;
    Gbl_StringCacheLast = NULL;


    /*
     * Parse Op cache
     */
    BufferCount = 0;
    while (Gbl_ParseOpCacheList)
    {
        Next = Gbl_ParseOpCacheList->Next;
        ACPI_FREE (Gbl_ParseOpCacheList);
        Gbl_ParseOpCacheList = Next;
        BufferCount++;
    }

    DbgPrint (ASL_DEBUG_OUTPUT,
        "%u ParseOps, Buffer size: %u ops (%u bytes), %u Buffers\n",
        Gbl_ParseOpCount, ASL_PARSEOP_CACHE_SIZE,
        (sizeof (ACPI_PARSE_OBJECT) * ASL_PARSEOP_CACHE_SIZE), BufferCount);

    /* Reset cache globals */

    Gbl_ParseOpCount = 0;
    Gbl_ParseOpCacheNext = NULL;
    Gbl_ParseOpCacheLast = NULL;
    Gbl_ParseTreeRoot = NULL;


    /*
     * Table Compiler - Field cache
     */
    BufferCount = 0;
    while (Gbl_FieldCacheList)
    {
        Next = Gbl_FieldCacheList->Next;
        ACPI_FREE (Gbl_FieldCacheList);
        Gbl_FieldCacheList = Next;
        BufferCount++;
    }

    DbgPrint (ASL_DEBUG_OUTPUT,
        "%u Fields, Buffer size: %u fields (%u bytes), %u Buffers\n",
        Gbl_FieldCount, ASL_FIELD_CACHE_SIZE,
        (sizeof (DT_FIELD) * ASL_FIELD_CACHE_SIZE), BufferCount);

    /* Reset cache globals */

    Gbl_FieldCount = 0;
    Gbl_FieldCacheNext = NULL;
    Gbl_FieldCacheLast = NULL;


    /*
     * Table Compiler - Subtable cache
     */
    BufferCount = 0;
    while (Gbl_SubtableCacheList)
    {
        Next = Gbl_SubtableCacheList->Next;
        ACPI_FREE (Gbl_SubtableCacheList);
        Gbl_SubtableCacheList = Next;
        BufferCount++;
    }

    DbgPrint (ASL_DEBUG_OUTPUT,
        "%u Subtables, Buffer size: %u subtables (%u bytes), %u Buffers\n",
        Gbl_SubtableCount, ASL_SUBTABLE_CACHE_SIZE,
        (sizeof (DT_SUBTABLE) * ASL_SUBTABLE_CACHE_SIZE), BufferCount);

    /* Reset cache globals */

    Gbl_SubtableCount = 0;
    Gbl_SubtableCacheNext = NULL;
    Gbl_SubtableCacheLast = NULL;
}
