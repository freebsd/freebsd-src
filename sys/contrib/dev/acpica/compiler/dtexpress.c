/******************************************************************************
 *
 * Module Name: dtexpress.c - Support for integer expressions and labels
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2013, Intel Corp.
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

#define __DTEXPRESS_C__

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>
#include "dtparser.y.h"

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtexpress")


/* Local prototypes */

static void
DtInsertLabelField (
    DT_FIELD                *Field);

static DT_FIELD *
DtLookupLabel (
    char                    *Name);

/* Global used for errors during parse and related functions */

DT_FIELD                *Gbl_CurrentField;


/******************************************************************************
 *
 * FUNCTION:    DtResolveIntegerExpression
 *
 * PARAMETERS:  Field               - Field object with Integer expression
 *              ReturnValue         - Where the integer is returned
 *
 * RETURN:      Status, and the resolved 64-bit integer value
 *
 * DESCRIPTION: Resolve an integer expression to a single value. Supports
 *              both integer constants and labels.
 *
 *****************************************************************************/

ACPI_STATUS
DtResolveIntegerExpression (
    DT_FIELD                *Field,
    UINT64                  *ReturnValue)
{
    UINT64                  Result;


    DbgPrint (ASL_DEBUG_OUTPUT, "Full Integer expression: %s\n",
        Field->Value);

    Gbl_CurrentField = Field;

    Result = DtEvaluateExpression (Field->Value);
    *ReturnValue = Result;
    return (AE_OK);
}


/******************************************************************************
 *
 * FUNCTION:    DtDoOperator
 *
 * PARAMETERS:  LeftValue           - First 64-bit operand
 *              Operator            - Parse token for the operator (EXPOP_*)
 *              RightValue          - Second 64-bit operand
 *
 * RETURN:      64-bit result of the requested operation
 *
 * DESCRIPTION: Perform the various 64-bit integer math functions
 *
 *****************************************************************************/

UINT64
DtDoOperator (
    UINT64                  LeftValue,
    UINT32                  Operator,
    UINT64                  RightValue)
{
    UINT64                  Result;


    /* Perform the requested operation */

    switch (Operator)
    {
    case EXPOP_ONES_COMPLIMENT:

        Result = ~RightValue;
        break;

    case EXPOP_LOGICAL_NOT:

        Result = !RightValue;
        break;

    case EXPOP_MULTIPLY:

        Result = LeftValue * RightValue;
        break;

    case EXPOP_DIVIDE:

        if (!RightValue)
        {
            DtError (ASL_ERROR, ASL_MSG_DIVIDE_BY_ZERO,
                Gbl_CurrentField, NULL);
            return (0);
        }
        Result = LeftValue / RightValue;
        break;

    case EXPOP_MODULO:

        if (!RightValue)
        {
            DtError (ASL_ERROR, ASL_MSG_DIVIDE_BY_ZERO,
                Gbl_CurrentField, NULL);
            return (0);
        }
        Result = LeftValue % RightValue;
        break;

    case EXPOP_ADD:
        Result = LeftValue + RightValue;
        break;

    case EXPOP_SUBTRACT:

        Result = LeftValue - RightValue;
        break;

    case EXPOP_SHIFT_RIGHT:

        Result = LeftValue >> RightValue;
        break;

    case EXPOP_SHIFT_LEFT:

        Result = LeftValue << RightValue;
        break;

    case EXPOP_LESS:

        Result = LeftValue < RightValue;
        break;

    case EXPOP_GREATER:

        Result = LeftValue > RightValue;
        break;

    case EXPOP_LESS_EQUAL:

        Result = LeftValue <= RightValue;
        break;

    case EXPOP_GREATER_EQUAL:

        Result = LeftValue >= RightValue;
        break;

    case EXPOP_EQUAL:

        Result = LeftValue == RightValue;
        break;

    case EXPOP_NOT_EQUAL:

        Result = LeftValue != RightValue;
        break;

    case EXPOP_AND:

        Result = LeftValue & RightValue;
        break;

    case EXPOP_XOR:

        Result = LeftValue ^ RightValue;
        break;

    case EXPOP_OR:

        Result = LeftValue | RightValue;
        break;

    case EXPOP_LOGICAL_AND:

        Result = LeftValue && RightValue;
        break;

    case EXPOP_LOGICAL_OR:

        Result = LeftValue || RightValue;
        break;

   default:

        /* Unknown operator */

        DtFatal (ASL_MSG_INVALID_EXPRESSION,
            Gbl_CurrentField, NULL);
        return (0);
    }

    DbgPrint (ASL_DEBUG_OUTPUT,
        "IntegerEval: (%8.8X%8.8X %s %8.8X%8.8X) = %8.8X%8.8X\n",
        ACPI_FORMAT_UINT64 (LeftValue),
        DtGetOpName (Operator),
        ACPI_FORMAT_UINT64 (RightValue),
        ACPI_FORMAT_UINT64 (Result));

    return (Result);
}


/******************************************************************************
 *
 * FUNCTION:    DtResolveLabel
 *
 * PARAMETERS:  LabelString         - Contains the label
 *
 * RETURN:      Table offset associated with the label
 *
 * DESCRIPTION: Lookup a lable and return its value.
 *
 *****************************************************************************/

UINT64
DtResolveLabel (
    char                    *LabelString)
{
    DT_FIELD                *LabelField;


    DbgPrint (ASL_DEBUG_OUTPUT, "Resolve Label: %s\n", LabelString);

    /* Resolve a label reference to an integer (table offset) */

    if (*LabelString != '$')
    {
        return (0);
    }

    LabelField = DtLookupLabel (LabelString);
    if (!LabelField)
    {
        DtError (ASL_ERROR, ASL_MSG_UNKNOWN_LABEL,
            Gbl_CurrentField, LabelString);
        return (0);
    }

    /* All we need from the label is the offset in the table */

    DbgPrint (ASL_DEBUG_OUTPUT, "Resolved Label: 0x%8.8X\n",
        LabelField->TableOffset);

    return (LabelField->TableOffset);
}


/******************************************************************************
 *
 * FUNCTION:    DtDetectAllLabels
 *
 * PARAMETERS:  FieldList           - Field object at start of generic list
 *
 * RETURN:      None
 *
 * DESCRIPTION: Detect all labels in a list of "generic" opcodes (such as
 *              a UEFI table.) and insert them into the global label list.
 *
 *****************************************************************************/

void
DtDetectAllLabels (
    DT_FIELD                *FieldList)
{
    ACPI_DMTABLE_INFO       *Info;
    DT_FIELD                *GenericField;
    UINT32                  TableOffset;


    TableOffset = Gbl_CurrentTableOffset;
    GenericField = FieldList;

    /*
     * Process all "Label:" fields within the parse tree. We need
     * to know the offsets for all labels before we can compile
     * the parse tree in order to handle forward references. Traverse
     * tree and get/set all field lengths of all operators in order to
     * determine the label offsets.
     */
    while (GenericField)
    {
        Info = DtGetGenericTableInfo (GenericField->Name);
        if (Info)
        {
            /* Maintain table offsets */

            GenericField->TableOffset = TableOffset;
            TableOffset += DtGetFieldLength (GenericField, Info);

            /* Insert all labels in the global label list */

            if (Info->Opcode == ACPI_DMT_LABEL)
            {
                DtInsertLabelField (GenericField);
            }
        }

        GenericField = GenericField->Next;
    }
}


/******************************************************************************
 *
 * FUNCTION:    DtInsertLabelField
 *
 * PARAMETERS:  Field               - Field object with Label to be inserted
 *
 * RETURN:      None
 *
 * DESCRIPTION: Insert a label field into the global label list
 *
 *****************************************************************************/

static void
DtInsertLabelField (
    DT_FIELD                *Field)
{

    DbgPrint (ASL_DEBUG_OUTPUT,
        "DtInsertLabelField: Found Label : %s at output table offset %X\n",
        Field->Value, Field->TableOffset);

    Field->NextLabel = Gbl_LabelList;
    Gbl_LabelList = Field;
}


/******************************************************************************
 *
 * FUNCTION:    DtLookupLabel
 *
 * PARAMETERS:  Name                - Label to be resolved
 *
 * RETURN:      Field object associated with the label
 *
 * DESCRIPTION: Lookup a label in the global label list. Used during the
 *              resolution of integer expressions.
 *
 *****************************************************************************/

static DT_FIELD *
DtLookupLabel (
    char                    *Name)
{
    DT_FIELD                *LabelField;


    /* Skip a leading $ */

    if (*Name == '$')
    {
        Name++;
    }

    /* Search global list */

    LabelField = Gbl_LabelList;
    while (LabelField)
    {
        if (!ACPI_STRCMP (Name, LabelField->Value))
        {
            return (LabelField);
        }
        LabelField = LabelField->NextLabel;
    }

    return (NULL);
}
