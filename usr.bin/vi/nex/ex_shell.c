/*-
 * Copyright (c) 1992, 1993
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
static char sccsid[] = "@(#)ex_shell.c	8.17 (Berkeley) 12/23/93";
#endif /* not lint */

#include <sys/param.h>
#include <sys/stat.h>

#include <curses.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "vi.h"
#include "excmd.h"
#include "../svi/svi_screen.h"

/*
 * ex_shell -- :sh[ell]
 *	Invoke the program named in the SHELL environment variable
 *	with the argument -i.
 */
int
ex_shell(sp, ep, cmdp)
	SCR *sp;
	EXF *ep;
	EXCMDARG *cmdp;
{
	char buf[MAXPATHLEN];

	(void)snprintf(buf, sizeof(buf), "%s -i", O_STR(sp, O_SHELL));
	return (ex_exec_proc(sp, buf, "\n", NULL));
}

/*
 * ex_exec_proc --
 *	Run a separate process.
 */
int
ex_exec_proc(sp, cmd, p1, p2)
	SCR *sp;
	char *cmd, *p1, *p2;
{
	struct sigaction act, oact;
	struct stat osb, sb;
	struct termios term;
	const char *name;
	pid_t pid;
	int isig, rval;

	/* Clear the rest of the screen. */
	if (sp->s_clear(sp))
		return (1);

	/* Save ex/vi terminal settings, and restore the original ones. */
	EX_LEAVE(sp, isig, act, oact, sb, osb, term);

	/* Put out various messages. */
	if (p1 != NULL)
		(void)write(STDOUT_FILENO, p1, strlen(p1));
	if (p2 != NULL)
		(void)write(STDOUT_FILENO, p2, strlen(p2));


	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "vfork");
		rval = 1;
		goto err;
	case 0:				/* Utility. */
		/*
		 * The utility has default signal behavior.  Don't bother
		 * using sigaction(2) 'cause we want the default behavior.
		 */
		(void)signal(SIGINT, SIG_DFL);
		(void)signal(SIGQUIT, SIG_DFL);

		if ((name = strrchr(O_STR(sp, O_SHELL), '/')) == NULL)
			name = O_STR(sp, O_SHELL);
		else
			++name;
		execl(O_STR(sp, O_SHELL), name, "-c", cmd, NULL);
		msgq(sp, M_ERR, "Error: execl: %s: %s",
		    O_STR(sp, O_SHELL), strerror(errno));
		_exit(127);
		/* NOTREACHED */
	}

	rval = proc_wait(sp, (long)pid, cmd, 0);

	/* Restore ex/vi terminal settings. */
err:	EX_RETURN(sp, isig, act, oact, sb, osb, term);

	/*
	 * XXX
	 * EX_LEAVE/EX_RETURN only give us 1-second resolution on the tty
	 * changes.  A fast '!' command, e.g. ":!pwd" can beat us to the
	 * refresh.  When there's better resolution from the stat(2) timers,
	 * this can go away.
	 */
	F_SET(sp, S_REFRESH);

	return (rval);
}
