/*
 * Copyright (c) 1988 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software written by Ken Arnold and
 * published in UNIX Review, Vol. 6, No. 8.
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
static char sccsid[] = "@(#)popen.c	5.15 (Berkeley) 2/23/91";
#endif /* LIBC_SCCS and not lint */

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <paths.h>

static pid_t *pids;

FILE *
popen(program, type)
	const char *program;
	const char *type;
{
	FILE *iop;
	int pdes[2], fds, pid;

	if (*type != 'r' && *type != 'w' || type[1])
		return (NULL);

	if (pids == NULL) {
		if ((fds = getdtablesize()) <= 0)
			return (NULL);
		if ((pids = (pid_t *)malloc((u_int)(fds * sizeof(int)))) == NULL)
			return (NULL);
		bzero((char *)pids, fds * sizeof(pid_t));
	}
	if (pipe(pdes) < 0)
		return (NULL);
	switch (pid = vfork()) {
	case -1:			/* error */
		(void) close(pdes[0]);
		(void) close(pdes[1]);
		return (NULL);
		/* NOTREACHED */
	case 0:				/* child */
		if (*type == 'r') {
			if (pdes[1] != STDOUT_FILENO) {
				(void) dup2(pdes[1], STDOUT_FILENO);
				(void) close(pdes[1]);
			}
			(void) close(pdes[0]);
		} else {
			if (pdes[0] != STDIN_FILENO) {
				(void) dup2(pdes[0], STDIN_FILENO);
				(void) close(pdes[0]);
			}
			(void) close(pdes[1]);
		}
		execl(_PATH_BSHELL, "sh", "-c", program, NULL);
		_exit(127);
		/* NOTREACHED */
	}
	/* parent; assume fdopen can't fail...  */
	if (*type == 'r') {
		iop = fdopen(pdes[0], type);
		(void) close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], type);
		(void) close(pdes[0]);
	}
	pids[fileno(iop)] = pid;
	return (iop);
}

int
pclose(iop)
	FILE *iop;
{
	register int fdes;
	int omask;
	union wait pstat;
	pid_t pid;

	/*
	 * pclose returns -1 if stream is not associated with a
	 * `popened' command, if already `pclosed', or waitpid
	 * returns an error.
	 */
	if (pids == NULL || pids[fdes = fileno(iop)] == 0)
		return (-1);
	(void) fclose(iop);
	omask = sigblock(sigmask(SIGINT)|sigmask(SIGQUIT)|sigmask(SIGHUP));
	do {
		pid = waitpid(pids[fdes], (int *) &pstat, 0);
	} while (pid == -1 && errno == EINTR);
	(void) sigsetmask(omask);
	pids[fdes] = 0;
	return (pid == -1 ? -1 : pstat.w_status);
}
