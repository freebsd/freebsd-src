/*-
 * Copyright (c) 1988, 1989, 1990 The Regents of the University of California.
 * Copyright (c) 1988, 1989 by Adam de Boor
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
 *	@(#)nonints.h	5.6 (Berkeley) 4/18/91
 */

char **brk_string(), *emalloc(), *str_concat();

ReturnStatus	Arch_ParseArchive ();
void	Arch_Touch ();
void	Arch_TouchLib ();
int	Arch_MTime ();
int	Arch_MemMTime ();
void	Arch_FindLib ();
Boolean	Arch_LibOODate ();
void	Arch_Init ();
void	Compat_Run();
void	Dir_Init ();
Boolean	Dir_HasWildcards ();
void	Dir_Expand ();
char *	Dir_FindFile ();
int	Dir_MTime ();
void	Dir_AddDir ();
ClientData	Dir_CopyDir ();
char *	Dir_MakeFlags ();
void	Dir_Destroy ();
void	Dir_ClearPath ();
void	Dir_Concat ();
int	Make_TimeStamp ();
Boolean	Make_OODate ();
int	Make_HandleUse ();
void	Make_Update ();
void	Make_DoAllVar ();
Boolean	Make_Run ();
void	Job_Touch ();
Boolean	Job_CheckCommands ();
void	Job_CatchChildren ();
void	Job_CatchOutput ();
void	Job_Make ();
void	Job_Init ();
Boolean	Job_Full ();
Boolean	Job_Empty ();
ReturnStatus	Job_ParseShell ();
int	Job_End ();
void	Job_Wait();
void	Job_AbortAll ();
void	Main_ParseArgLine ();
void	Error ();
void	Fatal ();
void	Punt ();
void	DieHorribly ();
void	Finish ();
void	Parse_Error ();
Boolean	Parse_IsVar ();
void	Parse_DoVar ();
void	Parse_AddIncludeDir ();
void	Parse_File();
Lst	Parse_MainName();
void	Suff_ClearSuffixes ();
Boolean	Suff_IsTransform ();
GNode *	Suff_AddTransform ();
void	Suff_AddSuffix ();
int	Suff_EndTransform ();
Lst	Suff_GetPath ();
void	Suff_DoPaths();
void	Suff_AddInclude ();
void	Suff_AddLib ();
void	Suff_FindDeps ();
void	Suff_SetNull();
void	Suff_Init ();
void	Targ_Init ();
GNode *	Targ_NewGN ();
GNode *	Targ_FindNode ();
Lst	Targ_FindList ();
Boolean	Targ_Ignore ();
Boolean	Targ_Silent ();
Boolean	Targ_Precious ();
void	Targ_SetMain ();
int	Targ_PrintCmd ();
char *	Targ_FmtTime ();
void	Targ_PrintType ();
char *	Str_Concat ();
int	Str_Match();
void	Var_Delete();
void	Var_Set ();
void	Var_Append ();
Boolean	Var_Exists();
char *	Var_Value ();
char *	Var_Parse ();
char *	Var_Subst ();
char *	Var_GetTail();
char *	Var_GetHead();
void	Var_Init ();
char *	Str_FindSubstring();
