/*
 * Copyright (c) 1988 The Regents of the University of California.
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

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)system.c	5.10 (Berkeley) 2/23/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/types.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <paths.h>

system(command)
	const char *command;
{
	union wait pstat;
	pid_t pid;
	int omask;
	sig_t intsave, quitsave;

	if (!command)		/* just checking... */
		return(1);

	omask = sigblock(sigmask(SIGCHLD));
	switch(pid = vfork()) {
	case -1:			/* error */
		(void)sigsetmask(omask);
		pstat.w_status = 0;
		pstat.w_retcode = 127;
		return(pstat.w_status);
	case 0:				/* child */
		(void)sigsetmask(omask);
		execl(_PATH_BSHELL, "sh", "-c", command, (char *)NULL);
		_exit(127);
	}
	intsave = signal(SIGINT, SIG_IGN);
	quitsave = signal(SIGQUIT, SIG_IGN);
	pid = waitpid(pid, (int *)&pstat, 0);
	(void)sigsetmask(omask);
	(void)signal(SIGINT, intsave);
	(void)signal(SIGQUIT, quitsave);
	return(pid == -1 ? -1 : pstat.w_status);
}
