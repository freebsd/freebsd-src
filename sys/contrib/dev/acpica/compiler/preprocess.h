/******************************************************************************
 *
 * Module Name: preprocess.h - header for iASL Preprocessor
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2017, Intel Corp.
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

#define __PREPROCESS_H__

#ifndef _PREPROCESS
#define _PREPROCESS

#undef PR_EXTERN

#ifdef _DECLARE_PR_GLOBALS
#define PR_EXTERN
#define PR_INIT_GLOBAL(a,b)         (a)=(b)
#else
#define PR_EXTERN                   extern
#define PR_INIT_GLOBAL(a,b)         (a)
#endif


/*
 * Configuration
 */
#define PR_MAX_MACRO_ARGS       32              /* Max number of macro args */
#define PR_MAX_ARG_INSTANCES    24              /* Max instances of any one arg */
#define PR_LINES_PER_BLOCK      4096            /* Max input source lines per block */


/*
 * Local defines and macros
 */
#define PR_TOKEN_SEPARATORS     " ,(){}\t\n"
#define PR_MACRO_SEPARATORS     " ,(){}~!*/%+-<>=&^|\"\t\n"
#define PR_MACRO_ARGUMENTS      " ,\t\n"
#define PR_EXPR_SEPARATORS      " ,(){}~!*/%+-<>=&^|\"\t\n"

#define PR_PREFIX_ID            "Pr(%.4u) - "             /* Used for debug output */

#define THIS_TOKEN_OFFSET(t)    ((t-Gbl_MainTokenBuffer) + 1)


/*
 * Preprocessor structures
 */
typedef struct pr_macro_arg
{
    char                        *Name;
    UINT32                      Offset[PR_MAX_ARG_INSTANCES];
    UINT16                      UseCount;

} PR_MACRO_ARG;

typedef struct pr_define_info
{
    struct pr_define_info       *Previous;
    struct pr_define_info       *Next;
    char                        *Identifier;
    char                        *Replacement;
    char                        *Body;          /* Macro body */
    PR_MACRO_ARG                *Args;          /* Macro arg list */
    UINT16                      ArgCount;       /* Macro arg count */
    BOOLEAN                     Persist;        /* Keep for entire compiler run */

} PR_DEFINE_INFO;

typedef struct pr_directive_info
{
    char                        *Name;          /* Directive name */
    UINT8                       ArgCount;       /* Required # of args */

} PR_DIRECTIVE_INFO;

typedef struct pr_operator_info
{
    char                        *Op;

} PR_OPERATOR_INFO;

typedef struct pr_file_node
{
    struct pr_file_node         *Next;
    FILE                        *File;
    char                        *Filename;
    UINT32                      CurrentLineNumber;

} PR_FILE_NODE;

#define MAX_ARGUMENT_LENGTH     24

typedef struct directive_info
{
    struct directive_info       *Next;
    char                        Argument[MAX_ARGUMENT_LENGTH];
    int                         Directive;
    BOOLEAN                     IgnoringThisCodeBlock;

} DIRECTIVE_INFO;


/*
 * Globals
 */
#if 0 /* TBD for macros */
PR_EXTERN char                  PR_INIT_GLOBAL (*XXXEvalBuffer, NULL); /* [ASL_LINE_BUFFER_SIZE]; */
#endif

PR_EXTERN char                  PR_INIT_GLOBAL (*Gbl_MainTokenBuffer, NULL); /* [ASL_LINE_BUFFER_SIZE]; */
PR_EXTERN char                  PR_INIT_GLOBAL (*Gbl_MacroTokenBuffer, NULL); /* [ASL_LINE_BUFFER_SIZE]; */
PR_EXTERN char                  PR_INIT_GLOBAL (*Gbl_ExpressionTokenBuffer, NULL); /* [ASL_LINE_BUFFER_SIZE]; */

PR_EXTERN UINT32                Gbl_PreprocessorLineNumber;
PR_EXTERN int                   Gbl_IfDepth;
PR_EXTERN PR_FILE_NODE          *Gbl_InputFileList;
PR_EXTERN PR_DEFINE_INFO        PR_INIT_GLOBAL (*Gbl_DefineList, NULL);
PR_EXTERN BOOLEAN               PR_INIT_GLOBAL (Gbl_PreprocessorError, FALSE);
PR_EXTERN BOOLEAN               PR_INIT_GLOBAL (Gbl_IgnoringThisCodeBlock, FALSE);
PR_EXTERN DIRECTIVE_INFO        PR_INIT_GLOBAL (*Gbl_DirectiveStack, NULL);

/*
 * prscan - Preprocessor entry
 */
void
PrInitializePreprocessor (
    void);

void
PrInitializeGlobals (
    void);

void
PrTerminatePreprocessor (
    void);

void
PrDoPreprocess (
    void);

UINT64
PrIsDefined (
    char                    *Identifier);

UINT64
PrResolveDefine (
    char                    *Identifier);

int
PrInitLexer (
    char                    *String);

void
PrTerminateLexer (
    void);


/*
 * prmacros - Support for #defines and macros
 */
void
PrDumpPredefinedNames (
    void);

PR_DEFINE_INFO *
PrAddDefine (
    char                    *Token,
    char                    *Token2,
    BOOLEAN                 Persist);

void
PrRemoveDefine (
    char                    *DefineName);

PR_DEFINE_INFO *
PrMatchDefine (
    char                    *MatchString);

void
PrAddMacro (
    char                    *Name,
    char                    **Next);

void
PrDoMacroInvocation (
    char                    *TokenBuffer,
    char                    *MacroStart,
    PR_DEFINE_INFO          *DefineInfo,
    char                    **Next);


/*
 * prexpress - #if expression support
 */
ACPI_STATUS
PrResolveIntegerExpression (
    char                    *Line,
    UINT64                  *ReturnValue);

char *
PrPrioritizeExpression (
    char                    *OriginalLine);

/*
 * prparser - lex/yacc expression parser
 */
UINT64
PrEvaluateExpression (
    char                    *ExprString);


/*
 * prutils - Preprocesor utilities
 */
char *
PrGetNextToken (
    char                    *Buffer,
    char                    *MatchString,
    char                    **Next);

void
PrError (
    UINT8                   Level,
    UINT16                  MessageId,
    UINT32                  Column);

void
PrReplaceData (
    char                    *Buffer,
    UINT32                  LengthToRemove,
    char                    *BufferToAdd,
    UINT32                  LengthToAdd);

FILE *
PrOpenIncludeFile (
    char                    *Filename,
    char                    *OpenMode,
    char                    **FullPathname);

FILE *
PrOpenIncludeWithPrefix (
    char                    *PrefixDir,
    char                    *Filename,
    char                    *OpenMode,
    char                    **FullPathname);

void
PrPushInputFileStack (
    FILE                    *InputFile,
    char                    *Filename);

BOOLEAN
PrPopInputFileStack (
    void);

#endif
