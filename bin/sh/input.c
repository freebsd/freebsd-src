/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
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
static char sccsid[] = "@(#)input.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*
 * This file implements the input routines used by the parser.
 */

#include <stdio.h>	/* defines BUFSIZ */
#include "shell.h"
#include <fcntl.h>
#include <errno.h>
#include "syntax.h"
#include "input.h"
#include "output.h"
#include "options.h"
#include "memalloc.h"
#include "error.h"
#include "alias.h"
#include "parser.h"
#include "myhistedit.h"

#define EOF_NLEFT -99		/* value of parsenleft when EOF pushed back */

MKINIT
struct strpush {
	struct strpush *prev;	/* preceding string on stack */
	char *prevstring;
	int prevnleft;
	struct alias *ap;	/* if push was associated with an alias */
};

/*
 * The parsefile structure pointed to by the global variable parsefile
 * contains information about the current file being read.
 */

MKINIT
struct parsefile {
	struct parsefile *prev;	/* preceding file on stack */
	int linno;		/* current line */
	int fd;			/* file descriptor (or -1 if string) */
	int nleft;		/* number of chars left in buffer */
	char *nextc;		/* next char in buffer */
	char *buf;		/* input buffer */
	struct strpush *strpush; /* for pushing strings at this level */
	struct strpush basestrpush; /* so pushing one is fast */
};


int plinno = 1;			/* input line number */
MKINIT int parsenleft;		/* copy of parsefile->nleft */
char *parsenextc;		/* copy of parsefile->nextc */
MKINIT struct parsefile basepf;	/* top level input file */
char basebuf[BUFSIZ];		/* buffer for top level input file */
struct parsefile *parsefile = &basepf;	/* current input file */
char *pushedstring;		/* copy of parsenextc when text pushed back */
int pushednleft;		/* copy of parsenleft when text pushed back */
int init_editline = 0;		/* editline library initialized? */
int whichprompt;		/* 1 == PS1, 2 == PS2 */

EditLine *el;			/* cookie for editline package */

#ifdef __STDC__
STATIC void pushfile(void);
#else
STATIC void pushfile();
#endif



#ifdef mkinit
INCLUDE "input.h"
INCLUDE "error.h"

INIT {
	extern char basebuf[];

	basepf.nextc = basepf.buf = basebuf;
}

RESET {
	if (exception != EXSHELLPROC)
		parsenleft = 0;            /* clear input buffer */
	popallfiles();
}

SHELLPROC {
	popallfiles();
}
#endif


/*
 * Read a line from the script.
 */

char *
pfgets(line, len)
	char *line;
	{
	register char *p = line;
	int nleft = len;
	int c;

	while (--nleft > 0) {
		c = pgetc_macro();
		if (c == PEOF) {
			if (p == line)
				return NULL;
			break;
		}
		*p++ = c;
		if (c == '\n')
			break;
	}
	*p = '\0';
	return line;
}



/*
 * Read a character from the script, returning PEOF on end of file.
 * Nul characters in the input are silently discarded.
 */

int
pgetc() {
	return pgetc_macro();
}


/*
 * Refill the input buffer and return the next input character:
 *
 * 1) If a string was pushed back on the input, pop it;
 * 2) If an EOF was pushed back (parsenleft == EOF_NLEFT) or we are reading
 *    from a string so we can't refill the buffer, return EOF.
 * 3) Call read to read in the characters.
 * 4) Delete all nul characters from the buffer.
 */

int
preadbuffer() {
	register char *p, *q;
	register int i;
	register int something;
	extern EditLine *el;

	if (parsefile->strpush) {
		popstring();
		if (--parsenleft >= 0)
			return (*parsenextc++);
	}
	if (parsenleft == EOF_NLEFT || parsefile->buf == NULL)
		return PEOF;
	flushout(&output);
	flushout(&errout);
retry:
	p = parsenextc = parsefile->buf;
	if (parsefile->fd == 0 && el) {
		const char *rl_cp;
		int len;

		rl_cp = el_gets(el, &len);
		if (rl_cp == NULL) {
			i = 0;
			goto eof;
		}
		strcpy(p, rl_cp);  /* XXX - BUFSIZE should redesign so not necessary */
		i = len;

	} else {
regular_read:
		i = read(parsefile->fd, p, BUFSIZ - 1);
	}
eof:
	if (i <= 0) {
                if (i < 0) {
                        if (errno == EINTR)
                                goto retry;
                        if (parsefile->fd == 0 && errno == EWOULDBLOCK) {
                                int flags = fcntl(0, F_GETFL, 0);
                                if (flags >= 0 && flags & O_NONBLOCK) {
                                        flags &=~ O_NONBLOCK;
                                        if (fcntl(0, F_SETFL, flags) >= 0) {
						out2str("sh: turning off NDELAY mode\n");
                                                goto retry;
                                        }
                                }
                        }
                }
                parsenleft = EOF_NLEFT;
                return PEOF;
	}
	parsenleft = i - 1;	/* we're returning one char in this call */

	/* delete nul characters */
	something = 0;
	for (;;) {
		if (*p == '\0')
			break;
		if (*p != ' ' && *p != '\t' && *p != '\n')
			something = 1;
		p++;
		if (--i <= 0) {
			*p = '\0';
			goto done;		/* no nul characters */
		}
	}
	/*
	 * remove nuls
	 */
	q = p++;
	while (--i > 0) {
		if (*p != '\0')
			*q++ = *p;
		p++;
	}
	*q = '\0';
	if (q == parsefile->buf)
		goto retry;			/* buffer contained nothing but nuls */
	parsenleft = q - parsefile->buf - 1;

done:
	if (parsefile->fd == 0 && hist && something) {
		INTOFF;
		history(hist, whichprompt == 1 ? H_ENTER : H_ADD, 
			   parsefile->buf);
		INTON;
	}
	if (vflag) {
		/*
		 * This isn't right.  Most shells coordinate it with
		 * reading a line at a time.  I honestly don't know if its
		 * worth it.
		 */
		i = parsenleft + 1;
		p = parsefile->buf;
		for (; i--; p++) 
			out2c(*p)
		flushout(out2);
	}
	return *parsenextc++;
}

/*
 * Undo the last call to pgetc.  Only one character may be pushed back.
 * PEOF may be pushed back.
 */

void
pungetc() {
	parsenleft++;
	parsenextc--;
}

/*
 * Push a string back onto the input at this current parsefile level.
 * We handle aliases this way.
 */
void
pushstring(s, len, ap)
	char *s;
	int len;
	void *ap;
	{
	struct strpush *sp;

	INTOFF;
/*dprintf("*** calling pushstring: %s, %d\n", s, len);*/
	if (parsefile->strpush) {
		sp = ckmalloc(sizeof (struct strpush));
		sp->prev = parsefile->strpush;
		parsefile->strpush = sp;
	} else
		sp = parsefile->strpush = &(parsefile->basestrpush);
	sp->prevstring = parsenextc;
	sp->prevnleft = parsenleft;
	sp->ap = (struct alias *)ap;
	if (ap)
		((struct alias *)ap)->flag |= ALIASINUSE;
	parsenextc = s;
	parsenleft = len;
	INTON;
}

popstring()
{
	struct strpush *sp = parsefile->strpush;

	INTOFF;
	parsenextc = sp->prevstring;
	parsenleft = sp->prevnleft;
/*dprintf("*** calling popstring: restoring to '%s'\n", parsenextc);*/
	if (sp->ap)
		sp->ap->flag &= ~ALIASINUSE;
	parsefile->strpush = sp->prev;
	if (sp != &(parsefile->basestrpush))
		ckfree(sp);
	INTON;
}

/*
 * Set the input to take input from a file.  If push is set, push the
 * old input onto the stack first.
 */

void
setinputfile(fname, push)
	char *fname;
	{
	int fd;
	int fd2;

	INTOFF;
	if ((fd = open(fname, O_RDONLY)) < 0)
		error("Can't open %s", fname);
	if (fd < 10) {
		fd2 = copyfd(fd, 10);
		close(fd);
		if (fd2 < 0)
			error("Out of file descriptors");
		fd = fd2;
	}
	setinputfd(fd, push);
	INTON;
}


/*
 * Like setinputfile, but takes an open file descriptor.  Call this with
 * interrupts off.
 */

void
setinputfd(fd, push) {
	if (push) {
		pushfile();
		parsefile->buf = ckmalloc(BUFSIZ);
	}
	if (parsefile->fd > 0)
		close(parsefile->fd);
	parsefile->fd = fd;
	if (parsefile->buf == NULL)
		parsefile->buf = ckmalloc(BUFSIZ);
	parsenleft = 0;
	plinno = 1;
}


/*
 * Like setinputfile, but takes input from a string.
 */

void
setinputstring(string, push)
	char *string;
	{
	INTOFF;
	if (push)
		pushfile();
	parsenextc = string;
	parsenleft = strlen(string);
	parsefile->buf = NULL;
	plinno = 1;
	INTON;
}



/*
 * To handle the "." command, a stack of input files is used.  Pushfile
 * adds a new entry to the stack and popfile restores the previous level.
 */

STATIC void
pushfile() {
	struct parsefile *pf;

	parsefile->nleft = parsenleft;
	parsefile->nextc = parsenextc;
	parsefile->linno = plinno;
	pf = (struct parsefile *)ckmalloc(sizeof (struct parsefile));
	pf->prev = parsefile;
	pf->fd = -1;
	pf->strpush = NULL;
	pf->basestrpush.prev = NULL;
	parsefile = pf;
}


void
popfile() {
	struct parsefile *pf = parsefile;

	INTOFF;
	if (pf->fd >= 0)
		close(pf->fd);
	if (pf->buf)
		ckfree(pf->buf);
	while (pf->strpush)
		popstring();
	parsefile = pf->prev;
	ckfree(pf);
	parsenleft = parsefile->nleft;
	parsenextc = parsefile->nextc;
	plinno = parsefile->linno;
	INTON;
}


/*
 * Return to top level.
 */

void
popallfiles() {
	while (parsefile != &basepf)
		popfile();
}



/*
 * Close the file(s) that the shell is reading commands from.  Called
 * after a fork is done.
 */

void
closescript() {
	popallfiles();
	if (parsefile->fd > 0) {
		close(parsefile->fd);
		parsefile->fd = 0;
	}
}
