/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)nonints.h	8.4 (Berkeley) 4/28/95
 * $FreeBSD$
 */

/* arch.c */
ReturnStatus Arch_ParseArchive(char **, Lst, GNode *);
void Arch_Touch(GNode *);
void Arch_TouchLib(GNode *);
int Arch_MTime(GNode *);
int Arch_MemMTime(GNode *);
void Arch_FindLib(GNode *, Lst);
Boolean Arch_LibOODate(GNode *);
void Arch_Init(void);
void Arch_End(void);

/* compat.c */
void Compat_Run(Lst);

/* cond.c */
int Cond_Eval(char *);
void Cond_End(void);

/* for.c */
int For_Eval(char *);
void For_Run(void);

/* main.c */
void Main_ParseArgLine(char *);
char *Cmd_Exec(char *, char **);
void Debug(const char *, ...);
void Error(const char *, ...);
void Fatal(const char *, ...);
void Punt(const char *, ...);
void DieHorribly(void);
int PrintAddr(void *, void *);
void Finish(int);
char *estrdup(const char *);
void *emalloc(size_t);
void *erealloc(void *, size_t);
void enomem(void);
int eunlink(const char *);

/* parse.c */
void Parse_Error(int, const char *, ...);
Boolean Parse_AnyExport(void);
Boolean Parse_IsVar(char *);
void Parse_DoVar(char *, GNode *);
void Parse_AddIncludeDir(char *);
void Parse_File(char *, FILE *);
void Parse_Init(void);
void Parse_End(void);
void Parse_FromString(char *);
Lst Parse_MainName(void);

/* str.c */
void str_init(void);
void str_end(void);
char *str_concat(char *, char *, int);
char **brk_string(char *, int *, Boolean);
char *Str_FindSubstring(char *, char *);
int Str_Match(char *, char *);
char *Str_SYSVMatch(char *, char *, int *len);
void Str_SYSVSubst(Buffer, char *, char *, int);

/* suff.c */
void Suff_ClearSuffixes(void);
Boolean Suff_IsTransform(char *);
GNode *Suff_AddTransform(char *);
int Suff_EndTransform(void *, void *);
void Suff_AddSuffix(char *);
Lst Suff_GetPath(char *);
void Suff_DoPaths(void);
void Suff_AddInclude(char *);
void Suff_AddLib(char *);
void Suff_FindDeps(GNode *);
void Suff_SetNull(char *);
void Suff_Init(void);
void Suff_End(void);
void Suff_PrintAll(void);

/* targ.c */
void Targ_Init(void);
void Targ_End(void);
GNode *Targ_NewGN(char *);
GNode *Targ_FindNode(char *, int);
Lst Targ_FindList(Lst, int);
Boolean Targ_Ignore(GNode *);
Boolean Targ_Silent(GNode *);
Boolean Targ_Precious(GNode *);
void Targ_SetMain(GNode *);
int Targ_PrintCmd(void *, void *);
char *Targ_FmtTime(time_t);
void Targ_PrintType(int);
void Targ_PrintGraph(int);

/* var.c */
void Var_Delete(char *, GNode *);
void Var_Set(char *, char *, GNode *);
void Var_Append(char *, char *, GNode *);
Boolean Var_Exists(char *, GNode *);
char *Var_Value(char *, GNode *, char **);
char *Var_Parse(char *, GNode *, Boolean, int *, Boolean *);
char *Var_Subst(char *, char *, GNode *, Boolean);
char *Var_GetTail(char *);
char *Var_GetHead(char *);
void Var_Init(void);
void Var_End(void);
void Var_Dump(GNode *);
