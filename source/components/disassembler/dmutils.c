/*******************************************************************************
 *
 * Module Name: dmutils - AML disassembler utilities
 *
 ******************************************************************************/

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

#include "acpi.h"
#include "accommon.h"
#include "amlcode.h"
#include "acdisasm.h"

#ifdef ACPI_ASL_COMPILER
#include <acnamesp.h>
#endif


#define _COMPONENT          ACPI_CA_DEBUGGER
        ACPI_MODULE_NAME    ("dmutils")


/* Data used in keeping track of fields */
#if 0
const char                      *AcpiGbl_FENames[] =
{
    "skip",
    "?access?"
};              /* FE = Field Element */
#endif

/* Operators for Match() */

const char                      *AcpiGbl_MatchOps[] =
{
    "MTR",
    "MEQ",
    "MLE",
    "MLT",
    "MGE",
    "MGT"
};

/* Access type decoding */

const char                      *AcpiGbl_AccessTypes[] =
{
    "AnyAcc",
    "ByteAcc",
    "WordAcc",
    "DWordAcc",
    "QWordAcc",
    "BufferAcc",
    "InvalidAccType",
    "InvalidAccType"
};

/* Lock rule decoding */

const char                      *AcpiGbl_LockRule[] =
{
    "NoLock",
    "Lock"
};

/* Update rule decoding */

const char                      *AcpiGbl_UpdateRules[] =
{
    "Preserve",
    "WriteAsOnes",
    "WriteAsZeros",
    "InvalidUpdateRule"
};

/* Strings used to decode resource descriptors */

const char                      *AcpiGbl_WordDecode[] =
{
    "Memory",
    "IO",
    "BusNumber",
    "UnknownResourceType"
};

const char                      *AcpiGbl_IrqDecode[] =
{
    "IRQNoFlags",
    "IRQ"
};


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmDecodeAttribute
 *
 * PARAMETERS:  Attribute       - Attribute field of AccessAs keyword
 *
 * RETURN:      None
 *
 * DESCRIPTION: Decode the AccessAs attribute byte. (Mostly SMBus and
 *              GenericSerialBus stuff.)
 *
 ******************************************************************************/

void
AcpiDmDecodeAttribute (
    UINT8                   Attribute)
{

    switch (Attribute)
    {
    case AML_FIELD_ATTRIB_QUICK:

        AcpiOsPrintf ("AttribQuick");
        break;

    case AML_FIELD_ATTRIB_SEND_RCV:

        AcpiOsPrintf ("AttribSendReceive");
        break;

    case AML_FIELD_ATTRIB_BYTE:

        AcpiOsPrintf ("AttribByte");
        break;

    case AML_FIELD_ATTRIB_WORD:

        AcpiOsPrintf ("AttribWord");
        break;

    case AML_FIELD_ATTRIB_BLOCK:

        AcpiOsPrintf ("AttribBlock");
        break;

    case AML_FIELD_ATTRIB_MULTIBYTE:

        AcpiOsPrintf ("AttribBytes");
        break;

    case AML_FIELD_ATTRIB_WORD_CALL:

        AcpiOsPrintf ("AttribProcessCall");
        break;

    case AML_FIELD_ATTRIB_BLOCK_CALL:

        AcpiOsPrintf ("AttribBlockProcessCall");
        break;

    case AML_FIELD_ATTRIB_RAW_BYTES:

        AcpiOsPrintf ("AttribRawBytes");
        break;

    case AML_FIELD_ATTRIB_RAW_PROCESS:

        AcpiOsPrintf ("AttribRawProcessBytes");
        break;

    default:

        /* A ByteConst is allowed by the grammar */

        AcpiOsPrintf ("0x%2.2X", Attribute);
        break;
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmIndent
 *
 * PARAMETERS:  Level               - Current source code indentation level
 *
 * RETURN:      None
 *
 * DESCRIPTION: Indent 4 spaces per indentation level.
 *
 ******************************************************************************/

void
AcpiDmIndent (
    UINT32                  Level)
{

    if (!Level)
    {
        return;
    }

    AcpiOsPrintf ("%*.s", (Level * 4), " ");
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCommaIfListMember
 *
 * PARAMETERS:  Op              - Current operator/operand
 *
 * RETURN:      TRUE if a comma was inserted
 *
 * DESCRIPTION: Insert a comma if this Op is a member of an argument list.
 *
 ******************************************************************************/

BOOLEAN
AcpiDmCommaIfListMember (
    ACPI_PARSE_OBJECT       *Op)
{

    if (!Op->Common.Next)
    {
        return (FALSE);
    }

    if (AcpiDmListType (Op->Common.Parent) & BLOCK_COMMA_LIST)
    {
        /* Exit if Target has been marked IGNORE */

        if (Op->Common.Next->Common.DisasmFlags & ACPI_PARSEOP_IGNORE)
        {
            return (FALSE);
        }

        /* Check for a NULL target operand */

        if ((Op->Common.Next->Common.AmlOpcode == AML_INT_NAMEPATH_OP) &&
            (!Op->Common.Next->Common.Value.String))
        {
            /*
             * To handle the Divide() case where there are two optional
             * targets, look ahead one more op. If null, this null target
             * is the one and only target -- no comma needed. Otherwise,
             * we need a comma to prepare for the next target.
             */
            if (!Op->Common.Next->Common.Next)
            {
                return (FALSE);
            }
        }

        if ((Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST) &&
            (!(Op->Common.Next->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST)))
        {
            return (FALSE);
        }

        /* Emit comma only if this is not a C-style operator */

        if (!Op->Common.OperatorSymbol)
        {
            AcpiOsPrintf (", ");
        }

        return (TRUE);
    }

    else if ((Op->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST) &&
             (Op->Common.Next->Common.DisasmFlags & ACPI_PARSEOP_PARAMETER_LIST))
    {
        AcpiOsPrintf (", ");
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AcpiDmCommaIfFieldMember
 *
 * PARAMETERS:  Op              - Current operator/operand
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert a comma if this Op is a member of a Field argument list.
 *
 ******************************************************************************/

void
AcpiDmCommaIfFieldMember (
    ACPI_PARSE_OBJECT       *Op)
{

    if (Op->Common.Next)
    {
        AcpiOsPrintf (", ");
    }
}
