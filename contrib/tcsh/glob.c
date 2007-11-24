/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Guido van Rossum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)glob.c	5.12 (Berkeley) 6/24/91";
#endif /* LIBC_SCCS and not lint */
/*
 * Glob: the interface is a superset of the one defined in POSIX 1003.2,
 * draft 9.
 *
 * The [!...] convention to negate a range is supported (SysV, Posix, ksh).
 *
 * Optional extra services, controlled by flags not defined by POSIX:
 *
 * GLOB_QUOTE:
 *	Escaping convention: \ inhibits any special meaning the following
 *	character might have (except \ at end of string is retained).
 * GLOB_MAGCHAR:
 *	Set in gl_flags if pattern contained a globbing character.
 * GLOB_ALTNOT:
 *	Use ^ instead of ! for "not".
 * gl_matchc:
 *	Number of matches in the current invocation of glob.
 */

#ifdef WINNT_NATIVE
	#pragma warning(disable:4244)
#endif /* WINNT_NATIVE */

#define Char __Char
#include "sh.h"
#include "glob.h"

#undef Char
#undef QUOTE
#undef TILDE
#undef META
#undef CHAR
#undef ismeta
#undef Strchr

#ifndef S_ISDIR
#define S_ISDIR(a)	(((a) & S_IFMT) == S_IFDIR)
#endif

#if !defined(S_ISLNK) && defined(S_IFLNK)
#define S_ISLNK(a)	(((a) & S_IFMT) == S_IFLNK)
#endif

#if !defined(S_ISLNK) && !defined(lstat)
#define lstat stat
#endif

typedef unsigned short Char;

static	int	 glob1 		__P((Char *, glob_t *, int));
static	int	 glob2		__P((Char *, Char *, Char *, glob_t *, int));
static	int	 glob3		__P((Char *, Char *, Char *, Char *,
				     glob_t *, int));
static	int	 globextend	__P((Char *, glob_t *));
static	int	 match		__P((Char *, Char *, Char *, int));
#ifndef __clipper__
static	int	 compare	__P((const ptr_t, const ptr_t));
#endif
static 	DIR	*Opendir	__P((Char *));
#ifdef S_IFLNK
static	int	 Lstat		__P((Char *, struct stat *));
#endif
static	int	 Stat		__P((Char *, struct stat *sb));
static 	Char 	*Strchr		__P((Char *, int));
#ifdef DEBUG
static	void	 qprintf	__P((Char *));
#endif

#define	DOLLAR		'$'
#define	DOT		'.'
#define	EOS		'\0'
#define	LBRACKET	'['
#define	NOT		'!'
#define ALTNOT		'^'
#define	QUESTION	'?'
#define	QUOTE		'\\'
#define	RANGE		'-'
#define	RBRACKET	']'
#define	SEP		'/'
#define	STAR		'*'
#define	TILDE		'~'
#define	UNDERSCORE	'_'

#define	M_META		0x8000
#define M_PROTECT	0x4000
#define	M_MASK		0xffff
#define	M_ASCII		0x00ff

#define	CHAR(c)		((c)&M_ASCII)
#define	META(c)		((c)|M_META)
#define	M_ALL		META('*')
#define	M_END		META(']')
#define	M_NOT		META('!')
#define	M_ALTNOT	META('^')
#define	M_ONE		META('?')
#define	M_RNG		META('-')
#define	M_SET		META('[')
#define	ismeta(c)	(((c)&M_META) != 0)

#ifndef BUFSIZE
#define GLOBBUFLEN	MAXPATHLEN
#else
#define GLOBBUFLEN	BUFSIZE
#endif

int
globcharcoll(c1, c2, cs)
    NLSChar c1, c2;
    int cs;
{
#if defined(NLS) && defined(LC_COLLATE) && !defined(NOSTRCOLL)
# if defined(SHORT_STRINGS)
    wchar_t s1[2], s2[2];

    if (c1 == c2)
	return (0);
    if (cs) {
	c1 = towlower(c1);
	c2 = towlower(c2);
    } else {
	/* This should not be here, but I'll rather leave it in than engage in
	   a LC_COLLATE flamewar about a shell I don't use... */
	if (iswlower(c1) && iswupper(c2))
	    return (1);
	if (iswupper(c1) && iswlower(c2))
	    return (-1);
    }
    s1[0] = c1;
    s2[0] = c2;
    s1[1] = s2[1] = '\0';
    return wcscoll(s1, s2);
# else /* not SHORT_STRINGS */
    char s1[2], s2[2];

    if (c1 == c2)
	return (0);
    /*
     * From kevin lyda <kevin@suberic.net>:
     * strcoll does not guarantee case sorting, so we pre-process now:
     */
    if (cs) {
	c1 = islower(c1) ? c1 : tolower(c1);
	c2 = islower(c2) ? c2 : tolower(c2);
    } else {
	if (islower(c1) && isupper(c2))
	    return (1);
	if (isupper(c1) && islower(c2))
	    return (-1);
    }
    s1[0] = c1;
    s2[0] = c2;
    s1[1] = s2[1] = '\0';
    return strcoll(s1, s2);
# endif
#else
    return (c1 - c2);
#endif
}

/*
 * Need to dodge two kernel bugs:
 * opendir("") != opendir(".")
 * NAMEI_BUG: on plain files trailing slashes are ignored in some kernels.
 *            POSIX specifies that they should be ignored in directories.
 */

static DIR *
Opendir(str)
    Char *str;
{
    char    buf[GLOBBUFLEN];
    char *dc = buf;
#if defined(hpux) || defined(__hpux)
    struct stat st;
#endif

    if (!*str)
	return (opendir("."));
    while ((*dc++ = *str++) != '\0')
	continue;
#if defined(hpux) || defined(__hpux)
    /*
     * Opendir on some device files hangs, so avoid it
     */
    if (stat(buf, &st) == -1 || !S_ISDIR(st.st_mode))
	return NULL;
#endif
    return (opendir(buf));
}

#ifdef S_IFLNK
static int
Lstat(fn, sb)
    Char *fn;
    struct stat *sb;
{
    char    buf[GLOBBUFLEN];
    char *dc = buf;

    while ((*dc++ = *fn++) != '\0')
	continue;
# ifdef NAMEI_BUG
    {
	int     st;

	st = lstat(buf, sb);
	if (*buf)
	    dc--;
	return (*--dc == '/' && !S_ISDIR(sb->st_mode) ? -1 : st);
    }
# else
    return (lstat(buf, sb));
# endif	/* NAMEI_BUG */
}
#else
#define Lstat Stat
#endif /* S_IFLNK */

static int
Stat(fn, sb)
    Char *fn;
    struct stat *sb;
{
    char    buf[GLOBBUFLEN];
    char *dc = buf;

    while ((*dc++ = *fn++) != '\0')
	continue;
#ifdef NAMEI_BUG
    {
	int     st;

	st = stat(buf, sb);
	if (*buf)
	    dc--;
	return (*--dc == '/' && !S_ISDIR(sb->st_mode) ? -1 : st);
    }
#else
    return (stat(buf, sb));
#endif /* NAMEI_BUG */
}

static Char *
Strchr(str, ch)
    Char *str;
    int ch;
{
    do
	if (*str == ch)
	    return (str);
    while (*str++);
    return (NULL);
}

#ifdef DEBUG
static void
qprintf(s)
Char *s;
{
    Char *p;

    for (p = s; *p; p++)
	printf("%c", *p & 0xff);
    printf("\n");
    for (p = s; *p; p++)
	printf("%c", *p & M_PROTECT ? '"' : ' ');
    printf("\n");
    for (p = s; *p; p++)
	printf("%c", *p & M_META ? '_' : ' ');
    printf("\n");
}
#endif /* DEBUG */

static int
compare(p, q)
    const ptr_t  p, q;
{
#if defined(NLS) && !defined(NOSTRCOLL)
    errno = 0;  /* strcoll sets errno, another brain-damage */
 
    return (strcoll(*(char **) p, *(char **) q));
#else
    return (strcmp(*(char **) p, *(char **) q));
#endif /* NLS && !NOSTRCOLL */
}

/*
 * The main glob() routine: compiles the pattern (optionally processing
 * quotes), calls glob1() to do the real pattern matching, and finally
 * sorts the list (unless unsorted operation is requested).  Returns 0
 * if things went well, nonzero if errors occurred.  It is not an error
 * to find no matches.
 */
int
glob(pattern, flags, errfunc, pglob)
    const char *pattern;
    int     flags;
    int     (*errfunc) __P((const char *, int));
    glob_t *pglob;
{
    int     err, oldpathc;
    Char *bufnext, *bufend, *compilebuf, m_not;
    const unsigned char *compilepat, *patnext;
    int     c, not;
    Char *qpatnext;
#ifdef WIDE_STRINGS
    Char patbuf[GLOBBUFLEN + MB_LEN_MAX + 1];
#else
    Char patbuf[GLOBBUFLEN + 1];
#endif
    int     no_match;

    patnext = (const unsigned char *) pattern;
    if (!(flags & GLOB_APPEND)) {
	pglob->gl_pathc = 0;
	pglob->gl_pathv = NULL;
	if (!(flags & GLOB_DOOFFS))
	    pglob->gl_offs = 0;
    }
    pglob->gl_flags = flags & ~GLOB_MAGCHAR;
    pglob->gl_errfunc = errfunc;
    oldpathc = pglob->gl_pathc;
    pglob->gl_matchc = 0;

    if (pglob->gl_flags & GLOB_ALTNOT) {
	not = ALTNOT;
	m_not = M_ALTNOT;
    }
    else {
	not = NOT;
	m_not = M_NOT;
    }

    bufnext = patbuf;
    bufend = bufnext + GLOBBUFLEN;
    compilebuf = bufnext;
    compilepat = patnext;

    no_match = *patnext == not;
    if (no_match)
	patnext++;

    if (flags & GLOB_QUOTE) {
	/* Protect the quoted characters */
	while (bufnext < bufend && (c = *patnext++) != EOS) {
#ifdef WIDE_STRINGS
	    int len;
	    
	    len = mblen((const char *)(patnext - 1), MB_LEN_MAX);
	    if (len == -1)
		mblen(NULL, 0);
	    if (len > 1) {
		*bufnext++ = (Char) c;
		while (--len != 0)
		    *bufnext++ = (Char) (*patnext++ | M_PROTECT);
	    } else
#endif /* WIDE_STRINGS */
	    if (c == QUOTE) {
		if ((c = *patnext++) == EOS) {
		    c = QUOTE;
		    --patnext;
		}
		*bufnext++ = (Char) (c | M_PROTECT);
	    }
	    else
		*bufnext++ = (Char) c;
	}
    }
    else 
	while (bufnext < bufend && (c = *patnext++) != EOS) 
	    *bufnext++ = (Char) c;
    *bufnext = EOS;

    bufnext = patbuf;
    qpatnext = patbuf;
    /* we don't need to check for buffer overflow any more */
    while ((c = *qpatnext++) != EOS) {
	switch (c) {
	case LBRACKET:
	    c = *qpatnext;
	    if (c == not)
		++qpatnext;
	    if (*qpatnext == EOS ||
		Strchr(qpatnext + 1, RBRACKET) == NULL) {
		*bufnext++ = LBRACKET;
		if (c == not)
		    --qpatnext;
		break;
	    }
	    pglob->gl_flags |= GLOB_MAGCHAR;
	    *bufnext++ = M_SET;
	    if (c == not)
		*bufnext++ = m_not;
	    c = *qpatnext++;
	    do {
		*bufnext++ = CHAR(c);
		if (*qpatnext == RANGE &&
		    (c = qpatnext[1]) != RBRACKET) {
		    *bufnext++ = M_RNG;
		    *bufnext++ = CHAR(c);
		    qpatnext += 2;
		}
	    } while ((c = *qpatnext++) != RBRACKET);
	    *bufnext++ = M_END;
	    break;
	case QUESTION:
	    pglob->gl_flags |= GLOB_MAGCHAR;
	    *bufnext++ = M_ONE;
	    break;
	case STAR:
	    pglob->gl_flags |= GLOB_MAGCHAR;
	    /* collapse adjacent stars to one, to avoid
	     * exponential behavior
	     */
	    if (bufnext == patbuf || bufnext[-1] != M_ALL)
		*bufnext++ = M_ALL;
	    break;
	default:
	    *bufnext++ = CHAR(c);
	    break;
	}
    }
    *bufnext = EOS;
#ifdef DEBUG
    qprintf(patbuf);
#endif

    if ((err = glob1(patbuf, pglob, no_match)) != 0)
	return (err);

    /*
     * If there was no match we are going to append the pattern 
     * if GLOB_NOCHECK was specified or if GLOB_NOMAGIC was specified
     * and the pattern did not contain any magic characters
     * GLOB_NOMAGIC is there just for compatibility with csh.
     */
    if (pglob->gl_pathc == oldpathc && 
	((flags & GLOB_NOCHECK) || 
	 ((flags & GLOB_NOMAGIC) && !(pglob->gl_flags & GLOB_MAGCHAR)))) {
	if (!(flags & GLOB_QUOTE)) {
	    Char *dp = compilebuf;
	    const unsigned char *sp = compilepat;

	    while ((*dp++ = *sp++) != '\0')
		continue;
	}
	else {
	    /*
	     * copy pattern, interpreting quotes; this is slightly different
	     * than the interpretation of quotes above -- which should prevail?
	     */
	    while (*compilepat != EOS) {
		if (*compilepat == QUOTE) {
		    if (*++compilepat == EOS)
			--compilepat;
		}
		*compilebuf++ = (unsigned char) *compilepat++;
	    }
	    *compilebuf = EOS;
	}
	return (globextend(patbuf, pglob));
    }
    else if (!(flags & GLOB_NOSORT) && (pglob->gl_pathc != oldpathc))
	qsort((char *) (pglob->gl_pathv + pglob->gl_offs + oldpathc),
	      pglob->gl_pathc - oldpathc, sizeof(char *),
	      (int (*) __P((const void *, const void *))) compare);
    return (0);
}

static int
glob1(pattern, pglob, no_match)
    Char *pattern;
    glob_t *pglob;
    int     no_match;
{
    Char pathbuf[GLOBBUFLEN + 1];

    /*
     * a null pathname is invalid -- POSIX 1003.1 sect. 2.4.
     */
    if (*pattern == EOS)
	return (0);
    return (glob2(pathbuf, pathbuf, pattern, pglob, no_match));
}

/*
 * functions glob2 and glob3 are mutually recursive; there is one level
 * of recursion for each segment in the pattern that contains one or
 * more meta characters.
 */
static int
glob2(pathbuf, pathend, pattern, pglob, no_match)
    Char *pathbuf, *pathend, *pattern;
    glob_t *pglob;
    int     no_match;
{
    struct stat sbuf;
    int anymeta;
    Char *p, *q;

    /*
     * loop over pattern segments until end of pattern or until segment with
     * meta character found.
     */
    anymeta = 0;
    for (;;) {
	if (*pattern == EOS) {	/* end of pattern? */
	    *pathend = EOS;

	    if (Lstat(pathbuf, &sbuf))
		return (0);

	    if (((pglob->gl_flags & GLOB_MARK) &&
		 pathend[-1] != SEP) &&
		(S_ISDIR(sbuf.st_mode)
#ifdef S_IFLNK
		 || (S_ISLNK(sbuf.st_mode) &&
		     (Stat(pathbuf, &sbuf) == 0) &&
		     S_ISDIR(sbuf.st_mode))
#endif
		 )) {
		*pathend++ = SEP;
		*pathend = EOS;
	    }
	    ++pglob->gl_matchc;
	    return (globextend(pathbuf, pglob));
	}

	/* find end of next segment, copy tentatively to pathend */
	q = pathend;
	p = pattern;
	while (*p != EOS && *p != SEP) {
	    if (ismeta(*p))
		anymeta = 1;
	    *q++ = *p++;
	}

	if (!anymeta) {		/* no expansion, do next segment */
	    pathend = q;
	    pattern = p;
	    while (*pattern == SEP)
		*pathend++ = *pattern++;
	}
	else			/* need expansion, recurse */
	    return (glob3(pathbuf, pathend, pattern, p, pglob, no_match));
    }
    /* NOTREACHED */
}


static int
glob3(pathbuf, pathend, pattern, restpattern, pglob, no_match)
    Char *pathbuf, *pathend, *pattern, *restpattern;
    glob_t *pglob;
    int     no_match;
{
    DIR    *dirp;
    struct dirent *dp;
    int     err;
    Char m_not = (pglob->gl_flags & GLOB_ALTNOT) ? M_ALTNOT : M_NOT;
    char cpathbuf[GLOBBUFLEN], *ptr;;

    *pathend = EOS;
    errno = 0;

    if (!(dirp = Opendir(pathbuf))) {
	/* todo: don't call for ENOENT or ENOTDIR? */
	for (ptr = cpathbuf; (*ptr++ = (char) *pathbuf++) != EOS;)
	    continue;
	if ((pglob->gl_errfunc && (*pglob->gl_errfunc) (cpathbuf, errno)) ||
	    (pglob->gl_flags & GLOB_ERR))
	    return (GLOB_ABEND);
	else
	    return (0);
    }

    err = 0;

    /* search directory for matching names */
    while ((dp = readdir(dirp)) != NULL) {
	unsigned char *sc;
	Char *dc;

	/* initial DOT must be matched literally */
	if (dp->d_name[0] == DOT && *pattern != DOT)
	    continue;
	for (sc = (unsigned char *) dp->d_name, dc = pathend; 
	     (*dc++ = *sc++) != '\0';)
	    continue;
	if (match(pathend, pattern, restpattern, (int) m_not) == no_match) {
	    *pathend = EOS;
	    continue;
	}
	err = glob2(pathbuf, --dc, restpattern, pglob, no_match);
	if (err)
	    break;
    }
    /* todo: check error from readdir? */
    (void) closedir(dirp);
    return (err);
}


/*
 * Extend the gl_pathv member of a glob_t structure to accomodate a new item,
 * add the new item, and update gl_pathc.
 *
 * This assumes the BSD realloc, which only copies the block when its size
 * crosses a power-of-two boundary; for v7 realloc, this would cause quadratic
 * behavior.
 *
 * Return 0 if new item added, error code if memory couldn't be allocated.
 *
 * Invariant of the glob_t structure:
 *	Either gl_pathc is zero and gl_pathv is NULL; or gl_pathc > 0 and
 *	 gl_pathv points to (gl_offs + gl_pathc + 1) items.
 */
static int
globextend(path, pglob)
    Char *path;
    glob_t *pglob;
{
    char **pathv;
    int i;
    unsigned int newsize;
    char   *copy;
    Char *p;

    newsize = sizeof(*pathv) * (2 + pglob->gl_pathc + pglob->gl_offs);
    pathv = (char **) (pglob->gl_pathv ?
		       xrealloc((ptr_t) pglob->gl_pathv, (size_t) newsize) :
		       xmalloc((size_t) newsize));
    if (pathv == NULL)
	return (GLOB_NOSPACE);

    if (pglob->gl_pathv == NULL && pglob->gl_offs > 0) {
	/* first time around -- clear initial gl_offs items */
	pathv += pglob->gl_offs;
	for (i = pglob->gl_offs; --i >= 0;)
	    *--pathv = NULL;
    }
    pglob->gl_pathv = pathv;

    for (p = path; *p++;)
	continue;
    if ((copy = (char *) xmalloc((size_t) (p - path))) != NULL) {
	char *dc = copy;
	Char *sc = path;

	while ((*dc++ = *sc++) != '\0')
	    continue;
	pathv[pglob->gl_offs + pglob->gl_pathc++] = copy;
    }
    pathv[pglob->gl_offs + pglob->gl_pathc] = NULL;
    return ((copy == NULL) ? GLOB_NOSPACE : 0);
}


static size_t
One_mbtowc(NLSChar *pwc, const Char *s, size_t n)
{
#ifdef WIDE_STRINGS
    char buf[MB_LEN_MAX], *p;

    if (n > MB_LEN_MAX)
	n = MB_LEN_MAX;
    p = buf;
    while (p < buf + n && (*p++ = CHAR(*s++)) != 0)
	;
    return one_mbtowc(pwc, buf, n);
#else
    return NLSFrom(s, n, pwc);
#endif
}

/*
 * pattern matching function for filenames.  Each occurrence of the *
 * pattern causes a recursion level.
 */
static  int
match(name, pat, patend, m_not)
    Char *name, *pat, *patend;
    int m_not;
{
    int ok, negate_range;
    Char c, k;

    while (pat < patend) {
	size_t lwk;
	NLSChar wc, wk;

	USE(k);
	c = *pat; /* Only for M_MASK bits */
	pat += One_mbtowc(&wc, pat, MB_LEN_MAX);
	lwk = One_mbtowc(&wk, name, MB_LEN_MAX);
	switch (c & M_MASK) {
	case M_ALL:
	    if (pat == patend)
		return (1);
	    for (;;) {
		if (match(name, pat, patend, m_not))
		    return (1);
		if (*name == EOS)
		    break;
		name += lwk;
		lwk = One_mbtowc(&wk, name, MB_LEN_MAX);
	    }
	    return (0);
	case M_ONE:
	    if (*name == EOS)
		return (0);
	    name += lwk;
	    break;
	case M_SET:
	    ok = 0;
	    if (*name == EOS)
		return (0);
	    name += lwk;
	    if ((negate_range = ((*pat & M_MASK) == m_not)) != 0)
		++pat;
	    while ((*pat & M_MASK) != M_END) {
		pat += One_mbtowc(&wc, pat, MB_LEN_MAX);
		if ((*pat & M_MASK) == M_RNG) {
		    NLSChar wc2;
		    
		    pat++;
		    pat += One_mbtowc(&wc2, pat, MB_LEN_MAX);
		    if (globcharcoll(wc, wk, 0) <= 0 &&
			globcharcoll(wk, wc2, 0) <= 0)
			ok = 1;
		} else if (wc == wk)
		    ok = 1;
	    }
	    pat += One_mbtowc(&wc, pat, MB_LEN_MAX);
	    if (ok == negate_range)
		return (0);
	    break;
	default:
	    name += lwk;
	    if (samecase(wk) != samecase(wc))
		return (0);
	    break;
	}
    }
    return (*name == EOS);
}

/* free allocated data belonging to a glob_t structure */
void
globfree(pglob)
    glob_t *pglob;
{
    int i;
    char **pp;

    if (pglob->gl_pathv != NULL) {
	pp = pglob->gl_pathv + pglob->gl_offs;
	for (i = pglob->gl_pathc; i--; ++pp)
	    if (*pp)
		xfree((ptr_t) *pp), *pp = NULL;
	xfree((ptr_t) pglob->gl_pathv), pglob->gl_pathv = NULL;
    }
}
