/*-
 * Copyright (c) 1993, 1994
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
static char sccsid[] = "@(#)ex_util.c	8.6 (Berkeley) 3/23/94";
#endif /* not lint */

#include <sys/types.h>
#include <queue.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <bitstring.h>
#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"

/*
 * ex_getline --
 *	Return a line from the terminal.
 */
int
ex_getline(sp, fp, lenp)
	SCR *sp;
	FILE *fp;
	size_t *lenp;
{
	EX_PRIVATE *exp;
	size_t off;
	int ch;
	char *p;

	exp = EXP(sp);
	for (off = 0, p = exp->ibp;; ++off) {
		ch = getc(fp);
		if (off >= exp->ibp_len) {
			BINC_RET(sp, exp->ibp, exp->ibp_len, off + 1);
			p = exp->ibp + off;
		}
		if (ch == EOF || ch == '\n') {
			if (ch == EOF && !off)
				return (1);
			*lenp = off;
			return (0);
		}
		*p++ = ch;
	}
	/* NOTREACHED */
}

/*
 * ex_sleave --
 *	Save the terminal/signal state, not screen modification time.
 * 	Specific to ex/filter.c and ex/ex_shell.c.
 */
int
ex_sleave(sp)
	SCR *sp;
{
	struct sigaction act;
	struct stat sb;
	EX_PRIVATE *exp;

	/* Ignore sessions not using tty's. */
	if (!F_ISSET(sp->gp, G_STDIN_TTY))
		return (1);
	
	/*
	 * The old terminal values almost certainly turn on VINTR, VQUIT and
	 * VSUSP.  We don't want to interrupt the parent(s), so we ignore
	 * VINTR.  VQUIT is ignored by main() because nvi never wants to catch
	 * it.  A VSUSP handler have been installed by the screen code.
	 */
	act.sa_handler = SIG_IGN;
	sigemptyset(&act.sa_mask);
	act.sa_flags = 0;

	exp = EXP(sp);
	if (sigaction(SIGINT, &act, &exp->leave_act)) {
		msgq(sp, M_SYSERR, "sigaction");
		return (1);
	}
	if (tcgetattr(STDIN_FILENO, &exp->leave_term)) {
		msgq(sp, M_SYSERR, "tcgetattr");
		goto err;
	}
	if (tcsetattr(STDIN_FILENO,
	    TCSANOW | TCSASOFT, &sp->gp->original_termios)) {
		msgq(sp, M_SYSERR, "tcsetattr");
		/*
		 * If an error occurs, back out the changes and run
		 * without interrupts.
		 */
err:		(void)sigaction(SIGINT, &exp->leave_act, NULL);
		return (1);
	}
	/*
	 * The process may write to the terminal.  Save the access time
	 * (read) and modification time (write) of the tty; if they have
	 * changed when we restore the modes, will have to refresh the
	 * screen.
	 */
	if (fstat(STDIN_FILENO, &sb)) {
		msgq(sp, M_SYSERR, "stat: stdin");
		exp->leave_atime = exp->leave_mtime = 0;
	} else {
		exp->leave_atime = sb.st_atime;
		exp->leave_mtime = sb.st_mtime;
	}
	return (0);
}

/*
 * ex_rleave --
 *	Return the terminal/signal state, not screen modification time.
 * 	Specific to ex/filter.c and ex/ex_shell.c.
 */
void
ex_rleave(sp)
	SCR *sp;
{
	EX_PRIVATE *exp;
	struct stat sb;

	exp = EXP(sp);

	/* Turn off interrupts. */
	if (tcsetattr(STDIN_FILENO, TCSANOW | TCSASOFT, &exp->leave_term))
		msgq(sp, M_SYSERR, "tcsetattr");

	/* Reset the signal state. */
	if (sigaction(SIGINT, &exp->leave_act, NULL))
		msgq(sp, M_SYSERR, "sigaction");

	/* If the terminal was used, refresh the screen. */
	if (fstat(STDIN_FILENO, &sb) || exp->leave_atime == 0 ||
	    exp->leave_atime != sb.st_atime || exp->leave_mtime != sb.st_mtime)
		F_SET(sp, S_REFRESH);
}
