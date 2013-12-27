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
 * $FreeBSD$
 */

#ifndef targ_h_6ded1830
#define	targ_h_6ded1830

#include <time.h>

/*
 * The TARG_ constants are used when calling the Targ_FindNode and
 * Targ_FindList functions in targ.c. They simply tell the functions what to
 * do if the desired node(s) is (are) not found. If the TARG_CREATE constant
 * is given, a new, empty node will be created for the target, placed in the
 * table of all targets and its address returned. If TARG_NOCREATE is given,
 * a NULL pointer will be returned.
 */
#define	TARG_CREATE	0x01	  /* create node if not found */
#define	TARG_NOCREATE	0x00	  /* don't create it */

struct GNode;
struct Lst;

void Targ_Init(void);
struct GNode *Targ_NewGN(const char *);
struct GNode *Targ_FindNode(const char *, int);
void Targ_FindList(struct Lst *, struct Lst *, int);
Boolean Targ_Ignore(struct GNode *);
Boolean Targ_Silent(struct GNode *);
Boolean Targ_Precious(struct GNode *);
void Targ_SetMain(struct GNode *);
int Targ_PrintCmd(void *, void *);
char *Targ_FmtTime(time_t);
void Targ_PrintType(int);
void Targ_PrintGraph(int);

#endif /* targ_h_6ded1830 */
