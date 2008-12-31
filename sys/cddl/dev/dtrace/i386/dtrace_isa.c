/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License, Version 1.0 only
 * (the "License").  You may not use this file except in compliance
 * with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 *
 * $FreeBSD: src/sys/cddl/dev/dtrace/i386/dtrace_isa.c,v 1.1.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/stack.h>
#include <sys/pcpu.h>

#include <machine/md_var.h>
#include <machine/stack.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

extern uintptr_t kernbase;
uintptr_t kernelbase = (uintptr_t) &kernbase;

#define INKERNEL(va) (((vm_offset_t)(va)) >= USRSTACK && \
	 ((vm_offset_t)(va)) < VM_MAX_KERNEL_ADDRESS)

uint8_t dtrace_fuword8_nocheck(void *);
uint16_t dtrace_fuword16_nocheck(void *);
uint32_t dtrace_fuword32_nocheck(void *);
uint64_t dtrace_fuword64_nocheck(void *);

void
dtrace_getpcstack(pc_t *pcstack, int pcstack_limit, int aframes,
    uint32_t *intrpc)
{
	int depth = 0;
	register_t ebp;
	struct i386_frame *frame;
	vm_offset_t callpc;
	pc_t caller = (pc_t) solaris_cpu[curcpu].cpu_dtrace_caller;

	if (intrpc != 0)
		pcstack[depth++] = (pc_t) intrpc;

	aframes++;

	__asm __volatile("movl %%ebp,%0" : "=r" (ebp));

	frame = (struct i386_frame *)ebp;
	while (depth < pcstack_limit) {
		if (!INKERNEL(frame))
			break;

		callpc = frame->f_retaddr;

		if (!INKERNEL(callpc))
			break;

		if (aframes > 0) {
			aframes--;
			if ((aframes == 0) && (caller != 0)) {
				pcstack[depth++] = caller;
			}
		}
		else {
			pcstack[depth++] = callpc;
		}

		if (frame->f_frame <= frame ||
		    (vm_offset_t)frame->f_frame >=
		    (vm_offset_t)ebp + KSTACK_PAGES * PAGE_SIZE)
			break;
		frame = frame->f_frame;
	}

	for (; depth < pcstack_limit; depth++) {
		pcstack[depth] = 0;
	}
}

#ifdef notyet
static int
dtrace_getustack_common(uint64_t *pcstack, int pcstack_limit, uintptr_t pc,
    uintptr_t sp)
{
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = curproc;
	uintptr_t oldcontext = lwp->lwp_oldcontext;
	volatile uint16_t *flags =
	    (volatile uint16_t *)&cpu_core[curcpu].cpuc_dtrace_flags;
	size_t s1, s2;
	int ret = 0;

	ASSERT(pcstack == NULL || pcstack_limit > 0);

	if (p->p_model == DATAMODEL_NATIVE) {
		s1 = sizeof (struct frame) + 2 * sizeof (long);
		s2 = s1 + sizeof (siginfo_t);
	} else {
		s1 = sizeof (struct frame32) + 3 * sizeof (int);
		s2 = s1 + sizeof (siginfo32_t);
	}

	while (pc != 0 && sp != 0) {
		ret++;
		if (pcstack != NULL) {
			*pcstack++ = (uint64_t)pc;
			pcstack_limit--;
			if (pcstack_limit <= 0)
				break;
		}

		if (oldcontext == sp + s1 || oldcontext == sp + s2) {
			if (p->p_model == DATAMODEL_NATIVE) {
				ucontext_t *ucp = (ucontext_t *)oldcontext;
				greg_t *gregs = ucp->uc_mcontext.gregs;

				sp = dtrace_fulword(&gregs[REG_FP]);
				pc = dtrace_fulword(&gregs[REG_PC]);

				oldcontext = dtrace_fulword(&ucp->uc_link);
			} else {
				ucontext32_t *ucp = (ucontext32_t *)oldcontext;
				greg32_t *gregs = ucp->uc_mcontext.gregs;

				sp = dtrace_fuword32(&gregs[EBP]);
				pc = dtrace_fuword32(&gregs[EIP]);

				oldcontext = dtrace_fuword32(&ucp->uc_link);
			}
		} else {
			if (p->p_model == DATAMODEL_NATIVE) {
				struct frame *fr = (struct frame *)sp;

				pc = dtrace_fulword(&fr->fr_savpc);
				sp = dtrace_fulword(&fr->fr_savfp);
			} else {
				struct frame32 *fr = (struct frame32 *)sp;

				pc = dtrace_fuword32(&fr->fr_savpc);
				sp = dtrace_fuword32(&fr->fr_savfp);
			}
		}

		/*
		 * This is totally bogus:  if we faulted, we're going to clear
		 * the fault and break.  This is to deal with the apparently
		 * broken Java stacks on x86.
		 */
		if (*flags & CPU_DTRACE_FAULT) {
			*flags &= ~CPU_DTRACE_FAULT;
			break;
		}
	}

	return (ret);
}

void
dtrace_getupcstack(uint64_t *pcstack, int pcstack_limit)
{
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = curproc;
	struct regs *rp;
	uintptr_t pc, sp;
	volatile uint16_t *flags =
	    (volatile uint16_t *)&cpu_core[curcpu].cpuc_dtrace_flags;
	int n;

	if (*flags & CPU_DTRACE_FAULT)
		return;

	if (pcstack_limit <= 0)
		return;

	/*
	 * If there's no user context we still need to zero the stack.
	 */
	if (lwp == NULL || p == NULL || (rp = lwp->lwp_regs) == NULL)
		goto zero;

	*pcstack++ = (uint64_t)p->p_pid;
	pcstack_limit--;

	if (pcstack_limit <= 0)
		return;

	pc = rp->r_pc;
	sp = rp->r_fp;

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_ENTRY)) {
		*pcstack++ = (uint64_t)pc;
		pcstack_limit--;
		if (pcstack_limit <= 0)
			return;

		if (p->p_model == DATAMODEL_NATIVE)
			pc = dtrace_fulword((void *)rp->r_sp);
		else
			pc = dtrace_fuword32((void *)rp->r_sp);
	}

	n = dtrace_getustack_common(pcstack, pcstack_limit, pc, sp);
	ASSERT(n >= 0);
	ASSERT(n <= pcstack_limit);

	pcstack += n;
	pcstack_limit -= n;

zero:
	while (pcstack_limit-- > 0)
		*pcstack++ = NULL;
}

int
dtrace_getustackdepth(void)
{
}

void
dtrace_getufpstack(uint64_t *pcstack, uint64_t *fpstack, int pcstack_limit)
{
	klwp_t *lwp = ttolwp(curthread);
	proc_t *p = curproc;
	struct regs *rp;
	uintptr_t pc, sp, oldcontext;
	volatile uint16_t *flags =
	    (volatile uint16_t *)&cpu_core[curcpu].cpuc_dtrace_flags;
	size_t s1, s2;

	if (*flags & CPU_DTRACE_FAULT)
		return;

	if (pcstack_limit <= 0)
		return;

	/*
	 * If there's no user context we still need to zero the stack.
	 */
	if (lwp == NULL || p == NULL || (rp = lwp->lwp_regs) == NULL)
		goto zero;

	*pcstack++ = (uint64_t)p->p_pid;
	pcstack_limit--;

	if (pcstack_limit <= 0)
		return;

	pc = rp->r_pc;
	sp = rp->r_fp;
	oldcontext = lwp->lwp_oldcontext;

	if (p->p_model == DATAMODEL_NATIVE) {
		s1 = sizeof (struct frame) + 2 * sizeof (long);
		s2 = s1 + sizeof (siginfo_t);
	} else {
		s1 = sizeof (struct frame32) + 3 * sizeof (int);
		s2 = s1 + sizeof (siginfo32_t);
	}

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_ENTRY)) {
		*pcstack++ = (uint64_t)pc;
		*fpstack++ = 0;
		pcstack_limit--;
		if (pcstack_limit <= 0)
			return;

		if (p->p_model == DATAMODEL_NATIVE)
			pc = dtrace_fulword((void *)rp->r_sp);
		else
			pc = dtrace_fuword32((void *)rp->r_sp);
	}

	while (pc != 0 && sp != 0) {
		*pcstack++ = (uint64_t)pc;
		*fpstack++ = sp;
		pcstack_limit--;
		if (pcstack_limit <= 0)
			break;

		if (oldcontext == sp + s1 || oldcontext == sp + s2) {
			if (p->p_model == DATAMODEL_NATIVE) {
				ucontext_t *ucp = (ucontext_t *)oldcontext;
				greg_t *gregs = ucp->uc_mcontext.gregs;

				sp = dtrace_fulword(&gregs[REG_FP]);
				pc = dtrace_fulword(&gregs[REG_PC]);

				oldcontext = dtrace_fulword(&ucp->uc_link);
			} else {
				ucontext_t *ucp = (ucontext_t *)oldcontext;
				greg_t *gregs = ucp->uc_mcontext.gregs;

				sp = dtrace_fuword32(&gregs[EBP]);
				pc = dtrace_fuword32(&gregs[EIP]);

				oldcontext = dtrace_fuword32(&ucp->uc_link);
			}
		} else {
			if (p->p_model == DATAMODEL_NATIVE) {
				struct frame *fr = (struct frame *)sp;

				pc = dtrace_fulword(&fr->fr_savpc);
				sp = dtrace_fulword(&fr->fr_savfp);
			} else {
				struct frame32 *fr = (struct frame32 *)sp;

				pc = dtrace_fuword32(&fr->fr_savpc);
				sp = dtrace_fuword32(&fr->fr_savfp);
			}
		}

		/*
		 * This is totally bogus:  if we faulted, we're going to clear
		 * the fault and break.  This is to deal with the apparently
		 * broken Java stacks on x86.
		 */
		if (*flags & CPU_DTRACE_FAULT) {
			*flags &= ~CPU_DTRACE_FAULT;
			break;
		}
	}

zero:
	while (pcstack_limit-- > 0)
		*pcstack++ = NULL;
}
#endif

uint64_t
dtrace_getarg(int arg, int aframes)
{
	uintptr_t val;
	struct i386_frame *fp = (struct i386_frame *)dtrace_getfp();
	uintptr_t *stack;
	int i;

	for (i = 1; i <= aframes; i++) {
		fp = fp->f_frame;

		if (fp->f_retaddr == (long)dtrace_invop_callsite) {
			/*
			 * If we pass through the invalid op handler, we will
			 * use the pointer that it passed to the stack as the
			 * second argument to dtrace_invop() as the pointer to
			 * the stack.  When using this stack, we must step
			 * beyond the EIP/RIP that was pushed when the trap was
			 * taken -- hence the "+ 1" below.
			 */
			stack = ((uintptr_t **)&fp[1])[1] + 1;
			goto load;
		}

	}

	/*
	 * We know that we did not come through a trap to get into
	 * dtrace_probe() -- the provider simply called dtrace_probe()
	 * directly.  As this is the case, we need to shift the argument
	 * that we're looking for:  the probe ID is the first argument to
	 * dtrace_probe(), so the argument n will actually be found where
	 * one would expect to find argument (n + 1).
	 */
	arg++;

	stack = (uintptr_t *)&fp[1];

load:
	DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
	val = stack[arg];
	DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT);

	return (val);
}

int
dtrace_getstackdepth(int aframes)
{
	int depth = 0;
	struct i386_frame *frame;
	vm_offset_t ebp;

	aframes++;
	ebp = dtrace_getfp();
	frame = (struct i386_frame *)ebp;
	depth++;
	for(;;) {
		if (!INKERNEL((long) frame))
			break;
		if (!INKERNEL((long) frame->f_frame))
			break;
		depth++;
		if (frame->f_frame <= frame ||
		    (vm_offset_t)frame->f_frame >=
		    (vm_offset_t)ebp + KSTACK_PAGES * PAGE_SIZE)
			break;
		frame = frame->f_frame;
	}
	if (depth < aframes)
		return 0;
	else
		return depth - aframes;
}

#ifdef notyet
ulong_t
dtrace_getreg(struct regs *rp, uint_t reg)
{
#if defined(__amd64)
	int regmap[] = {
		REG_GS,		/* GS */
		REG_FS,		/* FS */
		REG_ES,		/* ES */
		REG_DS,		/* DS */
		REG_RDI,	/* EDI */
		REG_RSI,	/* ESI */
		REG_RBP,	/* EBP */
		REG_RSP,	/* ESP */
		REG_RBX,	/* EBX */
		REG_RDX,	/* EDX */
		REG_RCX,	/* ECX */
		REG_RAX,	/* EAX */
		REG_TRAPNO,	/* TRAPNO */
		REG_ERR,	/* ERR */
		REG_RIP,	/* EIP */
		REG_CS,		/* CS */
		REG_RFL,	/* EFL */
		REG_RSP,	/* UESP */
		REG_SS		/* SS */
	};

	if (reg <= SS) {
		if (reg >= sizeof (regmap) / sizeof (int)) {
			DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
			return (0);
		}

		reg = regmap[reg];
	} else {
		reg -= SS + 1;
	}

	switch (reg) {
	case REG_RDI:
		return (rp->r_rdi);
	case REG_RSI:
		return (rp->r_rsi);
	case REG_RDX:
		return (rp->r_rdx);
	case REG_RCX:
		return (rp->r_rcx);
	case REG_R8:
		return (rp->r_r8);
	case REG_R9:
		return (rp->r_r9);
	case REG_RAX:
		return (rp->r_rax);
	case REG_RBX:
		return (rp->r_rbx);
	case REG_RBP:
		return (rp->r_rbp);
	case REG_R10:
		return (rp->r_r10);
	case REG_R11:
		return (rp->r_r11);
	case REG_R12:
		return (rp->r_r12);
	case REG_R13:
		return (rp->r_r13);
	case REG_R14:
		return (rp->r_r14);
	case REG_R15:
		return (rp->r_r15);
	case REG_DS:
		return (rp->r_ds);
	case REG_ES:
		return (rp->r_es);
	case REG_FS:
		return (rp->r_fs);
	case REG_GS:
		return (rp->r_gs);
	case REG_TRAPNO:
		return (rp->r_trapno);
	case REG_ERR:
		return (rp->r_err);
	case REG_RIP:
		return (rp->r_rip);
	case REG_CS:
		return (rp->r_cs);
	case REG_SS:
		return (rp->r_ss);
	case REG_RFL:
		return (rp->r_rfl);
	case REG_RSP:
		return (rp->r_rsp);
	default:
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return (0);
	}

#else
	if (reg > SS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return (0);
	}

	return ((&rp->r_gs)[reg]);
#endif
}
#endif

static int
dtrace_copycheck(uintptr_t uaddr, uintptr_t kaddr, size_t size)
{
	ASSERT(kaddr >= kernelbase && kaddr + size >= kaddr);

	if (uaddr + size >= kernelbase || uaddr + size < uaddr) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = uaddr;
		return (0);
	}

	return (1);
}

void
dtrace_copyin(uintptr_t uaddr, uintptr_t kaddr, size_t size,
    volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size))
		dtrace_copy(uaddr, kaddr, size);
}

void
dtrace_copyout(uintptr_t kaddr, uintptr_t uaddr, size_t size,
    volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size))
		dtrace_copy(kaddr, uaddr, size);
}

void
dtrace_copyinstr(uintptr_t uaddr, uintptr_t kaddr, size_t size,
    volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size))
		dtrace_copystr(uaddr, kaddr, size, flags);
}

void
dtrace_copyoutstr(uintptr_t kaddr, uintptr_t uaddr, size_t size,
    volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size))
		dtrace_copystr(kaddr, uaddr, size, flags);
}

uint8_t
dtrace_fuword8(void *uaddr)
{
	if ((uintptr_t)uaddr >= kernelbase) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}
	return (dtrace_fuword8_nocheck(uaddr));
}

uint16_t
dtrace_fuword16(void *uaddr)
{
	if ((uintptr_t)uaddr >= kernelbase) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}
	return (dtrace_fuword16_nocheck(uaddr));
}

uint32_t
dtrace_fuword32(void *uaddr)
{
	if ((uintptr_t)uaddr >= kernelbase) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}
	return (dtrace_fuword32_nocheck(uaddr));
}

uint64_t
dtrace_fuword64(void *uaddr)
{
	if ((uintptr_t)uaddr >= kernelbase) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}
	return (dtrace_fuword64_nocheck(uaddr));
}
