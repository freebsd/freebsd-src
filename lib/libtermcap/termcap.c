/*
 * Copyright (c) 1980, 1993
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
static char sccsid[] = "@(#)termcap.c	8.1 (Berkeley) 6/4/93";
#endif /* not lint */

#define	PBUFSIZ		512	/* max length of filename path */
#define	PVECSIZ		32	/* max number of names in path */
#define TBUFSIZ         1024    /* max length of tgetent buffer */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include "termcap.h"
#include "pathnames.h"

/*
 * termcap - routines for dealing with the terminal capability data base
 *
 * BUG:		Should use a "last" pointer in tbuf, so that searching
 *		for capabilities alphabetically would not be a n**2/2
 *		process when large numbers of capabilities are given.
 * Note:	If we add a last pointer now we will screw up the
 *		tc capability. We really should compile termcap.
 *
 * Essentially all the work here is scanning and decoding escapes
 * in string capabilities.  We don't use stdio because the editor
 * doesn't, and because living w/o it is not hard.
 */

static	char *tbuf;	/* termcap buffer */

/*
 * Get an entry for terminal name in buffer bp from the termcap file.
 */
int
tgetent(char *bp, const char *name)
{
	register char *p;
	register char *cp;
	char  *dummy;
	char **fname;
	char  *home;
	int    i;
	char   pathbuf[PBUFSIZ];	/* holds raw path of filenames */
	char  *pathvec[PVECSIZ];	/* to point to names in pathbuf */
	char **pvec;			/* holds usable tail of path vector */
	char  *termpath;
	struct termios tty;

	dummy = NULL;
	fname = pathvec;
	pvec = pathvec;
	tbuf = bp;
	p = pathbuf;
	cp = getenv("TERMCAP");
	/*
	 * TERMCAP can have one of two things in it. It can be the
	 * name of a file to use instead of /etc/termcap. In this
	 * case it better start with a "/". Or it can be an entry to
	 * use so we don't have to read the file. In this case it
	 * has to already have the newlines crunched out.  If TERMCAP
	 * does not hold a file name then a path of names is searched
	 * instead.  The path is found in the TERMPATH variable, or
	 * becomes "$HOME/.termcap /etc/termcap" if no TERMPATH exists.
	 */
	if (!cp || *cp != '/') {	/* no TERMCAP or it holds an entry */
		if ( (termpath = getenv("TERMPATH")) )
			strncpy(pathbuf, termpath, PBUFSIZ);
		else {
			if ( (home = getenv("HOME")) ) {/* set up default */
				strncpy(pathbuf, home, PBUFSIZ - 1); /* $HOME first */
				pathbuf[PBUFSIZ - 2] = '\0'; /* -2 because we add a slash */
				p += strlen(pathbuf);	/* path, looking in */
				*p++ = '/';
			}	/* if no $HOME look in current directory */
			strncpy(p, _PATH_DEF, PBUFSIZ - (p - pathbuf));
		}
	}
	else				/* user-defined name in TERMCAP */
		strncpy(pathbuf, cp, PBUFSIZ);	/* still can be tokenized */
	pathbuf[PBUFSIZ - 1] = '\0';

	if (issetugid())
		strcpy(pathbuf, _PATH_DEF_SEC);

	*fname++ = pathbuf;	/* tokenize path into vector of names */
	while (*++p)
		if (*p == ' ' || *p == ':') {
			*p = '\0';
			while (*++p)
				if (*p != ' ' && *p != ':')
					break;
			if (*p == '\0')
				break;
			*fname++ = p;
			if (fname >= pathvec + PVECSIZ) {
				fname--;
				break;
			}
		}
	*fname = (char *) 0;			/* mark end of vector */
	if (cp && *cp && *cp != '/')
		if (cgetset(cp) < 0)
			return(-2);

	i = cgetent(&dummy, pathvec, (char *)name);

	if (i == 0) {
		char *pd, *ps, *tok, *s, *tcs;
		size_t len;

		pd = bp;
		ps = dummy;
		if ((tok = strchr(ps, ':')) == NULL) {
			len = strlen(ps);
			if (len >= TBUFSIZ)
				i = -1;
			else
				strcpy(pd, ps);
			goto done;
		}
		len = tok - ps + 1;
		if (pd + len + 1 - bp >= TBUFSIZ) {
			i = -1;
			goto done;
		}
		memcpy(pd, ps, len);
		ps += len;
		pd += len;
		*pd = '\0';
		tcs = pd - 1;
		for (;;) {
			while ((tok = strsep(&ps, ":")) != NULL &&
			       (*tok == '\0' || *tok == '\\' || !isgraph(*tok)))
				;
			if (tok == NULL)
				break;
			for (s = tcs; s != NULL && s[1] != '\0'; s = strchr(s, ':')) {
				s++;
				if (s[0] == tok[0] && s[1] == tok[1])
					goto skip_it;
			}
			len = strlen(tok);
			if (pd + len + 1 - bp >= TBUFSIZ) {
				i = -1;
				break;
			}
			memcpy(pd, tok, len);
			pd += len;
			*pd++ = ':';
			*pd = '\0';
		skip_it: ;
		}
	}
done:
	if (   i == 0
	    && (   tcgetattr(STDERR_FILENO, &tty) != -1
		|| tcgetattr(STDOUT_FILENO, &tty) != -1
	       )
	   )
		__set_ospeed(cfgetospeed(&tty));
	if (dummy)
		free(dummy);
	/* no tc reference loop return code in libterm XXX */
	if (i == -3)
		return(-1);
	return(i + 1);
}

/*
 * Return the (numeric) option id.
 * Numeric options look like
 *	li#80
 * i.e. the option string is separated from the numeric value by
 * a # character.  If the option is not found we return -1.
 * Note that we handle octal numbers beginning with 0.
 */
int
tgetnum(const char *id)
{
	long num;

	if (cgetnum(tbuf, (char *)id, &num) == 0)
		return(num);
	else
		return(-1);
}

/*
 * Handle a flag option.
 * Flag options are given "naked", i.e. followed by a : or the end
 * of the buffer.  Return 1 if we find the option, or 0 if it is
 * not given.
 */
int
tgetflag(const char *id)
{
	return(cgetcap(tbuf, (char *)id, ':') != NULL);
}

/*
 * Get a string valued option.
 * These are given as
 *	cl=^Z
 * Much decoding is done on the strings, and the strings are
 * placed in area, which is a ref parameter which is updated.
 * No checking on area overflow.
 */
char *
tgetstr(const char *id, char **area)
{
	char ids[3];
	char *s;
	int i;

	/*
	 * XXX
	 * This is for all the boneheaded programs that relied on tgetstr
	 * to look only at the first 2 characters of the string passed...
	 */
	*ids = *id;
	ids[1] = id[1];
	ids[2] = '\0';

	if ((i = cgetstr(tbuf, ids, &s)) < 0)
		return NULL;

	strcpy(*area, s);
	*area += i + 1;
	return(s);
}
