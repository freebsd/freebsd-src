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
static char sccsid[] = "@(#)wwpty.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "ww.h"
#include <fcntl.h>
#if !defined(OLD_TTY) && !defined(TIOCPKT)
#include <sys/ioctl.h>
#endif

wwgetpty(w)
register struct ww *w;
{
	register char c, *p;
	int tty;
	int on = 1;
#define PTY "/dev/XtyXX"
#define _PT	5
#define _PQRS	8
#define _0_9	9

	(void) strcpy(w->ww_ttyname, PTY);
	for (c = 'p'; c <= 'u'; c++) {
		w->ww_ttyname[_PT] = 'p';
		w->ww_ttyname[_PQRS] = c;
		w->ww_ttyname[_0_9] = '0';
		if (access(w->ww_ttyname, 0) < 0)
			break;
		for (p = "0123456789abcdef"; *p; p++) {
			w->ww_ttyname[_PT] = 'p';
			w->ww_ttyname[_0_9] = *p;
			if ((w->ww_pty = open(w->ww_ttyname, 2)) < 0)
				continue;
			w->ww_ttyname[_PT] = 't';
			if ((tty = open(w->ww_ttyname, 2)) < 0) {
				(void) close(w->ww_pty);
				continue;
			}
			(void) close(tty);
			if (ioctl(w->ww_pty, TIOCPKT, (char *)&on) < 0) {
				(void) close(w->ww_pty);
				continue;
			}
			(void) fcntl(w->ww_pty, F_SETFD, 1);
			return 0;
		}
	}
	w->ww_pty = -1;
	wwerrno = WWE_NOPTY;
	return -1;
}
