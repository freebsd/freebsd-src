/*******************************************************************************
 *
 * Module Name: nsnames - Name manipulation and search
 *              $Revision: 59 $
 *
 ******************************************************************************/

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

#define __NSNAMES_C__

#include "acpi.h"
#include "amlcode.h"
#include "acinterp.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_NAMESPACE
        MODULE_NAME         ("nsnames")


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsGetTablePathname
 *
 * PARAMETERS:  Node        - Scope whose name is needed
 *
 * RETURN:      Pointer to storage containing the fully qualified name of
 *              the scope, in Label format (all segments strung together
 *              with no separators)
 *
 * DESCRIPTION: Used for debug printing in AcpiNsSearchTable().
 *
 ******************************************************************************/

NATIVE_CHAR *
AcpiNsGetTablePathname (
    ACPI_NAMESPACE_NODE     *Node)
{
    NATIVE_CHAR             *NameBuffer;
    UINT32                  Size;
    ACPI_NAME               Name;
    ACPI_NAMESPACE_NODE     *ChildNode;
    ACPI_NAMESPACE_NODE     *ParentNode;


    FUNCTION_TRACE_PTR ("AcpiNsGetTablePathname", Node);


    if (!AcpiGbl_RootNode || !Node)
    {
        /*
         * If the name space has not been initialized,
         * this function should not have been called.
         */
        return_PTR (NULL);
    }

    ChildNode = Node->Child;


    /* Calculate required buffer size based on depth below root */

    Size = 1;
    ParentNode = ChildNode;
    while (ParentNode)
    {
        ParentNode = AcpiNsGetParentObject (ParentNode);
        if (ParentNode)
        {
            Size += ACPI_NAME_SIZE;
        }
    }


    /* Allocate a buffer to be returned to caller */

    NameBuffer = AcpiUtCallocate (Size + 1);
    if (!NameBuffer)
    {
        REPORT_ERROR (("NsGetTablePathname: allocation failure\n"));
        return_PTR (NULL);
    }


    /* Store terminator byte, then build name backwards */

    NameBuffer[Size] = '\0';
    while ((Size > ACPI_NAME_SIZE) &&
        AcpiNsGetParentObject (ChildNode))
    {
        Size -= ACPI_NAME_SIZE;
        Name = AcpiNsFindParentName (ChildNode);

        /* Put the name into the buffer */

        MOVE_UNALIGNED32_TO_32 ((NameBuffer + Size), &Name);
        ChildNode = AcpiNsGetParentObject (ChildNode);
    }

    NameBuffer[--Size] = AML_ROOT_PREFIX;

    if (Size != 0)
    {
        DEBUG_PRINTP (ACPI_ERROR, ("Bad pointer returned; size=%X\n", Size));
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

UINT32
AcpiNsGetPathnameLength (
    ACPI_NAMESPACE_NODE     *Node)
{
    UINT32                  Size;
    ACPI_NAMESPACE_NODE     *NextNode;

    /*
     * Compute length of pathname as 5 * number of name segments.
     * Go back up the parent tree to the root
     */
    for (Size = 0, NextNode = Node;
          AcpiNsGetParentObject (NextNode);
          NextNode = AcpiNsGetParentObject (NextNode))
    {
        Size += PATH_SEGMENT_LENGTH;
    }

    /* Special case for size still 0 - no parent for "special" nodes */

    if (!Size)
    {
        Size = PATH_SEGMENT_LENGTH;
    }

    return (Size + 1);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiNsHandleToPathname
 *
 * PARAMETERS:  TargetHandle            - Handle of named object whose name is
 *                                        to be found
 *              BufSize                 - Size of the buffer provided
 *              UserBuffer              - Where the pathname is returned
 *
 * RETURN:      Status, Buffer is filled with pathname if status is AE_OK
 *
 * DESCRIPTION: Build and return a full namespace pathname
 *
 * MUTEX:       Locks Namespace
 *
 ******************************************************************************/

ACPI_STATUS
AcpiNsHandleToPathname (
    ACPI_HANDLE             TargetHandle,
    UINT32                  *BufSize,
    NATIVE_CHAR             *UserBuffer)
{
    ACPI_STATUS             Status = AE_OK;
    ACPI_NAMESPACE_NODE     *Node;
    UINT32                  PathLength;
    UINT32                  UserBufSize;
    ACPI_NAME               Name;
    UINT32                  Size;

    FUNCTION_TRACE_PTR ("NsHandleToPathname", TargetHandle);


    if (!AcpiGbl_RootNode)
    {
        /*
         * If the name space has not been initialized,
         * this function should not have been called.
         */

        return_ACPI_STATUS (AE_NO_NAMESPACE);
    }

    Node = AcpiNsConvertHandleToEntry (TargetHandle);
    if (!Node)
    {
        return_ACPI_STATUS (AE_BAD_PARAMETER);
    }


    /* Set return length to the required path length */

    PathLength = AcpiNsGetPathnameLength (Node);
    Size = PathLength - 1;

    UserBufSize = *BufSize;
    *BufSize = PathLength;

    /* Check if the user buffer is sufficiently large */

    if (PathLength > UserBufSize)
    {
        Status = AE_BUFFER_OVERFLOW;
        goto Exit;
    }

    /* Store null terminator */

    UserBuffer[Size] = 0;
    Size -= ACPI_NAME_SIZE;

    /* Put the original ACPI name at the end of the path */

    MOVE_UNALIGNED32_TO_32 ((UserBuffer + Size),
                            &Node->Name);

    UserBuffer[--Size] = PATH_SEPARATOR;

    /* Build name backwards, putting "." between segments */

    while ((Size > ACPI_NAME_SIZE) && Node)
    {
        Size -= ACPI_NAME_SIZE;
        Name = AcpiNsFindParentName (Node);
        MOVE_UNALIGNED32_TO_32 ((UserBuffer + Size), &Name);

        UserBuffer[--Size] = PATH_SEPARATOR;
        Node = AcpiNsGetParentObject (Node);
    }

    /*
     * Overlay the "." preceding the first segment with
     * the root name "\"
     */

    UserBuffer[Size] = '\\';

    DEBUG_PRINTP (TRACE_EXEC, ("Len=%X, %s \n", PathLength, UserBuffer));

Exit:
    return_ACPI_STATUS (Status);
}


