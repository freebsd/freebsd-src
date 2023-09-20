/*-
 * Copyright (c) 2004 Tim J. Robbins
 * Copyright (c) 2002 Doug Rabson
 * Copyright (c) 2000 Marcel Moolenaar
 * All rights reserved.
 * Copyright (c) 2013 Dmitry Chagin <dchagin@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in this position and unchanged.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/ptrace.h>
#include <sys/syscallsubr.h>

#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/specialreg.h>

#include <vm/pmap.h>
#include <vm/vm.h>
#include <vm/vm_param.h>

#include <x86/ifunc.h>
#include <x86/reg.h>
#include <x86/sysarch.h>

#include <amd64/linux/linux.h>
#include <amd64/linux/linux_proto.h>
#include <compat/linux/linux_fork.h>
#include <compat/linux/linux_misc.h>
#include <compat/linux/linux_util.h>

#define	LINUX_ARCH_AMD64		0xc000003e

int
linux_set_upcall(struct thread *td, register_t stack)
{

	if (stack)
		td->td_frame->tf_rsp = stack;

	/*
	 * The newly created Linux thread returns
	 * to the user space by the same path that a parent does.
	 */
	td->td_frame->tf_rax = 0;
	return (0);
}

int
linux_iopl(struct thread *td, struct linux_iopl_args *args)
{
	int error;

	LINUX_CTR(iopl);

	if (args->level > 3)
		return (EINVAL);
	if ((error = priv_check(td, PRIV_IO)) != 0)
		return (error);
	if ((error = securelevel_gt(td->td_ucred, 0)) != 0)
		return (error);
	td->td_frame->tf_rflags = (td->td_frame->tf_rflags & ~PSL_IOPL) |
	    (args->level * (PSL_IOPL / 3));

	return (0);
}

int
linux_pause(struct thread *td, struct linux_pause_args *args)
{
	struct proc *p = td->td_proc;
	sigset_t sigmask;

	LINUX_CTR(pause);

	PROC_LOCK(p);
	sigmask = td->td_sigmask;
	PROC_UNLOCK(p);
	return (kern_sigsuspend(td, sigmask));
}

int
linux_arch_prctl(struct thread *td, struct linux_arch_prctl_args *args)
{
	unsigned long long cet[3];
	struct pcb *pcb;
	int error;

	pcb = td->td_pcb;
	LINUX_CTR2(arch_prctl, "0x%x, %p", args->code, args->addr);

	switch (args->code) {
	case LINUX_ARCH_SET_GS:
		if (args->addr < VM_MAXUSER_ADDRESS) {
			update_pcb_bases(pcb);
			pcb->pcb_gsbase = args->addr;
			td->td_frame->tf_gs = _ugssel;
			error = 0;
		} else
			error = EPERM;
		break;
	case LINUX_ARCH_SET_FS:
		if (args->addr < VM_MAXUSER_ADDRESS) {
			update_pcb_bases(pcb);
			pcb->pcb_fsbase = args->addr;
			td->td_frame->tf_fs = _ufssel;
			error = 0;
		} else
			error = EPERM;
		break;
	case LINUX_ARCH_GET_FS:
		error = copyout(&pcb->pcb_fsbase, PTRIN(args->addr),
		    sizeof(args->addr));
		break;
	case LINUX_ARCH_GET_GS:
		error = copyout(&pcb->pcb_gsbase, PTRIN(args->addr),
		    sizeof(args->addr));
		break;
	case LINUX_ARCH_CET_STATUS:
		memset(cet, 0, sizeof(cet));
		error = copyout(&cet, PTRIN(args->addr), sizeof(cet));
		break;
	default:
		linux_msg(td, "unsupported arch_prctl code %#x", args->code);
		error = EINVAL;
	}
	return (error);
}

int
linux_set_cloned_tls(struct thread *td, void *desc)
{
	struct pcb *pcb;

	if ((uint64_t)desc >= VM_MAXUSER_ADDRESS)
		return (EPERM);

	pcb = td->td_pcb;
	update_pcb_bases(pcb);
	pcb->pcb_fsbase = (register_t)desc;
	td->td_frame->tf_fs = _ufssel;

	return (0);
}

int futex_xchgl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_xchgl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_xchgl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_xchgl_smap : futex_xchgl_nosmap);
}

int futex_addl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_addl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_addl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_addl_smap : futex_addl_nosmap);
}

int futex_orl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_orl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_orl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_orl_smap : futex_orl_nosmap);
}

int futex_andl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_andl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_andl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_andl_smap : futex_andl_nosmap);
}

int futex_xorl_nosmap(int oparg, uint32_t *uaddr, int *oldval);
int futex_xorl_smap(int oparg, uint32_t *uaddr, int *oldval);
DEFINE_IFUNC(, int, futex_xorl, (int, uint32_t *, int *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    futex_xorl_smap : futex_xorl_nosmap);
}

void
bsd_to_linux_regset(const struct reg *b_reg, struct linux_pt_regset *l_regset)
{

	l_regset->r15 = b_reg->r_r15;
	l_regset->r14 = b_reg->r_r14;
	l_regset->r13 = b_reg->r_r13;
	l_regset->r12 = b_reg->r_r12;
	l_regset->rbp = b_reg->r_rbp;
	l_regset->rbx = b_reg->r_rbx;
	l_regset->r11 = b_reg->r_r11;
	l_regset->r10 = b_reg->r_r10;
	l_regset->r9 = b_reg->r_r9;
	l_regset->r8 = b_reg->r_r8;
	l_regset->rax = b_reg->r_rax;
	l_regset->rcx = b_reg->r_rcx;
	l_regset->rdx = b_reg->r_rdx;
	l_regset->rsi = b_reg->r_rsi;
	l_regset->rdi = b_reg->r_rdi;
	l_regset->orig_rax = b_reg->r_rax;
	l_regset->rip = b_reg->r_rip;
	l_regset->cs = b_reg->r_cs;
	l_regset->eflags = b_reg->r_rflags;
	l_regset->rsp = b_reg->r_rsp;
	l_regset->ss = b_reg->r_ss;
	l_regset->fs_base = 0;
	l_regset->gs_base = 0;
	l_regset->ds = b_reg->r_ds;
	l_regset->es = b_reg->r_es;
	l_regset->fs = b_reg->r_fs;
	l_regset->gs = b_reg->r_gs;
}

void
linux_to_bsd_regset(struct reg *b_reg, const struct linux_pt_regset *l_regset)
{

	b_reg->r_r15 = l_regset->r15;
	b_reg->r_r14 = l_regset->r14;
	b_reg->r_r13 = l_regset->r13;
	b_reg->r_r12 = l_regset->r12;
	b_reg->r_rbp = l_regset->rbp;
	b_reg->r_rbx = l_regset->rbx;
	b_reg->r_r11 = l_regset->r11;
	b_reg->r_r10 = l_regset->r10;
	b_reg->r_r9 = l_regset->r9;
	b_reg->r_r8 = l_regset->r8;
	b_reg->r_rax = l_regset->rax;
	b_reg->r_rcx = l_regset->rcx;
	b_reg->r_rdx = l_regset->rdx;
	b_reg->r_rsi = l_regset->rsi;
	b_reg->r_rdi = l_regset->rdi;
	b_reg->r_rax = l_regset->orig_rax;
	b_reg->r_rip = l_regset->rip;
	b_reg->r_cs = l_regset->cs;
	b_reg->r_rflags = l_regset->eflags;
	b_reg->r_rsp = l_regset->rsp;
	b_reg->r_ss = l_regset->ss;
	b_reg->r_ds = l_regset->ds;
	b_reg->r_es = l_regset->es;
	b_reg->r_fs = l_regset->fs;
	b_reg->r_gs = l_regset->gs;
}

void
linux_ptrace_get_syscall_info_machdep(const struct reg *reg,
    struct syscall_info *si)
{

	si->arch = LINUX_ARCH_AMD64;
	si->instruction_pointer = reg->r_rip;
	si->stack_pointer = reg->r_rsp;
}

int
linux_ptrace_getregs_machdep(struct thread *td, pid_t pid,
    struct linux_pt_regset *l_regset)
{
	struct ptrace_lwpinfo lwpinfo;
	struct pcb *pcb;
	int error;

	pcb = td->td_pcb;
	if (td == curthread)
		update_pcb_bases(pcb);

	l_regset->fs_base = pcb->pcb_fsbase;
	l_regset->gs_base = pcb->pcb_gsbase;

	error = kern_ptrace(td, PT_LWPINFO, pid, &lwpinfo, sizeof(lwpinfo));
	if (error != 0) {
		linux_msg(td, "PT_LWPINFO failed with error %d", error);
		return (error);
	}
	if ((lwpinfo.pl_flags & (PL_FLAG_SCE | PL_FLAG_SCX)) != 0) {
		/*
		 * In Linux, the syscall number - passed to the syscall
		 * as rax - is preserved in orig_rax; rax gets overwritten
		 * with syscall return value.
		 */
		l_regset->orig_rax = lwpinfo.pl_syscall_code;
	}

	return (0);
}

#define	LINUX_URO(a,m) ((uintptr_t)a == offsetof(struct linux_pt_regset, m))

int
linux_ptrace_peekuser(struct thread *td, pid_t pid, void *addr, void *data)
{
	struct linux_pt_regset reg;
	struct reg b_reg;
	uint64_t val;
	int error;

	if ((uintptr_t)addr & (sizeof(data) -1) || (uintptr_t)addr < 0)
		return (EIO);
	if ((uintptr_t)addr >= sizeof(struct linux_pt_regset)) {
		LINUX_RATELIMIT_MSG_OPT1("PTRACE_PEEKUSER offset %ld "
		    "not implemented; returning EINVAL", (uintptr_t)addr);
		return (EINVAL);
	}

	if (LINUX_URO(addr, fs_base))
		return (kern_ptrace(td, PT_GETFSBASE, pid, data, 0));
	if (LINUX_URO(addr, gs_base))
		return (kern_ptrace(td, PT_GETGSBASE, pid, data, 0));
	if ((error = kern_ptrace(td, PT_GETREGS, pid, &b_reg, 0)) != 0)
		return (error);
	bsd_to_linux_regset(&b_reg, &reg);
	val = *(&reg.r15 + ((uintptr_t)addr / sizeof(reg.r15)));
	return (copyout(&val, data, sizeof(val)));
}

static inline bool
linux_invalid_selector(u_short val)
{

	return (val != 0 && ISPL(val) != SEL_UPL);
}

struct linux_segreg_off {
	uintptr_t	reg;
	bool		is0;
};

const struct linux_segreg_off linux_segregs_off[] = {
	{
		.reg = offsetof(struct linux_pt_regset, gs),
		.is0 = true,
	},
	{
		.reg = offsetof(struct linux_pt_regset, fs),
		.is0 = true,
	},
	{
		.reg = offsetof(struct linux_pt_regset, ds),
		.is0 = true,
	},
	{
		.reg = offsetof(struct linux_pt_regset, es),
		.is0 = true,
	},
	{
		.reg = offsetof(struct linux_pt_regset, cs),
		.is0 = false,
	},
	{
		.reg = offsetof(struct linux_pt_regset, ss),
		.is0 = false,
	},
};

int
linux_ptrace_pokeuser(struct thread *td, pid_t pid, void *addr, void *data)
{
	struct linux_pt_regset reg;
	struct reg b_reg, b_reg1;
	int error, i;

	if ((uintptr_t)addr & (sizeof(data) -1) || (uintptr_t)addr < 0)
		return (EIO);
	if ((uintptr_t)addr >= sizeof(struct linux_pt_regset)) {
		LINUX_RATELIMIT_MSG_OPT1("PTRACE_POKEUSER offset %ld "
		    "not implemented; returning EINVAL", (uintptr_t)addr);
		return (EINVAL);
	}

	if (LINUX_URO(addr, fs_base))
		return (kern_ptrace(td, PT_SETFSBASE, pid, data, 0));
	if (LINUX_URO(addr, gs_base))
		return (kern_ptrace(td, PT_SETGSBASE, pid, data, 0));
	for (i = 0; i < nitems(linux_segregs_off); i++) {
		if ((uintptr_t)addr == linux_segregs_off[i].reg) {
			if (linux_invalid_selector((uintptr_t)data))
				return (EIO);
			if (!linux_segregs_off[i].is0 && (uintptr_t)data == 0)
				return (EIO);
		}
	}
	if ((error = kern_ptrace(td, PT_GETREGS, pid, &b_reg, 0)) != 0)
		return (error);
	bsd_to_linux_regset(&b_reg, &reg);
	*(&reg.r15 + ((uintptr_t)addr / sizeof(reg.r15))) = (uint64_t)data;
	linux_to_bsd_regset(&b_reg1, &reg);
	b_reg1.r_err = b_reg.r_err;
	b_reg1.r_trapno = b_reg.r_trapno;
	return (kern_ptrace(td, PT_SETREGS, pid, &b_reg, 0));
}
#undef LINUX_URO
