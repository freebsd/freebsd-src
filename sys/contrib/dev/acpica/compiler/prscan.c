/******************************************************************************
 *
 * Module Name: prscan - Preprocessor start-up and file scan module
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

#define _DECLARE_PR_GLOBALS

#include <contrib/dev/acpica/compiler/aslcompiler.h>
#include <contrib/dev/acpica/compiler/dtcompiler.h>

/*
 * TBDs:
 *
 * No nested macros, maybe never
 * Implement ASL "Include" as well as "#include" here?
 */
#define _COMPONENT          ASL_PREPROCESSOR
        ACPI_MODULE_NAME    ("prscan")


/* Local prototypes */

static void
PrPreprocessInputFile (
    void);

static void
PrDoDirective (
    char                    *DirectiveToken,
    char                    **Next);

static int
PrMatchDirective (
    char                    *Directive);

static void
PrPushDirective (
    int                     Directive,
    char                    *Argument);

static ACPI_STATUS
PrPopDirective (
    void);

static void
PrDbgPrint (
    char                    *Action,
    char                    *DirectiveName);


/*
 * Supported preprocessor directives
 */
static const PR_DIRECTIVE_INFO      Gbl_DirectiveInfo[] =
{
    {"define",  1},
    {"elif",    0}, /* Converted to #else..#if internally */
    {"else",    0},
    {"endif",   0},
    {"error",   1},
    {"if",      1},
    {"ifdef",   1},
    {"ifndef",  1},
    {"include", 0}, /* Argument is not standard format, so 0 */
    {"line",    1},
    {"pragma",  1},
    {"undef",   1},
    {"warning", 1},
    {NULL,      0}
};

enum Gbl_DirectiveIndexes
{
    PR_DIRECTIVE_DEFINE = 0,
    PR_DIRECTIVE_ELIF,
    PR_DIRECTIVE_ELSE,
    PR_DIRECTIVE_ENDIF,
    PR_DIRECTIVE_ERROR,
    PR_DIRECTIVE_IF,
    PR_DIRECTIVE_IFDEF,
    PR_DIRECTIVE_IFNDEF,
    PR_DIRECTIVE_INCLUDE,
    PR_DIRECTIVE_LINE,
    PR_DIRECTIVE_PRAGMA,
    PR_DIRECTIVE_UNDEF,
    PR_DIRECTIVE_WARNING,
};

#define ASL_DIRECTIVE_NOT_FOUND     -1


/*******************************************************************************
 *
 * FUNCTION:    PrInitializePreprocessor
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Startup initialization for the Preprocessor.
 *
 ******************************************************************************/

void
PrInitializePreprocessor (
    void)
{
    /* Init globals and the list of #defines */

    PrInitializeGlobals ();
    Gbl_DefineList = NULL;
}


/*******************************************************************************
 *
 * FUNCTION:    PrInitializeGlobals
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Initialize globals for the Preprocessor. Used for startuup
 *              initialization and re-initialization between compiles during
 *              a multiple source file compile.
 *
 ******************************************************************************/

void
PrInitializeGlobals (
    void)
{
    /* Init globals */

    Gbl_InputFileList = NULL;
    Gbl_CurrentLineNumber = 0;
    Gbl_PreprocessorLineNumber = 1;
    Gbl_PreprocessorError = FALSE;

    /* These are used to track #if/#else blocks (possibly nested) */

    Gbl_IfDepth = 0;
    Gbl_IgnoringThisCodeBlock = FALSE;
    Gbl_DirectiveStack = NULL;
}


/*******************************************************************************
 *
 * FUNCTION:    PrTerminatePreprocessor
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Termination of the preprocessor. Delete lists. Keep any
 *              defines that were specified on the command line, in order to
 *              support multiple compiles with a single compiler invocation.
 *
 ******************************************************************************/

void
PrTerminatePreprocessor (
    void)
{
    PR_DEFINE_INFO          *DefineInfo;


    /*
     * The persistent defines (created on the command line) are always at the
     * end of the list. We save them.
     */
    while ((Gbl_DefineList) && (!Gbl_DefineList->Persist))
    {
        DefineInfo = Gbl_DefineList;
        Gbl_DefineList = DefineInfo->Next;

        ACPI_FREE (DefineInfo->Replacement);
        ACPI_FREE (DefineInfo->Identifier);
        ACPI_FREE (DefineInfo);
    }
}


/*******************************************************************************
 *
 * FUNCTION:    PrDoPreprocess
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Main entry point for the iASL Preprocessor. Input file must
 *              be already open. Handles multiple input files via the
 *              #include directive.
 *
 ******************************************************************************/

void
PrDoPreprocess (
    void)
{
    BOOLEAN                 MoreInputFiles;


    DbgPrint (ASL_DEBUG_OUTPUT, "Starting preprocessing phase\n\n");


    FlSeekFile (ASL_FILE_INPUT, 0);
    PrDumpPredefinedNames ();

    /* Main preprocessor loop, handles include files */

    do
    {
        PrPreprocessInputFile ();
        MoreInputFiles = PrPopInputFileStack ();

    } while (MoreInputFiles);

    /* Point compiler input to the new preprocessor output file (.i) */

    FlCloseFile (ASL_FILE_INPUT);
    Gbl_Files[ASL_FILE_INPUT].Handle = Gbl_Files[ASL_FILE_PREPROCESSOR].Handle;
    AslCompilerin = Gbl_Files[ASL_FILE_INPUT].Handle;

    /* Reset globals to allow compiler to run */

    FlSeekFile (ASL_FILE_INPUT, 0);
    Gbl_CurrentLineNumber = 1;

    DbgPrint (ASL_DEBUG_OUTPUT, "Preprocessing phase complete \n\n");
}


/*******************************************************************************
 *
 * FUNCTION:    PrPreprocessInputFile
 *
 * PARAMETERS:  None
 *
 * RETURN:      None
 *
 * DESCRIPTION: Preprocess one entire file, line-by-line.
 *
 * Input:  Raw user ASL from ASL_FILE_INPUT
 * Output: Preprocessed file written to ASL_FILE_PREPROCESSOR
 *
 ******************************************************************************/

static void
PrPreprocessInputFile (
    void)
{
    UINT32                  Offset;
    char                    *Token;
    char                    *ReplaceString;
    PR_DEFINE_INFO          *DefineInfo;
    ACPI_SIZE               TokenOffset;
    char                    *Next;
    int                     OffsetAdjust;


    /* Scan line-by-line. Comments and blank lines are skipped by this function */

    while ((Offset = DtGetNextLine (Gbl_Files[ASL_FILE_INPUT].Handle)) != ASL_EOF)
    {
        /* Need a copy of the input line for strok() */

        strcpy (Gbl_MainTokenBuffer, Gbl_CurrentLineBuffer);
        Token = PrGetNextToken (Gbl_MainTokenBuffer, PR_TOKEN_SEPARATORS, &Next);
        OffsetAdjust = 0;

        /* All preprocessor directives must begin with '#' */

        if (Token && (*Token == '#'))
        {
            if (strlen (Token) == 1)
            {
                Token = PrGetNextToken (NULL, PR_TOKEN_SEPARATORS, &Next);
            }
            else
            {
                Token++;    /* Skip leading # */
            }

            /* Execute the directive, do not write line to output file */

            PrDoDirective (Token, &Next);
            continue;
        }

        /*
         * If we are currently within the part of an IF/ELSE block that is
         * FALSE, ignore the line and do not write it to the output file.
         * This continues until an #else or #endif is encountered.
         */
        if (Gbl_IgnoringThisCodeBlock)
        {
            continue;
        }

        /* Match and replace all #defined names within this source line */

        while (Token)
        {
            DefineInfo = PrMatchDefine (Token);
            if (DefineInfo)
            {
                if (DefineInfo->Body)
                {
                    /* This is a macro */

                    DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
                        "Matched Macro: %s->%s\n",
                        Gbl_CurrentLineNumber, DefineInfo->Identifier,
                        DefineInfo->Replacement);

                    PrDoMacroInvocation (Gbl_MainTokenBuffer, Token,
                        DefineInfo, &Next);
                }
                else
                {
                    ReplaceString = DefineInfo->Replacement;

                    /* Replace the name in the original line buffer */

                    TokenOffset = Token - Gbl_MainTokenBuffer + OffsetAdjust;
                    PrReplaceData (
                        &Gbl_CurrentLineBuffer[TokenOffset], strlen (Token),
                        ReplaceString, strlen (ReplaceString));

                    /* Adjust for length difference between old and new name length */

                    OffsetAdjust += strlen (ReplaceString) - strlen (Token);

                    DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
                        "Matched #define: %s->%s\n",
                        Gbl_CurrentLineNumber, Token,
                        *ReplaceString ? ReplaceString : "(NULL STRING)");
                }
            }

            Token = PrGetNextToken (NULL, PR_TOKEN_SEPARATORS, &Next);
        }

#if 0
/* Line prefix */
        FlPrintFile (ASL_FILE_PREPROCESSOR, "/* %14s  %.5u  i:%.5u */ ",
            Gbl_Files[ASL_FILE_INPUT].Filename,
            Gbl_CurrentLineNumber, Gbl_PreprocessorLineNumber);
#endif

        /*
         * Emit a #line directive if necessary, to keep the line numbers in
         * the (.i) file synchronized with the original source code file, so
         * that the correct line number appears in any error messages
         * generated by the actual compiler.
         */
        if (Gbl_CurrentLineNumber > (Gbl_PreviousLineNumber + 1))
        {
            FlPrintFile (ASL_FILE_PREPROCESSOR, "#line %u\n",
                Gbl_CurrentLineNumber);
        }

        Gbl_PreviousLineNumber = Gbl_CurrentLineNumber;
        Gbl_PreprocessorLineNumber++;

        /*
         * Now we can write the possibly modified source line to the
         * preprocessor (.i) file
         */
        FlWriteFile (ASL_FILE_PREPROCESSOR, Gbl_CurrentLineBuffer,
            strlen (Gbl_CurrentLineBuffer));
    }
}


/*******************************************************************************
 *
 * FUNCTION:    PrDoDirective
 *
 * PARAMETERS:  Directive               - Pointer to directive name token
 *              Next                    - "Next" buffer from GetNextToken
 *
 * RETURN:      None.
 *
 * DESCRIPTION: Main processing for all preprocessor directives
 *
 ******************************************************************************/

static void
PrDoDirective (
    char                    *DirectiveToken,
    char                    **Next)
{
    char                    *Token = Gbl_MainTokenBuffer;
    char                    *Token2;
    char                    *End;
    UINT64                  Value;
    ACPI_SIZE               TokenOffset;
    int                     Directive;
    ACPI_STATUS             Status;


    if (!DirectiveToken)
    {
        goto SyntaxError;
    }

    Directive = PrMatchDirective (DirectiveToken);
    if (Directive == ASL_DIRECTIVE_NOT_FOUND)
    {
        PrError (ASL_ERROR, ASL_MSG_UNKNOWN_DIRECTIVE,
            THIS_TOKEN_OFFSET (DirectiveToken));

        DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
            "#%s: Unknown directive\n",
            Gbl_CurrentLineNumber, DirectiveToken);
        return;
    }

    /*
     * If we are currently ignoring this block and we encounter a #else or
     * #elif, we must ignore their blocks also if the parent block is also
     * being ignored.
     */
    if (Gbl_IgnoringThisCodeBlock)
    {
        switch (Directive)
        {
        case PR_DIRECTIVE_ELSE:
        case PR_DIRECTIVE_ELIF:

            if (Gbl_DirectiveStack && Gbl_DirectiveStack->IgnoringThisCodeBlock)
            {
                PrDbgPrint ("Ignoring", Gbl_DirectiveInfo[Directive].Name);
                return;
            }
            break;

        default:
            break;
        }
    }

    /*
     * Need to always check for #else, #elif, #endif regardless of
     * whether we are ignoring the current code block, since these
     * are conditional code block terminators.
     */
    switch (Directive)
    {
    case PR_DIRECTIVE_ELSE:

        Gbl_IgnoringThisCodeBlock = !(Gbl_IgnoringThisCodeBlock);
        PrDbgPrint ("Executing", "else block");
        return;

    case PR_DIRECTIVE_ELIF:

        Gbl_IgnoringThisCodeBlock = !(Gbl_IgnoringThisCodeBlock);
        Directive = PR_DIRECTIVE_IF;

        if (Gbl_IgnoringThisCodeBlock == TRUE)
        {
            /* Not executing the ELSE part -- all done here */
            PrDbgPrint ("Ignoring", "elif block");
            return;
        }

        /*
         * After this, we will execute the IF part further below.
         * First, however, pop off the original #if directive.
         */
        if (ACPI_FAILURE (PrPopDirective ()))
        {
            PrError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL,
                THIS_TOKEN_OFFSET (DirectiveToken));
        }

        PrDbgPrint ("Executing", "elif block");
        break;

    case PR_DIRECTIVE_ENDIF:

        PrDbgPrint ("Executing", "endif");

        /* Pop the owning #if/#ifdef/#ifndef */

        if (ACPI_FAILURE (PrPopDirective ()))
        {
            PrError (ASL_ERROR, ASL_MSG_ENDIF_MISMATCH,
                THIS_TOKEN_OFFSET (DirectiveToken));
        }
        return;

    default:
        break;
    }

    /* Most directives have at least one argument */

    if (Gbl_DirectiveInfo[Directive].ArgCount == 1)
    {
        Token = PrGetNextToken (NULL, PR_TOKEN_SEPARATORS, Next);
        if (!Token)
        {
            goto SyntaxError;
        }
    }

    /*
     * At this point, if we are ignoring the current code block,
     * do not process any more directives (i.e., ignore them also.)
     * For "if" style directives, open/push a new block anyway. We
     * must do this to keep track of #endif directives
     */
    if (Gbl_IgnoringThisCodeBlock)
    {
        switch (Directive)
        {
        case PR_DIRECTIVE_IF:
        case PR_DIRECTIVE_IFDEF:
        case PR_DIRECTIVE_IFNDEF:

            PrPushDirective (Directive, Token);
            PrDbgPrint ("Ignoring", Gbl_DirectiveInfo[Directive].Name);
            break;

        default:
            break;
        }

        return;
    }

    /*
     * Execute the directive
     */
    PrDbgPrint ("Begin execution", Gbl_DirectiveInfo[Directive].Name);

    switch (Directive)
    {
    case PR_DIRECTIVE_IF:

        TokenOffset = Token - Gbl_MainTokenBuffer;

        /* Need to expand #define macros in the expression string first */

        Status = PrResolveIntegerExpression (
            &Gbl_CurrentLineBuffer[TokenOffset-1], &Value);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        PrPushDirective (Directive, Token);
        if (!Value)
        {
            Gbl_IgnoringThisCodeBlock = TRUE;
        }

        DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
            "Resolved #if: %8.8X%8.8X %s\n",
            Gbl_CurrentLineNumber, ACPI_FORMAT_UINT64 (Value),
            Gbl_IgnoringThisCodeBlock ? "<Skipping Block>" : "<Executing Block>");
        break;

    case PR_DIRECTIVE_IFDEF:

        PrPushDirective (Directive, Token);
        if (!PrMatchDefine (Token))
        {
            Gbl_IgnoringThisCodeBlock = TRUE;
        }

        PrDbgPrint ("Evaluated", "ifdef");
        break;

    case PR_DIRECTIVE_IFNDEF:

        PrPushDirective (Directive, Token);
        if (PrMatchDefine (Token))
        {
            Gbl_IgnoringThisCodeBlock = TRUE;
        }

        PrDbgPrint ("Evaluated", "ifndef");
        break;

    case PR_DIRECTIVE_DEFINE:
        /*
         * By definition, if first char after the name is a paren,
         * this is a function macro.
         */
        TokenOffset = Token - Gbl_MainTokenBuffer + strlen (Token);
        if (*(&Gbl_CurrentLineBuffer[TokenOffset]) == '(')
        {
#ifndef MACROS_SUPPORTED
            AcpiOsPrintf ("%s ERROR - line %u: #define macros are not supported yet\n",
                Gbl_CurrentLineBuffer, Gbl_CurrentLineNumber);
            exit(1);
#else
            PrAddMacro (Token, Next);
#endif
        }
        else
        {
            /* Use the remainder of the line for the #define */

            Token2 = *Next;
            if (Token2)
            {
                while ((*Token2 == ' ') || (*Token2 == '\t'))
                {
                    Token2++;
                }
                End = Token2;
                while (*End != '\n')
                {
                    End++;
                }
                *End = 0;
            }
            else
            {
                Token2 = "";
            }
#if 0
            Token2 = PrGetNextToken (NULL, "\n", /*PR_TOKEN_SEPARATORS,*/ Next);
            if (!Token2)
            {
                Token2 = "";
            }
#endif
            DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
                "New #define: %s->%s\n",
                Gbl_CurrentLineNumber, Token, Token2);

            PrAddDefine (Token, Token2, FALSE);
        }
        break;

    case PR_DIRECTIVE_ERROR:

        /* Note: No macro expansion */

        PrError (ASL_ERROR, ASL_MSG_ERROR_DIRECTIVE,
            THIS_TOKEN_OFFSET (Token));

        Gbl_SourceLine = 0;
        Gbl_NextError = Gbl_ErrorLog;
        CmCleanupAndExit ();
        exit(1);

    case PR_DIRECTIVE_INCLUDE:

        Token = PrGetNextToken (NULL, " \"<>", Next);
        if (!Token)
        {
            goto SyntaxError;
        }

        DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
            "Start #include file \"%s\"\n", Gbl_CurrentLineNumber,
            Token, Gbl_CurrentLineNumber);

        PrOpenIncludeFile (Token);
        break;

    case PR_DIRECTIVE_LINE:

        TokenOffset = Token - Gbl_MainTokenBuffer;

        Status = PrResolveIntegerExpression (
            &Gbl_CurrentLineBuffer[TokenOffset-1], &Value);
        if (ACPI_FAILURE (Status))
        {
            return;
        }

        DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
            "User #line invocation %s\n", Gbl_CurrentLineNumber,
            Token);

        /* Update local line numbers */

        Gbl_CurrentLineNumber = (UINT32) Value;
        Gbl_PreviousLineNumber = 0;

        /* Emit #line into the preprocessor file */

        FlPrintFile (ASL_FILE_PREPROCESSOR, "#line %u \"%s\"\n",
            Gbl_CurrentLineNumber, Gbl_Files[ASL_FILE_INPUT].Filename);
        break;

    case PR_DIRECTIVE_PRAGMA:

        if (!strcmp (Token, "disable"))
        {
            Token = PrGetNextToken (NULL, PR_TOKEN_SEPARATORS, Next);
            if (!Token)
            {
                goto SyntaxError;
            }

            TokenOffset = Token - Gbl_MainTokenBuffer;
            AslDisableException (&Gbl_CurrentLineBuffer[TokenOffset]);
        }
        else if (!strcmp (Token, "message"))
        {
            Token = PrGetNextToken (NULL, PR_TOKEN_SEPARATORS, Next);
            if (!Token)
            {
                goto SyntaxError;
            }

            TokenOffset = Token - Gbl_MainTokenBuffer;
            AcpiOsPrintf ("%s\n", &Gbl_CurrentLineBuffer[TokenOffset]);
        }
        else
        {
            PrError (ASL_ERROR, ASL_MSG_UNKNOWN_PRAGMA,
                THIS_TOKEN_OFFSET (Token));
            return;
        }

        break;

    case PR_DIRECTIVE_UNDEF:

        DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
            "#undef: %s\n", Gbl_CurrentLineNumber, Token);

        PrRemoveDefine (Token);
        break;

    case PR_DIRECTIVE_WARNING:

        PrError (ASL_WARNING, ASL_MSG_WARNING_DIRECTIVE,
            THIS_TOKEN_OFFSET (Token));
        break;

    default:

        /* Should never get here */
        DbgPrint (ASL_DEBUG_OUTPUT, PR_PREFIX_ID
            "Unrecognized directive: %u\n",
            Gbl_CurrentLineNumber, Directive);
        break;
    }

    return;

SyntaxError:

    PrError (ASL_ERROR, ASL_MSG_DIRECTIVE_SYNTAX,
        THIS_TOKEN_OFFSET (DirectiveToken));
    return;
}


/*******************************************************************************
 *
 * FUNCTION:    PrMatchDirective
 *
 * PARAMETERS:  Directive           - Pointer to directive name token
 *
 * RETURN:      Index into command array, -1 if not found
 *
 * DESCRIPTION: Lookup the incoming directive in the known directives table.
 *
 ******************************************************************************/

static int
PrMatchDirective (
    char                    *Directive)
{
    int                     i;


    if (!Directive || Directive[0] == 0)
    {
        return (ASL_DIRECTIVE_NOT_FOUND);
    }

    for (i = 0; Gbl_DirectiveInfo[i].Name; i++)
    {
        if (!strcmp (Gbl_DirectiveInfo[i].Name, Directive))
        {
            return (i);
        }
    }

    return (ASL_DIRECTIVE_NOT_FOUND);    /* Command not recognized */
}


/*******************************************************************************
 *
 * FUNCTION:    PrPushDirective
 *
 * PARAMETERS:  Directive           - Encoded directive ID
 *              Argument            - String containing argument to the
 *                                    directive
 *
 * RETURN:      None
 *
 * DESCRIPTION: Push an item onto the directive stack. Used for processing
 *              nested #if/#else type conditional compilation directives.
 *              Specifically: Used on detection of #if/#ifdef/#ifndef to open
 *              a block.
 *
 ******************************************************************************/

static void
PrPushDirective (
    int                     Directive,
    char                    *Argument)
{
    DIRECTIVE_INFO          *Info;


    /* Allocate and populate a stack info item */

    Info = ACPI_ALLOCATE (sizeof (DIRECTIVE_INFO));

    Info->Next = Gbl_DirectiveStack;
    Info->Directive = Directive;
    Info->IgnoringThisCodeBlock = Gbl_IgnoringThisCodeBlock;
    strncpy (Info->Argument, Argument, MAX_ARGUMENT_LENGTH);

    DbgPrint (ASL_DEBUG_OUTPUT,
        "Pr(%.4u) - [%u %s] %*s Pushed [#%s %s]: IgnoreFlag = %s\n",
        Gbl_CurrentLineNumber, Gbl_IfDepth,
        Gbl_IgnoringThisCodeBlock ? "I" : "E",
        Gbl_IfDepth * 4, " ",
        Gbl_DirectiveInfo[Directive].Name,
        Argument, Gbl_IgnoringThisCodeBlock ? "TRUE" : "FALSE");

    /* Push new item */

    Gbl_DirectiveStack = Info;
    Gbl_IfDepth++;
}


/*******************************************************************************
 *
 * FUNCTION:    PrPopDirective
 *
 * PARAMETERS:  None
 *
 * RETURN:      Status. Error if the stack is empty.
 *
 * DESCRIPTION: Pop an item off the directive stack. Used for processing
 *              nested #if/#else type conditional compilation directives.
 *              Specifically: Used on detection of #elif and #endif to remove
 *              the original #if/#ifdef/#ifndef from the stack and close
 *              the block.
 *
 ******************************************************************************/

static ACPI_STATUS
PrPopDirective (
    void)
{
    DIRECTIVE_INFO          *Info;


    /* Check for empty stack */

    Info = Gbl_DirectiveStack;
    if (!Info)
    {
        return (AE_ERROR);
    }

    /* Pop one item, keep globals up-to-date */

    Gbl_IfDepth--;
    Gbl_IgnoringThisCodeBlock = Info->IgnoringThisCodeBlock;
    Gbl_DirectiveStack = Info->Next;

    DbgPrint (ASL_DEBUG_OUTPUT,
        "Pr(%.4u) - [%u %s] %*s Popped [#%s %s]: IgnoreFlag now = %s\n",
        Gbl_CurrentLineNumber, Gbl_IfDepth,
        Gbl_IgnoringThisCodeBlock ? "I" : "E",
        Gbl_IfDepth * 4, " ",
        Gbl_DirectiveInfo[Info->Directive].Name,
        Info->Argument, Gbl_IgnoringThisCodeBlock ? "TRUE" : "FALSE");

    ACPI_FREE (Info);
    return (AE_OK);
}


/*******************************************************************************
 *
 * FUNCTION:    PrDbgPrint
 *
 * PARAMETERS:  Action              - Action being performed
 *              DirectiveName       - Directive being processed
 *
 * RETURN:      None
 *
 * DESCRIPTION: Special debug print for directive processing.
 *
 ******************************************************************************/

static void
PrDbgPrint (
    char                    *Action,
    char                    *DirectiveName)
{

    DbgPrint (ASL_DEBUG_OUTPUT, "Pr(%.4u) - [%u %s] "
        "%*s %s #%s, Depth %u\n",
        Gbl_CurrentLineNumber, Gbl_IfDepth,
        Gbl_IgnoringThisCodeBlock ? "I" : "E",
        Gbl_IfDepth * 4, " ",
        Action, DirectiveName, Gbl_IfDepth);
}
