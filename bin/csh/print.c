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
static char sccsid[] = "@(#)print.c	5.11 (Berkeley) 6/8/91";
#endif /* not lint */

#include <sys/types.h>
#include <unistd.h>
#if __STDC__
# include <stdarg.h>
#else
# include <varargs.h>
#endif

#include "csh.h"
#include "extern.h"

extern int Tty_eight_bit;
extern int Tty_raw_mode;
extern Char GettingInput;

int     lbuffed = 1;		/* true if line buffered */

static void	p2dig __P((int));

void
psecs(l)
    long    l;
{
    register int i;

    i = l / 3600;
    if (i) {
	xprintf("%d:", i);
	i = l % 3600;
	p2dig(i / 60);
	goto minsec;
    }
    i = l;
    xprintf("%d", i / 60);
minsec:
    i %= 60;
    xprintf(":");
    p2dig(i);
}

void
pcsecs(l)			/* PWP: print mm:ss.dd, l is in sec*100 */
    long    l;
{
    register int i;

    i = l / 360000;
    if (i) {
	xprintf("%d:", i);
	i = (l % 360000) / 100;
	p2dig(i / 60);
	goto minsec;
    }
    i = l / 100;
    xprintf("%d", i / 60);
minsec:
    i %= 60;
    xprintf(":");
    p2dig(i);
    xprintf(".");
    p2dig((int) (l % 100));
}

static void
p2dig(i)
    register int i;
{

    xprintf("%d%d", i / 10, i % 10);
}

char    linbuf[2048];
char   *linp = linbuf;
bool    output_raw = 0;		/* PWP */

void
xputchar(c)
    register int c;
{
    c &= CHAR | QUOTE;
    if (!output_raw && (c & QUOTE) == 0) {
	if (Iscntrl(c)) {
	    if (c != '\t' && c != '\n' && c != '\r') {
		xputchar('^');
		if (c == ASCII)
		    c = '?';
		else
		    c |= 0100;
	    }
	}
	else if (!Isprint(c)) {
	    xputchar('\\');
	    xputchar((((c >> 6) & 7) + '0'));
	    xputchar((((c >> 3) & 7) + '0'));
	    c = (c & 7) + '0';
	}
	(void) putraw(c);
    }
    else {
	c &= TRIM;
	(void) putpure(c);
    }
    if (lbuffed && (c & CHAR) == '\n')
	flush();
}

int
putraw(c)
    register int c;
{
    return putpure(c);
}

int
putpure(c)
    register int c;
{
    c &= CHAR;

    *linp++ = c;
    if (linp >= &linbuf[sizeof linbuf - 10])
	flush();
    return (1);
}

void
draino()
{
    linp = linbuf;
}

void
flush()
{
    register int unit;

    /* int lmode; */

    if (linp == linbuf)
	return;
#ifdef notdef
    if (linp < &linbuf[sizeof linbuf - 10])
	return;
#endif
    if (haderr)
	unit = didfds ? 2 : SHDIAG;
    else
	unit = didfds ? 1 : SHOUT;
    (void) write(unit, linbuf, (size_t) (linp - linbuf));
    linp = linbuf;
}
