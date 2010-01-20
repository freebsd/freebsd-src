/*-
 * Copyright (c) 2008 John Birrell (jb@freebsd.org)
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "_libproc.h"
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

int
proc_attach(pid_t pid, int flags, struct proc_handle **pphdl)
{
	struct proc_handle *phdl;
	struct kevent kev;
	int error = 0;
	int status;

	if (pid == 0 || pphdl == NULL)
		return (EINVAL);

	/*
	 * Allocate memory for the process handle, a structure containing
	 * all things related to the process.
	 */
	if ((phdl = malloc(sizeof(struct proc_handle))) == NULL)
		return (ENOMEM);

	memset(phdl, 0, sizeof(struct proc_handle));
	phdl->pid = pid;
	phdl->flags = flags;
	phdl->status = PS_RUN;

	EV_SET(&kev, pid, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT,
	    0, NULL);

	if ((phdl->kq = kqueue()) == -1)
		err(1, "ERROR: cannot create kernel evet queue");

	if (kevent(phdl->kq, &kev, 1, NULL, 0, NULL) < 0)
		err(2, "ERROR: cannot monitor child process %d", pid);

	if (ptrace(PT_ATTACH, phdl->pid, NULL, 0) != 0)
		error = errno;

	/* Wait for the child process to stop. */
	else if (waitpid(pid, &status, WUNTRACED) == -1)
		err(3, "ERROR: child process %d didn't stop as expected", pid);

	/* Check for an unexpected status. */
	else if (WIFSTOPPED(status) == 0)
		err(4, "ERROR: child process %d status 0x%x", pid, status);
	else
		phdl->status = PS_STOP;

	if (error)
		proc_free(phdl);
	else
		*pphdl = phdl;

	return (error);
}

int
proc_create(const char *file, char * const *argv, proc_child_func *pcf,
    void *child_arg, struct proc_handle **pphdl)
{
	struct proc_handle *phdl;
	struct kevent kev;
	int error = 0;
	int status;
	pid_t pid;

	/*
	 * Allocate memory for the process handle, a structure containing
	 * all things related to the process.
	 */
	if ((phdl = malloc(sizeof(struct proc_handle))) == NULL)
		return (ENOMEM);

	/* Fork a new process. */
	if ((pid = vfork()) == -1)
		error = errno;
	else if (pid == 0) {
		/* The child expects to be traced. */
		if (ptrace(PT_TRACE_ME, 0, 0, 0) != 0)
			_exit(1);

		if (pcf != NULL)
			(*pcf)(child_arg);

		/* Execute the specified file: */
		execvp(file, argv);

		/* Couldn't execute the file. */
		_exit(2);
	} else {
		/* The parent owns the process handle. */
		memset(phdl, 0, sizeof(struct proc_handle));
		phdl->pid = pid;
		phdl->status = PS_IDLE;

		EV_SET(&kev, pid, EVFILT_PROC, EV_ADD | EV_ONESHOT, NOTE_EXIT,
		    0, NULL);

		if ((phdl->kq = kqueue()) == -1)
                	err(1, "ERROR: cannot create kernel evet queue");

        	if (kevent(phdl->kq, &kev, 1, NULL, 0, NULL) < 0)
                	err(2, "ERROR: cannot monitor child process %d", pid);

		/* Wait for the child process to stop. */
		if (waitpid(pid, &status, WUNTRACED) == -1)
                	err(3, "ERROR: child process %d didn't stop as expected", pid);

		/* Check for an unexpected status. */
		if (WIFSTOPPED(status) == 0)
                	err(4, "ERROR: child process %d status 0x%x", pid, status);
		else
			phdl->status = PS_STOP;
	}

	if (error)
		proc_free(phdl);
	else
		*pphdl = phdl;

	return (error);
}

void
proc_free(struct proc_handle *phdl)
{
	free(phdl);
}
