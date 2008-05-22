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
#include <errno.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <stdio.h>

int
proc_clearflags(struct proc_handle *phdl, int mask)
{
	if (phdl == NULL)
		return (EINVAL);

	phdl->flags &= ~mask;

	return (0);
}

int
proc_continue(struct proc_handle *phdl)
{
	if (phdl == NULL)
		return (EINVAL);

	if (ptrace(PT_CONTINUE, phdl->pid, (caddr_t)(uintptr_t) 1, 0) != 0)
		return (errno);

	phdl->status = PS_RUN;

	return (0);
}

int
proc_detach(struct proc_handle *phdl)
{
	if (phdl == NULL)
		return (EINVAL);

	if (ptrace(PT_DETACH, phdl->pid, 0, 0) != 0)
		return (errno);

	return (0);
}

int
proc_getflags(struct proc_handle *phdl)
{
	if (phdl == NULL)
		return (-1);

	return(phdl->flags);
}

int
proc_setflags(struct proc_handle *phdl, int mask)
{
	if (phdl == NULL)
		return (EINVAL);

	phdl->flags |= mask;

	return (0);
}

int
proc_state(struct proc_handle *phdl)
{
	if (phdl == NULL)
		return (-1);

	return (phdl->status);
}

int
proc_wait(struct proc_handle *phdl)
{
	int status = 0;
	struct kevent kev;

	if (phdl == NULL)
		return (EINVAL);

	if (kevent(phdl->kq, NULL, 0, &kev, 1, NULL) <= 0)
		return (0);

	switch (kev.filter) {
	/* Child has exited */
	case EVFILT_PROC:  /* target has exited */
		phdl->status = PS_UNDEAD;
		break;
	default:
		break;
	}

	return (status);
}

pid_t
proc_getpid(struct proc_handle *phdl)
{
	if (phdl == NULL)
		return (-1);

	return (phdl->pid);
}
