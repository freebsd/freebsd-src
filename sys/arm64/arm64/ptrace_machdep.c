/*-
 * Copyright (c) 2014 Andrew Turner
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
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/elf.h>
#include <sys/exec.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#include <sys/rwlock.h>
#include <sys/signalvar.h>
#include <sys/syscallsubr.h>
#include <sys/sysent.h>
#include <sys/sysproto.h>
#include <sys/ucontext.h>

#include <machine/armreg.h>
#include <machine/pcb.h>

/* Only used to get/set 32bits VFP regs */
int
cpu_ptrace(struct thread *td, int req, void *arg, int data)
{
#if defined(VFP) && defined(COMPAT_FREEBSD32)
	mcontext32_vfp_t vfp;
	int error;

	if (!SV_CURPROC_FLAG(SV_ILP32))
		return (EINVAL);
	switch (req) {
		case PT_GETVFPREGS32:
			get_fpcontext32(td, &vfp);
			error = copyout(&vfp, arg, sizeof(vfp));
			break;
		case PT_SETVFPREGS32:
			error = copyin(arg, &vfp, sizeof(vfp));
			if (error == 0)
				set_fpcontext32(td, &vfp);
			break;
		default:
			error = EINVAL;
	}

	return (error);
#else
	return (EINVAL);
#endif
}

#if defined(VFP) && defined(COMPAT_FREEBSD32)
static bool
get_arm_vfp(struct regset *rs, struct thread *td, void *buf, size_t *sizep)
{
	if (buf != NULL) {
		KASSERT(*sizep == sizeof(mcontext32_vfp_t),
		    ("%s: invalid size", __func__));
		get_fpcontext32(td, buf);
	}
	*sizep = sizeof(mcontext32_vfp_t);
	return (true);
}

static bool
set_arm_vfp(struct regset *rs, struct thread *td, void *buf,
    size_t size)
{
	KASSERT(size == sizeof(mcontext32_vfp_t), ("%s: invalid size",
	    __func__));
	set_fpcontext32(td, buf);
	return (true);
}

static struct regset regset_arm_vfp = {
	.note = NT_ARM_VFP,
	.size = sizeof(mcontext32_vfp_t),
	.get = get_arm_vfp,
	.set = set_arm_vfp,
};
ELF32_REGSET(regset_arm_vfp);
#endif

static bool
get_arm64_tls(struct regset *rs, struct thread *td, void *buf,
    size_t *sizep)
{
	if (buf != NULL) {
		KASSERT(*sizep == sizeof(td->td_pcb->pcb_tpidr_el0),
		    ("%s: invalid size", __func__));
		memcpy(buf, &td->td_pcb->pcb_tpidr_el0,
		    sizeof(td->td_pcb->pcb_tpidr_el0));
	}
	*sizep = sizeof(td->td_pcb->pcb_tpidr_el0);

	return (true);
}

static struct regset regset_arm64_tls = {
	.note = NT_ARM_TLS,
	.size = sizeof(uint64_t),
	.get = get_arm64_tls,
};
ELF_REGSET(regset_arm64_tls);

#ifdef COMPAT_FREEBSD32
static bool
get_arm_tls(struct regset *rs, struct thread *td, void *buf,
    size_t *sizep)
{
	if (buf != NULL) {
		uint32_t tp;

		KASSERT(*sizep == sizeof(uint32_t),
		    ("%s: invalid size", __func__));
		tp = (uint32_t)td->td_pcb->pcb_tpidr_el0;
		memcpy(buf, &tp, sizeof(tp));
	}
	*sizep = sizeof(uint32_t);

	return (true);
}

static struct regset regset_arm_tls = {
	.note = NT_ARM_TLS,
	.size = sizeof(uint32_t),
	.get = get_arm_tls,
};
ELF32_REGSET(regset_arm_tls);
#endif

int
ptrace_set_pc(struct thread *td, u_long addr)
{

	td->td_frame->tf_elr = addr;
	return (0);
}

int
ptrace_single_step(struct thread *td)
{
	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);
	if ((td->td_frame->tf_spsr & PSR_SS) == 0) {
		td->td_frame->tf_spsr |= PSR_SS;
		td->td_pcb->pcb_flags |= PCB_SINGLE_STEP;
		td->td_dbgflags |= TDB_STEP;
	}
	return (0);
}

int
ptrace_clear_single_step(struct thread *td)
{
	PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);
	td->td_frame->tf_spsr &= ~PSR_SS;
	td->td_pcb->pcb_flags &= ~PCB_SINGLE_STEP;
	td->td_dbgflags &= ~TDB_STEP;
	return (0);
}

