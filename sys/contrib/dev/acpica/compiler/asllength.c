
/******************************************************************************
 *
 * Module Name: asllength - Tree walk to determine package and opcode lengths
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


#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <contrib/dev/acpica/include/amlcode.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("asllength")

/* Local prototypes */

static UINT8
CgGetPackageLenByteCount (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  PackageLength);

static void
CgGenerateAmlOpcodeLength (
    ACPI_PARSE_OBJECT       *Op);


#ifdef ACPI_OBSOLETE_FUNCTIONS
void
LnAdjustLengthToRoot (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  LengthDelta);
#endif


/*******************************************************************************
 *
 * FUNCTION:    LnInitLengthsWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk callback to initialize (and re-initialize) the node
 *              subtree length(s) to zero.  The Subtree lengths are bubbled
 *              up to the root node in order to get a total AML length.
 *
 ******************************************************************************/

ACPI_STATUS
LnInitLengthsWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    Op->Asl.AmlSubtreeLength = 0;
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    LnPackageLengthWalk
 *
 * PARAMETERS:  ASL_WALK_CALLBACK
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Walk callback to calculate the total AML length.
 *              1) Calculate the AML lengths (opcode, package length, etc.) for
 *                 THIS node.
 *              2) Bubbble up all of these lengths to the parent node by summing
 *                 them all into the parent subtree length.
 *
 * Note:  The SubtreeLength represents the total AML length of all child nodes
 *        in all subtrees under a given node.  Therefore, once this walk is
 *        complete, the Root Node subtree length is the AML length of the entire
 *        tree (and thus, the entire ACPI table)
 *
 ******************************************************************************/

ACPI_STATUS
LnPackageLengthWalk (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  Level,
    void                    *Context)
{

    /* Generate the AML lengths for this node */

    CgGenerateAmlLengths (Op);

    /* Bubble up all lengths (this node and all below it) to the parent */

    if ((Op->Asl.Parent) &&
        (Op->Asl.ParseOpcode != PARSEOP_DEFAULT_ARG))
    {
        Op->Asl.Parent->Asl.AmlSubtreeLength += (Op->Asl.AmlLength +
                                           Op->Asl.AmlOpcodeLength +
                                           Op->Asl.AmlPkgLenBytes +
                                           Op->Asl.AmlSubtreeLength);
    }
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    CgGetPackageLenByteCount
 *
 * PARAMETERS:  Op              - Parse node
 *              PackageLength   - Length to be encoded
 *
 * RETURN:      Required length of the package length encoding
 *
 * DESCRIPTION: Calculate the number of bytes required to encode the given
 *              package length.
 *
 ******************************************************************************/

static UINT8
CgGetPackageLenByteCount (
    ACPI_PARSE_OBJECT       *Op,
    UINT32                  PackageLength)
{

    /*
     * Determine the number of bytes required to encode the package length
     * Note: the package length includes the number of bytes used to encode
     * the package length, so we must account for this also.
     */
    if (PackageLength <= (0x0000003F - 1))
    {
        return (1);
    }
    else if (PackageLength <= (0x00000FFF - 2))
    {
        return (2);
    }
    else if (PackageLength <= (0x000FFFFF - 3))
    {
        return (3);
    }
    else if (PackageLength <= (0x0FFFFFFF - 4))
    {
        return (4);
    }
    else
    {
        /* Fatal error - the package length is too large to encode */

        AslError (ASL_ERROR, ASL_MSG_ENCODING_LENGTH, Op, NULL);
    }

    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    CgGenerateAmlOpcodeLength
 *
 * PARAMETERS:  Op          - Parse node whose AML opcode lengths will be
 *                            calculated
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Calculate the AmlOpcodeLength, AmlPkgLenBytes, and AmlLength
 *              fields for this node.
 *
 ******************************************************************************/

static void
CgGenerateAmlOpcodeLength (
    ACPI_PARSE_OBJECT       *Op)
{

    /* Check for two-byte opcode */

    if (Op->Asl.AmlOpcode > 0x00FF)
    {
        Op->Asl.AmlOpcodeLength = 2;
    }
    else
    {
        Op->Asl.AmlOpcodeLength = 1;
    }

    /* Does this opcode have an associated "PackageLength" field? */

    Op->Asl.AmlPkgLenBytes = 0;
    if (Op->Asl.CompileFlags & NODE_AML_PACKAGE)
    {
        Op->Asl.AmlPkgLenBytes = CgGetPackageLenByteCount (
                                    Op, Op->Asl.AmlSubtreeLength);
    }

    /* Data opcode lengths are easy */

    switch (Op->Asl.AmlOpcode)
    {
    case AML_BYTE_OP:

        Op->Asl.AmlLength = 1;
        break;

    case AML_WORD_OP:

        Op->Asl.AmlLength = 2;
        break;

    case AML_DWORD_OP:

        Op->Asl.AmlLength = 4;
        break;

    case AML_QWORD_OP:

        Op->Asl.AmlLength = 8;
        break;

    default:
        /* All data opcodes must be above */
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    CgGenerateAmlLengths
 *
 * PARAMETERS:  Op        - Parse node
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Generate internal length fields based on the AML opcode or
 *              parse opcode.
 *
 ******************************************************************************/

void
CgGenerateAmlLengths (
    ACPI_PARSE_OBJECT       *Op)
{
    char                    *Buffer;
    ACPI_STATUS             Status;


    switch (Op->Asl.AmlOpcode)
    {
    case AML_RAW_DATA_BYTE:

        Op->Asl.AmlOpcodeLength = 0;
        Op->Asl.AmlLength = 1;
        return;

    case AML_RAW_DATA_WORD:

        Op->Asl.AmlOpcodeLength = 0;
        Op->Asl.AmlLength = 2;
        return;

    case AML_RAW_DATA_DWORD:

        Op->Asl.AmlOpcodeLength = 0;
        Op->Asl.AmlLength = 4;
        return;

    case AML_RAW_DATA_QWORD:

        Op->Asl.AmlOpcodeLength = 0;
        Op->Asl.AmlLength = 8;
        return;

    case AML_RAW_DATA_BUFFER:

        /* Aml length is/was set by creator */

        Op->Asl.AmlOpcodeLength = 0;
        return;

    case AML_RAW_DATA_CHAIN:

        /* Aml length is/was set by creator */

        Op->Asl.AmlOpcodeLength = 0;
        return;

    default:
        break;
    }

    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_DEFINITIONBLOCK:

        Gbl_TableLength = sizeof (ACPI_TABLE_HEADER) +
                            Op->Asl.AmlSubtreeLength;
        break;

    case PARSEOP_NAMESEG:

        Op->Asl.AmlOpcodeLength = 0;
        Op->Asl.AmlLength = 4;
        Op->Asl.ExternalName = Op->Asl.Value.String;
        break;

    case PARSEOP_NAMESTRING:
    case PARSEOP_METHODCALL:

        if (Op->Asl.CompileFlags & NODE_NAME_INTERNALIZED)
        {
            break;
        }

        Op->Asl.AmlOpcodeLength = 0;
        Status = UtInternalizeName (Op->Asl.Value.String, &Buffer);
        if (ACPI_FAILURE (Status))
        {
            DbgPrint (ASL_DEBUG_OUTPUT,
                "Failure from internalize name %X\n", Status);
            break;
        }

        Op->Asl.ExternalName = Op->Asl.Value.String;
        Op->Asl.Value.String = Buffer;
        Op->Asl.CompileFlags |= NODE_NAME_INTERNALIZED;

        Op->Asl.AmlLength = strlen (Buffer);

        /*
         * Check for single backslash reference to root,
         * make it a null terminated string in the AML
         */
        if (Op->Asl.AmlLength == 1)
        {
            Op->Asl.AmlLength = 2;
        }
        break;

    case PARSEOP_STRING_LITERAL:

        Op->Asl.AmlOpcodeLength = 1;

        /* Get null terminator */

        Op->Asl.AmlLength = strlen (Op->Asl.Value.String) + 1;
        break;

    case PARSEOP_PACKAGE_LENGTH:

        Op->Asl.AmlOpcodeLength = 0;
        Op->Asl.AmlPkgLenBytes = CgGetPackageLenByteCount (Op,
                                    (UINT32) Op->Asl.Value.Integer);
        break;

    case PARSEOP_RAW_DATA:

        Op->Asl.AmlOpcodeLength = 0;
        break;

    case PARSEOP_DEFAULT_ARG:
    case PARSEOP_EXTERNAL:
    case PARSEOP_INCLUDE:
    case PARSEOP_INCLUDE_END:

        /* Ignore the "default arg" nodes, they are extraneous at this point */

        break;

    default:

        CgGenerateAmlOpcodeLength (Op);
        break;
    }
}


#ifdef ACPI_OBSOLETE_FUNCTIONS
/*******************************************************************************
 *
 * FUNCTION:    LnAdjustLengthToRoot
 *
 * PARAMETERS:  Op      - Node whose Length was changed
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Change the Subtree length of the given node, and bubble the
 *              change all the way up to the root node.  This allows for
 *              last second changes to a package length (for example, if the
 *              package length encoding gets shorter or longer.)
 *
 ******************************************************************************/

void
LnAdjustLengthToRoot (
    ACPI_PARSE_OBJECT       *SubtreeOp,
    UINT32                  LengthDelta)
{
    ACPI_PARSE_OBJECT       *Op;


    /* Adjust all subtree lengths up to the root */

    Op = SubtreeOp->Asl.Parent;
    while (Op)
    {
        Op->Asl.AmlSubtreeLength -= LengthDelta;
        Op = Op->Asl.Parent;
    }

    /* Adjust the global table length */

    Gbl_TableLength -= LengthDelta;
}
#endif


