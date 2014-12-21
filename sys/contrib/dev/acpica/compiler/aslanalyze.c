/******************************************************************************
 *
 * Module Name: aslanalyze.c - Support functions for parse tree walks
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

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include "aslcompiler.y.h"
#include <string.h>


#define _COMPONENT          ACPI_COMPILER
        ACPI_MODULE_NAME    ("aslanalyze")


/*******************************************************************************
 *
 * FUNCTION:    AnIsInternalMethod
 *
 * PARAMETERS:  Op                  - Current op
 *
 * RETURN:      Boolean
 *
 * DESCRIPTION: Check for an internal control method.
 *
 ******************************************************************************/

BOOLEAN
AnIsInternalMethod (
    ACPI_PARSE_OBJECT       *Op)
{

    if ((!ACPI_STRCMP (Op->Asl.ExternalName, "\\_OSI")) ||
        (!ACPI_STRCMP (Op->Asl.ExternalName, "_OSI")))
    {
        return (TRUE);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AnGetInternalMethodReturnType
 *
 * PARAMETERS:  Op                  - Current op
 *
 * RETURN:      Btype
 *
 * DESCRIPTION: Get the return type of an internal method
 *
 ******************************************************************************/

UINT32
AnGetInternalMethodReturnType (
    ACPI_PARSE_OBJECT       *Op)
{

    if ((!ACPI_STRCMP (Op->Asl.ExternalName, "\\_OSI")) ||
        (!ACPI_STRCMP (Op->Asl.ExternalName, "_OSI")))
    {
        return (ACPI_BTYPE_STRING);
    }

    return (0);
}


/*******************************************************************************
 *
 * FUNCTION:    AnCheckId
 *
 * PARAMETERS:  Op                  - Current parse op
 *              Type                - HID or CID
 *
 * RETURN:      None
 *
 * DESCRIPTION: Perform various checks on _HID and _CID strings. Only limited
 *              checks can be performed on _CID strings.
 *
 ******************************************************************************/

void
AnCheckId (
    ACPI_PARSE_OBJECT       *Op,
    ACPI_NAME               Type)
{
    UINT32                  i;
    ACPI_SIZE               Length;


    /* Only care about string versions of _HID/_CID (integers are legal) */

    if (Op->Asl.ParseOpcode != PARSEOP_STRING_LITERAL)
    {
        return;
    }

    /* For both _HID and _CID, the string must be non-null */

    Length = strlen (Op->Asl.Value.String);
    if (!Length)
    {
        AslError (ASL_ERROR, ASL_MSG_NULL_STRING,
            Op, NULL);
        return;
    }

    /*
     * One of the things we want to catch here is the use of a leading
     * asterisk in the string -- an odd construct that certain platform
     * manufacturers are fond of. Technically, a leading asterisk is OK
     * for _CID, but a valid use of this has not been seen.
     */
    if (*Op->Asl.Value.String == '*')
    {
        AslError (ASL_ERROR, ASL_MSG_LEADING_ASTERISK,
            Op, Op->Asl.Value.String);
        return;
    }

    /* _CID strings are bus-specific, no more checks can be performed */

    if (Type == ASL_TYPE_CID)
    {
        return;
    }

    /* For _HID, all characters must be alphanumeric */

    for (i = 0; Op->Asl.Value.String[i]; i++)
    {
        if (!isalnum ((int) Op->Asl.Value.String[i]))
        {
            AslError (ASL_ERROR, ASL_MSG_ALPHANUMERIC_STRING,
                Op, Op->Asl.Value.String);
            return;
        }
    }

    /*
     * _HID String must be one of these forms:
     *
     * "AAA####"    A is an uppercase letter and # is a hex digit
     * "ACPI####"   # is a hex digit
     * "NNNN####"   N is an uppercase letter or decimal digit (0-9)
     *              # is a hex digit (ACPI 5.0)
     */
    if ((Length < 7) || (Length > 8))
    {
        AslError (ASL_ERROR, ASL_MSG_HID_LENGTH,
            Op, Op->Asl.Value.String);
        return;
    }

    /* _HID Length is valid (7 or 8), now check the prefix (first 3 or 4 chars) */

    if (Length == 7)
    {
        /* AAA####: Ensure the alphabetic prefix is all uppercase */

        for (i = 0; i < 3; i++)
        {
            if (!isupper ((int) Op->Asl.Value.String[i]))
            {
                AslError (ASL_ERROR, ASL_MSG_UPPER_CASE,
                    Op, &Op->Asl.Value.String[i]);
                return;
            }
        }
    }
    else /* Length == 8 */
    {
        /*
         * ACPI#### or NNNN####:
         * Ensure the prefix contains only uppercase alpha or decimal digits
         */
        for (i = 0; i < 4; i++)
        {
            if (!isupper ((int) Op->Asl.Value.String[i]) &&
                !isdigit ((int) Op->Asl.Value.String[i]))
            {
                AslError (ASL_ERROR, ASL_MSG_HID_PREFIX,
                    Op, &Op->Asl.Value.String[i]);
                return;
            }
        }
    }

    /* Remaining characters (suffix) must be hex digits */

    for (; i < Length; i++)
    {
        if (!isxdigit ((int) Op->Asl.Value.String[i]))
        {
         AslError (ASL_ERROR, ASL_MSG_HID_SUFFIX,
            Op, &Op->Asl.Value.String[i]);
            break;
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AnLastStatementIsReturn
 *
 * PARAMETERS:  Op                  - A method parse node
 *
 * RETURN:      TRUE if last statement is an ASL RETURN. False otherwise
 *
 * DESCRIPTION: Walk down the list of top level statements within a method
 *              to find the last one. Check if that last statement is in
 *              fact a RETURN statement.
 *
 ******************************************************************************/

BOOLEAN
AnLastStatementIsReturn (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Next;


    /* Check if last statement is a return */

    Next = ASL_GET_CHILD_NODE (Op);
    while (Next)
    {
        if ((!Next->Asl.Next) &&
            (Next->Asl.ParseOpcode == PARSEOP_RETURN))
        {
            return (TRUE);
        }

        Next = ASL_GET_PEER_NODE (Next);
    }

    return (FALSE);
}


/*******************************************************************************
 *
 * FUNCTION:    AnCheckMethodReturnValue
 *
 * PARAMETERS:  Op                  - Parent
 *              OpInfo              - Parent info
 *              ArgOp               - Method invocation op
 *              RequiredBtypes      - What caller requires
 *              ThisNodeBtype       - What this node returns (if anything)
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check a method invocation for 1) A return value and if it does
 *              in fact return a value, 2) check the type of the return value.
 *
 ******************************************************************************/

void
AnCheckMethodReturnValue (
    ACPI_PARSE_OBJECT       *Op,
    const ACPI_OPCODE_INFO  *OpInfo,
    ACPI_PARSE_OBJECT       *ArgOp,
    UINT32                  RequiredBtypes,
    UINT32                  ThisNodeBtype)
{
    ACPI_PARSE_OBJECT       *OwningOp;
    ACPI_NAMESPACE_NODE     *Node;


    Node = ArgOp->Asl.Node;


    /* Examine the parent op of this method */

    OwningOp = Node->Op;
    if (OwningOp->Asl.CompileFlags & NODE_METHOD_NO_RETVAL)
    {
        /* Method NEVER returns a value */

        AslError (ASL_ERROR, ASL_MSG_NO_RETVAL, Op, Op->Asl.ExternalName);
    }
    else if (OwningOp->Asl.CompileFlags & NODE_METHOD_SOME_NO_RETVAL)
    {
        /* Method SOMETIMES returns a value, SOMETIMES not */

        AslError (ASL_WARNING, ASL_MSG_SOME_NO_RETVAL, Op, Op->Asl.ExternalName);
    }
    else if (!(ThisNodeBtype & RequiredBtypes))
    {
        /* Method returns a value, but the type is wrong */

        AnFormatBtype (StringBuffer, ThisNodeBtype);
        AnFormatBtype (StringBuffer2, RequiredBtypes);

        /*
         * The case where the method does not return any value at all
         * was already handled in the namespace cross reference
         * -- Only issue an error if the method in fact returns a value,
         * but it is of the wrong type
         */
        if (ThisNodeBtype != 0)
        {
            sprintf (MsgBuffer,
                "Method returns [%s], %s operator requires [%s]",
                StringBuffer, OpInfo->Name, StringBuffer2);

            AslError (ASL_ERROR, ASL_MSG_INVALID_TYPE, ArgOp, MsgBuffer);
        }
    }
}


/*******************************************************************************
 *
 * FUNCTION:    AnIsResultUsed
 *
 * PARAMETERS:  Op                  - Parent op for the operator
 *
 * RETURN:      TRUE if result from this operation is actually consumed
 *
 * DESCRIPTION: Determine if the function result value from an operator is
 *              used.
 *
 ******************************************************************************/

BOOLEAN
AnIsResultUsed (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Parent;


    switch (Op->Asl.ParseOpcode)
    {
    case PARSEOP_INCREMENT:
    case PARSEOP_DECREMENT:

        /* These are standalone operators, no return value */

        return (TRUE);

    default:

        break;
    }

    /* Examine parent to determine if the return value is used */

    Parent = Op->Asl.Parent;
    switch (Parent->Asl.ParseOpcode)
    {
    /* If/While - check if the operator is the predicate */

    case PARSEOP_IF:
    case PARSEOP_WHILE:

        /* First child is the predicate */

        if (Parent->Asl.Child == Op)
        {
            return (TRUE);
        }
        return (FALSE);

    /* Not used if one of these is the parent */

    case PARSEOP_METHOD:
    case PARSEOP_DEFINITIONBLOCK:
    case PARSEOP_ELSE:

        return (FALSE);

    default:

        /* Any other type of parent means that the result is used */

        return (TRUE);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckForGpeNameConflict
 *
 * PARAMETERS:  Op                  - Current parse op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Check for a conflict between GPE names within this scope.
 *              Conflict means two GPE names with the same GPE number, but
 *              different types -- such as _L1C and _E1C.
 *
 ******************************************************************************/

void
ApCheckForGpeNameConflict (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *NextOp;
    UINT32                  GpeNumber;
    char                    Name[ACPI_NAME_SIZE + 1];
    char                    Target[ACPI_NAME_SIZE];


    /* Need a null-terminated string version of NameSeg */

    ACPI_MOVE_32_TO_32 (Name, &Op->Asl.NameSeg);
    Name[ACPI_NAME_SIZE] = 0;

    /*
     * For a GPE method:
     * 1st char must be underscore
     * 2nd char must be L or E
     * 3rd/4th chars must be a hex number
     */
    if ((Name[0] != '_') ||
       ((Name[1] != 'L') && (Name[1] != 'E')))
    {
        return;
    }

    /* Verify 3rd/4th chars are a valid hex value */

    GpeNumber = ACPI_STRTOUL (&Name[2], NULL, 16);
    if (GpeNumber == ACPI_UINT32_MAX)
    {
        return;
    }

    /*
     * We are now sure we have an _Lxx or _Exx.
     * Create the target name that would cause collision (Flip E/L)
     */
    ACPI_MOVE_32_TO_32 (Target, Name);

    /* Inject opposite letter ("L" versus "E") */

    if (Name[1] == 'L')
    {
        Target[1] = 'E';
    }
    else /* Name[1] == 'E' */
    {
        Target[1] = 'L';
    }

    /* Search all peers (objects within this scope) for target match */

    NextOp = Op->Asl.Next;
    while (NextOp)
    {
        /*
         * We mostly care about methods, but check Name() constructs also,
         * even though they will get another error for not being a method.
         * All GPE names must be defined as control methods.
         */
        if ((NextOp->Asl.ParseOpcode == PARSEOP_METHOD) ||
            (NextOp->Asl.ParseOpcode == PARSEOP_NAME))
        {
            if (ACPI_COMPARE_NAME (Target, NextOp->Asl.NameSeg))
            {
                /* Found both _Exy and _Lxy in the same scope, error */

                AslError (ASL_ERROR, ASL_MSG_GPE_NAME_CONFLICT, NextOp,
                    Name);
                return;
            }
        }

        NextOp = NextOp->Asl.Next;
    }

    /* OK, no conflict found */

    return;
}


/*******************************************************************************
 *
 * FUNCTION:    ApCheckRegMethod
 *
 * PARAMETERS:  Op                  - Current parse op
 *
 * RETURN:      None
 *
 * DESCRIPTION: Ensure that a _REG method has a corresponding Operation
 *              Region declaration within the same scope. Note: _REG is defined
 *              to have two arguments and must therefore be defined as a
 *              control method.
 *
 ******************************************************************************/

void
ApCheckRegMethod (
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Next;
    ACPI_PARSE_OBJECT       *Parent;


    /* We are only interested in _REG methods */

    if (!ACPI_COMPARE_NAME (METHOD_NAME__REG, &Op->Asl.NameSeg))
    {
        return;
    }

    /* Get the start of the current scope */

    Parent = Op->Asl.Parent;
    Next = Parent->Asl.Child;

    /* Search entire scope for an operation region declaration */

    while (Next)
    {
        if (Next->Asl.ParseOpcode == PARSEOP_OPERATIONREGION)
        {
            return; /* Found region, OK */
        }

        Next = Next->Asl.Next;
    }

    /* No region found, issue warning */

    AslError (ASL_WARNING, ASL_MSG_NO_REGION, Op, NULL);
}


/*******************************************************************************
 *
 * FUNCTION:    ApFindNameInScope
 *
 * PARAMETERS:  Name                - Name to search for
 *              Op                  - Current parse op
 *
 * RETURN:      TRUE if name found in the same scope as Op.
 *
 * DESCRIPTION: Determine if a name appears in the same scope as Op, as either
 *              a Method() or a Name().
 *
 ******************************************************************************/

BOOLEAN
ApFindNameInScope (
    char                    *Name,
    ACPI_PARSE_OBJECT       *Op)
{
    ACPI_PARSE_OBJECT       *Next;
    ACPI_PARSE_OBJECT       *Parent;


    /* Get the start of the current scope */

    Parent = Op->Asl.Parent;
    Next = Parent->Asl.Child;

    /* Search entire scope for a match to the name */

    while (Next)
    {
        if ((Next->Asl.ParseOpcode == PARSEOP_METHOD) ||
            (Next->Asl.ParseOpcode == PARSEOP_NAME))
        {
            if (ACPI_COMPARE_NAME (Name, Next->Asl.NameSeg))
            {
                return (TRUE);
            }
        }

        Next = Next->Asl.Next;
    }

    return (FALSE);
}
