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

struct Buffer;
struct GNode;
struct List;

/* Variables defined in a global context, e.g in the Makefile itself */
extern struct GNode	*VAR_GLOBAL;

/* Variables defined on the command line */
extern struct GNode	*VAR_CMD;

/*
 * Value returned by Var_Parse when an error is encountered.  It actually
 * points to an empty string, so naive callers needn't worry about it.
 */
extern char		var_Error[];

/*
 * TRUE if environment should be searched for all variables before
 * the global context
 */
extern Boolean		checkEnvFirst;

/* Do old-style variable substitution */
extern Boolean		oldVars;

void Var_Append(const char *, const char *, struct GNode *);
void Var_Delete(const char *, struct GNode *);
void Var_Dump(void);
Boolean Var_Exists(const char *, struct GNode *);
void Var_Init(char **);
size_t Var_Match(const char [], struct GNode *);
char *Var_Parse(const char *, struct GNode *, Boolean, size_t *, Boolean *);
void Var_Print(struct Lst *, Boolean);
void Var_Set(const char *, const char *, struct GNode *);
void Var_SetEnv(const char *, struct GNode *);
struct Buffer *Var_Subst(const char *, struct GNode *, Boolean);
struct Buffer *Var_SubstOnly(const char *, const char *, Boolean);
char *Var_Value(const char *, struct GNode *, char **);

#endif /* var_h_9cccafce */
