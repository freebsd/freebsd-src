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
static char sccsid[] = "@(#)mvscanw.c	5.5 (Berkeley) 4/15/91";
#endif /* not lint */

#if __STDC__
#include <stdarg.h>
#else
#include <varargs.h>
#endif
#include "curses.ext"

/*
 * implement the mvscanw commands.  Due to the variable number of
 * arguments, they cannot be macros.  Another sigh....
 */

#if __STDC__
mvscanw(reg int y, reg int x, const char *fmt, ...)
#else
mvscanw(y, x, fmt, va_alist)
	reg int y, x;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int ret;

	if (move(y, x) != OK)
		return ERR;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	ret = _sscans(stdscr, fmt, ap);
	va_end(ap);
	return ret;
}

#if __STDC__
mvwscanw(reg WINDOW *win, reg int y, reg int x, const char *fmt, ...)
#else
mvwscanw(win, y, x, fmt, va_alist)
	reg WINDOW *win;
	reg int y, x;
	char *fmt;
	va_dcl
#endif
{
	va_list ap;
	int ret;

	if (move(y, x) != OK)
		return ERR;
#if __STDC__
	va_start(ap, fmt);
#else
	va_start(ap);
#endif
	ret = _sscans(win, fmt, ap);
	va_end(ap);
	return ret;
}
