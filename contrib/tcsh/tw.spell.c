/* $Header: /src/pub/tcsh/tw.spell.c,v 3.17 2004/11/23 02:10:50 christos Exp $ */
/*
 * tw.spell.c: Spell check words
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
#include "sh.h"

RCSID("$Id: tw.spell.c,v 3.17 2004/11/23 02:10:50 christos Exp $")

#include "tw.h"

/* spell_me : return corrrectly spelled filename.  From K&P spname */
int
spell_me(oldname, oldsize, looking, pat, suf)
    Char   *oldname;
    int     oldsize, looking;
    Char   *pat;
    int     suf;
{
    /* The +1 is to fool hp's optimizer */
    Char    guess[FILSIZ + 1], newname[FILSIZ + 1];
    Char *new = newname, *old = oldname;
    Char *p, *cp, *ws;
    int    foundslash = 0;
    int     retval;

    for (;;) {
	while (*old == '/') {	/* skip '/' */
	    *new++ = *old++;
	    foundslash = 1;
	}
	/* do not try to correct spelling of single letter words */
	if (*old != '\0' && old[1] == '\0')
	    *new++ = *old++;
	*new = '\0';
	if (*old == '\0') {
	    retval = (StrQcmp(oldname, newname) != 0);
	    copyn(oldname, newname, oldsize);	/* shove it back. */
	    return retval;
	}
	p = guess;		/* start at beginning of buf */
	if (newname[0])		/* add current dir if any */
	    for (cp = newname; *cp; cp++)
		if (p < guess + FILSIZ)
		    *p++ = *cp;
	ws = p;
	for (; *old != '/' && *old != '\0'; old++)/* add current file name */
	    if (p < guess + FILSIZ)
		*p++ = *old;
	*p = '\0';		/* terminate it */

	/*
	 * Don't tell t_search we're looking for cmd if no '/' in the name so
	 * far but there are later - or it will look for *all* commands
	 */
	/* (*should* say "looking for directory" whenever '/' is next...) */
	retval = t_search(guess, p, SPELL, FILSIZ,
			  looking == TW_COMMAND && (foundslash || *old != '/') ?
			  TW_COMMAND : looking, 1, pat, suf);
	if (retval >= 4 || retval < 0)
	    return -1;		/* hopeless */
	for (p = ws; (*new = *p++) != '\0'; new++)
	    continue;
    }
/*NOTREACHED*/
#ifdef notdef
    return (0);			/* lint on the vax under mtXinu complains! */
#endif
}

#define EQ(s,t)	(StrQcmp(s,t) == 0)

/*
 * spdist() is taken from Kernighan & Pike,
 *  _The_UNIX_Programming_Environment_
 * and adapted somewhat to correspond better to psychological reality.
 * (Note the changes to the return values)
 *
 * According to Pollock and Zamora, CACM April 1984 (V. 27, No. 4),
 * page 363, the correct order for this is:
 * OMISSION = TRANSPOSITION > INSERTION > SUBSTITUTION
 * thus, it was exactly backwards in the old version. -- PWP
 */

int
spdist(s, t)
    Char *s, *t;
{
    for (; (*s & TRIM) == (*t & TRIM); t++, s++)
	if (*t == '\0')
	    return 0;		/* exact match */
    if (*s) {
	if (*t) {
	    if (s[1] && t[1] && (*s & TRIM) == (t[1] & TRIM) &&
		(*t & TRIM) == (s[1] & TRIM) && EQ(s + 2, t + 2))
		return 1;	/* transposition */
	    if (EQ(s + 1, t + 1))
		return 3;	/* 1 char mismatch */
	}
	if (EQ(s + 1, t))
	    return 2;		/* extra character */
    }
    if (*t && EQ(s, t + 1))
	return 1;		/* missing character */
    return 4;
}

int
spdir(extended_name, tilded_dir, item, name)
    Char   *extended_name;
    Char   *tilded_dir;
    Char   *item;
    Char   *name;
{
    Char    path[MAXPATHLEN + 1];
    Char   *s;
    Char    oldch;

    if (ISDOT(item) || ISDOTDOT(item))
	return 0;

    for (s = name; *s != 0 && (*s & TRIM) == (*item & TRIM); s++, item++)
	continue;
    if (*s == 0 || s[1] == 0 || *item != 0)
	return 0;

    (void) Strcpy(path, tilded_dir);
    oldch = *s;
    *s = '/';
    catn(path, name, (int) (sizeof(path) / sizeof(Char)));
    if (access(short2str(path), F_OK) == 0) {
	(void) Strcpy(extended_name, name);
	return 1;
    }
    *s = oldch;
    return 0;
}
