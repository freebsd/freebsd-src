/*-
 * Copyright (c) 1980, 1991, 1993
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
 *
 *	$FreeBSD$
 */

#ifndef lint
static char sccsid[] = "@(#)misc.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

#include <sys/param.h>
#include <stdlib.h>
#include <unistd.h>
#if __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include "csh.h"
#include "extern.h"

static int	renum __P((int, int));

int
any(s, c)
    register char *s;
    register int c;
{
    if (!s)
	return (0);		/* Check for nil pointer */
    while (*s)
	if (*s++ == c)
	    return (1);
    return (0);
}

void
setzero(cp, i)
    char   *cp;
    int     i;
{
    if (i != 0)
	do
	    *cp++ = 0;
	while (--i);
}

char   *
strsave(s)
    register char *s;
{
    char   *n;
    register char *p;

    if (s == NULL)
	s = "";
    for (p = s; *p++;)
	continue;
    n = p = (char *) xmalloc((size_t) ((p - s) * sizeof(char)));
    while ((*p++ = *s++) != '\0')
	continue;
    return (n);
}

Char  **
blkend(up)
    register Char **up;
{

    while (*up)
	up++;
    return (up);
}


void
blkpr(fp, av)
    FILE *fp;
    register Char **av;
{

    for (; *av; av++) {
	(void) fprintf(fp, "%s", vis_str(*av));
	if (av[1])
	    (void) fprintf(fp, " ");
    }
}

int
blklen(av)
    register Char **av;
{
    register int i = 0;

    while (*av++)
	i++;
    return (i);
}

Char  **
blkcpy(oav, bv)
    Char  **oav;
    register Char **bv;
{
    register Char **av = oav;

    while ((*av++ = *bv++) != NULL)
	continue;
    return (oav);
}

Char  **
blkcat(up, vp)
    Char  **up, **vp;
{

    (void) blkcpy(blkend(up), vp);
    return (up);
}

void
blkfree(av0)
    Char  **av0;
{
    register Char **av = av0;

    if (!av0)
	return;
    for (; *av; av++)
	xfree((ptr_t) * av);
    xfree((ptr_t) av0);
}

Char  **
saveblk(v)
    register Char **v;
{
    register Char **newv =
    (Char **) xcalloc((size_t) (blklen(v) + 1), sizeof(Char **));
    Char  **onewv = newv;

    while (*v)
	*newv++ = Strsave(*v++);
    return (onewv);
}

#ifdef NOTUSED
char   *
strstr(s, t)
    register char *s, *t;
{
    do {
	register char *ss = s;
	register char *tt = t;

	do
	    if (*tt == '\0')
		return (s);
	while (*ss++ == *tt++);
    } while (*s++ != '\0');
    return (NULL);
}

#endif /* NOTUSED */

#ifndef SHORT_STRINGS
char   *
strspl(cp, dp)
    char   *cp, *dp;
{
    char   *ep;
    register char *p, *q;

    if (!cp)
	cp = "";
    if (!dp)
	dp = "";
    for (p = cp; *p++;)
	continue;
    for (q = dp; *q++;)
	continue;
    ep = (char *) xmalloc((size_t) (((p - cp) + (q - dp) - 1) * sizeof(char)));
    for (p = ep, q = cp; *p++ = *q++;)
	continue;
    for (p--, q = dp; *p++ = *q++;)
	continue;
    return (ep);
}

#endif

Char  **
blkspl(up, vp)
    register Char **up, **vp;
{
    register Char **wp =
    (Char **) xcalloc((size_t) (blklen(up) + blklen(vp) + 1),
		      sizeof(Char **));

    (void) blkcpy(wp, up);
    return (blkcat(wp, vp));
}

Char
lastchr(cp)
    register Char *cp;
{

    if (!cp)
	return (0);
    if (!*cp)
	return (0);
    while (cp[1])
	cp++;
    return (*cp);
}

/*
 * This routine is called after an error to close up
 * any units which may have been left open accidentally.
 */
void
closem()
{
    register int f, flimit;

    for (f = 0, flimit = getdtablesize(); f < flimit; f++)
	if (f != SHIN && f != SHOUT && f != SHERR && f != OLDSTD &&
	    f != FSHTTY)
	    (void) close(f);
}

void
donefds()
{
    (void) close(0);
    (void) close(1);
    (void) close(2);

    didfds = 0;
}

/*
 * Move descriptor i to j.
 * If j is -1 then we just want to get i to a safe place,
 * i.e. to a unit > 2.  This also happens in dcopy.
 */
int
dmove(i, j)
    register int i, j;
{

    if (i == j || i < 0)
	return (i);
    if (j >= 0) {
	(void) dup2(i, j);
	if (j != i)
	    (void) close(i);
	return (j);
    }
    j = dcopy(i, j);
    if (j != i)
	(void) close(i);
    return (j);
}

int
dcopy(i, j)
    register int i, j;
{

    if (i == j || i < 0 || (j < 0 && i > 2))
	return (i);
    if (j >= 0) {
	(void) dup2(i, j);
	return (j);
    }
    (void) close(j);
    return (renum(i, j));
}

static int
renum(i, j)
    register int i, j;
{
    register int k = dup(i);

    if (k < 0)
	return (-1);
    if (j == -1 && k > 2)
	return (k);
    if (k != j) {
	j = renum(k, j);
	(void) close(k);
	return (j);
    }
    return (k);
}

/*
 * Left shift a command argument list, discarding
 * the first c arguments.  Used in "shift" commands
 * as well as by commands like "repeat".
 */
void
lshift(v, c)
    register Char **v;
    register int c;
{
    register Char **u;

    for (u = v; *u && --c >= 0; u++)
	xfree((ptr_t) *u);
    (void) blkcpy(v, u);
}

int
number(cp)
    Char   *cp;
{
    if (!cp)
	return(0);
    if (*cp == '-') {
	cp++;
	if (!Isdigit(*cp))
	    return (0);
	cp++;
    }
    while (*cp && Isdigit(*cp))
	cp++;
    return (*cp == 0);
}

Char  **
copyblk(v)
    register Char **v;
{
    Char  **nv = (Char **) xcalloc((size_t) (blklen(v) + 1), sizeof(Char **));

    return (blkcpy(nv, v));
}

#ifndef SHORT_STRINGS
char   *
strend(cp)
    register char *cp;
{
    if (!cp)
	return (cp);
    while (*cp)
	cp++;
    return (cp);
}

#endif /* SHORT_STRINGS */

Char   *
strip(cp)
    Char   *cp;
{
    register Char *dp = cp;

    if (!cp)
	return (cp);
    while ((*dp++ &= TRIM) != '\0')
	continue;
    return (cp);
}

void
udvar(name)
    Char   *name;
{

    setname(vis_str(name));
    stderror(ERR_NAME | ERR_UNDVAR);
}

int
prefix(sub, str)
    register Char *sub, *str;
{

    for (;;) {
	if (*sub == 0)
	    return (1);
	if (*str == 0)
	    return (0);
	if (*sub++ != *str++)
	    return (0);
    }
}
