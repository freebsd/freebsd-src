/*-
 * Copyright (c) 2005 Peter Wemm
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_compat.h"

#include <sys/param.h>
#include <sys/exec.h>
#include <sys/fcntl.h>
#include <sys/imgact.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mman.h>
#include <sys/namei.h>
#include <sys/pioctl.h>
#include <sys/proc.h>
#include <sys/procfs.h>
#include <sys/resourcevar.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/stat.h>
#include <sys/sx.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>
#include <sys/sysent.h>
#include <sys/vnode.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_extern.h>

#include <compat/freebsd32/freebsd32_util.h>
#include <compat/freebsd32/freebsd32_proto.h>
#include <machine/fpu.h>
#include <compat/ia32/ia32_reg.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/cpufunc.h>

#define	CS_SECURE(cs)		(ISPL(cs) == SEL_UPL)
#define	EFL_SECURE(ef, oef)	((((ef) ^ (oef)) & ~PSL_USERCHANGE) == 0)

int
fill_regs32(struct thread *td, struct reg32 *regs)
{
	struct pcb *pcb;
	struct trapframe *tp;

	tp = td->td_frame;
	pcb = td->td_pcb;
	regs->r_fs = pcb->pcb_fs;
	regs->r_es = pcb->pcb_es;
	regs->r_ds = pcb->pcb_ds;
	regs->r_edi = tp->tf_rdi;
	regs->r_esi = tp->tf_rsi;
	regs->r_ebp = tp->tf_rbp;
	regs->r_ebx = tp->tf_rbx;
	regs->r_edx = tp->tf_rdx;
	regs->r_ecx = tp->tf_rcx;
	regs->r_eax = tp->tf_rax;
	regs->r_eip = tp->tf_rip;
	regs->r_cs = tp->tf_cs;
	regs->r_eflags = tp->tf_rflags;
	regs->r_esp = tp->tf_rsp;
	regs->r_ss = tp->tf_ss;
	regs->r_gs = pcb->pcb_gs;
	return (0);
}

int
set_regs32(struct thread *td, struct reg32 *regs)
{
	struct pcb *pcb;
	struct trapframe *tp;

	tp = td->td_frame;
	if (!EFL_SECURE(regs->r_eflags, tp->tf_rflags) || !CS_SECURE(regs->r_cs))
		return (EINVAL);
	pcb = td->td_pcb;
#if 0
	load_fs(regs->r_fs);
	pcb->pcb_fs = regs->r_fs;
	load_es(regs->r_es);
	pcb->pcb_es = regs->r_es;
	load_ds(regs->r_ds);
	pcb->pcb_ds = regs->r_ds;
#endif
	tp->tf_rdi = regs->r_edi;
	tp->tf_rsi = regs->r_esi;
	tp->tf_rbp = regs->r_ebp;
	tp->tf_rbx = regs->r_ebx;
	tp->tf_rdx = regs->r_edx;
	tp->tf_rcx = regs->r_ecx;
	tp->tf_rax = regs->r_eax;
	tp->tf_rip = regs->r_eip;
	tp->tf_cs = regs->r_cs;
	tp->tf_rflags = regs->r_eflags;
	tp->tf_rsp = regs->r_esp;
	tp->tf_ss = regs->r_ss;
#if 0
	load_gs(regs->r_gs);
	pcb->pcb_gs = regs->r_gs;
#endif
	return (0);
}

int
fill_fpregs32(struct thread *td, struct fpreg32 *regs)
{
	struct save87 *sv_87 = (struct save87 *)regs;
	struct env87 *penv_87 = &sv_87->sv_env;
	struct savefpu *sv_fpu = &td->td_pcb->pcb_save;
	struct envxmm *penv_xmm = &sv_fpu->sv_env;
	int i;

	bzero(regs, sizeof(*regs));
	
	/* FPU control/status */
	penv_87->en_cw = penv_xmm->en_cw;
	penv_87->en_sw = penv_xmm->en_sw;
	penv_87->en_tw = penv_xmm->en_tw;
	/*
	 * XXX for en_fip/fcs/foo/fos, check if the fxsave format
	 * uses the old-style layout for 32 bit user apps.  If so,
	 * read the ip and operand segment registers from there.
	 * For now, use the process's %cs/%ds.
	 */
	penv_87->en_fip = penv_xmm->en_rip;
	penv_87->en_fcs = td->td_frame->tf_cs;
	penv_87->en_opcode = penv_xmm->en_opcode;
	penv_87->en_foo = penv_xmm->en_rdp;
	penv_87->en_fos = td->td_pcb->pcb_ds;

	/* FPU registers */
	for (i = 0; i < 8; ++i)
		sv_87->sv_ac[i] = sv_fpu->sv_fp[i].fp_acc;

	return (0);
}

int
set_fpregs32(struct thread *td, struct fpreg32 *regs)
{
	struct save87 *sv_87 = (struct save87 *)regs;
	struct env87 *penv_87 = &sv_87->sv_env;
	struct savefpu *sv_fpu = &td->td_pcb->pcb_save;
	struct envxmm *penv_xmm = &sv_fpu->sv_env;
	int i;

	/* FPU control/status */
	penv_xmm->en_cw = penv_87->en_cw;
	penv_xmm->en_sw = penv_87->en_sw;
	penv_xmm->en_tw = penv_87->en_tw;
	penv_xmm->en_rip = penv_87->en_fip;
	/* penv_87->en_fcs and en_fos ignored, see above */
	penv_xmm->en_opcode = penv_87->en_opcode;
	penv_xmm->en_rdp = penv_87->en_foo;

	/* FPU registers */
	for (i = 0; i < 8; ++i)
		sv_fpu->sv_fp[i].fp_acc = sv_87->sv_ac[i];
	for (i = 8; i < 16; ++i)
		bzero(&sv_fpu->sv_fp[i].fp_acc, sizeof(sv_fpu->sv_fp[i].fp_acc));

	return (0);
}

int
fill_dbregs32(struct thread *td, struct dbreg32 *regs)
{
	struct dbreg dr;
	int err, i;

	err = fill_dbregs(td, &dr);
	for (i = 0; i < 8; i++)
		regs->dr[i] = dr.dr[i];
	for (i = 8; i < 16; i++)
		regs->dr[i] = 0;
	return (err);
}

int
set_dbregs32(struct thread *td, struct dbreg32 *regs)
{
	struct dbreg dr;
	int i;

	for (i = 0; i < 8; i++)
		dr.dr[i] = regs->dr[i];
	for (i = 8; i < 16; i++)
		dr.dr[i] = 0;
	return (set_dbregs(td, &dr));
}
