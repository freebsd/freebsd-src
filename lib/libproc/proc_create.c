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
#include <stdio.h>
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
	int error = 0;
	int status;

	if (pid == 0 || pid == getpid())
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
	elf_version(EV_CURRENT);

	if (ptrace(PT_ATTACH, phdl->pid, 0, 0) != 0) {
		error = errno;
		DPRINTF("ERROR: cannot ptrace child process %d", pid);
		goto out;
	}

	/* Wait for the child process to stop. */
	if (waitpid(pid, &status, WUNTRACED) == -1) {
		error = errno;
		DPRINTF("ERROR: child process %d didn't stop as expected", pid);
		goto out;
	}

	/* Check for an unexpected status. */
	if (WIFSTOPPED(status) == 0)
		DPRINTF("ERROR: child process %d status 0x%x", pid, status);
	else
		phdl->status = PS_STOP;

	if (error)
		proc_free(phdl);
	else
		*pphdl = phdl;
out:
	proc_free(phdl);
	return (error);
}

int
proc_create(const char *file, char * const *argv, proc_child_func *pcf,
    void *child_arg, struct proc_handle **pphdl)
{
	struct proc_handle *phdl;
	int error = 0;
	int status;
	pid_t pid;

	/*
	 * Allocate memory for the process handle, a structure containing
	 * all things related to the process.
	 */
	if ((phdl = malloc(sizeof(struct proc_handle))) == NULL)
		return (ENOMEM);

	elf_version(EV_CURRENT);

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

		/* Wait for the child process to stop. */
		if (waitpid(pid, &status, WUNTRACED) == -1) {
			error = errno;
                	DPRINTF("ERROR: child process %d didn't stop as expected", pid);
			goto bad;
		}

		/* Check for an unexpected status. */
		if (WIFSTOPPED(status) == 0) {
			error = errno;
                	DPRINTF("ERROR: child process %d status 0x%x", pid, status);
			goto bad;
		} else
			phdl->status = PS_STOP;
	}
bad:
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
