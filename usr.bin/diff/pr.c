/*-
 * Copyright (c) 2017 Baptiste Daroussin <bapt@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/procdesc.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "pr.h"
#include "diff.h"
#include "xmalloc.h"

#define _PATH_PR "/usr/bin/pr"

extern char **environ;

struct pr *
start_pr(char *file1, char *file2)
{
	int pfd[2];
	pid_t pid;
	char *header;
	struct pr *pr;
	posix_spawn_file_actions_t fa;
	posix_spawnattr_t sa;
	int error;

	pr = xcalloc(1, sizeof(*pr));

	xasprintf(&header, "%s %s %s", diffargs, file1, file2);
	signal(SIGPIPE, SIG_IGN);
	fflush(stdout);
	if (pipe2(pfd, O_CLOEXEC) == -1)
		err(2, "pipe");

	if ((error = posix_spawnattr_init(&sa)) != 0)
		errc(2, error, "posix_spawnattr_init");
	if ((error = posix_spawn_file_actions_init(&fa)) != 0)
		errc(2, error, "posix_spawn_file_actions_init");

	posix_spawnattr_setprocdescp_np(&sa, &pr->procd, 0);

	if (pfd[0] != STDIN_FILENO)
		posix_spawn_file_actions_adddup2(&fa, pfd[0], STDIN_FILENO);

	char *argv[] = { __DECONST(char *, _PATH_PR),
	    __DECONST(char *, "-h"), header, NULL };
	error = posix_spawn(&pid, _PATH_PR, &fa, &sa, argv, environ);
	if (error != 0)
		errc(2, error, "could not spawn pr");

	posix_spawn_file_actions_destroy(&fa);
	posix_spawnattr_destroy(&sa);

	/* parent */
	if (pfd[1] == STDOUT_FILENO) {
		pr->ostdout = STDOUT_FILENO;
	} else {
		if ((pr->ostdout = dup(STDOUT_FILENO)) < 0 ||
		    dup2(pfd[1], STDOUT_FILENO) < 0) {
			err(2, "stdout");
		}
		close(pfd[1]);
	}
	close(pfd[0]);
	free(header);
	return (pr);
}

/* close the pipe to pr and restore stdout */
void
stop_pr(struct pr *pr)
{
	int wstatus;

	if (pr == NULL)
		return;

	fflush(stdout);
	if (pr->ostdout != STDOUT_FILENO) {
		dup2(pr->ostdout, STDOUT_FILENO);
		close(pr->ostdout);
	}
	while (pdwait(pr->procd, &wstatus, WEXITED, NULL, NULL) == -1) {
		if (errno != EINTR)
			err(2, "pdwait");
	}
	close(pr->procd);
	free(pr);
	if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0)
		errx(2, "pr exited abnormally");
	else if (WIFSIGNALED(wstatus))
		errx(2, "pr killed by signal %d",
		    WTERMSIG(wstatus));
}
