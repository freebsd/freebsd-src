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

#ifndef util_h_b7020fdb
#define	util_h_b7020fdb

#include <sys/types.h>
#include <stdio.h>

/*
 * A boolean type is defined as an integer, not an enum. This allows a
 * boolean argument to be an expression that isn't strictly 0 or 1 valued.
 */

typedef int Boolean;
#ifndef TRUE
#define	TRUE	1
#define	FALSE	0
#endif /* TRUE */

typedef int  ReturnStatus;

#define	SUCCESS		0
#define	FAILURE		1

#define	CONCAT(a,b)	a##b

struct flag2str {
	u_int		flag;
	const char	*str;
};

/*
 * debug control:
 *	There is one bit per module.  It is up to the module what debug
 *	information to print.
 */
#define	DEBUG_ARCH	0x0001
#define	DEBUG_COND	0x0002
#define	DEBUG_DIR	0x0004
#define	DEBUG_GRAPH1	0x0008
#define	DEBUG_GRAPH2	0x0010
#define	DEBUG_JOB	0x0020
#define	DEBUG_MAKE	0x0040
#define	DEBUG_SUFF	0x0080
#define	DEBUG_TARG	0x0100
#define	DEBUG_VAR	0x0200
#define	DEBUG_FOR	0x0400
#define	DEBUG_LOUD	0x0800

#define	DEBUG(module)	(debug & CONCAT(DEBUG_,module))
#define	DEBUGF(module,args)		\
do {						\
	if (DEBUG(module)) {			\
		Debug args ;			\
	}					\
} while (0)
#define	DEBUGM(module, args) do {		\
	if (DEBUG(module)) {			\
		DebugM args;			\
	}					\
    } while (0)

#define	ISDOT(c) ((c)[0] == '.' && (((c)[1] == '\0') || ((c)[1] == '/')))
#define	ISDOTDOT(c) ((c)[0] == '.' && ISDOT(&((c)[1])))

#ifndef MAX
#define	MAX(a, b)  ((a) > (b) ? (a) : (b))
#endif

void Debug(const char *, ...);
void DebugM(const char *, ...);
void Error(const char *, ...);
void Fatal(const char *, ...) __dead2;
void Punt(const char *, ...) __dead2;
void DieHorribly(void) __dead2;
void Finish(int) __dead2;
char *estrdup(const char *);
void *emalloc(size_t);
void *erealloc(void *, size_t);
int eunlink(const char *);
void print_flags(FILE *, const struct flag2str *, u_int);

#endif /* util_h_b7020fdb */
