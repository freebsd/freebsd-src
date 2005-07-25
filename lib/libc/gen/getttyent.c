/*
 * Copyright (c) 1989, 1993
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
 * $FreeBSD$
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)getttyent.c	8.1 (Berkeley) 6/4/93";
#endif /* LIBC_SCCS and not lint */

#include <ttyent.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

static char zapchar;
static FILE *tf;
static size_t lbsize;
static char *line;

#define	MALLOCCHUNK	100

static char *skip __P((char *));
static char *value __P((char *));

struct ttyent *
getttynam(tty)
	const char *tty;
{
	register struct ttyent *t;

	if (strncmp(tty, "/dev/", 5) == 0)
		tty += 5;
	setttyent();
	while ( (t = getttyent()) )
		if (!strcmp(tty, t->ty_name))
			break;
	endttyent();
	return (t);
}

struct ttyent *
getttyent()
{
	static struct ttyent tty;
	register char *p;
	register int c;
	size_t i;

	if (!tf && !setttyent())
		return (NULL);
	for (;;) {
		if (!fgets(p = line, lbsize, tf))
			return (NULL);
		/* extend buffer if line was too big, and retry */
		while (!index(p, '\n')) {
			i = strlen(p);
			lbsize += MALLOCCHUNK;
			if ((p = realloc(line, lbsize)) == NULL) {
				(void)endttyent();
				return (NULL);
			}
			line = p;
			if (!fgets(&line[i], lbsize - i, tf))
				return (NULL);
		}
		while (isspace((unsigned char)*p))
			++p;
		if (*p && *p != '#')
			break;
	}

#define scmp(e) !strncmp(p, e, sizeof(e) - 1) && isspace((unsigned char)p[sizeof(e) - 1])
#define	vcmp(e)	!strncmp(p, e, sizeof(e) - 1) && p[sizeof(e) - 1] == '='

	zapchar = 0;
	tty.ty_name = p;
	tty.ty_status = 0;
	tty.ty_window = NULL;
	tty.ty_group  = _TTYS_NOGROUP;

	p = skip(p);
	if (!*(tty.ty_getty = p))
		tty.ty_getty = tty.ty_type = NULL;
	else {
		p = skip(p);
		if (!*(tty.ty_type = p))
			tty.ty_type = NULL;
		else {
			/* compatibility kludge: handle network/dialup specially */
			if (scmp(_TTYS_DIALUP))
				tty.ty_status |= TTY_DIALUP;
			else if (scmp(_TTYS_NETWORK))
				tty.ty_status |= TTY_NETWORK;
			p = skip(p);
		}
	}

	for (; *p; p = skip(p)) {
		if (scmp(_TTYS_OFF))
			tty.ty_status &= ~TTY_ON;
		else if (scmp(_TTYS_ON))
			tty.ty_status |= TTY_ON;
		else if (scmp(_TTYS_SECURE))
			tty.ty_status |= TTY_SECURE;
		else if (scmp(_TTYS_INSECURE))
			tty.ty_status &= ~TTY_SECURE;
		else if (scmp(_TTYS_DIALUP))
			tty.ty_status |= TTY_DIALUP;
		else if (scmp(_TTYS_NETWORK))
			tty.ty_status |= TTY_NETWORK;
		else if (vcmp(_TTYS_WINDOW))
			tty.ty_window = value(p);
		else if (vcmp(_TTYS_GROUP))
			tty.ty_group = value(p);
		else
			break;
	}

	if (zapchar == '#' || *p == '#')
		while ((c = *++p) == ' ' || c == '\t')
			;
	tty.ty_comment = p;
	if (*p == 0)
		tty.ty_comment = 0;
	if ( (p = index(p, '\n')) )
		*p = '\0';
	return (&tty);
}

#define	QUOTED	1

/*
 * Skip over the current field, removing quotes, and return a pointer to
 * the next field.
 */
static char *
skip(p)
	register char *p;
{
	register char *t;
	register int c, q;

	for (q = 0, t = p; (c = *p) != '\0'; p++) {
		if (c == '"') {
			q ^= QUOTED;	/* obscure, but nice */
			continue;
		}
		if (q == QUOTED && *p == '\\' && *(p+1) == '"')
			p++;
		*t++ = *p;
		if (q == QUOTED)
			continue;
		if (c == '#') {
			zapchar = c;
			*p = 0;
			break;
		}
		if (c == '\t' || c == ' ' || c == '\n') {
			zapchar = c;
			*p++ = 0;
			while ((c = *p) == '\t' || c == ' ' || c == '\n')
				p++;
			break;
		}
	}
	*--t = '\0';
	return (p);
}

static char *
value(p)
	register char *p;
{

	return ((p = index(p, '=')) ? ++p : NULL);
}

int
setttyent()
{

	if (line == NULL) {
		if ((line = malloc(MALLOCCHUNK)) == NULL)
			return (0);
		lbsize = MALLOCCHUNK;
	}
	if (tf) {
		rewind(tf);
		return (1);
	} else if ( (tf = fopen(_PATH_TTYS, "r")) )
		return (1);
	return (0);
}

int
endttyent()
{
	int rval;

	/*
         * NB: Don't free `line' because getttynam()
	 * may still be referencing it
	 */
	if (tf) {
		rval = (fclose(tf) != EOF);
		tf = NULL;
		return (rval);
	}
	return (1);
}

static int
isttystat(tty, flag)
	const char *tty;
	int flag;
{
	register struct ttyent *t;

	return ((t = getttynam(tty)) == NULL) ? 0 : !!(t->ty_status & flag);
}


int
isdialuptty(tty)
	const char *tty;
{

	return isttystat(tty, TTY_DIALUP);
}

int isnettty(tty)
	const char *tty;
{

	return isttystat(tty, TTY_NETWORK);
}
