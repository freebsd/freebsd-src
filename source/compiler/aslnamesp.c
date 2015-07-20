/******************************************************************************
 *
 * Module Name: aslnamesp - Namespace output file generation
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2015, Intel Corp.
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

#include "aslcompiler.h"
#include "aslcompiler.y.h"
#include "acnamesp.h"


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslnamesp")

/* Local prototypes */

static ACPI_STATUS
NsDoOneNamespaceObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);

static ACPI_STATUS
NsDoOnePathname (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue);


/*******************************************************************************
 *
 * FUNCTION:    NsSetupNamespaceListing
 *
 * PARAMETERS:  Handle          - local file handle
 *
 * RETURN:      None
 *
 * DESCRIPTION: Set the namespace output file to the input handle
 *
 ******************************************************************************/

void
NsSetupNamespaceListing (
    void                    *Handle)
{

    Gbl_NsOutputFlag = TRUE;
    Gbl_Files[ASL_FILE_NAMESPACE_OUTPUT].Handle = Handle;
}


/*******************************************************************************
 *
 * FUNCTION:    NsDisplayNamespace
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk the namespace an display information about each node
 *              in the tree. Information is written to the optional
 *              namespace output file.
 *
 ******************************************************************************/

ACPI_STATUS
NsDisplayNamespace (
    void)
{
    ACPI_STATUS             Status;


    if (!Gbl_NsOutputFlag)
    {
        return (AE_OK);
    }

    Gbl_NumNamespaceObjects = 0;

    /* File header */

    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "Contents of ACPI Namespace\n\n");
    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "Count  Depth    Name - Type\n\n");

    /* Walk entire namespace from the root */

    Status = AcpiNsWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
                ACPI_UINT32_MAX, FALSE, NsDoOneNamespaceObject, NULL,
                NULL, NULL);

    /* Print the full pathname for each namespace node */

    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "\nNamespace pathnames\n\n");

    Status = AcpiNsWalkNamespace (ACPI_TYPE_ANY, ACPI_ROOT_OBJECT,
                ACPI_UINT32_MAX, FALSE, NsDoOnePathname, NULL,
                NULL, NULL);

    return (Status);
}


/*******************************************************************************
 *
 * FUNCTION:    NsDoOneNamespaceObject
 *
 * PARAMETERS:  ACPI_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Dump a namespace object to the namespace output file.
 *              Called during the walk of the namespace to dump all objects.
 *
 ******************************************************************************/

static ACPI_STATUS
NsDoOneNamespaceObject (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_OPERAND_OBJECT     *ObjDesc;
    ACPI_PARSE_OBJECT       *Op;


    Gbl_NumNamespaceObjects++;

    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "%5u  [%u]  %*s %4.4s - %s",
        Gbl_NumNamespaceObjects, Level, (Level * 3), " ",
        &Node->Name,
        AcpiUtGetTypeName (Node->Type));

    Op = Node->Op;
    ObjDesc = ACPI_CAST_PTR (ACPI_OPERAND_OBJECT, Node->Object);

    if (!Op)
    {
        FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "\n");
        return (AE_OK);
    }


    if ((ObjDesc) &&
        (ACPI_GET_DESCRIPTOR_TYPE (ObjDesc) == ACPI_DESC_TYPE_OPERAND))
    {
        switch (Node->Type)
        {
        case ACPI_TYPE_INTEGER:

            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "       [Initial Value   0x%8.8X%8.8X]",
                ACPI_FORMAT_UINT64 (ObjDesc->Integer.Value));
            break;

        case ACPI_TYPE_STRING:

            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "        [Initial Value   \"%s\"]",
                ObjDesc->String.Pointer);
            break;

        default:

            /* Nothing to do for other types */

            break;
        }

    }
    else
    {
        switch (Node->Type)
        {
        case ACPI_TYPE_INTEGER:

            if (Op->Asl.ParseOpcode == PARSEOP_NAME)
            {
                Op = Op->Asl.Child;
            }
            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Next;
            }
            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "       [Initial Value   0x%8.8X%8.8X]",
                ACPI_FORMAT_UINT64 (Op->Asl.Value.Integer));
            break;

        case ACPI_TYPE_STRING:

            if (Op->Asl.ParseOpcode == PARSEOP_NAME)
            {
                Op = Op->Asl.Child;
            }
            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Next;
            }
            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "        [Initial Value   \"%s\"]",
                Op->Asl.Value.String);
            break;

        case ACPI_TYPE_LOCAL_REGION_FIELD:

            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Child;
            }
            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "   [Offset 0x%04X   Length 0x%04X bits]",
                Op->Asl.Parent->Asl.ExtraValue, (UINT32) Op->Asl.Value.Integer);
            break;

        case ACPI_TYPE_BUFFER_FIELD:

            switch (Op->Asl.ParseOpcode)
            {
            case PARSEOP_CREATEBYTEFIELD:

                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [BYTE  ( 8 bit)]");
                break;

            case PARSEOP_CREATEDWORDFIELD:

                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [DWORD (32 bit)]");
                break;

            case PARSEOP_CREATEQWORDFIELD:

                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [QWORD (64 bit)]");
                break;

            case PARSEOP_CREATEWORDFIELD:

                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [WORD  (16 bit)]");
                break;

            case PARSEOP_CREATEBITFIELD:

                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [BIT   ( 1 bit)]");
                break;

            case PARSEOP_CREATEFIELD:

                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "   [Arbitrary Bit Field]");
                break;

            default:

                break;

            }
            break;

        case ACPI_TYPE_PACKAGE:

            if (Op->Asl.ParseOpcode == PARSEOP_NAME)
            {
                Op = Op->Asl.Child;
            }
            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Next;
            }
            Op = Op->Asl.Child;

            if ((Op->Asl.ParseOpcode == PARSEOP_BYTECONST) ||
                (Op->Asl.ParseOpcode == PARSEOP_RAW_DATA))
            {
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                    "       [Initial Length  0x%.2X elements]",
                    Op->Asl.Value.Integer);
            }
            break;

        case ACPI_TYPE_BUFFER:

            if (Op->Asl.ParseOpcode == PARSEOP_NAME)
            {
                Op = Op->Asl.Child;
            }
            if ((Op->Asl.ParseOpcode == PARSEOP_NAMESEG)  ||
                (Op->Asl.ParseOpcode == PARSEOP_NAMESTRING))
            {
                Op = Op->Asl.Next;
            }
            Op = Op->Asl.Child;

            if (Op && (Op->Asl.ParseOpcode == PARSEOP_INTEGER))
            {
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                    "        [Initial Length  0x%.2X bytes]",
                    Op->Asl.Value.Integer);
            }
            break;

        case ACPI_TYPE_METHOD:

            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "        [Code Length     0x%.4X bytes]",
                Op->Asl.AmlSubtreeLength);
            break;

        case ACPI_TYPE_LOCAL_RESOURCE:

            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "  [Desc Offset     0x%.4X Bytes]", Node->Value);
            break;

        case ACPI_TYPE_LOCAL_RESOURCE_FIELD:

            FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                "   [Field Offset    0x%.4X Bits 0x%.4X Bytes] ",
                Node->Value, Node->Value / 8);

            if (Node->Flags & ANOBJ_IS_REFERENCED)
            {
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                    "Referenced");
            }
            else
            {
                FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT,
                    "Name not referenced");
            }
            break;

        default:

            /* Nothing to do for other types */

            break;
        }
    }

    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "\n");
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    NsDoOnePathname
 *
 * PARAMETERS:  ACPI_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Print the full pathname for a namespace node.
 *
 ******************************************************************************/

static ACPI_STATUS
NsDoOnePathname (
    ACPI_HANDLE             ObjHandle,
    UINT32                  Level,
    void                    *Context,
    void                    **ReturnValue)
{
    ACPI_NAMESPACE_NODE     *Node = (ACPI_NAMESPACE_NODE *) ObjHandle;
    ACPI_STATUS             Status;
    ACPI_BUFFER             TargetPath;


    TargetPath.Length = ACPI_ALLOCATE_LOCAL_BUFFER;
    Status = AcpiNsHandleToPathname (Node, &TargetPath, FALSE);
    if (ACPI_FAILURE (Status))
    {
        return (Status);
    }

    FlPrintFile (ASL_FILE_NAMESPACE_OUTPUT, "%s\n", TargetPath.Pointer);
    ACPI_FREE (TargetPath.Pointer);

    return (AE_OK);
}
