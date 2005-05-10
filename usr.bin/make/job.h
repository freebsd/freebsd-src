/*-
 * Copyright (c) 1988, 1989, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)job.h	8.1 (Berkeley) 6/6/93
 * $FreeBSD$
 */

#ifndef job_h_4678dfd1
#define	job_h_4678dfd1

/*-
 * job.h --
 *	Definitions pertaining to the running of jobs in parallel mode.
 */

#include <stdio.h>

#include "sprite.h"

struct Buffer;
struct GNode;
struct Lst;

#define	TMPPAT	"/tmp/makeXXXXXXXXXX"

#ifndef USE_KQUEUE
/*
 * The SEL_ constants determine the maximum amount of time spent in select
 * before coming out to see if a child has finished. SEL_SEC is the number of
 * seconds and SEL_USEC is the number of micro-seconds
 */
#define	SEL_SEC		2
#define	SEL_USEC	0
#endif /* !USE_KQUEUE */

extern char *shellPath;
extern char *shellName;
extern int	maxJobs;	/* Number of jobs that may run */

void Shell_Init(void);
void Job_Touch(struct GNode *, Boolean);
Boolean Job_CheckCommands(struct GNode *, void (*abortProc)(const char *, ...));
void Job_CatchChildren(Boolean);
void Job_CatchOutput(int flag);
void Job_Make(struct GNode *);
void Job_Init(int);
Boolean Job_Full(void);
Boolean Job_Empty(void);
ReturnStatus Job_ParseShell(char *);
int Job_Finish(void);
void Job_Wait(void);
void Job_AbortAll(void);

struct Buffer *Cmd_Exec(const char *, const char **);

void Compat_Run(struct Lst *);
int Compat_RunCommand(char *, struct GNode *);

#endif /* job_h_4678dfd1 */
