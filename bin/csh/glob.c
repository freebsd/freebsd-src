/*-
 * Copyright (c) 1980, 1991 The Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)glob.c	5.21 (Berkeley) 6/25/91";
#endif /* not lint */

#include <sys/param.h>
#include <glob.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include "csh.h"
#include "extern.h"

static int noglob, nonomatch;
static int pargsiz, gargsiz;

/*
 * Values for gflag
 */
#define	G_NONE	0		/* No globbing needed			*/
#define	G_GLOB	1		/* string contains *?[] characters	*/
#define	G_CSH	2		/* string contains ~`{ characters	*/

#define	GLOBSPACE	100	/* Alloc increment			*/

#define LBRC '{'
#define RBRC '}'
#define LBRK '['
#define RBRK ']'
#define EOS '\0'

Char  **gargv = NULL;
long    gargc = 0;
Char  **pargv = NULL;
long    pargc = 0;

/*
 * globbing is now done in two stages. In the first pass we expand
 * csh globbing idioms ~`{ and then we proceed doing the normal
 * globbing if needed ?*[
 *
 * Csh type globbing is handled in globexpand() and the rest is
 * handled in glob() which is part of the 4.4BSD libc.
 *
 */
static Char	*globtilde __P((Char **, Char *));
static Char	**libglob __P((Char **));
static Char	**globexpand __P((Char **));
static int	globbrace __P((Char *, Char *, Char ***));
static void	pword __P((void));
static void	psave __P((int));
static void	backeval __P((Char *, bool));


static Char *
globtilde(nv, s)
    Char  **nv, *s;
{
    Char    gbuf[MAXPATHLEN], *gstart, *b, *u, *e;

    gstart = gbuf;
    *gstart++ = *s++;
    u = s;
    for (b = gstart, e = &gbuf[MAXPATHLEN - 1]; *s && *s != '/' && b < e;
	 *b++ = *s++);
    *b = EOS;
    if (gethdir(gstart)) {
	blkfree(nv);
	if (*gstart)
	    stderror(ERR_UNKUSER, short2str(gstart));
	else
	    stderror(ERR_NOHOME);
    }
    b = &gstart[Strlen(gstart)];
    while (*s)
	*b++ = *s++;
    *b = EOS;
    --u;
    xfree((ptr_t) u);
    return (Strsave(gstart));
}

static int
globbrace(s, p, bl)
    Char   *s, *p, ***bl;
{
    int     i, len;
    Char   *pm, *pe, *lm, *pl;
    Char  **nv, **vl;
    Char    gbuf[MAXPATHLEN];
    int     size = GLOBSPACE;

    nv = vl = (Char **) xmalloc((size_t) sizeof(Char *) * size);
    *vl = NULL;

    len = 0;
    /* copy part up to the brace */
    for (lm = gbuf, p = s; *p != LBRC; *lm++ = *p++)
	continue;

    /* check for balanced braces */
    for (i = 0, pe = ++p; *pe; pe++)
	if (*pe == LBRK) {
	    /* Ignore everything between [] */
	    for (++pe; *pe != RBRK && *pe != EOS; pe++)
		continue;
	    if (*pe == EOS) {
		blkfree(nv);
		return (-LBRK);
	    }
	}
	else if (*pe == LBRC)
	    i++;
	else if (*pe == RBRC) {
	    if (i == 0)
		break;
	    i--;
	}

    if (i != 0) {
	blkfree(nv);
	return (-LBRC);
    }

    for (i = 0, pl = pm = p; pm <= pe; pm++)
	switch (*pm) {
	case LBRK:
	    for (++pm; *pm != RBRK && *pm != EOS; pm++)
		continue;
	    if (*pm == EOS) {
		*vl = NULL;
		blkfree(nv);
		return (-RBRK);
	    }
	    break;
	case LBRC:
	    i++;
	    break;
	case RBRC:
	    if (i) {
		i--;
		break;
	    }
	    /* FALLTHROUGH */
	case ',':
	    if (i && *pm == ',')
		break;
	    else {
		Char    savec = *pm;

		*pm = EOS;
		(void) Strcpy(lm, pl);
		(void) Strcat(gbuf, pe + 1);
		*pm = savec;
		*vl++ = Strsave(gbuf);
		len++;
		pl = pm + 1;
		if (vl == &nv[size]) {
		    size += GLOBSPACE;
		    nv = (Char **) xrealloc((ptr_t) nv, (size_t)
					    size * sizeof(Char *));
		    vl = &nv[size - GLOBSPACE];
		}
	    }
	    break;
	}
    *vl = NULL;
    *bl = nv;
    return (len);
}

static Char **
globexpand(v)
    Char  **v;
{
    Char   *s;
    Char  **nv, **vl, **el;
    int     size = GLOBSPACE;


    nv = vl = (Char **) xmalloc((size_t) sizeof(Char *) * size);
    *vl = NULL;

    /*
     * Step 1: expand backquotes.
     */
    while (s = *v++) {
	if (Strchr(s, '`')) {
	    int     i;

	    (void) dobackp(s, 0);
	    for (i = 0; i < pargc; i++) {
		*vl++ = pargv[i];
		if (vl == &nv[size]) {
		    size += GLOBSPACE;
		    nv = (Char **) xrealloc((ptr_t) nv,
					    (size_t) size * sizeof(Char *));
		    vl = &nv[size - GLOBSPACE];
		}
	    }
	    xfree((ptr_t) pargv);
	    pargv = NULL;
	}
	else {
	    *vl++ = Strsave(s);
	    if (vl == &nv[size]) {
		size += GLOBSPACE;
		nv = (Char **) xrealloc((ptr_t) nv, (size_t)
					size * sizeof(Char *));
		vl = &nv[size - GLOBSPACE];
	    }
	}
    }
    *vl = NULL;

    if (noglob)
	return (nv);

    /*
     * Step 2: expand braces
     */
    el = vl;
    vl = nv;
    for (s = *vl; s; s = *++vl) {
	Char   *b;
	Char  **vp, **bp;

	if (b = Strchr(s, LBRC)) {
	    Char  **bl;
	    int     len;

	    if ((len = globbrace(s, b, &bl)) < 0) {
		blkfree(nv);
		stderror(ERR_MISSING, -len);
	    }
	    xfree((ptr_t) s);
	    if (len == 1) {
		*vl-- = *bl;
		xfree((ptr_t) bl);
		continue;
	    }
	    len = blklen(bl);
	    if (&el[len] >= &nv[size]) {
		int     l, e;

		l = &el[len] - &nv[size];
		size += GLOBSPACE > l ? GLOBSPACE : l;
		l = vl - nv;
		e = el - nv;
		nv = (Char **) xrealloc((ptr_t) nv, (size_t)
					size * sizeof(Char *));
		vl = nv + l;
		el = nv + e;
	    }
	    vp = vl--;
	    *vp = *bl;
	    len--;
	    for (bp = el; bp != vp; bp--)
		bp[len] = *bp;
	    el += len;
	    vp++;
	    for (bp = bl + 1; *bp; *vp++ = *bp++)
		continue;
	    xfree((ptr_t) bl);
	}

    }

    /*
     * Step 3: expand ~
     */
    vl = nv;
    for (s = *vl; s; s = *++vl)
	if (*s == '~')
	    *vl = globtilde(nv, s);
    vl = nv;
    return (vl);
}

static Char *
handleone(str, vl, action)
    Char   *str, **vl;
    int     action;
{

    Char   *cp, **vlp = vl;

    switch (action) {
    case G_ERROR:
	setname(short2str(str));
	blkfree(vl);
	stderror(ERR_NAME | ERR_AMBIG);
	break;
    case G_APPEND:
	trim(vlp);
	str = Strsave(*vlp++);
	do {
	    cp = Strspl(str, STRspace);
	    xfree((ptr_t) str);
	    str = Strspl(cp, *vlp);
	    xfree((ptr_t) cp);
	}
	while (*++vlp);
	blkfree(vl);
	break;
    case G_IGNORE:
	str = Strsave(strip(*vlp));
	blkfree(vl);
	break;
    }
    return (str);
}

static Char **
libglob(vl)
    Char  **vl;
{
    int     gflgs = GLOB_QUOTE | GLOB_NOCHECK, badmagic = 0, goodmagic = 0;
    glob_t  globv;
    char   *ptr;

    globv.gl_offs = 0;
    globv.gl_pathv = 0;
    globv.gl_pathc = 0;
    nonomatch = adrof(STRnonomatch) != 0;
    do {
	ptr = short2qstr(*vl);
	switch (glob(ptr, gflgs, 0, &globv)) {
	case GLOB_ABEND:
	    setname(ptr);
	    stderror(ERR_NAME | ERR_GLOB);
	    /* NOTREACHED */
	case GLOB_NOSPACE:
	    stderror(ERR_NOMEM);
	    /* NOTREACHED */
	default:
	    break;
	}
	if (!nonomatch && (globv.gl_matchc == 0) &&
	    (globv.gl_flags & GLOB_MAGCHAR)) {
	    badmagic = 1;
	    globv.gl_pathc--;
            free(globv.gl_pathv[globv.gl_pathc]);
	    globv.gl_pathv[globv.gl_pathc] = (char *)0;
	} else
	   if (!nonomatch && (globv.gl_matchc > 0) &&
		(globv.gl_flags & GLOB_MAGCHAR))
		goodmagic = 1;
	gflgs |= GLOB_APPEND;
    }
    while (*++vl);
    if (badmagic && !goodmagic) {
	globfree(&globv);
	return (NULL);
    }
    vl = blk2short(globv.gl_pathv);
    globfree(&globv);
    return (vl);
}

Char   *
globone(str, action)
    Char   *str;
    int     action;
{

    Char   *v[2], **vl, **vo;

    noglob = adrof(STRnoglob) != 0;
    gflag = 0;
    v[0] = str;
    v[1] = 0;
    tglob(v);
    if (gflag == G_NONE)
	return (strip(Strsave(str)));

    if (gflag & G_CSH) {
	/*
	 * Expand back-quote, tilde and brace
	 */
	vo = globexpand(v);
	if (noglob || (gflag & G_GLOB) == 0) {
	    if (vo[0] == NULL) {
		xfree((ptr_t) vo);
		return (Strsave(STRNULL));
	    }
	    if (vo[1] != NULL)
		return (handleone(str, vo, action));
	    else {
		str = strip(vo[0]);
		xfree((ptr_t) vo);
		return (str);
	    }
	}
    }
    else if (noglob || (gflag & G_GLOB) == 0)
	return (strip(Strsave(str)));
    else
	vo = v;

    vl = libglob(vo);
    if (gflag & G_CSH)
	blkfree(vo);
    if (vl == NULL) {
	setname(short2str(str));
	stderror(ERR_NAME | ERR_NOMATCH);
    }
    if (vl[0] == NULL) {
	xfree((ptr_t) vl);
	return (Strsave(STRNULL));
    }
    if (vl[1] != NULL)
	return (handleone(str, vl, action));
    else {
	str = strip(*vl);
	xfree((ptr_t) vl);
	return (str);
    }
}

Char  **
globall(v)
    Char  **v;
{
    Char  **vl, **vo;

    if (!v || !v[0]) {
	gargv = saveblk(v);
	gargc = blklen(gargv);
	return (gargv);
    }

    noglob = adrof(STRnoglob) != 0;

    if (gflag & G_CSH)
	/*
	 * Expand back-quote, tilde and brace
	 */
	vl = vo = globexpand(v);
    else
	vl = vo = saveblk(v);

    if (!noglob && (gflag & G_GLOB)) {
	vl = libglob(vo);
	if (gflag & G_CSH)
	    blkfree(vo);
    }

    gargc = vl ? blklen(vl) : 0;
    return (gargv = vl);
}

void
ginit()
{
    gargsiz = GLOBSPACE;
    gargv = (Char **) xmalloc((size_t) sizeof(Char *) * gargsiz);
    gargv[0] = 0;
    gargc = 0;
}

void
rscan(t, f)
    register Char **t;
    void    (*f) ();
{
    register Char *p;

    while (p = *t++)
	while (*p)
	    (*f) (*p++);
}

void
trim(t)
    register Char **t;
{
    register Char *p;

    while (p = *t++)
	while (*p)
	    *p++ &= TRIM;
}

void
tglob(t)
    register Char **t;
{
    register Char *p, c;

    while (p = *t++) {
	if (*p == '~' || *p == '=')
	    gflag |= G_CSH;
	else if (*p == '{' &&
		 (p[1] == '\0' || p[1] == '}' && p[2] == '\0'))
	    continue;
	while (c = *p++)
	    if (isglob(c))
		gflag |= (c == '{' || c == '`') ? G_CSH : G_GLOB;
    }
}

/*
 * Command substitute cp.  If literal, then this is a substitution from a
 * << redirection, and so we should not crunch blanks and tabs, separating
 * words only at newlines.
 */
Char  **
dobackp(cp, literal)
    Char   *cp;
    bool    literal;
{
    register Char *lp, *rp;
    Char   *ep, word[MAXPATHLEN];

    if (pargv) {
	abort();
	blkfree(pargv);
    }
    pargsiz = GLOBSPACE;
    pargv = (Char **) xmalloc((size_t) sizeof(Char *) * pargsiz);
    pargv[0] = NULL;
    pargcp = pargs = word;
    pargc = 0;
    pnleft = MAXPATHLEN - 4;
    for (;;) {
	for (lp = cp; *lp != '`'; lp++) {
	    if (*lp == 0) {
		if (pargcp != pargs)
		    pword();
		return (pargv);
	    }
	    psave(*lp);
	}
	lp++;
	for (rp = lp; *rp && *rp != '`'; rp++)
	    if (*rp == '\\') {
		rp++;
		if (!*rp)
		    goto oops;
	    }
	if (!*rp)
    oops:  stderror(ERR_UNMATCHED, '`');
	ep = Strsave(lp);
	ep[rp - lp] = 0;
	backeval(ep, literal);
	cp = rp + 1;
    }
}

static void
backeval(cp, literal)
    Char   *cp;
    bool    literal;
{
    register int icnt, c;
    register Char *ip;
    struct command faket;
    bool    hadnl;
    int     pvec[2], quoted;
    Char   *fakecom[2], ibuf[BUFSIZ];
    char    tibuf[BUFSIZ];

    hadnl = 0;
    icnt = 0;
    quoted = (literal || (cp[0] & QUOTE)) ? QUOTE : 0;
    faket.t_dtyp = NODE_COMMAND;
    faket.t_dflg = 0;
    faket.t_dlef = 0;
    faket.t_drit = 0;
    faket.t_dspr = 0;
    faket.t_dcom = fakecom;
    fakecom[0] = STRfakecom1;
    fakecom[1] = 0;

    /*
     * We do the psave job to temporarily change the current job so that the
     * following fork is considered a separate job.  This is so that when
     * backquotes are used in a builtin function that calls glob the "current
     * job" is not corrupted.  We only need one level of pushed jobs as long as
     * we are sure to fork here.
     */
    psavejob();

    /*
     * It would be nicer if we could integrate this redirection more with the
     * routines in sh.sem.c by doing a fake execute on a builtin function that
     * was piped out.
     */
    mypipe(pvec);
    if (pfork(&faket, -1) == 0) {
	struct wordent paraml;
	struct command *t;

	(void) close(pvec[0]);
	(void) dmove(pvec[1], 1);
	(void) dmove(SHDIAG, 2);
	initdesc();
	/*
	 * Bugfix for nested backquotes by Michael Greim <greim@sbsvax.UUCP>,
	 * posted to comp.bugs.4bsd 12 Sep. 1989.
	 */
	if (pargv)		/* mg, 21.dec.88 */
	    blkfree(pargv), pargv = 0, pargsiz = 0;
	/* mg, 21.dec.88 */
	arginp = cp;
	while (*cp)
	    *cp++ &= TRIM;
	(void) lex(&paraml);
	if (seterr)
	    stderror(ERR_OLD);
	alias(&paraml);
	t = syntax(paraml.next, &paraml, 0);
	if (seterr)
	    stderror(ERR_OLD);
	if (t)
	    t->t_dflg |= F_NOFORK;
	(void) signal(SIGTSTP, SIG_IGN);
	(void) signal(SIGTTIN, SIG_IGN);
	(void) signal(SIGTTOU, SIG_IGN);
	execute(t, -1, NULL, NULL);
	exitstat();
    }
    xfree((ptr_t) cp);
    (void) close(pvec[1]);
    c = 0;
    ip = NULL;
    do {
	int     cnt = 0;

	for (;;) {
	    if (icnt == 0) {
		int     i;

		ip = ibuf;
		do
		    icnt = read(pvec[0], tibuf, BUFSIZ);
		while (icnt == -1 && errno == EINTR);
		if (icnt <= 0) {
		    c = -1;
		    break;
		}
		for (i = 0; i < icnt; i++)
		    ip[i] = (unsigned char) tibuf[i];
	    }
	    if (hadnl)
		break;
	    --icnt;
	    c = (*ip++ & TRIM);
	    if (c == 0)
		break;
	    if (c == '\n') {
		/*
		 * Continue around the loop one more time, so that we can eat
		 * the last newline without terminating this word.
		 */
		hadnl = 1;
		continue;
	    }
	    if (!quoted && (c == ' ' || c == '\t'))
		break;
	    cnt++;
	    psave(c | quoted);
	}
	/*
	 * Unless at end-of-file, we will form a new word here if there were
	 * characters in the word, or in any case when we take text literally.
	 * If we didn't make empty words here when literal was set then we
	 * would lose blank lines.
	 */
	if (c != -1 && (cnt || literal))
	    pword();
	hadnl = 0;
    } while (c >= 0);
    (void) close(pvec[0]);
    pwait();
    prestjob();
}

static void
psave(c)
    int    c;
{
    if (--pnleft <= 0)
	stderror(ERR_WTOOLONG);
    *pargcp++ = c;
}

static void
pword()
{
    psave(0);
    if (pargc == pargsiz - 1) {
	pargsiz += GLOBSPACE;
	pargv = (Char **) xrealloc((ptr_t) pargv,
				   (size_t) pargsiz * sizeof(Char *));
    }
    pargv[pargc++] = Strsave(pargs);
    pargv[pargc] = NULL;
    pargcp = pargs;
    pnleft = MAXPATHLEN - 4;
}

int
Gmatch(string, pattern)
    register Char *string, *pattern;
{
    register Char stringc, patternc;
    int     match;
    Char    rangec;

    for (;; ++string) {
	stringc = *string & TRIM;
	patternc = *pattern++;
	switch (patternc) {
	case 0:
	    return (stringc == 0);
	case '?':
	    if (stringc == 0)
		return (0);
	    break;
	case '*':
	    if (!*pattern)
		return (1);
	    while (*string)
		if (Gmatch(string++, pattern))
		    return (1);
	    return (0);
	case '[':
	    match = 0;
	    while (rangec = *pattern++) {
		if (rangec == ']')
		    if (match)
			break;
		    else
			return (0);
		if (match)
		    continue;
		if (rangec == '-' && *(pattern - 2) != '[' && *pattern != ']') {
		    match = (stringc <= (*pattern & TRIM) &&
			     (*(pattern - 2) & TRIM) <= stringc);
		    pattern++;
		}
		else
		    match = (stringc == rangec);
	    }
	    if (rangec == 0)
		stderror(ERR_NAME | ERR_MISSING, ']');
	    break;
	default:
	    if ((patternc & TRIM) != stringc)
		return (0);
	    break;

	}
    }
}

void
Gcat(s1, s2)
    Char   *s1, *s2;
{
    register Char *p, *q;
    int     n;

    for (p = s1; *p++;);
    for (q = s2; *q++;);
    n = (p - s1) + (q - s2) - 1;
    if (++gargc >= gargsiz) {
	gargsiz += GLOBSPACE;
	gargv = (Char **) xrealloc((ptr_t) gargv,
				   (size_t) gargsiz * sizeof(Char *));
    }
    gargv[gargc] = 0;
    p = gargv[gargc - 1] = (Char *) xmalloc((size_t) n * sizeof(Char));
    for (q = s1; *p++ = *q++;);
    for (p--, q = s2; *p++ = *q++;);
}

#ifdef FILEC
int
sortscmp(a, b)
    register Char **a, **b;
{
#if defined(NLS) && !defined(NOSTRCOLL)
    char    buf[2048];

#endif

    if (!a)			/* check for NULL */
	return (b ? 1 : 0);
    if (!b)
	return (-1);

    if (!*a)			/* check for NULL */
	return (*b ? 1 : 0);
    if (!*b)
	return (-1);

#if defined(NLS) && !defined(NOSTRCOLL)
    (void) strcpy(buf, short2str(*a));
    return ((int) strcoll(buf, short2str(*b)));
#else
    return ((int) Strcmp(*a, *b));
#endif
}
#endif /* FILEC */
