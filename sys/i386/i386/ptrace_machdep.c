/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2005 Doug Rabson
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

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/elf.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

static uint32_t
get_segbase(struct segment_descriptor *sdp)
{
	return (sdp->sd_hibase << 24 | sdp->sd_lobase);
}

static bool
get_segbases(struct regset *rs, struct thread *td, void *buf,
    size_t *sizep)
{
	struct segbasereg *reg;

	if (buf != NULL) {
		KASSERT(*sizep == sizeof(*reg), ("%s: invalid size", __func__));
		reg = buf;
		reg->r_fsbase = get_segbase(&td->td_pcb->pcb_fsd);
		reg->r_gsbase = get_segbase(&td->td_pcb->pcb_gsd);
	}
	*sizep = sizeof(*reg);
	return (true);
}

static bool
set_segbases(struct regset *rs, struct thread *td, void *buf,
    size_t size)
{
	struct segbasereg *reg;

	KASSERT(size == sizeof(*reg), ("%s: invalid size", __func__));
	reg = buf;

	fill_based_sd(&td->td_pcb->pcb_fsd, reg->r_fsbase);
	td->td_frame->tf_fs = GSEL(GUFS_SEL, SEL_UPL);
	fill_based_sd(&td->td_pcb->pcb_gsd, reg->r_gsbase);
	td->td_pcb->pcb_gs = GSEL(GUGS_SEL, SEL_UPL);

	return (true);
}

static struct regset regset_segbases = {
	.note = NT_X86_SEGBASES,
	.size = sizeof(struct segbasereg),
	.get = get_segbases,
	.set = set_segbases,
};
ELF_REGSET(regset_segbases);

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
		npxgetregs(td);
		savefpu = (char *)(get_pcb_user_save_td(td) + 1);
		error = copyout(savefpu, addr,
		    cpu_max_ext_state_size - sizeof(union savefpu));
		break;

	case PT_SETXSTATE_OLD:
		if (data > cpu_max_ext_state_size - sizeof(union savefpu)) {
			error = EINVAL;
			break;
		}
		savefpu = malloc(data, M_TEMP, M_WAITOK);
		error = copyin(addr, savefpu, data);
		if (error == 0) {
			npxgetregs(td);
			error = npxsetxstate(td, savefpu, data);
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
		npxgetregs(td);
		savefpu = (char *)(get_pcb_user_save_td(td));
		error = copyout(savefpu, addr, cpu_max_ext_state_size);
		break;

	case PT_SETXSTATE:
		if (data < sizeof(union savefpu) ||
		    data > cpu_max_ext_state_size) {
			error = EINVAL;
			break;
		}
		savefpu = malloc(data, M_TEMP, M_WAITOK);
		error = copyin(addr, savefpu, data);
		if (error == 0)
			error = npxsetregs(td, (union savefpu *)savefpu,
			    savefpu + sizeof(union savefpu), data -
			    sizeof(union savefpu));
		free(savefpu, M_TEMP);
		break;

	default:
		error = EINVAL;
		break;
	}

	return (error);
}

static int
cpu_ptrace_xmm(struct thread *td, int req, void *addr, int data)
{
	struct savexmm *fpstate;
	int error;

	if (!cpu_fxsr)
		return (EINVAL);

	fpstate = &get_pcb_user_save_td(td)->sv_xmm;
	switch (req) {
	case PT_GETXMMREGS:
		npxgetregs(td);
		error = copyout(fpstate, addr, sizeof(*fpstate));
		break;

	case PT_SETXMMREGS:
		npxgetregs(td);
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

	default:
		return (EINVAL);
	}

	return (error);
}

int
cpu_ptrace(struct thread *td, int req, void *addr, int data)
{
	struct segment_descriptor *sdp, sd;
	register_t r;
	int error;

	switch (req) {
	case PT_GETXMMREGS:
	case PT_SETXMMREGS:
	case PT_GETXSTATE_OLD:
	case PT_SETXSTATE_OLD:
	case PT_GETXSTATE_INFO:
	case PT_GETXSTATE:
	case PT_SETXSTATE:
		error = cpu_ptrace_xmm(td, req, addr, data);
		break;

	case PT_GETFSBASE:
	case PT_GETGSBASE:
		sdp = req == PT_GETFSBASE ? &td->td_pcb->pcb_fsd :
		    &td->td_pcb->pcb_gsd;
		r = get_segbase(sdp);
		error = copyout(&r, addr, sizeof(r));
		break;

	case PT_SETFSBASE:
	case PT_SETGSBASE:
		error = copyin(addr, &r, sizeof(r));
		if (error != 0)
			break;
		fill_based_sd(&sd, r);
		if (req == PT_SETFSBASE) {
			td->td_pcb->pcb_fsd = sd;
			td->td_frame->tf_fs = GSEL(GUFS_SEL, SEL_UPL);
		} else {
			td->td_pcb->pcb_gsd = sd;
			td->td_pcb->pcb_gs = GSEL(GUGS_SEL, SEL_UPL);
		}
		break;

	default:
		return (EINVAL);
	}

	return (error);
}

int
ptrace_set_pc(struct thread *td, u_long addr)
{

	td->td_frame->tf_eip = addr;
	return (0);
}

int
ptrace_single_step(struct thread *td)
{

	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);
	if ((td->td_frame->tf_eflags & PSL_T) == 0) {
		td->td_frame->tf_eflags |= PSL_T;
		td->td_dbgflags |= TDB_STEP;
	}
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{

	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);
	td->td_frame->tf_eflags &= ~PSL_T;
	td->td_dbgflags &= ~TDB_STEP;
	return (0);
}
