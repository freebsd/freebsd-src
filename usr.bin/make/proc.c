/*-
 * Copyright (C) 2005 Max Okumoto.
 *	All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#include "proc.h"
#include "shell.h"
#include "util.h"

/**
 * Replace the current process.
 */
void
Proc_Exec(const ProcStuff *ps)
{

	if (ps->in != STDIN_FILENO) {
		/*
		 * Redirect the child's stdin to the input fd
		 * and reset it to the beginning (again).
		 */
		if (dup2(ps->in, STDIN_FILENO) == -1)
			Punt("Cannot dup2: %s", strerror(errno));
		lseek(STDIN_FILENO, (off_t)0, SEEK_SET);
	}

	if (ps->out != STDOUT_FILENO) {
		/*
		 * Redirect the child's stdout to the output fd.
		 */
		if (dup2(ps->out, STDOUT_FILENO) == -1)
			Punt("Cannot dup2: %s", strerror(errno));
		close(ps->out);
	}

	if (ps->err != STDERR_FILENO) {
		/*
		 * Redirect the child's stderr to the err fd.
		 */
		if (dup2(ps->err, STDERR_FILENO) == -1)
			Punt("Cannot dup2: %s", strerror(errno));
		close(ps->err);
	}

	if (ps->merge_errors) {
		/*
		 * Send stderr to parent process too.
		 */
		if (dup2(STDOUT_FILENO, STDERR_FILENO) == -1)
			Punt("Cannot dup2: %s", strerror(errno));
	}

	if (commandShell->unsetenv) {
		/* for the benfit of ksh */
		unsetenv("ENV");
	}

	/*
	 * The file descriptors for stdin, stdout, or stderr might
	 * have been marked close-on-exec.  Clear the flag on all
	 * of them.
	 */
	fcntl(STDIN_FILENO, F_SETFD,
	    fcntl(STDIN_FILENO, F_GETFD) & (~FD_CLOEXEC));
	fcntl(STDOUT_FILENO, F_SETFD,
	    fcntl(STDOUT_FILENO, F_GETFD) & (~FD_CLOEXEC));
	fcntl(STDERR_FILENO, F_SETFD,
	    fcntl(STDERR_FILENO, F_GETFD) & (~FD_CLOEXEC));

	if (ps->pgroup) {
#ifdef USE_PGRP
		/*
		 * Become a process group leader, so we can kill it and all
		 * its descendants in one fell swoop, by killing its process
		 * family, but not commit suicide.
		 */
#if defined(SYSV)
		setsid();
#else
		setpgid(0, getpid());
#endif
#endif /* USE_PGRP */
	}

	if (ps->searchpath) {
		execvp(ps->argv[0], ps->argv);

		write(STDERR_FILENO, ps->argv[0], strlen(ps->argv[0]));
		write(STDERR_FILENO, ":", 1);
		write(STDERR_FILENO, strerror(errno), strlen(strerror(errno)));
		write(STDERR_FILENO, "\n", 1);
	} else {
		execv(commandShell->path, ps->argv);

		write(STDERR_FILENO,
		    "Could not execute shell\n",
		    sizeof("Could not execute shell"));
	}

	/*
	 * Since we are the child process, exit without flushing buffers.
	 */
	_exit(1);
}
