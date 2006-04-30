/*	$NetBSD: popen.c,v 1.30 2004-08-09 12:56:48 lukem Exp $	*/

/*-
 * Copyright (c) 1999-2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Luke Mewburn.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)popen.c	8.3 (Berkeley) 4/6/94";
#else
__RCSID("$NetBSD: popen.c,v 1.30 2004-08-09 12:56:48 lukem Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>

#include <errno.h>
#include <glob.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stringlist.h>
#include <syslog.h>
#include <unistd.h>

#ifdef KERBEROS5
#include <krb5/krb5.h>
#endif

#include "extern.h"

#define INCR	100
/*
 * Special version of popen which avoids call to shell.  This ensures no-one
 * may create a pipe to a hidden program as a side effect of a list or dir
 * command.
 * If stderrfd != -1, then send stderr of a read command there,
 * otherwise close stderr.
 */
static int *pids;
static int fds;

extern int ls_main(int, char *[]);

FILE *
ftpd_popen(char *argv[], const char *ptype, int stderrfd)
{
	FILE *iop;
	int argc, pdes[2], pid, isls;
	char **pop;
	StringList *sl;

	iop = NULL;
	isls = 0;
	if ((*ptype != 'r' && *ptype != 'w') || ptype[1])
		return (NULL);

	if (!pids) {
		if ((fds = getdtablesize()) <= 0)
			return (NULL);
		if ((pids = (int *)malloc((u_int)(fds * sizeof(int)))) == NULL)
			return (NULL);
		memset(pids, 0, fds * sizeof(int));
	}
	if (pipe(pdes) < 0)
		return (NULL);

	if ((sl = sl_init()) == NULL)
		goto pfree;

					/* glob each piece */
	if (sl_add(sl, xstrdup(argv[0])) == -1)
		goto pfree;
	for (argc = 1; argv[argc]; argc++) {
		glob_t gl;
		int flags = GLOB_BRACE|GLOB_NOCHECK|GLOB_TILDE|GLOB_LIMIT;

		memset(&gl, 0, sizeof(gl));
		if (glob(argv[argc], flags, NULL, &gl)) {
			if (sl_add(sl, xstrdup(argv[argc])) == -1) {
				globfree(&gl);
				goto pfree;
			}
		} else {
			for (pop = gl.gl_pathv; *pop; pop++) {
				if (sl_add(sl, xstrdup(*pop)) == -1) {
					globfree(&gl);
					goto pfree;
				}
			}
		}
		globfree(&gl);
	}
	if (sl_add(sl, NULL) == -1)
		goto pfree;

#ifndef NO_INTERNAL_LS
	isls = (strcmp(sl->sl_str[0], INTERNAL_LS) == 0);
#endif

	pid = isls ? fork() : vfork();
	switch (pid) {
	case -1:			/* error */
		(void)close(pdes[0]);
		(void)close(pdes[1]);
		goto pfree;
		/* NOTREACHED */
	case 0:				/* child */
		if (*ptype == 'r') {
			if (pdes[1] != STDOUT_FILENO) {
				dup2(pdes[1], STDOUT_FILENO);
				(void)close(pdes[1]);
			}
			if (stderrfd == -1)
				(void)close(STDERR_FILENO);
			else
				dup2(stderrfd, STDERR_FILENO);
			(void)close(pdes[0]);
		} else {
			if (pdes[0] != STDIN_FILENO) {
				dup2(pdes[0], STDIN_FILENO);
				(void)close(pdes[0]);
			}
			(void)close(pdes[1]);
		}
#ifndef NO_INTERNAL_LS
		if (isls) {	/* use internal ls */
			optreset = optind = optopt = 1;
			closelog();
			exit(ls_main(sl->sl_cur - 1, sl->sl_str));
		}
#endif

		execv(sl->sl_str[0], sl->sl_str);
		_exit(1);
	}
	/* parent; assume fdopen can't fail...  */
	if (*ptype == 'r') {
		iop = fdopen(pdes[0], ptype);
		(void)close(pdes[1]);
	} else {
		iop = fdopen(pdes[1], ptype);
		(void)close(pdes[0]);
	}
	pids[fileno(iop)] = pid;

 pfree:
	if (sl)
		sl_free(sl, 1);
	return (iop);
}

int
ftpd_pclose(FILE *iop)
{
	int fdes, status;
	pid_t pid;
	sigset_t nsigset, osigset;

	/*
	 * pclose returns -1 if stream is not associated with a
	 * `popened' command, or, if already `pclosed'.
	 */
	if (pids == 0 || pids[fdes = fileno(iop)] == 0)
		return (-1);
	(void)fclose(iop);
	sigemptyset(&nsigset);
	sigaddset(&nsigset, SIGINT);
	sigaddset(&nsigset, SIGQUIT);
	sigaddset(&nsigset, SIGHUP);
	sigprocmask(SIG_BLOCK, &nsigset, &osigset);
	while ((pid = waitpid(pids[fdes], &status, 0)) < 0 && errno == EINTR)
		continue;
	sigprocmask(SIG_SETMASK, &osigset, NULL);
	pids[fdes] = 0;
	if (pid < 0)
		return (pid);
	if (WIFEXITED(status))
		return (WEXITSTATUS(status));
	return (1);
}
