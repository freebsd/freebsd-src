/*-
 * Copyright (c) 2010 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Pawel Jakub Dawidek under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/wait.h>

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <libgen.h>
#include <paths.h>

#include <pjdlog.h>

#include "hooks.h"

static void
descriptors(void)
{
	long maxfd;
	int fd;

	/*
	 * Close all descriptors.
	 */
	maxfd = sysconf(_SC_OPEN_MAX);
	if (maxfd < 0) {
		pjdlog_errno(LOG_WARNING, "sysconf(_SC_OPEN_MAX) failed");
		maxfd = 1024;
	}
	for (fd = 0; fd <= maxfd; fd++) {
		switch (fd) {
		case STDIN_FILENO:
		case STDOUT_FILENO:
		case STDERR_FILENO:
			if (pjdlog_mode_get() == PJDLOG_MODE_STD)
				break;
			/* FALLTHROUGH */
		default:
			close(fd);
			break;
		}
	}
	if (pjdlog_mode_get() == PJDLOG_MODE_STD)
		return;
	/*
	 * Redirect stdin, stdout and stderr to /dev/null.
	 */
	fd = open(_PATH_DEVNULL, O_RDONLY);
	if (fd < 0) {
		pjdlog_errno(LOG_WARNING, "Unable to open %s for reading",
		    _PATH_DEVNULL);
	} else if (fd != STDIN_FILENO) {
		if (dup2(fd, STDIN_FILENO) < 0) {
			pjdlog_errno(LOG_WARNING,
			    "Unable to duplicate descriptor for stdin");
		}
		close(fd);
	}
	fd = open(_PATH_DEVNULL, O_WRONLY);
	if (fd < 0) {
		pjdlog_errno(LOG_WARNING, "Unable to open %s for writing",
		    _PATH_DEVNULL);
	} else {
		if (fd != STDOUT_FILENO && dup2(fd, STDOUT_FILENO) < 0) {
			pjdlog_errno(LOG_WARNING,
			    "Unable to duplicate descriptor for stdout");
		}
		if (fd != STDERR_FILENO && dup2(fd, STDERR_FILENO) < 0) {
			pjdlog_errno(LOG_WARNING,
			    "Unable to duplicate descriptor for stderr");
		}
		if (fd != STDOUT_FILENO && fd != STDERR_FILENO)
			close(fd);
	}
}

int
hook_exec(const char *path, ...)
{
	va_list ap;
	int ret;

	va_start(ap, path);
	ret = hook_execv(path, ap);
	va_end(ap);
	return (ret);
}

int
hook_execv(const char *path, va_list ap)
{
	char *args[64];
	unsigned int ii;
	pid_t pid, wpid;
	int status;

	if (path == NULL || path[0] == '\0')
		return (0);

	memset(args, 0, sizeof(args));
	args[0] = basename(path);
	for (ii = 1; ii < sizeof(args) / sizeof(args[0]); ii++) {
		args[ii] = va_arg(ap, char *);
		if (args[ii] == NULL)
			break;
	}
	assert(ii < sizeof(args) / sizeof(args[0]));

	pid = fork();
	switch (pid) {
	case -1:	/* Error. */
		pjdlog_errno(LOG_ERR, "Unable to fork %s", path);
		return (-1);
	case 0:		/* Child. */
		descriptors();
		execv(path, args);
		pjdlog_errno(LOG_ERR, "Unable to execute %s", path);
		exit(EX_SOFTWARE);
	default:	/* Parent. */
		break;
	}

	wpid = waitpid(pid, &status, 0);
	assert(wpid == pid);

	return (WEXITSTATUS(status));
}
