/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
#if 0
static char sccsid[] = "@(#)misc.c	8.1 (Berkeley) 6/6/93";
#endif
static const char rcsid[] =
	"$Id: misc.c,v 1.4 1997/10/01 06:30:01 charnier Exp $";
#endif /*not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <err.h>
#include <fts.h>
#include <stdio.h>
#include "mtree.h"
#include "extern.h"

extern int lineno;

typedef struct _key {
	char *name;			/* key name */
	u_int val;			/* value */

#define	NEEDVALUE	0x01
	u_int flags;
} KEY;

/* NB: the following table must be sorted lexically. */
static KEY keylist[] = {
	{"cksum",	F_CKSUM,	NEEDVALUE},
	{"gid",		F_GID,		NEEDVALUE},
	{"gname",	F_GNAME,	NEEDVALUE},
	{"ignore",	F_IGN,		0},
	{"link",	F_SLINK,	NEEDVALUE},
	{"md5digest",	F_MD5,		NEEDVALUE},
	{"mode",	F_MODE,		NEEDVALUE},
	{"nlink",	F_NLINK,	NEEDVALUE},
	{"nochange",	F_NOCHANGE,	0},
	{"size",	F_SIZE,		NEEDVALUE},
	{"time",	F_TIME,		NEEDVALUE},
	{"type",	F_TYPE,		NEEDVALUE},
	{"uid",		F_UID,		NEEDVALUE},
	{"uname",	F_UNAME,	NEEDVALUE},
};

u_int
parsekey(name, needvaluep)
	char *name;
	int *needvaluep;
{
	KEY *k, tmp;
	int keycompare __P((const void *, const void *));

	tmp.name = name;
	k = (KEY *)bsearch(&tmp, keylist, sizeof(keylist) / sizeof(KEY),
	    sizeof(KEY), keycompare);
	if (k == NULL)
		errx(1, "line %d: unknown keyword %s", lineno, name);

	if (needvaluep)
		*needvaluep = k->flags & NEEDVALUE ? 1 : 0;
	return (k->val);
}

int
keycompare(a, b)
	const void *a, *b;
{
	return (strcmp(((KEY *)a)->name, ((KEY *)b)->name));
}
