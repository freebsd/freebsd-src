/* $Header: /src/pub/tcsh/sh.glob.c,v 3.47 2000/11/11 23:03:37 christos Exp $ */
/*
 * sh.glob.c: Regular expression expansion
 */
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
#include "sh.h"

RCSID("$Id: sh.glob.c,v 3.47 2000/11/11 23:03:37 christos Exp $")

#include "tc.h"

#include "glob.h"

static int noglob;
static int pargsiz, gargsiz;

/*
 * Values for gflag
 */
#define	G_NONE	0		/* No globbing needed			*/
#define	G_GLOB	1		/* string contains *?[] characters	*/
#define	G_CSH	2		/* string contains ~`{ characters	*/

#define	GLOBSPACE	100	/* Alloc increment			*/
#define LONGBSIZE	10240	/* Backquote expansion buffer size	*/


#define LBRC '{'
#define RBRC '}'
#define LBRK '['
#define RBRK ']'
#define EOS '\0'

Char  **gargv = NULL;
int     gargc = 0;
Char  **pargv = NULL;
static int pargc = 0;

/*
 * globbing is now done in two stages. In the first pass we expand
 * csh globbing idioms ~`{ and then we proceed doing the normal
 * globbing if needed ?*[
 *
 * Csh type globbing is handled in globexpand() and the rest is
 * handled in glob() which is part of the 4.4BSD libc.
 *
 */
static	Char	 *globtilde	__P((Char **, Char *));
static	Char     *handleone	__P((Char *, Char **, int));
static	Char	**libglob	__P((Char **));
static	Char	**globexpand	__P((Char **));
static	int	  globbrace	__P((Char *, Char *, Char ***));
static  void	  expbrace	__P((Char ***, Char ***, int));
static  int	  pmatch	__P((Char *, Char *, Char **));
static	void	  pword		__P((int));
static	void	  psave		__P((int));
static	void	  backeval	__P((Char *, bool));

static Char *
globtilde(nv, s)
    Char  **nv, *s;
{
    Char    gbuf[BUFSIZE], *gstart, *b, *u, *e;
#ifdef apollo
    int slash;
#endif

    gstart = gbuf;
    *gstart++ = *s++;
    u = s;
    for (b = gstart, e = &gbuf[BUFSIZE - 1]; 
	 *s && *s != '/' && *s != ':' && b < e;
	 *b++ = *s++)
	continue;
    *b = EOS;
    if (gethdir(gstart)) {
	if (adrof(STRnonomatch))
	    return (--u);
	blkfree(nv);
	if (*gstart)
	    stderror(ERR_UNKUSER, short2str(gstart));
	else
	    stderror(ERR_NOHOME);
    }
    b = &gstart[Strlen(gstart)];
#ifdef apollo
    slash = gstart[0] == '/' && gstart[1] == '\0';
#endif
    while (*s)
	*b++ = *s++;
    *b = EOS;
    --u;
    xfree((ptr_t) u);
#ifdef apollo
    if (slash && gstart[1] == '/')
	gstart++;
#endif
    return (Strsave(gstart));
}

Char *
globequal(new, old)
    Char *new, *old;
{
    int     dig;
    Char    *b, *d;

    /*
     * kfk - 17 Jan 1984 - stack hack allows user to get at arbitrary dir names
     * in stack. PWP: let =foobar pass through (for X windows)
     */
    if (old[1] == '-' && (old[2] == '\0' || old[2] == '/')) {
	/* =- */
	dig = -1;
	b = &old[2];
    }
    else if (Isdigit(old[1])) {
	/* =<number> */
	dig = old[1] - '0';
	for (b = &old[2]; Isdigit(*b); b++)
	    dig = dig * 10 + (*b - '0');
	if (*b != '\0' && *b != '/')
	    /* =<number>foobar */
	    return old;
    }
    else
	/* =foobar */
	return old;

    if (!getstakd(new, dig))
	return NULL;

    /* Copy the rest of the string */
    for (d = &new[Strlen(new)]; 
	 d < &new[BUFSIZE - 1] && (*d++ = *b++) != '\0';)
	continue;
    *d = '\0';

    return new;
}

static int
globbrace(s, p, bl)
    Char   *s, *p, ***bl;
{
    int     i, len;
    Char   *pm, *pe, *lm, *pl;
    Char  **nv, **vl;
    Char    gbuf[BUFSIZE];
    int     size = GLOBSPACE;

    nv = vl = (Char **) xmalloc((size_t) (sizeof(Char *) * size));
    *vl = NULL;

    len = 0;
    /* copy part up to the brace */
    for (lm = gbuf, p = s; *p != LBRC; *lm++ = *p++)
	continue;

    /* check for balanced braces */
    for (i = 0, pe = ++p; *pe; pe++)
#ifdef DSPMBYTE
	if (Ismbyte1(*pe) && *(pe + 1) != EOS)
	    pe ++;
	else
#endif /* DSPMBYTE */
	if (*pe == LBRK) {
	    /* Ignore everything between [] */
	    for (++pe; *pe != RBRK && *pe != EOS; pe++)
#ifdef DSPMBYTE
	      if (Ismbyte1(*pe) && *(pe + 1) != EOS)
		pe ++;
	      else
#endif /* DSPMBYTE */
		continue;
	    if (*pe == EOS) {
		blkfree(nv);
		return (-RBRK);
	    }
	}
	else if (*pe == LBRC)
	    i++;
	else if (*pe == RBRC) {
	    if (i == 0)
		break;
	    i--;
	}

    if (i != 0 || *pe == '\0') {
	blkfree(nv);
	return (-RBRC);
    }

    for (i = 0, pl = pm = p; pm <= pe; pm++)
#ifdef DSPMBYTE
	if (Ismbyte1(*pm) && pm + 1 <= pe)
	    pm ++;
	else
#endif /* DSPMBYTE */
	switch (*pm) {
	case LBRK:
	    for (++pm; *pm != RBRK && *pm != EOS; pm++)
#ifdef DSPMBYTE
	      if (Ismbyte1(*pm) && *(pm + 1) != EOS)
		pm ++;
	      else
#endif /* DSPMBYTE */
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
		    nv = (Char **) xrealloc((ptr_t) nv, 
					    (size_t) (size * sizeof(Char *)));
		    vl = &nv[size - GLOBSPACE];
		}
	    }
	    break;
	default:
	    break;
	}
    *vl = NULL;
    *bl = nv;
    return (len);
}


static void
expbrace(nvp, elp, size)
    Char ***nvp, ***elp;
    int size;
{
    Char **vl, **el, **nv, *s;

    vl = nv = *nvp;
    if (elp != NULL)
	el = *elp;
    else
	for (el = vl; *el; el++)
	    continue;

    for (s = *vl; s; s = *++vl) {
	Char   *b;
	Char  **vp, **bp;

	/* leave {} untouched for find */
	if (s[0] == '{' && (s[1] == '\0' || (s[1] == '}' && s[2] == '\0')))
	    continue;
	if ((b = Strchr(s, '{')) != NULL) {
	    Char  **bl;
	    int     len;

#if defined (DSPMBYTE)
	    if (b != s && Ismbyte2(*b) && Ismbyte1(*(b-1))) {
		/* The "{" is the 2nd byte of a MB character */
		continue;
	    }
#endif /* DSPMBYTE */
	    if ((len = globbrace(s, b, &bl)) < 0) {
		xfree((ptr_t) nv);
		stderror(ERR_MISSING, -len);
	    }
	    xfree((ptr_t) s);
	    if (len == 1) {
		*vl-- = *bl;
		xfree((ptr_t) bl);
		continue;
	    }
	    if (&el[len] >= &nv[size]) {
		int     l, e;
		l = (int) (&el[len] - &nv[size]);
		size += GLOBSPACE > l ? GLOBSPACE : l;
		l = (int) (vl - nv);
		e = (int) (el - nv);
		nv = (Char **) xrealloc((ptr_t) nv, 
					(size_t) (size * sizeof(Char *)));
		vl = nv + l;
		el = nv + e;
	    }
	    /* nv vl   el     bl
	     * |  |    |      |
	     * -.--..--	      x--
	     *   |            len
	     *   vp
	     */
	    vp = vl--;
	    *vp = *bl;
	    len--;
	    for (bp = el; bp != vp; bp--)
		bp[len] = *bp;
	    el += len;
	    /* nv vl    el bl
	     * |  |     |  |
	     * -.-x  ---    --
	     *   |len
	     *   vp
	     */
	    vp++;
	    for (bp = bl + 1; *bp; *vp++ = *bp++)
		continue;
	    xfree((ptr_t) bl);
	}

    }
    if (elp != NULL)
	*elp = el;
    *nvp = nv;
}

static Char **
globexpand(v)
    Char  **v;
{
    Char   *s;
    Char  **nv, **vl, **el;
    int     size = GLOBSPACE;


    nv = vl = (Char **) xmalloc((size_t) (sizeof(Char *) * size));
    *vl = NULL;

    /*
     * Step 1: expand backquotes.
     */
    while ((s = *v++) != '\0') {
	if (Strchr(s, '`')) {
	    int     i;

	    (void) dobackp(s, 0);
	    for (i = 0; i < pargc; i++) {
		*vl++ = pargv[i];
		if (vl == &nv[size]) {
		    size += GLOBSPACE;
		    nv = (Char **) xrealloc((ptr_t) nv,
					    (size_t) (size * sizeof(Char *)));
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
		nv = (Char **) xrealloc((ptr_t) nv, 
					(size_t) (size * sizeof(Char *)));
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
    expbrace(&nv, &el, size);


    /*
     * Step 3: expand ~ =
     */
    vl = nv;
    for (s = *vl; s; s = *++vl)
	switch (*s) {
	    Char gp[BUFSIZE], *ns;
	case '~':
	    *vl = globtilde(nv, s);
	    break;
	case '=':
	    if ((ns = globequal(gp, s)) == NULL) {
		if (!adrof(STRnonomatch)) {
		    /* Error */
		    blkfree(nv);
		    stderror(ERR_DEEP);
		}
	    }
	    if (ns && ns != s) {
		/* Expansion succeeded */
		xfree((ptr_t) s);
		*vl = Strsave(gp);
	    }
	    break;
	default:
	    break;
	}
    vl = nv;

    /*
     * Step 4: expand .. if the variable symlinks==expand is set
     */
    if ( symlinks == SYM_EXPAND )
	for (s = *vl; s; s = *++vl) {
	    char *path = short2str(s);
	    if (strstr(path, "..") != NULL && access(path, F_OK) == 0) {
		*vl = dnormalize(s, 1);
		xfree((ptr_t) s);
	    }
	}
    vl = nv;

    return (vl);
}

static Char *
handleone(str, vl, action)
    Char   *str, **vl;
    int     action;
{

    Char   **vlp = vl;
    int chars;
    Char **t, *p, *strp;

    switch (action) {
    case G_ERROR:
	setname(short2str(str));
	blkfree(vl);
	stderror(ERR_NAME | ERR_AMBIG);
	break;
    case G_APPEND:
	chars = 0;
	for (t = vlp; (p = *t++) != '\0'; chars++)
	    while (*p++)
		chars++;
	str = (Char *)xmalloc((size_t)(chars * sizeof(Char)));
	for (t = vlp, strp = str; (p = *t++) != '\0'; chars++) {
	    while (*p)
		 *strp++ = *p++ & TRIM;
	    *strp++ = ' ';
	}
	*--strp = '\0';
	blkfree(vl);
	break;
    case G_IGNORE:
	str = Strsave(strip(*vlp));
	blkfree(vl);
	break;
    default:
	break;
    }
    return (str);
}

static Char **
libglob(vl)
    Char  **vl;
{
    int     gflgs = GLOB_QUOTE | GLOB_NOMAGIC | GLOB_ALTNOT;
    glob_t  globv;
    char   *ptr;
    int     nonomatch = adrof(STRnonomatch) != 0, magic = 0, match = 0;

    if (!vl || !vl[0])
	return(vl);

    globv.gl_offs = 0;
    globv.gl_pathv = 0;
    globv.gl_pathc = 0;

    if (nonomatch)
	gflgs |= GLOB_NOCHECK;

    do {
	ptr = short2qstr(*vl);
	switch (glob(ptr, gflgs, 0, &globv)) {
	case GLOB_ABEND:
	    globfree(&globv);
	    setname(ptr);
	    stderror(ERR_NAME | ERR_GLOB);
	    /* NOTREACHED */
	case GLOB_NOSPACE:
	    globfree(&globv);
	    stderror(ERR_NOMEM);
	    /* NOTREACHED */
	default:
	    break;
	}
	if (globv.gl_flags & GLOB_MAGCHAR) {
	    match |= (globv.gl_matchc != 0);
	    magic = 1;
	}
	gflgs |= GLOB_APPEND;
    }
    while (*++vl);
    vl = (globv.gl_pathc == 0 || (magic && !match && !nonomatch)) ? 
	NULL : blk2short(globv.gl_pathv);
    globfree(&globv);
    return (vl);
}

Char   *
globone(str, action)
    Char   *str;
    int     action;
{

    Char   *v[2], **vl, **vo;
    int gflg;

    noglob = adrof(STRnoglob) != 0;
    gflag = 0;
    v[0] = str;
    v[1] = 0;
    tglob(v);
    gflg = gflag;
    if (gflg == G_NONE)
	return (strip(Strsave(str)));

    if (gflg & G_CSH) {
	/*
	 * Expand back-quote, tilde and brace
	 */
	vo = globexpand(v);
	if (noglob || (gflg & G_GLOB) == 0) {
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
    else if (noglob || (gflg & G_GLOB) == 0)
	return (strip(Strsave(str)));
    else
	vo = v;

    vl = libglob(vo);
    if ((gflg & G_CSH) && vl != vo)
	blkfree(vo);
    if (vl == NULL) {
	setname(short2str(str));
	stderror(ERR_NAME | ERR_NOMATCH);
    }
    if (vl[0] == NULL) {
	xfree((ptr_t) vl);
	return (Strsave(STRNULL));
    }
    if (vl[1]) 
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
    int gflg = gflag;

    if (!v || !v[0]) {
	gargv = saveblk(v);
	gargc = blklen(gargv);
	return (gargv);
    }

    noglob = adrof(STRnoglob) != 0;

    if (gflg & G_CSH)
	/*
	 * Expand back-quote, tilde and brace
	 */
	vl = vo = globexpand(v);
    else
	vl = vo = saveblk(v);

    if (!noglob && (gflg & G_GLOB)) {
	vl = libglob(vo);
	if (vl != vo)
	    blkfree(vo);
    }
    else
	trim(vl);

    gargc = vl ? blklen(vl) : 0;
    return (gargv = vl);
}

void
ginit()
{
    gargsiz = GLOBSPACE;
    gargv = (Char **) xmalloc((size_t) (sizeof(Char *) * gargsiz));
    gargv[0] = 0;
    gargc = 0;
}

void
rscan(t, f)
    register Char **t;
    void    (*f) __P((int));
{
    register Char *p;

    while ((p = *t++) != '\0')
	while (*p)
	    (*f) (*p++);
}

void
trim(t)
    register Char **t;
{
    register Char *p;

    while ((p = *t++) != '\0')
	while (*p)
	    *p++ &= TRIM;
}

void
tglob(t)
    register Char **t;
{
    register Char *p, *c;

    while ((p = *t++) != '\0') {
	if (*p == '~' || *p == '=')
	    gflag |= G_CSH;
	else if (*p == '{' &&
		 (p[1] == '\0' || (p[1] == '}' && p[2] == '\0')))
	    continue;
	/*
	 * The following line used to be *(c = p++), but hp broke their
	 * optimizer in 9.01, so we break the assignment into two pieces
	 * The careful reader here will note that *most* compiler workarounds
	 * in tcsh are either for apollo/DomainOS or hpux. Is it a coincidence?
	 */
	while ( *(c = p) != '\0') {
	    p++;
	    if (*c == '`') {
		gflag |= G_CSH;
#ifdef notdef
		/*
		 * We do want to expand echo `echo '*'`, so we don't\
		 * use this piece of code anymore.
		 */
		while (*p && *p != '`') 
		    if (*p++ == '\\') {
			if (*p)		/* Quoted chars */
			    p++;
			else
			    break;
		    }
		if (*p)			/* The matching ` */
		    p++;
		else
		    break;
#endif
	    }
	    else if (*c == '{')
		gflag |= G_CSH;
	    else if (isglob(*c))
		gflag |= G_GLOB;
	    else if (symlinks == SYM_EXPAND && 
		*p && ISDOTDOT(c) && (c == *(t-1) || *(c-1) == '/') )
	    	gflag |= G_CSH;
	}
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
    Char   *ep, word[LONGBSIZE];

    if (pargv) {
#ifdef notdef
	abort();
#endif
	blkfree(pargv);
    }
    pargsiz = GLOBSPACE;
    pargv = (Char **) xmalloc((size_t) (sizeof(Char *) * pargsiz));
    pargv[0] = NULL;
    pargcp = pargs = word;
    pargc = 0;
    pnleft = LONGBSIZE - 4;
    for (;;) {
#if defined(DSPMBYTE)
	for (lp = cp;; lp++) {
	    if (*lp == '`' &&
		(lp-1 < cp || !Ismbyte2(*lp) || !Ismbyte1(*(lp-1)))) {
		break;
	    }
#else /* DSPMBYTE */
	for (lp = cp; *lp != '`'; lp++) {
#endif /* DSPMBYTE */
	    if (*lp == 0) {
		if (pargcp != pargs)
		    pword(LONGBSIZE);
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
    Char   *fakecom[2], ibuf[BUFSIZE];
    char    tibuf[BUFSIZE];

    hadnl = 0;
    icnt = 0;
    quoted = (literal || (cp[0] & QUOTE)) ? QUOTE : 0;
    faket.t_dtyp = NODE_COMMAND;
    faket.t_dflg = F_BACKQ;
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
	struct command *t;

	(void) close(pvec[0]);
	(void) dmove(pvec[1], 1);
	(void) dmove(SHDIAG,  2);
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

        /*
	 * In the child ``forget'' everything about current aliases or
	 * eval vectors.
	 */
	alvec = NULL;
	evalvec = NULL;
	alvecp = NULL;
	evalp = NULL;
	(void) lex(&paraml);
	if (seterr)
	    stderror(ERR_OLD);
	alias(&paraml);
	t = syntax(paraml.next, &paraml, 0);
	if (seterr)
	    stderror(ERR_OLD);
	if (t)
	    t->t_dflg |= F_NOFORK;
#ifdef SIGTSTP
	(void) sigignore(SIGTSTP);
#endif
#ifdef SIGTTIN
	(void) sigignore(SIGTTIN);
#endif
#ifdef SIGTTOU
	(void) sigignore(SIGTTOU);
#endif
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
		    icnt = read(pvec[0], tibuf, BUFSIZE);
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
#ifdef WINNT_NATIVE
	    if (c == '\r')
	    	c = ' ';
#endif /* WINNT_NATIVE */
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
	    pword(BUFSIZE);
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
    *pargcp++ = (Char) c;
}

static void
pword(bufsiz)
    int    bufsiz;
{
    psave(0);
    if (pargc == pargsiz - 1) {
	pargsiz += GLOBSPACE;
	pargv = (Char **) xrealloc((ptr_t) pargv,
				   (size_t) (pargsiz * sizeof(Char *)));
    }
    pargv[pargc++] = Strsave(pargs);
    pargv[pargc] = NULL;
    pargcp = pargs;
    pnleft = bufsiz - 4;
}

int
Gmatch(string, pattern)
    Char *string, *pattern;
{
    return Gnmatch(string, pattern, NULL);
}

int 
Gnmatch(string, pattern, endstr)
    Char *string, *pattern, **endstr;
{
    Char **blk, **p, *tstring = string;
    int	   gpol = 1, gres = 0;

    if (*pattern == '^') {
	gpol = 0;
	pattern++;
    }

    blk = (Char **) xmalloc((size_t) (GLOBSPACE * sizeof(Char *)));
    blk[0] = Strsave(pattern);
    blk[1] = NULL;

    expbrace(&blk, NULL, GLOBSPACE);

    if (endstr == NULL)
	/* Exact matches only */
	for (p = blk; *p; p++) 
	    gres |= pmatch(string, *p, &tstring) == 2 ? 1 : 0;
    else {
	/* partial matches */
	int minc = 0x7fffffff;
	for (p = blk; *p; p++) 
	    if (pmatch(string, *p, &tstring) != 0) {
		int t = (int) (tstring - string);
		gres |= 1;
		if (minc == -1 || minc > t)
		    minc = t;
	    }
	*endstr = string + minc;
    }

    blkfree(blk);
    return(gres == gpol);
} 

/* pmatch():
 *	Return 2 on exact match, 	
 *	Return 1 on substring match.
 *	Return 0 on no match.
 *	*estr will point to the end of the longest exact or substring match.
 */
static int
pmatch(string, pattern, estr)
    register Char *string, *pattern, **estr;
{
    register Char stringc, patternc;
    int     match, negate_range;
    Char    rangec, *oestr, *pestr;

    for (;; ++string) {
	stringc = *string & TRIM;
	/*
	 * apollo compiler bug: switch (patternc = *pattern++) dies
	 */
	patternc = *pattern++;
	switch (patternc) {
	case 0:
	    *estr = string;
	    return (stringc == 0 ? 2 : 1);
	case '?':
	    if (stringc == 0)
		return (0);
	    *estr = string;
	    break;
	case '*':
	    if (!*pattern) {
		while (*string) string++;
		*estr = string;
		return (2);
	    }
	    oestr = *estr;
	    pestr = NULL;

	    do {
		switch(pmatch(string, pattern, estr)) {
		case 0:
		    break;
		case 1:
		    pestr = *estr;
		    break;
		case 2:
		    return 2;
		default:
		    abort();	/* Cannot happen */
		}
		*estr = string;
	    }
	    while (*string++);

	    if (pestr) {
		*estr = pestr;
		return 1;
	    }
	    else {
		*estr = oestr;
		return 0;
	    }

	case '[':
	    match = 0;
	    if ((negate_range = (*pattern == '^')) != 0)
		pattern++;
	    while ((rangec = *pattern++) != '\0') {
		if (rangec == ']')
		    break;
		if (match)
		    continue;
		if (rangec == '-' && *(pattern-2) != '[' && *pattern  != ']') {
		    match = (globcharcoll(stringc, *pattern & TRIM) <= 0 &&
		    globcharcoll(*(pattern-2) & TRIM, stringc) <= 0);
		    pattern++;
		}
		else 
		    match = (stringc == (rangec & TRIM));
	    }
	    if (rangec == 0)
		stderror(ERR_NAME | ERR_MISSING, ']');
	    if (match == negate_range)
		return (0);
	    *estr = string;
	    break;
	default:
	    if ((patternc & TRIM) != stringc)
		return (0);
	    *estr = string;
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

    for (p = s1; *p++;)
	continue;
    for (q = s2; *q++;)
	continue;
    n = (int) ((p - s1) + (q - s2) - 1);
    if (++gargc >= gargsiz) {
	gargsiz += GLOBSPACE;
	gargv = (Char **) xrealloc((ptr_t) gargv,
				   (size_t) (gargsiz * sizeof(Char *)));
    }
    gargv[gargc] = 0;
    p = gargv[gargc - 1] = (Char *) xmalloc((size_t) (n * sizeof(Char)));
    for (q = s1; (*p++ = *q++) != '\0';)
	continue;
    for (p--, q = s2; (*p++ = *q++) != '\0';)
	continue;
}

#ifdef FILEC
int
sortscmp(a, b)
    register Char **a, **b;
{
    if (!a)			/* check for NULL */
	return (b ? 1 : 0);
    if (!b)
	return (-1);

    if (!*a)			/* check for NULL */
	return (*b ? 1 : 0);
    if (!*b)
	return (-1);

    return (int) collate(*a, *b);
}

#endif
