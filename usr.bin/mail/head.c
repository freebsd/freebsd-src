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
static char sccsid[] = "@(#)head.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "rcv.h"
#include "extern.h"

/*
 * Mail -- a mail program
 *
 * Routines for processing and detecting headlines.
 */

/*
 * See if the passed line buffer is a mail header.
 * Return true if yes.  Note the extreme pains to
 * accomodate all funny formats.
 */
int
ishead(linebuf)
	char linebuf[];
{
	register char *cp;
	struct headline hl;
	char parbuf[BUFSIZ];

	cp = linebuf;
	if (*cp++ != 'F' || *cp++ != 'r' || *cp++ != 'o' || *cp++ != 'm' ||
	    *cp++ != ' ')
		return (0);
	parse(linebuf, &hl, parbuf);
	if (hl.l_from == NOSTR || hl.l_date == NOSTR) {
		fail(linebuf, "No from or date field");
		return (0);
	}
	if (!isdate(hl.l_date)) {
		fail(linebuf, "Date field not legal date");
		return (0);
	}
	/*
	 * I guess we got it!
	 */
	return (1);
}

/*ARGSUSED*/
void
fail(linebuf, reason)
	char linebuf[], reason[];
{

	/*
	if (value("debug") == NOSTR)
		return;
	fprintf(stderr, "\"%s\"\nnot a header because %s\n", linebuf, reason);
	*/
}

/*
 * Split a headline into its useful components.
 * Copy the line into dynamic string space, then set
 * pointers into the copied line in the passed headline
 * structure.  Actually, it scans.
 */
void
parse(line, hl, pbuf)
	char line[], pbuf[];
	register struct headline *hl;
{
	register char *cp;
	char *sp;
	char word[LINESIZE];

	hl->l_from = NOSTR;
	hl->l_tty = NOSTR;
	hl->l_date = NOSTR;
	cp = line;
	sp = pbuf;
	/*
	 * Skip over "From" first.
	 */
	cp = nextword(cp, word);
	cp = nextword(cp, word);
	if (*word)
		hl->l_from = copyin(word, &sp);
	if (cp != NOSTR && cp[0] == 't' && cp[1] == 't' && cp[2] == 'y') {
		cp = nextword(cp, word);
		hl->l_tty = copyin(word, &sp);
	}
	if (cp != NOSTR)
		hl->l_date = copyin(cp, &sp);
}

/*
 * Copy the string on the left into the string on the right
 * and bump the right (reference) string pointer by the length.
 * Thus, dynamically allocate space in the right string, copying
 * the left string into it.
 */
char *
copyin(src, space)
	register char *src;
	char **space;
{
	register char *cp;
	char *top;

	top = cp = *space;
	while (*cp++ = *src++)
		;
	*space = cp;
	return (top);
}

/*
 * Test to see if the passed string is a ctime(3) generated
 * date string as documented in the manual.  The template
 * below is used as the criterion of correctness.
 * Also, we check for a possible trailing time zone using
 * the tmztype template.
 */

/*
 * 'A'	An upper case char
 * 'a'	A lower case char
 * ' '	A space
 * '0'	A digit
 * 'O'	An optional digit or space
 * ':'	A colon
 * 'N'	A new line
 */
char ctype[] = "Aaa Aaa O0 00:00:00 0000";
char tmztype[] = "Aaa Aaa O0 00:00:00 AAA 0000";

int
isdate(date)
	char date[];
{

	return cmatch(date, ctype) || cmatch(date, tmztype);
}

/*
 * Match the given string (cp) against the given template (tp).
 * Return 1 if they match, 0 if they don't
 */
int
cmatch(cp, tp)
	register char *cp, *tp;
{

	while (*cp && *tp)
		switch (*tp++) {
		case 'a':
			if (!islower(*cp++))
				return 0;
			break;
		case 'A':
			if (!isupper(*cp++))
				return 0;
			break;
		case ' ':
			if (*cp++ != ' ')
				return 0;
			break;
		case '0':
			if (!isdigit(*cp++))
				return 0;
			break;
		case 'O':
			if (*cp != ' ' && !isdigit(*cp))
				return 0;
			cp++;
			break;
		case ':':
			if (*cp++ != ':')
				return 0;
			break;
		case 'N':
			if (*cp++ != '\n')
				return 0;
			break;
		}
	if (*cp || *tp)
		return 0;
	return (1);
}

/*
 * Collect a liberal (space, tab delimited) word into the word buffer
 * passed.  Also, return a pointer to the next word following that,
 * or NOSTR if none follow.
 */
char *
nextword(wp, wbuf)
	register char *wp, *wbuf;
{
	register c;

	if (wp == NOSTR) {
		*wbuf = 0;
		return (NOSTR);
	}
	while ((c = *wp++) && c != ' ' && c != '\t') {
		*wbuf++ = c;
		if (c == '"') {
 			while ((c = *wp++) && c != '"')
 				*wbuf++ = c;
 			if (c == '"')
 				*wbuf++ = c;
			else
				wp--;
 		}
	}
	*wbuf = '\0';
	for (; c == ' ' || c == '\t'; c = *wp++)
		;
	if (c == 0)
		return (NOSTR);
	return (wp - 1);
}
