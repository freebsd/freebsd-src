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

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/user.h>
#include <sys/wait.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libelf.h>
#include <libprocstat.h>

#include "_libproc.h"

static int	getelfclass(int);
static int	proc_init(pid_t, int, int, struct proc_handle **);

static int
getelfclass(int fd)
{
	GElf_Ehdr ehdr;
	Elf *e;
	int class;

	class = ELFCLASSNONE;

	if ((e = elf_begin(fd, ELF_C_READ, NULL)) == NULL)
		goto out;
	if (gelf_getehdr(e, &ehdr) == NULL)
		goto out;
	class = ehdr.e_ident[EI_CLASS];
out:
	(void)elf_end(e);
	return (class);
}

static int
proc_init(pid_t pid, int flags, int status, struct proc_handle **pphdl)
{
	struct kinfo_proc *kp;
	struct proc_handle *phdl;
	int error, class, count, fd;

	*pphdl = NULL;
	if ((phdl = malloc(sizeof(*phdl))) == NULL)
		return (ENOMEM);

	memset(phdl, 0, sizeof(*phdl));
	phdl->pid = pid;
	phdl->flags = flags;
	phdl->status = status;
	phdl->procstat = procstat_open_sysctl();
	if (phdl->procstat == NULL)
		return (ENOMEM);

	/* Obtain a path to the executable. */
	if ((kp = procstat_getprocs(phdl->procstat, KERN_PROC_PID, pid,
	    &count)) == NULL)
		return (ENOMEM);
	error = procstat_getpathname(phdl->procstat, kp, phdl->execpath,
	    sizeof(phdl->execpath));
	procstat_freeprocs(phdl->procstat, kp);
	if (error != 0)
		return (error);

	/* Use it to determine the data model for the process. */
	if ((fd = open(phdl->execpath, O_RDONLY)) < 0) {
		error = errno;
		goto out;
	}
	class = getelfclass(fd);
	switch (class) {
	case ELFCLASS64:
		phdl->model = PR_MODEL_LP64;
		break;
	case ELFCLASS32:
		phdl->model = PR_MODEL_ILP32;
		break;
	case ELFCLASSNONE:
	default:
		error = EINVAL;
		break;
	}
	(void)close(fd);

out:
	*pphdl = phdl;
	return (error);
}

int
proc_attach(pid_t pid, int flags, struct proc_handle **pphdl)
{
	struct proc_handle *phdl;
	int error, status;

	if (pid == 0 || pid == getpid())
		return (EINVAL);
	if (elf_version(EV_CURRENT) == EV_NONE)
		return (ENOENT);

	/*
	 * Allocate memory for the process handle, a structure containing
	 * all things related to the process.
	 */
	error = proc_init(pid, flags, PS_RUN, &phdl);
	if (error != 0)
		goto out;

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
	if (!WIFSTOPPED(status))
		DPRINTFX("ERROR: child process %d status 0x%x", pid, status);
	else
		phdl->status = PS_STOP;

out:
	if (error && phdl != NULL) {
		proc_free(phdl);
		phdl = NULL;
	}
	*pphdl = phdl;
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

	if (elf_version(EV_CURRENT) == EV_NONE)
		return (ENOENT);

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
		/* NOTREACHED */
	} else {
		/* The parent owns the process handle. */
		error = proc_init(pid, 0, PS_IDLE, &phdl);
		if (error != 0)
			goto bad;

		/* Wait for the child process to stop. */
		if (waitpid(pid, &status, WUNTRACED) == -1) {
			error = errno;
			DPRINTF("ERROR: child process %d didn't stop as expected", pid);
			goto bad;
		}

		/* Check for an unexpected status. */
		if (!WIFSTOPPED(status)) {
			error = errno;
			DPRINTFX("ERROR: child process %d status 0x%x", pid, status);
			goto bad;
		} else
			phdl->status = PS_STOP;
	}
bad:
	if (error && phdl != NULL) {
		proc_free(phdl);
		phdl = NULL;
	}
	*pphdl = phdl;
	return (error);
}

void
proc_free(struct proc_handle *phdl)
{

	if (phdl->procstat != NULL)
		procstat_close(phdl->procstat);
	free(phdl);
}
