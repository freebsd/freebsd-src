/******************************************************************************
 *
 * Module Name: dmdeferred - Disassembly of deferred AML opcodes
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2014, Intel Corp.
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

#include <contrib/dev/acpica/include/acpi.h>
#include <contrib/dev/acpica/include/accommon.h>
#include <contrib/dev/acpica/include/acdispat.h>
#include <contrib/dev/acpica/include/amlcode.h>
#include <contrib/dev/acpica/include/acdisasm.h>
#include <contrib/dev/acpica/include/acparser.h>

#define _COMPONENT          ACPI_CA_DISASSEMBLER
        ACPI_MODULE_NAME    ("dmdeferred")


/* Local prototypes */

static ACPI_STATUS
AcpiDmDeferredParse (
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   *Aml,
    UINT32                  AmlLength);


/******************************************************************************
 *
 * FUNCTION:    AcpiDmParseDeferredOps
 *
 * PARAMETERS:  Root                - Root of the parse tree
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse the deferred opcodes (Methods, regions, etc.)
 *
 *****************************************************************************/

ACPI_STATUS
AcpiDmParseDeferredOps (
    ACPI_PARSE_OBJECT       *Root)
{
    const ACPI_OPCODE_INFO  *OpInfo;
    ACPI_PARSE_OBJECT       *Op = Root;
    ACPI_STATUS             Status;


    ACPI_FUNCTION_ENTRY ();


    /* Traverse the entire parse tree */

    while (Op)
    {
        OpInfo = AcpiPsGetOpcodeInfo (Op->Common.AmlOpcode);
        if (!(OpInfo->Flags & AML_DEFER))
        {
            Op = AcpiPsGetDepthNext (Root, Op);
            continue;
        }

        /* Now we know we have a deferred opcode */

        switch (Op->Common.AmlOpcode)
        {
        case AML_METHOD_OP:
        case AML_BUFFER_OP:
        case AML_PACKAGE_OP:
        case AML_VAR_PACKAGE_OP:

            Status = AcpiDmDeferredParse (Op, Op->Named.Data, Op->Named.Length);
            if (ACPI_FAILURE (Status))
            {
                return (Status);
            }
            break;

        /* We don't need to do anything for these deferred opcodes */

        case AML_REGION_OP:
        case AML_DATA_REGION_OP:
        case AML_CREATE_QWORD_FIELD_OP:
        case AML_CREATE_DWORD_FIELD_OP:
        case AML_CREATE_WORD_FIELD_OP:
        case AML_CREATE_BYTE_FIELD_OP:
        case AML_CREATE_BIT_FIELD_OP:
        case AML_CREATE_FIELD_OP:
        case AML_BANK_FIELD_OP:

            break;

        default:

            ACPI_ERROR ((AE_INFO, "Unhandled deferred AML opcode [0x%.4X]",
                 Op->Common.AmlOpcode));
            break;
        }

        Op = AcpiPsGetDepthNext (Root, Op);
    }

    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    AcpiDmDeferredParse
 *
 * PARAMETERS:  Op                  - Root Op of the deferred opcode
 *              Aml                 - Pointer to the raw AML
 *              AmlLength           - Length of the AML
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Parse one deferred opcode
 *              (Methods, operation regions, etc.)
 *
 *****************************************************************************/

static ACPI_STATUS
AcpiDmDeferredParse (
    ACPI_PARSE_OBJECT       *Op,
    UINT8                   *Aml,
    UINT32                  AmlLength)
{
    ACPI_WALK_STATE         *WalkState;
    ACPI_STATUS             Status;
    ACPI_PARSE_OBJECT       *SearchOp;
    ACPI_PARSE_OBJECT       *StartOp;
    UINT32                  BaseAmlOffset;
    ACPI_PARSE_OBJECT       *NewRootOp;
    ACPI_PARSE_OBJECT       *ExtraOp;


    ACPI_FUNCTION_TRACE (DmDeferredParse);


    if (!Aml || !AmlLength)
    {
        return_ACPI_STATUS (AE_OK);
    }

    ACPI_DEBUG_PRINT ((ACPI_DB_INFO, "Parsing deferred opcode %s [%4.4s]\n",
        Op->Common.AmlOpName, (char *) &Op->Named.Name));

    /* Need a new walk state to parse the AML */

    WalkState = AcpiDsCreateWalkState (0, Op, NULL, NULL);
    if (!WalkState)
    {
        return_ACPI_STATUS (AE_NO_MEMORY);
    }

    Status = AcpiDsInitAmlWalk (WalkState, Op, NULL, Aml,
        AmlLength, NULL, ACPI_IMODE_LOAD_PASS1);
    if (ACPI_FAILURE (Status))
    {
        return_ACPI_STATUS (Status);
    }

    /* Parse the AML for this deferred opcode */

    WalkState->ParseFlags &= ~ACPI_PARSE_DELETE_TREE;
    WalkState->ParseFlags |= ACPI_PARSE_DISASSEMBLE;
    Status = AcpiPsParseAml (WalkState);

    /*
     * We need to update all of the AML offsets, since the parser thought
     * that the method began at offset zero. In reality, it began somewhere
     * within the ACPI table, at the BaseAmlOffset. Walk the entire tree that
     * was just created and update the AmlOffset in each Op.
     */
    BaseAmlOffset = (Op->Common.Value.Arg)->Common.AmlOffset + 1;
    StartOp = (Op->Common.Value.Arg)->Common.Next;
    SearchOp = StartOp;

    while (SearchOp)
    {
        SearchOp->Common.AmlOffset += BaseAmlOffset;
        SearchOp = AcpiPsGetDepthNext (StartOp, SearchOp);
    }

    /*
     * For Buffer and Package opcodes, link the newly parsed subtree
     * into the main parse tree
     */
    switch (Op->Common.AmlOpcode)
    {
    case AML_BUFFER_OP:
    case AML_PACKAGE_OP:
    case AML_VAR_PACKAGE_OP:

        switch (Op->Common.AmlOpcode)
        {
        case AML_PACKAGE_OP:

            ExtraOp = Op->Common.Value.Arg;
            NewRootOp = ExtraOp->Common.Next;
            ACPI_FREE (ExtraOp);
            break;

        case AML_VAR_PACKAGE_OP:
        case AML_BUFFER_OP:
        default:

            NewRootOp = Op->Common.Value.Arg;
            break;
        }

        Op->Common.Value.Arg = NewRootOp->Common.Value.Arg;

        /* Must point all parents to the main tree */

        StartOp = Op;
        SearchOp = StartOp;
        while (SearchOp)
        {
            if (SearchOp->Common.Parent == NewRootOp)
            {
                SearchOp->Common.Parent = Op;
            }

            SearchOp = AcpiPsGetDepthNext (StartOp, SearchOp);
        }

        ACPI_FREE (NewRootOp);
        break;

    default:

        break;
    }

    return_ACPI_STATUS (AE_OK);
}
