/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2007-2008 John Birrell <jb@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_kdb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/dtrace_bsd.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>

#define KDTRACE_PROC_SIZE	64
#define	KDTRACE_THREAD_SIZE	256

FEATURE(kdtrace_hooks,
    "Kernel DTrace hooks which are required to load DTrace kernel modules");

static MALLOC_DEFINE(M_KDTRACE, "kdtrace", "DTrace hooks");

/* Hooks used in the machine-dependent trap handlers. */
dtrace_trap_func_t		dtrace_trap_func;
dtrace_doubletrap_func_t	dtrace_doubletrap_func;
dtrace_pid_probe_ptr_t		dtrace_pid_probe_ptr;
dtrace_return_probe_ptr_t	dtrace_return_probe_ptr;

bool __read_frequently		systrace_enabled;
systrace_probe_func_t 		systrace_probe_func;

/* Return the DTrace process data size compiled in the kernel hooks. */
size_t
kdtrace_proc_size()
{

	return (KDTRACE_PROC_SIZE);
}

void
kdtrace_proc_ctor(struct proc *p)
{

	p->p_dtrace = malloc(KDTRACE_PROC_SIZE, M_KDTRACE, M_WAITOK|M_ZERO);
}

void
kdtrace_proc_dtor(struct proc *p)
{

	free(p->p_dtrace, M_KDTRACE);
	p->p_dtrace = NULL;
}

/* Return the DTrace thread data size compiled in the kernel hooks. */
size_t
kdtrace_thread_size()
{

	return (KDTRACE_THREAD_SIZE);
}

void
kdtrace_thread_ctor(struct thread *td)
{

	td->td_dtrace = malloc(KDTRACE_THREAD_SIZE, M_KDTRACE, M_WAITOK|M_ZERO);
}

void
kdtrace_thread_dtor(struct thread *td)
{

	free(td->td_dtrace, M_KDTRACE);
	td->td_dtrace = NULL;
}
