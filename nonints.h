/*	$NetBSD: nonints.h,v 1.149 2020/11/01 00:24:57 rillig Exp $	*/

/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)nonints.h	8.3 (Berkeley) 3/19/94
 */

/*-
 * Copyright (c) 1989 by Berkeley Softworks
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Adam de Boor.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)nonints.h	8.3 (Berkeley) 3/19/94
 */

/* arch.c */
void Arch_Init(void);
void Arch_End(void);

Boolean Arch_ParseArchive(char **, GNodeList *, GNode *);
void Arch_Touch(GNode *);
void Arch_TouchLib(GNode *);
time_t Arch_MTime(GNode *);
time_t Arch_MemMTime(GNode *);
void Arch_FindLib(GNode *, SearchPath *);
Boolean Arch_LibOODate(GNode *);
Boolean Arch_IsLib(GNode *);

/* compat.c */
int Compat_RunCommand(const char *, GNode *);
void Compat_Run(GNodeList *);
void Compat_Make(GNode *, GNode *);

/* cond.c */
CondEvalResult Cond_EvalCondition(const char *, Boolean *);
CondEvalResult Cond_EvalLine(const char *);
void Cond_restore_depth(unsigned int);
unsigned int Cond_save_depth(void);

/* for.c */
int For_Eval(const char *);
Boolean For_Accum(const char *);
void For_Run(int);

/* job.c */
#ifdef WAIT_T
void JobReapChild(pid_t, WAIT_T, Boolean);
#endif

/* main.c */
void Main_ParseArgLine(const char *);
void MakeMode(const char *);
char *Cmd_Exec(const char *, const char **);
void Error(const char *, ...) MAKE_ATTR_PRINTFLIKE(1, 2);
void Fatal(const char *, ...) MAKE_ATTR_PRINTFLIKE(1, 2) MAKE_ATTR_DEAD;
void Punt(const char *, ...) MAKE_ATTR_PRINTFLIKE(1, 2) MAKE_ATTR_DEAD;
void DieHorribly(void) MAKE_ATTR_DEAD;
void Finish(int) MAKE_ATTR_DEAD;
int eunlink(const char *);
void execDie(const char *, const char *);
char *getTmpdir(void);
Boolean s2Boolean(const char *, Boolean);
Boolean getBoolean(const char *, Boolean);
char *cached_realpath(const char *, char *);

/* parse.c */
void Parse_Init(void);
void Parse_End(void);

typedef enum VarAssignOp {
    VAR_NORMAL,			/* = */
    VAR_SUBST,			/* := */
    VAR_SHELL,			/* != or :sh= */
    VAR_APPEND,			/* += */
    VAR_DEFAULT			/* ?= */
} VarAssignOp;

typedef struct VarAssign {
    char *varname;		/* unexpanded */
    VarAssignOp op;
    const char *value;		/* unexpanded */
} VarAssign;

typedef char *(*NextBufProc)(void *, size_t *);

void Parse_Error(ParseErrorLevel, const char *, ...) MAKE_ATTR_PRINTFLIKE(2, 3);
Boolean Parse_IsVar(const char *, VarAssign *out_var);
void Parse_DoVar(VarAssign *, GNode *);
void Parse_AddIncludeDir(const char *);
void Parse_File(const char *, int);
void Parse_SetInput(const char *, int, int, NextBufProc, void *);
GNodeList *Parse_MainName(void);
int Parse_GetFatals(void);

/* str.c */
typedef struct Words {
    char **words;
    size_t len;
    void *freeIt;
} Words;

Words Str_Words(const char *, Boolean);
static inline MAKE_ATTR_UNUSED void
Words_Free(Words w) {
    free(w.words);
    free(w.freeIt);
}

char *str_concat2(const char *, const char *);
char *str_concat3(const char *, const char *, const char *);
char *str_concat4(const char *, const char *, const char *, const char *);
Boolean Str_Match(const char *, const char *);

#ifndef HAVE_STRLCPY
/* strlcpy.c */
size_t strlcpy(char *, const char *, size_t);
#endif

/* suff.c */
void Suff_Init(void);
void Suff_End(void);

void Suff_ClearSuffixes(void);
Boolean Suff_IsTransform(const char *);
GNode *Suff_AddTransform(const char *);
void Suff_EndTransform(GNode *);
void Suff_AddSuffix(const char *, GNode **);
SearchPath *Suff_GetPath(const char *);
void Suff_DoPaths(void);
void Suff_AddInclude(const char *);
void Suff_AddLib(const char *);
void Suff_FindDeps(GNode *);
SearchPath *Suff_FindPath(GNode *);
void Suff_SetNull(const char *);
void Suff_PrintAll(void);

/* targ.c */
void Targ_Init(void);
void Targ_End(void);

void Targ_Stats(void);
GNodeList *Targ_List(void);
GNode *Targ_NewGN(const char *);
GNode *Targ_FindNode(const char *);
GNode *Targ_GetNode(const char *);
GNode *Targ_NewInternalNode(const char *);
GNode *Targ_GetEndNode(void);
GNodeList *Targ_FindList(StringList *);
Boolean Targ_Ignore(GNode *);
Boolean Targ_Silent(GNode *);
Boolean Targ_Precious(GNode *);
void Targ_SetMain(GNode *);
void Targ_PrintCmds(GNode *);
void Targ_PrintNode(GNode *, int);
void Targ_PrintNodes(GNodeList *, int);
char *Targ_FmtTime(time_t);
void Targ_PrintType(int);
void Targ_PrintGraph(int);
void Targ_Propagate(void);

/* var.c */
void Var_Init(void);
void Var_End(void);

typedef enum VarEvalFlags {
    VARE_NONE		= 0,
    /* Treat undefined variables as errors. */
    VARE_UNDEFERR	= 0x01,
    /* Expand and evaluate variables during parsing. */
    VARE_WANTRES	= 0x02,
    /* In an assignment using the ':=' operator, keep '$$' as '$$' instead
     * of reducing it to a single '$'. */
    VARE_ASSIGN		= 0x04
} VarEvalFlags;

typedef enum VarSet_Flags {
    VAR_NO_EXPORT	= 0x01,	/* do not export */
    /* Make the variable read-only. No further modification is possible,
     * except for another call to Var_Set with the same flag. */
    VAR_SET_READONLY	= 0x02
} VarSet_Flags;

/* The state of error handling returned by Var_Parse.
 *
 * As of 2020-09-13, this bitset looks quite bloated,
 * with all the constants doubled.
 *
 * Its purpose is to first document the existing behavior,
 * and then migrate away from the SILENT constants, step by step,
 * as these are not suited for reliable, consistent error handling
 * and reporting. */
typedef enum VarParseResult {

    /* Both parsing and evaluation succeeded. */
    VPR_OK		= 0x0000,

    /* See if a message has already been printed for this error. */
    VPR_ANY_MSG		= 0x0001,

    /* Parsing failed.
     * No error message has been printed yet.
     * Deprecated, migrate to VPR_PARSE_MSG instead. */
    VPR_PARSE_SILENT	= 0x0002,

    /* Parsing failed.
     * An error message has already been printed. */
    VPR_PARSE_MSG	= VPR_PARSE_SILENT | VPR_ANY_MSG,

    /* Parsing succeeded.
     * During evaluation, VARE_UNDEFERR was set and there was an undefined
     * variable.
     * No error message has been printed yet.
     * Deprecated, migrate to VPR_UNDEF_MSG instead. */
    VPR_UNDEF_SILENT	= 0x0004,

    /* Parsing succeeded.
     * During evaluation, VARE_UNDEFERR was set and there was an undefined
     * variable.
     * An error message has already been printed. */
    VPR_UNDEF_MSG	= VPR_UNDEF_SILENT | VPR_ANY_MSG,

    /* Parsing succeeded.
     * Evaluation failed.
     * No error message has been printed yet.
     * Deprecated, migrate to VPR_EVAL_MSG instead. */
    VPR_EVAL_SILENT	= 0x0006,

    /* Parsing succeeded.
     * Evaluation failed.
     * An error message has already been printed. */
    VPR_EVAL_MSG	= VPR_EVAL_SILENT | VPR_ANY_MSG,

    /* The exact error handling status is not known yet.
     * Deprecated, migrate to VPR_OK or any VPE_*_MSG instead. */
    VPR_UNKNOWN		= 0x0008
} VarParseResult;

void Var_Delete(const char *, GNode *);
void Var_Set(const char *, const char *, GNode *);
void Var_Set_with_flags(const char *, const char *, GNode *, VarSet_Flags);
void Var_Append(const char *, const char *, GNode *);
Boolean Var_Exists(const char *, GNode *);
const char *Var_Value(const char *, GNode *, void **);
const char *Var_ValueDirect(const char *, GNode *);
VarParseResult Var_Parse(const char **, GNode *, VarEvalFlags,
			 const char **, void **);
VarParseResult Var_Subst(const char *, GNode *, VarEvalFlags, char **);
void Var_Stats(void);
void Var_Dump(GNode *);
void Var_ExportVars(void);
void Var_Export(const char *, Boolean);
void Var_UnExport(const char *);

/* util.c */
typedef void (*SignalProc)(int);
SignalProc bmake_signal(int, SignalProc);
