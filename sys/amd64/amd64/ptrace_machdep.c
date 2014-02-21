/*-
 * Copyright (c) 2011 Konstantin Belousov <kib@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/sysent.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

static int
cpu_ptrace_xstate(struct thread *td, int req, void *addr, int data)
{
	char *savefpu;
	int error;

	if (!use_xsave)
		return (EOPNOTSUPP);

	switch (req) {
	case PT_GETXSTATE:
		fpugetregs(td);
		savefpu = (char *)(get_pcb_user_save_td(td) + 1);
		error = copyout(savefpu, addr,
		    cpu_max_ext_state_size - sizeof(struct savefpu));
		break;

	case PT_SETXSTATE:
		if (data > cpu_max_ext_state_size - sizeof(struct savefpu)) {
			error = EINVAL;
			break;
		}
		savefpu = malloc(data, M_TEMP, M_WAITOK);
		error = copyin(addr, savefpu, data);
		if (error == 0) {
			fpugetregs(td);
			error = fpusetxstate(td, savefpu, data);
		}
		free(savefpu, M_TEMP);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

#ifdef COMPAT_FREEBSD32
#define PT_I386_GETXMMREGS	(PT_FIRSTMACH + 0)
#define PT_I386_SETXMMREGS	(PT_FIRSTMACH + 1)
#define PT_I386_GETXSTATE	(PT_FIRSTMACH + 2)
#define PT_I386_SETXSTATE	(PT_FIRSTMACH + 3)

static int
cpu32_ptrace(struct thread *td, int req, void *addr, int data)
{
	struct savefpu *fpstate;
	int error;

	switch (req) {
	case PT_I386_GETXMMREGS:
		fpugetregs(td);
		error = copyout(get_pcb_user_save_td(td), addr,
		    sizeof(*fpstate));
		break;

	case PT_I386_SETXMMREGS:
		fpugetregs(td);
		fpstate = get_pcb_user_save_td(td);
		error = copyin(addr, fpstate, sizeof(*fpstate));
		fpstate->sv_env.en_mxcsr &= cpu_mxcsr_mask;
		break;

	case PT_I386_GETXSTATE:
		error = cpu_ptrace_xstate(td, PT_GETXSTATE, addr, data);
		break;

	case PT_I386_SETXSTATE:
		error = cpu_ptrace_xstate(td, PT_SETXSTATE, addr, data);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}
#endif

int
cpu_ptrace(struct thread *td, int req, void *addr, int data)
{
	int error;

#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32))
		return (cpu32_ptrace(td, req, addr, data));
#endif

	/* Support old values of PT_GETXSTATE and PT_SETXSTATE. */
	if (req == PT_FIRSTMACH + 0)
		req = PT_GETXSTATE;
	if (req == PT_FIRSTMACH + 1)
		req = PT_SETXSTATE;

	switch (req) {
	case PT_GETXSTATE:
	case PT_SETXSTATE:
		error = cpu_ptrace_xstate(td, req, addr, data);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}
