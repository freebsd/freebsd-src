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
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/frame.h>
#include <machine/vmparam.h>

static int
cpu_ptrace_xstate(struct thread *td, int req, void *addr, int data)
{
	struct ptrace_xstate_info info;
	char *savefpu;
	int error;

	if (!use_xsave)
		return (EOPNOTSUPP);

	switch (req) {
	case PT_GETXSTATE_OLD:
		fpugetregs(td);
		savefpu = (char *)(get_pcb_user_save_td(td) + 1);
		error = copyout(savefpu, addr,
		    cpu_max_ext_state_size - sizeof(struct savefpu));
		break;

	case PT_SETXSTATE_OLD:
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

	case PT_GETXSTATE_INFO:
		if (data != sizeof(info)) {
			error  = EINVAL;
			break;
		}
		info.xsave_len = cpu_max_ext_state_size;
		info.xsave_mask = xsave_mask;
		error = copyout(&info, addr, data);
		break;

	case PT_GETXSTATE:
		fpugetregs(td);
		savefpu = (char *)(get_pcb_user_save_td(td));
		error = copyout(savefpu, addr, cpu_max_ext_state_size);
		break;

	case PT_SETXSTATE:
		if (data < sizeof(struct savefpu) ||
		    data > cpu_max_ext_state_size) {
			error = EINVAL;
			break;
		}
		savefpu = malloc(data, M_TEMP, M_WAITOK);
		error = copyin(addr, savefpu, data);
		if (error == 0)
			error = fpusetregs(td, (struct savefpu *)savefpu,
			    savefpu + sizeof(struct savefpu), data -
			    sizeof(struct savefpu));
		free(savefpu, M_TEMP);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static void
cpu_ptrace_setbase(struct thread *td, int req, register_t r)
{

	if (req == PT_SETFSBASE) {
		td->td_pcb->pcb_fsbase = r;
		td->td_frame->tf_fs = _ufssel;
	} else {
		td->td_pcb->pcb_gsbase = r;
		td->td_frame->tf_gs = _ugssel;
	}
	set_pcb_flags(td->td_pcb, PCB_FULL_IRET);
}

#ifdef COMPAT_FREEBSD32
#define PT_I386_GETXMMREGS	(PT_FIRSTMACH + 0)
#define PT_I386_SETXMMREGS	(PT_FIRSTMACH + 1)

static int
cpu32_ptrace(struct thread *td, int req, void *addr, int data)
{
	struct savefpu *fpstate;
	uint32_t r;
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

	case PT_GETXSTATE_OLD:
	case PT_SETXSTATE_OLD:
	case PT_GETXSTATE_INFO:
	case PT_GETXSTATE:
	case PT_SETXSTATE:
		error = cpu_ptrace_xstate(td, req, addr, data);
		break;

	case PT_GETFSBASE:
	case PT_GETGSBASE:
		if (!SV_PROC_FLAG(td->td_proc, SV_ILP32)) {
			error = EINVAL;
			break;
		}
		r = req == PT_GETFSBASE ? td->td_pcb->pcb_fsbase :
		    td->td_pcb->pcb_gsbase;
		error = copyout(&r, addr, sizeof(r));
		break;

	case PT_SETFSBASE:
	case PT_SETGSBASE:
		if (!SV_PROC_FLAG(td->td_proc, SV_ILP32)) {
			error = EINVAL;
			break;
		}
		error = copyin(addr, &r, sizeof(r));
		if (error != 0)
			break;
		cpu_ptrace_setbase(td, req, r);
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
	register_t *r, rv;
	int error;

#ifdef COMPAT_FREEBSD32
	if (SV_CURPROC_FLAG(SV_ILP32))
		return (cpu32_ptrace(td, req, addr, data));
#endif

	/* Support old values of PT_GETXSTATE_OLD and PT_SETXSTATE_OLD. */
	if (req == PT_FIRSTMACH + 0)
		req = PT_GETXSTATE_OLD;
	if (req == PT_FIRSTMACH + 1)
		req = PT_SETXSTATE_OLD;

	switch (req) {
	case PT_GETXSTATE_OLD:
	case PT_SETXSTATE_OLD:
	case PT_GETXSTATE_INFO:
	case PT_GETXSTATE:
	case PT_SETXSTATE:
		error = cpu_ptrace_xstate(td, req, addr, data);
		break;

	case PT_GETFSBASE:
	case PT_GETGSBASE:
		r = req == PT_GETFSBASE ? &td->td_pcb->pcb_fsbase :
		    &td->td_pcb->pcb_gsbase;
		error = copyout(r, addr, sizeof(*r));
		break;

	case PT_SETFSBASE:
	case PT_SETGSBASE:
		error = copyin(addr, &rv, sizeof(rv));
		if (error != 0)
			break;
		if (rv >= td->td_proc->p_sysent->sv_maxuser) {
			error = EINVAL;
			break;
		}
		cpu_ptrace_setbase(td, req, rv);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}
