/*
 * Copyright (c) 1988 Mark Nudleman
 * Copyright (c) 1988, 1993
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
static char sccsid[] = "@(#)output.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * High level routines dealing with the output to the screen.
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "less.h"

extern int bs_mode;
extern int sigs;
extern int sc_width, sc_height;
extern int ul_width, ue_width;
extern int so_width, se_width;
extern int bo_width, be_width;
extern int tabstop;
extern int screen_trashed;
extern char *line;
extern int horiz_off;
extern int mode_flags;

static int last_pos_highlighted = 0;

/* static markup()
 *
 * Output correct markup char; return number of columns eaten-up.
 * Called by put_line() just before doing any actual output.
 */
#define ENTER 1
#define ACQUIESCE 0  /* XXX Check actual def'n... */
#define EXIT -1

static
markup(ent_ul, ent_bo)
	int *ent_ul, *ent_bo;
{
	int retr;

	retr = 0;
	switch (*ent_ul) {
	case ENTER:
		ul_enter();
		retr += ul_width;
		break;
	case EXIT:
		ul_exit();
		retr += ue_width;
		break;
	}
	switch (*ent_bo) {
	case ENTER:
		bo_enter();
		retr += bo_width;
		break;
	case EXIT:
		bo_exit();
		retr += be_width;
		break;
	}
	*ent_ul = *ent_bo = ACQUIESCE;
	return retr;
}

/* put_line()
 *
 * Display the line which is in the line buffer.  The number of output
 * characters in the line buffer cannot exceed screen columns available.
 * Output characters in the line buffer that precede horiz_off are skipped.  
 * The caller may depend on this behaviour to ensure that the number of output
 * characters in the line buffer does not exceed the screen columns
 * available.
 *
 * This routine will get confused if the line buffer has non-sensical
 * UL_CHAR, UE_CHAR, BO_CHAR, BE_CHAR markups.
 */
#define MAYPUTCHR(char) \
	if (column >= horiz_off) { \
		column += markup(&ent_ul, &ent_bo); \
		putchr(char); \
	}

put_line()
{
	register char *p;
	register int c;
	register int column;
	extern int auto_wrap, ignaw;
	int ent_ul, ent_bo;  /* enter or exit ul|bo mode for next char */

	if (sigs)
	{
		/*
		 * Don't output if a signal is pending.
		 */
		screen_trashed = 1;
		return;
	}

	if (line == NULL)
		line = "";

	if (last_pos_highlighted)
	{
		clear_eol();
		last_pos_highlighted = 0;
	}
	column = 0;
	ent_ul = ent_bo = ACQUIESCE;
	for (p = line;  *p != '\0';  p++)
	{
		/*
		 * XXX line.c needs to be rewritten to store markup
		 * information as metadata associated with each character.
		 * This will make several things much nicer, fixing problems,
		 * etc.  Until then, this kludge will hold the fort well
		 * enough.
		 */
		switch ((char)(c = (unsigned char)*p))
		{
		case UL_CHAR:
			ent_ul = ENTER;
			break;
		case UE_CHAR:
			if (ent_ul != ENTER)
				ent_ul = EXIT;
			else
				ent_ul = ACQUIESCE;
			break;
		case BO_CHAR:
			ent_bo = ENTER;
			break;
		case BE_CHAR:
			if (ent_bo != ENTER)
				ent_bo = EXIT;
			else
				ent_bo = ACQUIESCE;
			break;
		case '\t':
			do
			{
				MAYPUTCHR(' ');
				column++;
			} while ((column % tabstop) != 0);
			break;
		case '\b':
			/*
			 * column must be at least one greater than
			 * horiz_off (ie. we must be in the second or
			 * beyond screen column) or we'll just end-up
			 * backspacing up to the previous line.
			 */
			if (column > horiz_off) {
				column += markup(&ent_ul, &ent_bo);
				putbs();
				column--;
			}
			break;
		case '\r':
			/* treat \r\n sequences like \n if -u flag not set. */
			if (bs_mode || p[1] != '\0')
			{
				/* -u was set, or this CR is not a CRLF, so
				 * treat this CR like any other control_char */
				MAYPUTCHR('^');
				column++;
				MAYPUTCHR(CARAT_CHAR(c));
				column++;
			}
			break;
		default:
			if (c == 0200 || CONTROL_CHAR(c))
			{
				c &= ~0200;
				MAYPUTCHR('^');
				column++;
				MAYPUTCHR(CARAT_CHAR(c));
				column++;
			} else
			{
				MAYPUTCHR(c);
				column++;
			}
		}
		if (column == sc_width + horiz_off && mode_flags)
			last_pos_highlighted = 1;
	}
	column += markup(&ent_ul, &ent_bo);
	if (column < sc_width + horiz_off || !auto_wrap || ignaw)
		putchr('\n');
}

static char obuf[2048];  /* just large enough for a full 25*80 screen */
static char *ob = obuf;

/*
 * Flush buffered output.
 */
flush()
{
	register int n;

	n = ob - obuf;
	if (n == 0)
		return;
	if (write(1, obuf, n) != n)
		screen_trashed = 1;
	ob = obuf;
}

/*
 * Purge any pending output.
 */
purge()
{

	ob = obuf;
}

/*
 * Output a character.
 */
putchr(c)
	int c;
{
	if (ob >= &obuf[sizeof(obuf)])
		flush();
	*ob++ = c;
}

/*
 * Output a string.
 */
putstr(s)
	register char *s;
{
	while (*s != '\0')
		putchr(*s++);
}

/*
 * Output a string, expanding control characters into printable sequences.
 * Returns the number of characters printed.
 */
int
putxstr(s)
	char *s;
{
	int c;
	int retr = 0;

	for (; c = *s; s++) {
		if (CONTROL_CHAR(c)) {
			putchr('^');
			retr++;
			c &= ~0200;
			c = CARAT_CHAR(c);
		}
		putchr(c);
	}

	return(retr);
}

static char intr_to_abort[] = "... (interrupt to abort)";

ierror(s)
	char *s;
{
	lower_left();
	clear_eol();
	so_enter();
	putstr(s);
	putstr(intr_to_abort);
	so_exit();
	flush();
}
