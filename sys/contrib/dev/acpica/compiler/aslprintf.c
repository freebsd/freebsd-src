/******************************************************************************
 *
 * Module Name: aslprintf - ASL Printf/Fprintf macro support
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2016, Intel Corp.
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/amlcode.h>

#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslprintf")


/* Local prototypes */

static void
OpcCreateConcatenateNode (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *Node);

static void
OpcParsePrintf (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *DestOp);


/*******************************************************************************
 *
 * FUNCTION:    OpcDoPrintf
 *
 * PARAMETERS:  Op                  - printf parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert printf macro to a Store(..., Debug) AML operation.
 *
 ******************************************************************************/

void
OpcDoPrintf (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *DestOp;


    /* Store destination is the Debug op */

    DestOp = TrAllocateNode (PARSEOP_DEBUG);
    DestOp->Asl.AmlOpcode = AML_DEBUG_OP;
    DestOp->Asl.Parent = Op;
    DestOp->Asl.LogicalLineNumber = Op->Asl.LogicalLineNumber;

    OpcParsePrintf (Op, DestOp);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcDoFprintf
 *
 * PARAMETERS:  Op                  - fprintf parse node
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert fprintf macro to a Store AML operation.
 *
 ******************************************************************************/

void
OpcDoFprintf (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *DestOp;


    /* Store destination is the first argument of fprintf */

    DestOp = Op->Asl.Child;
    Op->Asl.Child = DestOp->Asl.Next;
    DestOp->Asl.Next = NULL;

    OpcParsePrintf (Op, DestOp);
}


/*******************************************************************************
 *
 * FUNCTION:    OpcParsePrintf
 *
 * PARAMETERS:  Op                  - Printf parse node
 *              DestOp              - Destination of Store operation
 *
 * RETURN:      None
 *
 * DESCRIPTION: Convert printf macro to a Store AML operation. The printf
 *              macro parse tree is layed out as follows:
 *
 *              Op        - printf parse op
 *              Op->Child - Format string
 *              Op->Next  - Format string arguments
 *
 ******************************************************************************/

static void
OpcParsePrintf (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *DestOp)
{
    char                    *Format;
    char                    *StartPosition = NULL;
    ACPI_PARSE_OBJECT       *ArgNode;
    ACPI_PARSE_OBJECT       *NextNode;
    UINT32                  StringLength = 0;
    char                    *NewString;
    BOOLEAN                 StringToProcess = FALSE;
    ACPI_PARSE_OBJECT       *NewOp;


    /* Get format string */

    Format = ACPI_CAST_PTR (char, Op->Asl.Child->Asl.Value.String);
    ArgNode = Op->Asl.Child->Asl.Next;

    /*
     * Detach argument list so that we can use a NULL check to distinguish
     * the first concatenation operation we need to make
     */
    Op->Asl.Child = NULL;

    for (; *Format; ++Format)
    {
        if (*Format != '%')
        {
            if (!StringToProcess)
            {
                /* Mark the beginning of a string */

                StartPosition = Format;
                StringToProcess = TRUE;
            }

            ++StringLength;
            continue;
        }

        /* Save string, if any, to new string object and concat it */

        if (StringToProcess)
        {
            NewString = UtStringCacheCalloc (StringLength + 1);
            strncpy (NewString, StartPosition, StringLength);

            NewOp = TrAllocateNode (PARSEOP_STRING_LITERAL);
            NewOp->Asl.Value.String = NewString;
            NewOp->Asl.AmlOpcode = AML_STRING_OP;
            NewOp->Asl.AcpiBtype = ACPI_BTYPE_STRING;
            NewOp->Asl.LogicalLineNumber = Op->Asl.LogicalLineNumber;

            OpcCreateConcatenateNode(Op, NewOp);

            StringLength = 0;
            StringToProcess = FALSE;
        }

        ++Format;

        /*
         * We have a format parameter and will need an argument to go
         * with it
         */
        if (!ArgNode ||
            ArgNode->Asl.ParseOpcode == PARSEOP_DEFAULT_ARG)
        {
            AslError(ASL_ERROR, ASL_MSG_ARG_COUNT_LO, Op, NULL);
            return;
        }

        /*
         * We do not support sub-specifiers of printf (flags, width,
         * precision, length). For specifiers we only support %x/%X for
         * hex or %s for strings. Also, %o for generic "acpi object".
         */
        switch (*Format)
        {
        case 's':

            if (ArgNode->Asl.ParseOpcode != PARSEOP_STRING_LITERAL)
            {
                AslError(ASL_ERROR, ASL_MSG_INVALID_TYPE, ArgNode,
                    "String required");
                return;
            }

            NextNode = ArgNode->Asl.Next;
            ArgNode->Asl.Next = NULL;
            OpcCreateConcatenateNode(Op, ArgNode);
            ArgNode = NextNode;
            continue;

        case 'X':
        case 'x':
        case 'o':

            NextNode = ArgNode->Asl.Next;
            ArgNode->Asl.Next = NULL;

            /*
             * Append an empty string if the first argument is
             * not a string. This will implicitly conver the 2nd
             * concat source to a string per the ACPI specification.
             */
            if (!Op->Asl.Child)
            {
                NewOp = TrAllocateNode (PARSEOP_STRING_LITERAL);
                NewOp->Asl.Value.String = "";
                NewOp->Asl.AmlOpcode = AML_STRING_OP;
                NewOp->Asl.AcpiBtype = ACPI_BTYPE_STRING;
                NewOp->Asl.LogicalLineNumber = Op->Asl.LogicalLineNumber;

                OpcCreateConcatenateNode(Op, NewOp);
            }

            OpcCreateConcatenateNode(Op, ArgNode);
            ArgNode = NextNode;
            break;

        default:

            AslError(ASL_ERROR, ASL_MSG_INVALID_OPERAND, Op,
                "Unrecognized format specifier");
            continue;
        }
    }

    /* Process any remaining string */

    if (StringToProcess)
    {
        NewString = UtStringCacheCalloc (StringLength + 1);
        strncpy (NewString, StartPosition, StringLength);

        NewOp = TrAllocateNode (PARSEOP_STRING_LITERAL);
        NewOp->Asl.Value.String = NewString;
        NewOp->Asl.AcpiBtype = ACPI_BTYPE_STRING;
        NewOp->Asl.AmlOpcode = AML_STRING_OP;
        NewOp->Asl.LogicalLineNumber = Op->Asl.LogicalLineNumber;

        OpcCreateConcatenateNode(Op, NewOp);
    }

    /*
     * If we get here and there's no child node then Format
     * was an empty string. Just make a no op.
     */
    if (!Op->Asl.Child)
    {
        Op->Asl.ParseOpcode = PARSEOP_NOOP;
        AslError(ASL_WARNING, ASL_MSG_NULL_STRING, Op,
            "Converted to NOOP");
        return;
    }

     /* Check for erroneous extra arguments */

    if (ArgNode &&
        ArgNode->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG)
    {
        AslError(ASL_WARNING, ASL_MSG_ARG_COUNT_HI, ArgNode,
            "Extra arguments ignored");
    }

    /* Change Op to a Store */

    Op->Asl.ParseOpcode = PARSEOP_STORE;
    Op->Common.AmlOpcode = AML_STORE_OP;
    Op->Asl.CompileFlags  = 0;

    /* Disable further optimization */

    Op->Asl.CompileFlags &= ~NODE_COMPILE_TIME_CONST;
    UtSetParseOpName (Op);

    /* Set Store destination */

    Op->Asl.Child->Asl.Next = DestOp;
}


/*******************************************************************************
 *
 * FUNCTION:    OpcCreateConcatenateNode
 *
 * PARAMETERS:  Op                  - Parse node
 *              Node                - Parse node to be concatenated
 *
 * RETURN:      None
 *
 * DESCRIPTION: Make Node the child of Op. If child node already exists, then
 *              concat child with Node and makes concat node the child of Op.
 *
 ******************************************************************************/

static void
OpcCreateConcatenateNode (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_PARSE_OBJECT       *Node)
{
    ACPI_PARSE_OBJECT       *NewConcatOp;


    if (!Op->Asl.Child)
    {
        Op->Asl.Child = Node;
        Node->Asl.Parent = Op;
        return;
    }

    NewConcatOp = TrAllocateNode (PARSEOP_CONCATENATE);
    NewConcatOp->Asl.AmlOpcode = AML_CONCAT_OP;
    NewConcatOp->Asl.AcpiBtype = 0x7;
    NewConcatOp->Asl.LogicalLineNumber = Op->Asl.LogicalLineNumber;

    /* First arg is child of Op*/

    NewConcatOp->Asl.Child = Op->Asl.Child;
    Op->Asl.Child->Asl.Parent = NewConcatOp;

    /* Second arg is Node */

    NewConcatOp->Asl.Child->Asl.Next = Node;
    Node->Asl.Parent = NewConcatOp;

    /* Third arg is Zero (not used) */

    NewConcatOp->Asl.Child->Asl.Next->Asl.Next =
        TrAllocateNode (PARSEOP_ZERO);
    NewConcatOp->Asl.Child->Asl.Next->Asl.Next->Asl.Parent =
        NewConcatOp;

    Op->Asl.Child = NewConcatOp;
    NewConcatOp->Asl.Parent = Op;
}
