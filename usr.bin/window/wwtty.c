/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
static char sccsid[] = "@(#)wwtty.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] = "@(#)$FreeBSD$";
#endif /* not lint */

#include "ww.h"
#include <sys/types.h>
#include <fcntl.h>
#if !defined(OLD_TTY) && !defined(TIOCGWINSZ)
#include <sys/ioctl.h>
#endif

wwgettty(d, t)
register struct ww_tty *t;
{
#ifdef OLD_TTY
	if (ioctl(d, TIOCGETP, (char *)&t->ww_sgttyb) < 0)
		goto bad;
	if (ioctl(d, TIOCGETC, (char *)&t->ww_tchars) < 0)
		goto bad;
	if (ioctl(d, TIOCGLTC, (char *)&t->ww_ltchars) < 0)
		goto bad;
	if (ioctl(d, TIOCLGET, (char *)&t->ww_lmode) < 0)
		goto bad;
	if (ioctl(d, TIOCGETD, (char *)&t->ww_ldisc) < 0)
		goto bad;
#else
	if (tcgetattr(d, &t->ww_termios) < 0)
		goto bad;
#endif
	if ((t->ww_fflags = fcntl(d, F_GETFL, 0)) < 0)
		goto bad;
	return 0;
bad:
	wwerrno = WWE_SYS;
	return -1;
}

/*
 * Set the modes of tty 'd' to 't'
 * 'o' is the current modes.  We set the line discipline only if
 * it changes, to avoid unnecessary flushing of typeahead.
 */
wwsettty(d, t)
register struct ww_tty *t;
{
#ifdef OLD_TTY
	int i;

	/* XXX, for buggy tty drivers that don't wait for output to drain */
	while (ioctl(d, TIOCOUTQ, &i) >= 0 && i > 0)
		usleep(100000);
	if (ioctl(d, TIOCSETN, (char *)&t->ww_sgttyb) < 0)
		goto bad;
	if (ioctl(d, TIOCSETC, (char *)&t->ww_tchars) < 0)
		goto bad;
	if (ioctl(d, TIOCSLTC, (char *)&t->ww_ltchars) < 0)
		goto bad;
	if (ioctl(d, TIOCLSET, (char *)&t->ww_lmode) < 0)
		goto bad;
	if (ioctl(d, TIOCGETD, (char *)&i) < 0)
		goto bad;
	if (t->ww_ldisc != i &&
	    ioctl(d, TIOCSETD, (char *)&t->ww_ldisc) < 0)
		goto bad;
#else
#ifdef sun
	/* XXX, for buggy tty drivers that don't wait for output to drain */
	(void) tcdrain(d);
#endif
	if (tcsetattr(d, TCSADRAIN, &t->ww_termios) < 0)
		goto bad;
#endif
	if (fcntl(d, F_SETFL, t->ww_fflags) < 0)
		goto bad;
	return 0;
bad:
	wwerrno = WWE_SYS;
	return -1;
}

/*
 * The ttysize and stop-start routines must also work
 * on the control side of pseudoterminals.
 */

wwgetttysize(d, r, c)
	int *r, *c;
{
	struct winsize winsize;

	if (ioctl(d, TIOCGWINSZ, (char *)&winsize) < 0) {
		wwerrno = WWE_SYS;
		return -1;
	}
	if (winsize.ws_row != 0)
		*r = winsize.ws_row;
	if (winsize.ws_col != 0)
		*c = winsize.ws_col;
	return 0;
}

wwsetttysize(d, r, c)
{
	struct winsize winsize;

	winsize.ws_row = r;
	winsize.ws_col = c;
	winsize.ws_xpixel = winsize.ws_ypixel = 0;
	if (ioctl(d, TIOCSWINSZ, (char *)&winsize) < 0) {
		wwerrno = WWE_SYS;
		return -1;
	}
	return 0;
}

wwstoptty(d)
{
#if !defined(OLD_TTY) && defined(TCOOFF)
	/* not guaranteed to work on the pty side */
	if (tcflow(d, TCOOFF) < 0)
#else
	if (ioctl(d, TIOCSTOP, (char *)0) < 0)
#endif
	{
		wwerrno = WWE_SYS;
		return -1;
	}
	return 0;
}

wwstarttty(d)
{
#if !defined(OLD_TTY) && defined(TCOON)
	/* not guaranteed to work on the pty side */
	if (tcflow(d, TCOON) < 0)
#else
	if (ioctl(d, TIOCSTART, (char *)0) < 0)
#endif
	{
		wwerrno = WWE_SYS;
		return -1;
	}
	return 0;
}
