/*
 * Copyright (c) 1981 Regents of the University of California.
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
static char sccsid[] = "@(#)printw.c	5.8 (Berkeley) 4/15/91";
#endif /* not lint */

/*
 * printw and friends.
 *
 * These routines make nonportable assumptions about varargs if __STDC__
 * is not in effect.
 */

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "curses.ext"

/*
 *	This routine implements a printf on the standard screen.
 */
#if __STDC__
printw(const char *fmt, ...)
#else
printw(fmt, va_alist)
	char *fmt;
	va_dcl
#endif
{
	va_list	ap;
	int	ret;

#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	ret = _sprintw(stdscr, fmt, ap);
	va_end(ap);
	return (ret);
}

/*
 *	This routine implements a printf on the given window.
 */
#if __STDC__
wprintw(WINDOW *win, const char *fmt, ...)
#else
wprintw(win, fmt, va_alist)
	WINDOW *win;
	char *fmt;
	va_dcl
#endif
{
	va_list	ap;
	int	ret;

#ifdef __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	ret = _sprintw(win, fmt, ap);
	va_end(ap);
	return (ret);
}

/*
 *	Internal write-buffer-to-window function.
 */
static int
_winwrite(cookie, buf, n)
	void *cookie;
	register char *buf;
	int n;
{
	register WINDOW *win = (WINDOW *)cookie;
	register int c = n;

	while (--c >= 0) {
		if (waddch(win, (unsigned char) *buf++) == ERR)
			return (-1);
	}
	return n;
}

/*
 *	This routine actually executes the printf and adds it to the window.
 *	It must not be declared static as it is used in mvprintw.c.
 *	THIS SHOULD BE RENAMED vwprintw AND EXPORTED
 */
_sprintw(win, fmt, ap)
	WINDOW *win;
#if __STDC__
	const char *fmt;
#else
	char *fmt;
#endif
	va_list	ap;
{
	FILE *f;

	if ((f = fwopen((void *)win, _winwrite)) == NULL)
		return ERR;
	(void) vfprintf(f, fmt, ap);
	return fclose(f) ? ERR : OK;
}
