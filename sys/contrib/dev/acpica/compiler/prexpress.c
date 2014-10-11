/******************************************************************************
 *
 * Module Name: prexpress - Preprocessor #if expression support
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
#include <contrib/dev/acpica/compiler/dtcompiler.h>


#define _COMPONENT          ASL_PREPROCESSOR
        ACPI_MODULE_NAME    ("prexpress")

/* Local prototypes */

static char *
PrExpandMacros (
    char                    *Line);


#ifdef _UNDER_DEVELOPMENT
/******************************************************************************
 *
 * FUNCTION:    PrUnTokenize
 *
 * PARAMETERS:  Buffer              - Token Buffer
 *              Next                - "Next" buffer from GetNextToken
 *
 * RETURN:      None
 *
 * DESCRIPTION: Un-tokenized the current token buffer. The implementation is
 *              to simply set the null inserted by GetNextToken to a blank.
 *              If Next is NULL, there were no tokens found in the Buffer,
 *              so there is nothing to do.
 *
 *****************************************************************************/

static void
PrUnTokenize (
    char                    *Buffer,
    char                    *Next)
{
    UINT32                  Length = strlen (Buffer);


    if (!Next)
    {
        return;
    }
    if (Buffer[Length] != '\n')
    {
        Buffer[strlen(Buffer)] = ' ';
    }
}
#endif


/******************************************************************************
 *
 * FUNCTION:    PrExpandMacros
 *
 * PARAMETERS:  Line                - Pointer into the current line
 *
 * RETURN:      Updated pointer into the current line
 *
 * DESCRIPTION: Expand any macros found in the current line buffer.
 *
 *****************************************************************************/

static char *
PrExpandMacros (
    char                    *Line)
{
    char                    *Token;
    char                    *ReplaceString;
    PR_DEFINE_INFO          *DefineInfo;
    ACPI_SIZE               TokenOffset;
    char                    *Next;
    int                     OffsetAdjust;


    strcpy (Gbl_ExpressionTokenBuffer, Gbl_CurrentLineBuffer);
    Token = PrGetNextToken (Gbl_ExpressionTokenBuffer, PR_EXPR_SEPARATORS, &Next);
    OffsetAdjust = 0;

    while (Token)
    {
        DefineInfo = PrMatchDefine (Token);
        if (DefineInfo)
        {
            if (DefineInfo->Body)
            {
                /* This is a macro. TBD: Is this allowed? */

                DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
                    "Matched Macro: %s->%s\n",
                    Gbl_CurrentLineNumber, DefineInfo->Identifier,
                    DefineInfo->Replacement);

                PrDoMacroInvocation (Gbl_ExpressionTokenBuffer, Token,
                    DefineInfo, &Next);
            }
            else
            {
                ReplaceString = DefineInfo->Replacement;

                /* Replace the name in the original line buffer */

                TokenOffset = Token - Gbl_ExpressionTokenBuffer + OffsetAdjust;
                PrReplaceData (
                    &Gbl_CurrentLineBuffer[TokenOffset], strlen (Token),
                    ReplaceString, strlen (ReplaceString));

                /* Adjust for length difference between old and new name length */

                OffsetAdjust += strlen (ReplaceString) - strlen (Token);

                DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
                    "Matched #define within expression: %s->%s\n",
                    Gbl_CurrentLineNumber, Token,
                    *ReplaceString ? ReplaceString : "(NULL STRING)");
            }
        }

        Token = PrGetNextToken (NULL, PR_EXPR_SEPARATORS, &Next);
    }

    return (Line);
}


/******************************************************************************
 *
 * FUNCTION:    PrIsDefined
 *
 * PARAMETERS:  Identifier          - Name to be resolved
 *
 * RETURN:      64-bit boolean integer value
 *
 * DESCRIPTION: Returns TRUE if the name is defined, FALSE otherwise (0).
 *
 *****************************************************************************/

UINT64
PrIsDefined (
    char                    *Identifier)
{
    UINT64                  Value;
    PR_DEFINE_INFO          *DefineInfo;


    DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
        "**** Is defined?:  %s\n", Gbl_CurrentLineNumber, Identifier);

    Value = 0; /* Default is "Not defined" -- FALSE */

    DefineInfo = PrMatchDefine (Identifier);
    if (DefineInfo)
    {
        Value = ACPI_UINT64_MAX; /* TRUE */
    }

    DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
        "[#if defined %s] resolved to: %8.8X%8.8X\n",
        Gbl_CurrentLineNumber, Identifier, ACPI_FORMAT_UINT64 (Value));

    return (Value);
}


/******************************************************************************
 *
 * FUNCTION:    PrResolveDefine
 *
 * PARAMETERS:  Identifier          - Name to be resolved
 *
 * RETURN:      A 64-bit boolean integer value
 *
 * DESCRIPTION: Returns TRUE if the name is defined, FALSE otherwise (0).
 *
 *****************************************************************************/

UINT64
PrResolveDefine (
    char                    *Identifier)
{
    UINT64                  Value;
    PR_DEFINE_INFO          *DefineInfo;


    DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
        "**** Resolve #define:  %s\n", Gbl_CurrentLineNumber, Identifier);

    Value = 0; /* Default is "Not defined" -- FALSE */

    DefineInfo = PrMatchDefine (Identifier);
    if (DefineInfo)
    {
        Value = ACPI_UINT64_MAX; /* TRUE */
    }

    DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
        "[#if defined %s] resolved to: %8.8X%8.8X\n",
        Gbl_CurrentLineNumber, Identifier, ACPI_FORMAT_UINT64 (Value));

    return (Value);
}


/******************************************************************************
 *
 * FUNCTION:    PrResolveIntegerExpression
 *
 * PARAMETERS:  Line                - Pointer to integer expression
 *              ReturnValue         - Where the resolved 64-bit integer is
 *                                    returned.
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Resolve an integer expression to a single value. Supports
 *              both integer constants and labels.
 *
 *****************************************************************************/

ACPI_STATUS
PrResolveIntegerExpression (
    char                    *Line,
    UINT64                  *ReturnValue)
{
    UINT64                  Result;
    char                    *ExpandedLine;


    DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
        "**** Resolve #if:  %s\n", Gbl_CurrentLineNumber, Line);

    /* Expand all macros within the expression first */

    ExpandedLine = PrExpandMacros (Line);

    /* Now we can evaluate the expression */

    Result = PrEvaluateExpression (ExpandedLine);
    DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
        "**** Expression Resolved to: %8.8X%8.8X\n",
        Gbl_CurrentLineNumber, ACPI_FORMAT_UINT64 (Result));

    *ReturnValue = Result;
    return (AE_OK);

#if 0
InvalidExpression:

    ACPI_FREE (EvalBuffer);
    PrError (ASL_ERROR, ASL_MSG_INVALID_EXPRESSION, 0);
    return (AE_ERROR);


NormalExit:

    DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
        "**** Expression Resolved to: %8.8X%8.8X\n",
        Gbl_CurrentLineNumber, ACPI_FORMAT_UINT64 (Value1));

    *ReturnValue = Value1;
    return (AE_OK);
#endif
}
