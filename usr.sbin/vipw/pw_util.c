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
#else
static const char rcsid[] =
 "$FreeBSD$";
#endif
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

#include "pw_util.h"

extern char *tempname;
static pid_t editpid = -1;
static int lockfd;

void
pw_cont(sig)
	int sig;
{

	if (editpid != -1)
		kill(editpid, sig);
}

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
	(void)signal(SIGALRM, SIG_IGN);
	(void)signal(SIGHUP, SIG_IGN);
	(void)signal(SIGINT, SIG_IGN);
	(void)signal(SIGPIPE, SIG_IGN);
	(void)signal(SIGQUIT, SIG_IGN);
	(void)signal(SIGTERM, SIG_IGN);
	(void)signal(SIGCONT, pw_cont);

	/* Create with exact permissions. */
	(void)umask(0);
}

int
pw_lock()
{
	/*
	 * If the master password file doesn't exist, the system is hosed.
	 * Might as well try to build one.  Set the close-on-exec bit so
	 * that users can't get at the encrypted passwords while editing.
	 * Open should allow flock'ing the file; see 4.4BSD.	XXX
	 */
	lockfd = open(_PATH_MASTERPASSWD, O_RDONLY, 0);
	if (lockfd < 0 || fcntl(lockfd, F_SETFD, 1) == -1)
		err(1, "%s", _PATH_MASTERPASSWD);
	if (flock(lockfd, LOCK_EX|LOCK_NB))
		errx(1, "the password db file is busy");
	return (lockfd);
}

int
pw_tmp()
{
	static char path[MAXPATHLEN] = _PATH_MASTERPASSWD;
	int fd;
	char *p;

	if (p = strrchr(path, '/'))
		++p;
	else
		p = path;
	strcpy(p, "pw.XXXXXX");
	if ((fd = mkstemp(path)) == -1)
		err(1, "%s", path);
	tempname = path;
	return (fd);
}

int
pw_mkdb()
{
	int pstat;
	pid_t pid;

	warnx("rebuilding the database...");
	(void)fflush(stderr);
	if (!(pid = vfork())) {
		execl(_PATH_PWD_MKDB, "pwd_mkdb", "-p", tempname, NULL);
		pw_error(_PATH_PWD_MKDB, 1, 1);
	}
	pid = waitpid(pid, &pstat, 0);
	if (pid == -1 || !WIFEXITED(pstat) || WEXITSTATUS(pstat) != 0)
		return (0);
	warnx("done");
	return (1);
}

void
pw_edit(notsetuid)
	int notsetuid;
{
	int pstat;
	char *p, *editor;

	if (!(editor = getenv("EDITOR")))
		editor = _PATH_VI;
	if (p = strrchr(editor, '/'))
		++p;
	else
		p = editor;

	if (!(editpid = vfork())) {
		if (notsetuid) {
			(void)setgid(getgid());
			(void)setuid(getuid());
		}
		execlp(editor, p, tempname, NULL);
		_exit(1);
	}
	for (;;) {
		editpid = waitpid(editpid, (int *)&pstat, WUNTRACED);
		if (editpid == -1)
			pw_error(editor, 1, 1);
		else if (WIFSTOPPED(pstat))
			raise(WSTOPSIG(pstat));
		else if (WIFEXITED(pstat) && WEXITSTATUS(pstat) == 0)
			break;
		else
			pw_error(editor, 1, 1);
	}
	editpid = -1;
}

void
pw_prompt()
{
	int c;

	(void)printf("re-edit the password file? [y]: ");
	(void)fflush(stdout);
	c = getchar();
	if (c != EOF && c != '\n')
		while (getchar() != '\n');
	if (c == 'n')
		pw_error(NULL, 0, 0);
}

void
pw_error(name, err, eval)
	char *name;
	int err, eval;
{
#ifdef YP
	extern int _use_yp;
#endif /* YP */
	if (err)
		warn("%s", name);
#ifdef YP
	if (_use_yp)
		warnx("NIS information unchanged");
	else
#endif /* YP */
	warnx("%s: unchanged", _PATH_MASTERPASSWD);
	(void)unlink(tempname);
	exit(eval);
}
