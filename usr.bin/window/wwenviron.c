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
static char sccsid[] = "@(#)wwenviron.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "ww.h"
#if !defined(OLD_TTY) && !defined(TIOCSCTTY) && !defined(TIOCNOTTY)
#include <sys/ioctl.h>
#endif
#include <signal.h>

/*
 * Set up the environment of this process to run in window 'wp'.
 */
wwenviron(wp)
register struct ww *wp;
{
	register i;
#ifndef TIOCSCTTY
	int pgrp = getpid();
#endif
	char buf[1024];

#ifndef TIOCSCTTY
	if ((i = open("/dev/tty", 0)) < 0)
		goto bad;
	if (ioctl(i, TIOCNOTTY, (char *)0) < 0)
		goto bad;
	(void) close(i);
#endif
	if ((i = wp->ww_socket) < 0) {
		if ((i = open(wp->ww_ttyname, 2)) < 0)
			goto bad;
		if (wwsettty(i, &wwwintty) < 0)
			goto bad;
		if (wwsetttysize(i, wp->ww_w.nr, wp->ww_w.nc) < 0)
			goto bad;
	}
	(void) dup2(i, 0);
	(void) dup2(i, 1);
	(void) dup2(i, 2);
	(void) close(i);
#ifdef TIOCSCTTY
	(void) setsid();
	(void) ioctl(0, TIOCSCTTY, 0);
#else
	(void) ioctl(0, TIOCSPGRP, (char *)&pgrp);
	(void) setpgrp(pgrp, pgrp);
#endif
	/* SIGPIPE is the only one we ignore */
	(void) signal(SIGPIPE, SIG_DFL);
	(void) sigsetmask(0);
	/*
	 * Two conditions that make destructive setenv ok:
	 * 1. setenv() copies the string,
	 * 2. we've already called tgetent which copies the termcap entry.
	 */
	(void) sprintf(buf, "%sco#%d:li#%d:%s",
		WWT_TERMCAP, wp->ww_w.nc, wp->ww_w.nr, wwwintermcap);
	(void) setenv("TERMCAP", buf, 1);
	(void) sprintf(buf, "%d", wp->ww_id + 1);
	(void) setenv("WINDOW_ID", buf, 1);
	return 0;
bad:
	wwerrno = WWE_SYS;
	return -1;
}
