/*******************************************************************************
 *
 * Module Name: nsnames - Name manipulation and search
 *
 ******************************************************************************/

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

#define __NSNAMES_C__

#include "acpi.h"
#include "accommon.h"
#include "amlcode.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_NAMESPACE
        ACPI_MODULE_NAME    ("nsnames")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsBuildExternalPath
 *
 * PARAMETERS:  Node            - NS node whose pathname is needed
 *              Size            - Size of the pathname
 *              *NameBuffer     - Where to return the pathname
 *
 * RETURN:      Status
 *              Places the pathname into the NameBuffer, in external format
 *              (name segments separated by path separators)
 *
 * DESCRIPTION: Generate a full pathaname
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsBuildExternalPath (
    ACPI_NAMESPACE_NODE     *Node,
    ACPI_SIZE               Size,
    char                    *NameBuffer)
{
    ACPI_SIZE               Index;
    ACPI_NAMESPACE_NODE     *ParentNode;


    ACPI_FUNCTION_ENTRY ();


    /* Special case for root */

    Index = Size - 1;
    if (Index < ACPI_NAME_SIZE)
    {
        NameBuffer[0] = AML_ROOT_PREFIX;
        NameBuffer[1] = 0;
        return (AE_OK);
    }

    /* Store terminator byte, then build name backwards */

    ParentNode = Node;
    NameBuffer[Index] = 0;

    while ((Index > ACPI_NAME_SIZE) && (ParentNode != AcpiGbl_RootNode))
    {
        Index -= ACPI_NAME_SIZE;

        /* Put the name into the buffer */

        ACPI_MOVE_32_TO_32 ((NameBuffer + Index), &ParentNode->Name);
        ParentNode = ParentNode->Parent;

        /* Prefix name with the path separator */

        Index--;
        NameBuffer[Index] = ACPI_PATH_SEPARATOR;
    }

    /* Overwrite final separator with the root prefix character */

    NameBuffer[Index] = AML_ROOT_PREFIX;

    if (Index != 0)
    {
        ACPI_ERROR ((AE_INFO,
            "Could not construct external pathname; index=%u, size=%u, Path=%s",
            (UINT32) Index, (UINT32) Size, &NameBuffer[Size]));

        return (AE_BAD_PARAMETER);
    }

    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetExternalPathname
 *
 * PARAMETERS:  Node            - Namespace node whose pathname is needed
 *
 * RETURN:      Pointer to storage containing the fully qualified name of
 *              the node, In external format (name segments separated by path
 *              separators.)
 *
 * DESCRIPTION: Used to obtain the full pathname to a namespace node, usually
 *              for error and debug statements.
 *
 ******************************************************************************/

char *
AcpiNsGetExternalPathname (
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_STATUS             Status;
    char                    *NameBuffer;
    ACPI_SIZE               Size;


    ACPI_FUNCTION_TRACE_PTR (NsGetExternalPathname, Node);


    /* Calculate required buffer size based on depth below root */

    Size = AcpiNsGetPathnameLength (Node);
    if (!Size)
    {
        return_PTR (NULL);
    }

    /* Allocate a buffer to be returned to caller */

    NameBuffer = ACPI_ALLOCATE_ZEROED (Size);
    if (!NameBuffer)
    {
        ACPI_ERROR ((AE_INFO, "Could not allocate %u bytes", (UINT32) Size));
        return_PTR (NULL);
    }

    /* Build the path in the allocated buffer */

    Status = AcpiNsBuildExternalPath (Node, Size, NameBuffer);
    if (ACPI_FAILURE (Status))
    {
        ACPI_FREE (NameBuffer);
        return_PTR (NULL);
    }

    return_PTR (NameBuffer);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetPathnameLength
 *
 * PARAMETERS:  Node        - Namespace node
 *
 * RETURN:      Length of path, including prefix
 *
 * DESCRIPTION: Get the length of the pathname string for this node
 *
 ******************************************************************************/

ACPI_SIZE
AcpiNsGetPathnameLength (
    ACPI_NAMESPACE_NODE     *Node)
{
    ACPI_SIZE               Size;
    ACPI_NAMESPACE_NODE     *NextNode;


    ACPI_FUNCTION_ENTRY ();


    /*
     * Compute length of pathname as 5 * number of name segments.
     * Go back up the parent tree to the root
     */
    Size = 0;
    NextNode = Node;

    while (NextNode && (NextNode != AcpiGbl_RootNode))
    {
        if (ACPI_GET_DESCRIPTOR_TYPE (NextNode) != ACPI_DESC_TYPE_NAMED)
        {
            ACPI_ERROR ((AE_INFO,
                "Invalid Namespace Node (%p) while traversing namespace",
                NextNode));
            return 0;
        }
        Size += ACPI_PATH_SEGMENT_LENGTH;
        NextNode = NextNode->Parent;
    }

    if (!Size)
    {
        Size = 1; /* Root node case */
    }

    return (Size + 1);  /* +1 for null string terminator */
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsHandleToPathname
 *
 * PARAMETERS:  TargetHandle            - Handle of named object whose name is
 *                                        to be found
 *              Buffer                  - Where the pathname is returned
 *
 * RETURN:      Status, Buffer is filled with pathname if status is AE_OK
 *
 * DESCRIPTION: Build and return a full namespace pathname
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsHandleToPathname (
    ACPI_HANDLE             TargetHandle,
    ACPI_BUFFER             *Buffer)
{
    ACPI_STATUS             Status;
    ACPI_NAMESPACE_NODE     *Node;
    ACPI_SIZE               RequiredSize;


    ACPI_FUNCTION_TRACE_PTR (NsHandleToPathname, TargetHandle);


    Node = AcpiNsValidateHandle (TargetHandle);
    if (!Node)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Determine size required for the caller buffer */

    RequiredSize = AcpiNsGetPathnameLength (Node);
    if (!RequiredSize)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }

    /* Validate/Allocate/Clear caller buffer */

    Status = AcpiUtInitializeBuffer (Buffer, RequiredSize);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Build the path in the caller buffer */

    Status = AcpiNsBuildExternalPath (Node, RequiredSize, Buffer->Pointer);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_EXEC, "%s [%X]\n",
        (char *) Buffer->Pointer, (UINT32) RequiredSize));
    return_ACPI_STATUS (AE_OK);
}


