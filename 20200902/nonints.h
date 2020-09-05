/*	$NetBSD: nonints.h,v 1.102 2020/08/30 19:56:02 rillig Exp $	*/

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
Boolean Arch_ParseArchive(char **, Lst, GNode *);
void Arch_Touch(GNode *);
void Arch_TouchLib(GNode *);
time_t Arch_MTime(GNode *);
time_t Arch_MemMTime(GNode *);
void Arch_FindLib(GNode *, Lst);
Boolean Arch_LibOODate(GNode *);
void Arch_Init(void);
void Arch_End(void);
Boolean Arch_IsLib(GNode *);

/* compat.c */
int CompatRunCommand(void *, void *);
void Compat_Run(Lst);
int Compat_Make(void *, void *);

/* cond.c */
struct If;
CondEvalResult Cond_EvalExpression(const struct If *, char *, Boolean *, int, Boolean);
CondEvalResult Cond_Eval(char *);
void Cond_restore_depth(unsigned int);
unsigned int Cond_save_depth(void);

/* for.c */
int For_Eval(char *);
int For_Accum(char *);
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
int PrintAddr(void *, void *);
void Finish(int) MAKE_ATTR_DEAD;
int eunlink(const char *);
void execError(const char *, const char *);
char *getTmpdir(void);
Boolean s2Boolean(const char *, Boolean);
Boolean getBoolean(const char *, Boolean);
char *cached_realpath(const char *, char *);

/* parse.c */
void Parse_Error(int, const char *, ...) MAKE_ATTR_PRINTFLIKE(2, 3);
Boolean Parse_IsVar(char *);
void Parse_DoVar(char *, GNode *);
void Parse_AddIncludeDir(char *);
void Parse_File(const char *, int);
void Parse_Init(void);
void Parse_End(void);
void Parse_SetInput(const char *, int, int, char *(*)(void *, size_t *), void *);
Lst Parse_MainName(void);

/* str.c */
typedef struct {
    char **words;
    size_t len;
    void *freeIt;
} Words;

Words Str_Words(const char *, Boolean);
static inline void MAKE_ATTR_UNUSED
Words_Free(Words w) {
    free(w.words);
    free(w.freeIt);
}

char *str_concat2(const char *, const char *);
char *str_concat3(const char *, const char *, const char *);
char *str_concat4(const char *, const char *, const char *, const char *);
char *Str_FindSubstring(const char *, const char *);
Boolean Str_Match(const char *, const char *);

#ifndef HAVE_STRLCPY
/* strlcpy.c */
size_t strlcpy(char *, const char *, size_t);
#endif

/* suff.c */
void Suff_ClearSuffixes(void);
Boolean Suff_IsTransform(char *);
GNode *Suff_AddTransform(char *);
int Suff_EndTransform(void *, void *);
void Suff_AddSuffix(const char *, GNode **);
Lst Suff_GetPath(char *);
void Suff_DoPaths(void);
void Suff_AddInclude(char *);
void Suff_AddLib(const char *);
void Suff_FindDeps(GNode *);
Lst Suff_FindPath(GNode *);
void Suff_SetNull(char *);
void Suff_Init(void);
void Suff_End(void);
void Suff_PrintAll(void);

/* targ.c */
void Targ_Init(void);
void Targ_End(void);
void Targ_Stats(void);
Lst Targ_List(void);
GNode *Targ_NewGN(const char *);
GNode *Targ_FindNode(const char *, int);
Lst Targ_FindList(Lst, int);
Boolean Targ_Ignore(GNode *);
Boolean Targ_Silent(GNode *);
Boolean Targ_Precious(GNode *);
void Targ_SetMain(GNode *);
int Targ_PrintCmd(void *, void *);
int Targ_PrintNode(void *, void *);
char *Targ_FmtTime(time_t);
void Targ_PrintType(int);
void Targ_PrintGraph(int);
void Targ_Propagate(void);

/* var.c */

typedef enum {
    /* Treat undefined variables as errors. */
    VARE_UNDEFERR	= 0x01,
    /* Expand and evaluate variables during parsing. */
    VARE_WANTRES	= 0x02,
    VARE_ASSIGN		= 0x04
} VarEvalFlags;

typedef enum {
    VAR_NO_EXPORT	= 0x01,	/* do not export */
    /* Make the variable read-only. No further modification is possible,
     * except for another call to Var_Set with the same flag. */
    VAR_SET_READONLY	= 0x02
} VarSet_Flags;


void Var_Delete(const char *, GNode *);
void Var_Set(const char *, const char *, GNode *);
void Var_Set_with_flags(const char *, const char *, GNode *, VarSet_Flags);
void Var_Append(const char *, const char *, GNode *);
Boolean Var_Exists(const char *, GNode *);
const char *Var_Value(const char *, GNode *, char **);
const char *Var_Parse(const char *, GNode *, VarEvalFlags, int *, void **);
char *Var_Subst(const char *, GNode *, VarEvalFlags);
void Var_Init(void);
void Var_End(void);
void Var_Stats(void);
void Var_Dump(GNode *);
void Var_ExportVars(void);
void Var_Export(const char *, Boolean);
void Var_UnExport(const char *);

/* util.c */
void (*bmake_signal(int, void (*)(int)))(int);
