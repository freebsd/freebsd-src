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
static char sccsid[] = "@(#)wwspawn.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] = "@(#)$FreeBSD$";
#endif /* not lint */

#include "ww.h"
#include <signal.h>

/*
 * There is a dead lock with vfork and closing of pseudo-ports.
 * So we have to be sneaky about error reporting.
 */
wwspawn(wp, file, argv)
register struct ww *wp;
char *file;
char **argv;
{
	int pid;
	int ret;
	char erred = 0;
	int s;

	s = sigblock(sigmask(SIGCHLD));
	switch (pid = vfork()) {
	case -1:
		wwerrno = WWE_SYS;
		ret = -1;
		break;
	case 0:
		if (wwenviron(wp) >= 0)
			execvp(file, argv);
		erred = 1;
		_exit(1);
	default:
		if (erred) {
			wwerrno = WWE_SYS;
			ret = -1;
		} else {
			wp->ww_pid = pid;
			wp->ww_state = WWS_HASPROC;
			ret = pid;
		}
	}
	(void) sigsetmask(s);
	if (wp->ww_socket >= 0) {
		(void) close(wp->ww_socket);
		wp->ww_socket = -1;
	}
	return ret;
}
