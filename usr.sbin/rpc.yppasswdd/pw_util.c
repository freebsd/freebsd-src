/*-
 * Copyright (c) 1990, 1993, 1994
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
#if 0
static char sccsid[] = "@(#)pw_util.c	8.3 (Berkeley) 4/2/94";
#endif
static const char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

/*
 * This file is used by all the "password" programs; vipw(8), chpass(1),
 * and passwd(1).
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <pw_util.h>
#include "yppasswdd_extern.h"

int pstat;
pid_t pid;

void
pw_init()
{
	struct rlimit rlim;

	/* Unlimited resource limits. */
	rlim.rlim_cur = rlim.rlim_max = RLIM_INFINITY;
	(void)setrlimit(RLIMIT_CPU, &rlim);
	(void)setrlimit(RLIMIT_FSIZE, &rlim);
	(void)setrlimit(RLIMIT_STACK, &rlim);
	(void)setrlimit(RLIMIT_DATA, &rlim);
	(void)setrlimit(RLIMIT_RSS, &rlim);

	/* Don't drop core (not really necessary, but GP's). */
	rlim.rlim_cur = rlim.rlim_max = 0;
	(void)setrlimit(RLIMIT_CORE, &rlim);

	/* Turn off signals. */
	/* (void)signal(SIGALRM, SIG_IGN); */
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGPIPE, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTSTP, SIG_IGN);
	(void)signal(SIGTTOU, SIG_IGN);

	/* Create with exact permissions. */
	(void)umask(0);
}

static int lockfd;

int
pw_lock()
{
	/*
	 * If the master password file doesn't exist, the system is hosed.
	 * Might as well try to build one.  Set the close-on-exec bit so
	 * that users can't get at the encrypted passwords while editing.
	 * Open should allow flock'ing the file; see 4.4BSD.	XXX
	 */
	lockfd = open(passfile, O_RDONLY, 0);
	if (lockfd < 0 || fcntl(lockfd, F_SETFD, 1) == -1) {
		yp_error("%s: %s", passfile, strerror(errno));
		return (-1);
	}
	if (flock(lockfd, LOCK_EX|LOCK_NB)) {
		yp_error("%s: the password db file is busy", passfile);
		return(-1);
	}
	return (lockfd);
}

int
pw_tmp()
{
	static char path[MAXPATHLEN];
	int fd;
	char *p;

	sprintf(path,"%s",passfile);
	if ((p = strrchr(path, '/')))
		++p;
	else
		p = path;
	strcpy(p, "pw.XXXXXX");
	if ((fd = mkstemp(path)) == -1) {
		yp_error("%s: %s", path, strerror(errno));
		return(-1);
	}
	tempname = path;
	return (fd);
}

int
pw_mkdb(username)
	const char *username;
{

	yp_error("rebuilding the database...");
	(void)fflush(stderr);
	/* Temporarily turn off SIGCHLD catching */
	install_reaper(0);
	if (!(pid = vfork())) {
		if(!username) {
			execl(_PATH_PWD_MKDB, "pwd_mkdb", "-p", tempname,
			    (char *)NULL);
		} else {
			execl(_PATH_PWD_MKDB, "pwd_mkdb", "-p", "-u", username,
			    tempname, (char *)NULL);
		}
		pw_error(_PATH_PWD_MKDB, 1, 1);
		return(-1);
	}
	/* Handle this ourselves. */
	reaper(-1);
	/* Put the handler back. Foo. */
	install_reaper(1);
	if (pid == -1 || !WIFEXITED(pstat) || WEXITSTATUS(pstat) != 0) {
		return (-1);
	}
	yp_error("done");
	return (0);
}

void
pw_error(name, err, eval)
	const char *name;
	int err, eval;
{
	if (err && name != NULL)
		yp_error("%s", name);

	yp_error("%s: unchanged", passfile);
	(void)unlink(tempname);
}
