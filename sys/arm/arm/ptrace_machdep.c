/*-
 * Copyright (c) 2017 John Baldwin <jhb@FreeBSD.org>
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

#include <sys/types.h>
#include <sys/elf.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/reg.h>
#ifdef VFP
#include <machine/vfp.h>
#endif

#ifdef VFP
static bool
get_arm_vfp(struct regset *rs, struct thread *td, void *buf, size_t *sizep)
{
	if (buf != NULL) {
		KASSERT(*sizep == sizeof(mcontext_vfp_t),
		    ("%s: invalid size", __func__));
		get_vfpcontext(td, buf);
	}
	*sizep = sizeof(mcontext_vfp_t);
	return (true);
}

static bool
set_arm_vfp(struct regset *rs, struct thread *td, void *buf,
    size_t size)
{
	KASSERT(size == sizeof(mcontext_vfp_t), ("%s: invalid size", __func__));
	set_vfpcontext(td, buf);
	return (true);
}

static struct regset regset_arm_vfp = {
	.note = NT_ARM_VFP,
	.size = sizeof(mcontext_vfp_t),
	.get = get_arm_vfp,
	.set = set_arm_vfp,
};
ELF_REGSET(regset_arm_vfp);
#endif

static bool
get_arm_tls(struct regset *rs, struct thread *td, void *buf,
    size_t *sizep)
{
	if (buf != NULL) {
		KASSERT(*sizep == sizeof(td->td_pcb->pcb_regs.sf_tpidrurw),
		    ("%s: invalid size", __func__));
		memcpy(buf, &td->td_pcb->pcb_regs.sf_tpidrurw,
		    sizeof(td->td_pcb->pcb_regs.sf_tpidrurw));
	}
	*sizep = sizeof(td->td_pcb->pcb_regs.sf_tpidrurw);

	return (true);
}

static struct regset regset_arm_tls = {
	.note = NT_ARM_TLS,
	.size = sizeof(uint32_t),
	.get = get_arm_tls,
};
ELF_REGSET(regset_arm_tls);

int
cpu_ptrace(struct thread *td, int req, void *addr, int data)
{
#ifdef VFP
	mcontext_vfp_t vfp;
#endif
	int error;

	switch (req) {
#ifdef VFP
	case PT_GETVFPREGS:
		get_vfpcontext(td, &vfp);
		error = copyout(&vfp, addr, sizeof(vfp));
		break;
	case PT_SETVFPREGS:
		error = copyin(addr, &vfp, sizeof(vfp));
		if (error == 0)
			set_vfpcontext(td, &vfp);
		break;
#endif
	default:
		error = EINVAL;
	}

	return (error);
}
