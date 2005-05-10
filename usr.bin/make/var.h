/*-
 * Copyright (c) 2002 Juli Mallett.
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
 * $FreeBSD$
 */

#ifndef var_h_9cccafce
#define	var_h_9cccafce

#include <regex.h>

#include "config.h"

struct GNode;
struct Buffer;

typedef struct Var {
	char		*name;	/* the variable's name */
	struct Buffer	*val;	/* its value */
	int		flags;	/* miscellaneous status flags */

#define	VAR_IN_USE	1	/* Variable's value currently being used.
				 * Used to avoid recursion */

#define	VAR_JUNK	4	/* Variable is a junk variable that
				 * should be destroyed when done with
				 * it. Used by Var_Parse for undefined,
				 * modified variables */

#define	VAR_TO_ENV	8	/* Place variable in environment */
} Var;

/* Var*Pattern flags */
#define	VAR_SUB_GLOBAL	0x01	/* Apply substitution globally */
#define	VAR_SUB_ONE	0x02	/* Apply substitution to one word */
#define	VAR_SUB_MATCHED	0x04	/* There was a match */
#define	VAR_MATCH_START	0x08	/* Match at start of word */
#define	VAR_MATCH_END	0x10	/* Match at end of word */

typedef struct {
	struct Buffer	*lhs;	/* String to match */
	struct Buffer	*rhs;	/* Replacement string (w/ &'s removed) */

	regex_t			re;
	int			nsub;
	regmatch_t		*matches;

	int	flags;
} VarPattern;

typedef Boolean VarModifyProc(const char *, Boolean, struct Buffer *, void *);

/*
 * var.c
 */
void VarREError(int, regex_t *, const char *);
void Var_Append(const char *, const char *, struct GNode *);
void Var_Delete(const char *, struct GNode *);
void Var_Dump(const struct GNode *);
Boolean Var_Exists(const char *, struct GNode *);
void Var_Init(char **);
size_t Var_Match(const char [], struct GNode *);
char *Var_Parse(const char *, struct GNode *, Boolean, size_t *, Boolean *);
char *Var_Quote(const char *);
void Var_Set(const char *, const char *, struct GNode *);
void Var_SetEnv(const char *, struct GNode *);
struct Buffer *Var_Subst(const char *, struct GNode *, Boolean);
struct Buffer *Var_SubstOnly(const char *, const char *, struct GNode *, Boolean);
char *Var_Value(const char *, struct GNode *, char **);

/*
 * var_modify.c
 */
VarModifyProc VarHead;
VarModifyProc VarMatch;
VarModifyProc VarNoMatch;
VarModifyProc VarRESubstitute;
VarModifyProc VarRoot;
VarModifyProc VarSubstitute;
VarModifyProc VarSuffix;
VarModifyProc VarTail;

#ifdef SYSVVARSUB
VarModifyProc VarSYSVMatch;
#endif

#endif /* var_h_9cccafce */
