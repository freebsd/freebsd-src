/******************************************************************************
 *
 * Module Name: dtexpress.c - Support for integer expressions and labels
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

#define __DTEXPRESS_C__

#include "aslcompiler.h"
#include "dtcompiler.h"

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtexpress")


/* Local prototypes */

static UINT64
DtResolveInteger (
    DT_FIELD                *Field,
    char                    *IntegerString);

static void
DtInsertLabelField (
    DT_FIELD                *Field);

static DT_FIELD *
DtLookupLabel (
    char                    *Name);


/******************************************************************************
 *
 * FUNCTION:    DtResolveIntegerExpression
 *
 * PARAMETERS:  Field               - Field object with Integer expression
 *
 * RETURN:      A 64-bit integer value
 *
 * DESCRIPTION: Resolve an integer expression to a single value. Supports
 *              both integer constants and labels. Supported operators are:
 *              +,-,*,/,%,|,&,^
 *
 *****************************************************************************/

UINT64
DtResolveIntegerExpression (
    DT_FIELD                *Field)
{
    char                    *IntegerString;
    char                    *Operator;
    UINT64                  Value;
    UINT64                  Value2;


    DbgPrint (ASL_DEBUG_OUTPUT, "Full Integer expression: %s\n",
        Field->Value);

    strcpy (MsgBuffer, Field->Value); /* Must take a copy for strtok() */

    /* Obtain and resolve the first operand */

    IntegerString = strtok (MsgBuffer, " ");
    if (!IntegerString)
    {
        DtError (ASL_ERROR, ASL_MSG_INVALID_EXPRESSION, Field, Field->Value);
        return (0);
    }

    Value = DtResolveInteger (Field, IntegerString);
    DbgPrint (ASL_DEBUG_OUTPUT, "Integer resolved to V1: %8.8X%8.8X\n",
        ACPI_FORMAT_UINT64 (Value));

    /*
     * Consume the entire expression string. For the rest of the
     * expression string, values are of the form:
     * <operator> <integer>
     */
    while (1)
    {
        Operator = strtok (NULL, " ");
        if (!Operator)
        {
            /* Normal exit */

            DbgPrint (ASL_DEBUG_OUTPUT, "Expression Resolved to: %8.8X%8.8X\n",
                ACPI_FORMAT_UINT64 (Value));

            return (Value);
        }

        IntegerString = strtok (NULL, " ");
        if (!IntegerString ||
            (strlen (Operator) > 1))
        {
            /* No corresponding operand for operator or invalid operator */

            DtError (ASL_ERROR, ASL_MSG_INVALID_EXPRESSION, Field, Field->Value);
            return (0);
        }

        Value2 = DtResolveInteger (Field, IntegerString);
        DbgPrint (ASL_DEBUG_OUTPUT, "Integer resolved to V2: %8.8X%8.8X\n",
            ACPI_FORMAT_UINT64 (Value2));

        /* Perform the requested operation */

        switch (*Operator)
        {
        case '-':
            Value -= Value2;
            break;

        case '+':
            Value += Value2;
            break;

        case '*':
            Value *= Value2;
            break;

        case '|':
            Value |= Value2;
            break;

        case '&':
            Value &= Value2;
            break;

        case '^':
            Value ^= Value2;
            break;

        case '/':
            if (!Value2)
            {
                DtError (ASL_ERROR, ASL_MSG_DIVIDE_BY_ZERO, Field, Field->Value);
                return (0);
            }
            Value /= Value2;
            break;

        case '%':
            if (!Value2)
            {
                DtError (ASL_ERROR, ASL_MSG_DIVIDE_BY_ZERO, Field, Field->Value);
                return (0);
            }
            Value %= Value2;
            break;

        default:

            /* Unknown operator */

            DtFatal (ASL_MSG_INVALID_EXPRESSION, Field, Field->Value);
            break;
        }
    }

    return (Value);
}


/******************************************************************************
 *
 * FUNCTION:    DtResolveInteger
 *
 * PARAMETERS:  Field               - Field object with string to be resolved
 *              IntegerString       - Integer to be resolved
 *
 * RETURN:      A 64-bit integer value
 *
 * DESCRIPTION: Resolve a single integer string to a value. Supports both
 *              integer constants and labels.
 *
 * NOTE:        References to labels must begin with a dollar sign ($)
 *
 *****************************************************************************/

static UINT64
DtResolveInteger (
    DT_FIELD                *Field,
    char                    *IntegerString)
{
    DT_FIELD                *LabelField;
    UINT64                  Value = 0;
    char                    *Message = NULL;
    ACPI_STATUS             Status;


    DbgPrint (ASL_DEBUG_OUTPUT, "Resolve Integer: %s\n", IntegerString);

    /* Resolve a label reference to an integer (table offset) */

    if (*IntegerString == '$')
    {
        LabelField = DtLookupLabel (IntegerString);
        if (!LabelField)
        {
            DtError (ASL_ERROR, ASL_MSG_UNKNOWN_LABEL, Field, IntegerString);
            return (0);
        }

        /* All we need from the label is the offset in the table */

        Value = LabelField->TableOffset;
        return (Value);
    }

    /* Convert string to an actual integer */

    Status = DtStrtoul64 (IntegerString, &Value);
    if (ACPI_FAILURE (Status))
    {
        if (Status == AE_LIMIT)
        {
            Message = "Constant larger than 64 bits";
        }
        else if (Status == AE_BAD_CHARACTER)
        {
            Message = "Invalid character in constant";
        }

        DtError (ASL_ERROR, ASL_MSG_INVALID_HEX_INTEGER, Field, Message);
    }

    return (Value);
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
