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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)glob.c	5.18 (Berkeley) 12/4/92";
#endif /* LIBC_SCCS and not lint */

/*
 * glob(3) -- a superset of the one defined in POSIX 1003.2.
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
 * GLOB_NOMAGIC:
 *	Same as GLOB_NOCHECK, but it will only append pattern if it did
 *	not contain any magic characters.  [Used in csh style globbing]
 * GLOB_ALTDIRFUNC:
 *	Use alternately specified directory access functions.
 * gl_matchc:
 *	Number of matches in the current invocation of glob.
 */

#include <sys/param.h>
#include <sys/stat.h>
#include <dirent.h>
#include <glob.h>
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define	DOLLAR		'$'
#define	DOT		'.'
#define	EOS		'\0'
#define	LBRACKET	'['
#define	NOT		'!'
#define	QUESTION	'?'
#define	QUOTE		'\\'
#define	RANGE		'-'
#define	RBRACKET	']'
#define	SEP		'/'
#define	STAR		'*'
#define	TILDE		'~'
#define	UNDERSCORE	'_'

#define	M_QUOTE		0x8000
#define	M_PROTECT	0x4000
#define	M_MASK		0xffff
#define	M_ASCII		0x00ff

#define	CHAR(c)		((c)&M_ASCII)
#define	META(c)		((c)|M_QUOTE)
#define	M_ALL		META('*')
#define	M_END		META(']')
#define	M_NOT		META('!')
#define	M_ONE		META('?')
#define	M_RNG		META('-')
#define	M_SET		META('[')
#define	ismeta(c)	(((c)&M_QUOTE) != 0)

typedef u_short Char;

static int	 compare __P((const void *, const void *));
static void	 g_Ctoc __P((Char *, char *));
static int	 g_lstat __P((Char *, struct stat *, glob_t *));
static DIR	*g_opendir __P((Char *, glob_t *));
static Char	*g_strchr __P((Char *, int));
static int	 g_stat __P((Char *, struct stat *, glob_t *));
static int	 glob1 __P((Char *, glob_t *));
static int	 glob2 __P((Char *, Char *, Char *, glob_t *));
static int	 glob3 __P((Char *, Char *, Char *, Char *, glob_t *));
static int	 globextend __P((Char *, glob_t *));
static int	 match __P((Char *, Char *, Char *));
#ifdef DEBUG
static void	 qprintf __P((Char *));
#endif

/*
 * The main glob() routine: compiles the pattern (optionally processing
 * quotes), calls glob1() to do the real pattern matching, and finally
 * sorts the list (unless unsorted operation is requested).  Returns 0
 * if things went well, nonzero if errors occurred.  It is not an error
 * to find no matches.
 */
glob(pattern, flags, errfunc, pglob)
	const char *pattern;
	int flags, (*errfunc) __P((char *, int));
	glob_t *pglob;
{
	const u_char *compilepat, *patnext;
	int c, err, oldpathc;
	Char *bufnext, *bufend, *compilebuf, *qpatnext, patbuf[MAXPATHLEN+1];

	patnext = (u_char *) pattern;
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

	bufnext = patbuf;
	bufend = bufnext + MAXPATHLEN;
	compilebuf = bufnext;
	compilepat = patnext;
	if (flags & GLOB_QUOTE) {
		/* Protect the quoted characters. */
		while (bufnext < bufend && (c = *patnext++) != EOS) 
			if (c == QUOTE) {
				if ((c = *patnext++) == EOS) {
					c = QUOTE;
					--patnext;
				}
				*bufnext++ = c | M_PROTECT;
			}
			else
				*bufnext++ = c;
	}
	else 
	    while (bufnext < bufend && (c = *patnext++) != EOS) 
		    *bufnext++ = c;
	*bufnext = EOS;

	bufnext = patbuf;
	qpatnext = patbuf;
	/* We don't need to check for buffer overflow any more. */
	while ((c = *qpatnext++) != EOS) {
		switch (c) {
		case LBRACKET:
			c = *qpatnext;
			if (c == NOT)
				++qpatnext;
			if (*qpatnext == EOS ||
			    g_strchr(qpatnext+1, RBRACKET) == NULL) {
				*bufnext++ = LBRACKET;
				if (c == NOT)
					--qpatnext;
				break;
			}
			*bufnext++ = M_SET;
			if (c == NOT)
				*bufnext++ = M_NOT;
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
			pglob->gl_flags |= GLOB_MAGCHAR;
			*bufnext++ = M_END;
			break;
		case QUESTION:
			pglob->gl_flags |= GLOB_MAGCHAR;
			*bufnext++ = M_ONE;
			break;
		case STAR:
			pglob->gl_flags |= GLOB_MAGCHAR;
			/* collapse adjacent stars to one, 
			 * to avoid exponential behavior
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

	if ((err = glob1(patbuf, pglob)) != 0)
		return(err);

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
			const u_char *sp = compilepat;
			while (*dp++ = *sp++);
		}
		else {
			/*
			 * Copy pattern, interpreting quotes; this is slightly
			 * different than the interpretation of quotes above
			 * -- which should prevail?
			 */
			while (*compilepat != EOS) {
				if (*compilepat == QUOTE) {
					if (*++compilepat == EOS)
						--compilepat;
				}
				*compilebuf++ = (u_char)*compilepat++;
			}
			*compilebuf = EOS;
		}
		return(globextend(patbuf, pglob));
	} else if (!(flags & GLOB_NOSORT)) 
		qsort(pglob->gl_pathv + pglob->gl_offs + oldpathc,
		    pglob->gl_pathc - oldpathc, sizeof(char *), compare);
	return(0);
}

static int
compare(p, q)
	const void *p, *q;
{
	return(strcmp(*(char **)p, *(char **)q));
}

static
glob1(pattern, pglob)
	Char *pattern;
	glob_t *pglob;
{
	Char pathbuf[MAXPATHLEN+1];

	/* A null pathname is invalid -- POSIX 1003.1 sect. 2.4. */
	if (*pattern == EOS)
		return(0);
	return(glob2(pathbuf, pathbuf, pattern, pglob));
}

/*
 * The functions glob2 and glob3 are mutually recursive; there is one level
 * of recursion for each segment in the pattern that contains one or more
 * meta characters.
 */
static
glob2(pathbuf, pathend, pattern, pglob)
	Char *pathbuf, *pathend, *pattern;
	glob_t *pglob;
{
	struct stat sb;
	Char *p, *q;
	int anymeta;

	/*
	 * Loop over pattern segments until end of pattern or until
	 * segment with meta character found.
	 */
	for (anymeta = 0;;) {
		if (*pattern == EOS) {		/* End of pattern? */
			*pathend = EOS;
			if (g_lstat(pathbuf, &sb, pglob))
				return(0);
		
			if (((pglob->gl_flags & GLOB_MARK) &&
			    pathend[-1] != SEP) && (S_ISDIR(sb.st_mode)
			    || (S_ISLNK(sb.st_mode) &&
			    (g_stat(pathbuf, &sb, pglob) == 0) &&
			    S_ISDIR(sb.st_mode)))) {
				*pathend++ = SEP;
				*pathend = EOS;
			}
			++pglob->gl_matchc;
			return(globextend(pathbuf, pglob));
		}

		/* Find end of next segment, copy tentatively to pathend. */
		q = pathend;
		p = pattern;
		while (*p != EOS && *p != SEP) {
			if (ismeta(*p))
				anymeta = 1;
			*q++ = *p++;
		}

		if (!anymeta) {		/* No expansion, do next segment. */
			pathend = q;
			pattern = p;
			while (*pattern == SEP)
				*pathend++ = *pattern++;
		} else			/* Need expansion, recurse. */
			return(glob3(pathbuf, pathend, pattern, p, pglob));
	}
	/* NOTREACHED */
}

static
glob3(pathbuf, pathend, pattern, restpattern, pglob)
	Char *pathbuf, *pathend, *pattern, *restpattern;
	glob_t *pglob;
{
	register struct dirent *dp;
	struct dirent *(*readdirfunc)();
	DIR *dirp;
	int len, err;
	char buf[MAXPATHLEN];

	*pathend = EOS;
	errno = 0;
	    
	if ((dirp = g_opendir(pathbuf, pglob)) == NULL) {
		/* TODO: don't call for ENOENT or ENOTDIR? */
		if (pglob->gl_errfunc) {
			g_Ctoc(pathbuf, buf);
			if (pglob->gl_errfunc(buf, errno) ||
			    pglob->gl_flags & GLOB_ERR)
				return (GLOB_ABEND);
		}
		return(0);
	}

	err = 0;

	/* Search directory for matching names. */
	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		readdirfunc = pglob->gl_readdir;
	else
		readdirfunc = readdir;
	while ((dp = (*readdirfunc)(dirp))) {
		register u_char *sc;
		register Char *dc;

		/* Initial DOT must be matched literally. */
		if (dp->d_name[0] == DOT && *pattern != DOT)
			continue;
		for (sc = (u_char *) dp->d_name, dc = pathend; 
		     *dc++ = *sc++;);
		if (!match(pathend, pattern, restpattern)) {
			*pathend = EOS;
			continue;
		}
		err = glob2(pathbuf, --dc, restpattern, pglob);
		if (err)
			break;
	}

	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		(*pglob->gl_closedir)(dirp);
	else
		closedir(dirp);
	return(err);
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
 *	gl_pathv points to (gl_offs + gl_pathc + 1) items.
 */
static int
globextend(path, pglob)
	Char *path;
	glob_t *pglob;
{
	register char **pathv;
	register int i;
	u_int newsize;
	char *copy;
	Char *p;

	newsize = sizeof(*pathv) * (2 + pglob->gl_pathc + pglob->gl_offs);
	pathv = (char **)realloc((char *)pglob->gl_pathv, newsize);
	if (pathv == NULL)
		return(GLOB_NOSPACE);

	if (pglob->gl_pathv == NULL && pglob->gl_offs > 0) {
		/* first time around -- clear initial gl_offs items */
		pathv += pglob->gl_offs;
		for (i = pglob->gl_offs; --i >= 0; )
			*--pathv = NULL;
	}
	pglob->gl_pathv = pathv;

	for (p = path; *p++;);
	if ((copy = malloc(p - path)) != NULL) {
		g_Ctoc(path, copy);
		pathv[pglob->gl_offs + pglob->gl_pathc++] = copy;
	}
	pathv[pglob->gl_offs + pglob->gl_pathc] = NULL;
	return(copy == NULL ? GLOB_NOSPACE : 0);
}


/*
 * pattern matching function for filenames.  Each occurrence of the *
 * pattern causes a recursion level.
 */
static
match(name, pat, patend)
	register Char *name, *pat, *patend;
{
	int ok, negate_range;
	Char c, k;

	while (pat < patend) {
		c = *pat++;
		switch (c & M_MASK) {
		case M_ALL:
			if (pat == patend)
				return(1);
			do 
			    if (match(name, pat, patend))
				    return(1);
			while (*name++ != EOS);
			return(0);
		case M_ONE:
			if (*name++ == EOS)
				return(0);
			break;
		case M_SET:
			ok = 0;
			if ((k = *name++) == EOS)
				return(0);
			if (negate_range = ((*pat & M_MASK) == M_NOT))
				++pat;
			while (((c = *pat++) & M_MASK) != M_END)
				if ((*pat & M_MASK) == M_RNG) {
					if (c <= k && k <= pat[1])
						ok = 1;
					pat += 2;
				} else if (c == k)
					ok = 1;
			if (ok == negate_range)
				return(0);
			break;
		default:
			if (*name++ != c)
				return(0);
			break;
		}
	}
	return(*name == EOS);
}

/* Free allocated data belonging to a glob_t structure. */
void
globfree(pglob)
	glob_t *pglob;
{
	register int i;
	register char **pp;

	if (pglob->gl_pathv != NULL) {
		pp = pglob->gl_pathv + pglob->gl_offs;
		for (i = pglob->gl_pathc; i--; ++pp)
			if (*pp)
				free(*pp);
		free(pglob->gl_pathv);
	}
}

static DIR *
g_opendir(str, pglob)
	register Char *str;
	glob_t *pglob;
{
	char buf[MAXPATHLEN];
	char *dirname;

	if (!*str)
		strcpy(buf, ".");
	else
		g_Ctoc(str, buf);
	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		return((*pglob->gl_opendir)(buf));
	return(opendir(buf));
}

static int
g_lstat(fn, sb, pglob)
	register Char *fn;
	struct stat *sb;
	glob_t *pglob;
{
	char buf[MAXPATHLEN];

	g_Ctoc(fn, buf);
	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		return((*pglob->gl_lstat)(buf, sb));
	return(lstat(buf, sb));
}

static int
g_stat(fn, sb, pglob)
	register Char *fn;
	struct stat *sb;
	glob_t *pglob;
{
	char buf[MAXPATHLEN];

	g_Ctoc(fn, buf);
	if (pglob->gl_flags & GLOB_ALTDIRFUNC)
		return((*pglob->gl_stat)(buf, sb));
	return(stat(buf, sb));
}

static Char *
g_strchr(str, ch)
	Char *str;
	int ch;
{
	do {
		if (*str == ch)
			return (str);
	} while (*str++);
	return (NULL);
}

static void
g_Ctoc(str, buf)
	register Char *str;
	char *buf;
{
	register char *dc;

	for (dc = buf; *dc++ = *str++;);
}

#ifdef DEBUG
static void 
qprintf(s)
	register Char *s;
{
	register Char *p;

	for (p = s; *p; p++)
		(void)printf("%c", CHAR(*p));
	(void)printf("\n");
	for (p = s; *p; p++)
		(void)printf("%c", *p & M_PROTECT ? '"' : ' ');
	(void)printf("\n");
	for (p = s; *p; p++)
		(void)printf("%c", ismeta(*p) ? '_' : ' ');
	(void)printf("\n");
}
#endif
