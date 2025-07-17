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
 */
/*
 * Copyright 2005 Sun Microsystems, Inc.  All rights reserved.
 * Use is subject to license terms.
 */
#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/dtrace_impl.h>
#include <sys/kernel.h>
#include <sys/msan.h>
#include <sys/stack.h>
#include <sys/pcpu.h>

#include <cddl/dev/dtrace/dtrace_cddl.h>

#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/stack.h>
#include <x86/ifunc.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include "regset.h"

uint8_t dtrace_fuword8_nocheck(void *);
uint16_t dtrace_fuword16_nocheck(void *);
uint32_t dtrace_fuword32_nocheck(void *);
uint64_t dtrace_fuword64_nocheck(void *);

int	dtrace_ustackdepth_max = 2048;

void
dtrace_getpcstack(pc_t *pcstack, int pcstack_limit, int aframes,
    uint32_t *intrpc)
{
	struct thread *td;
	int depth = 0;
	register_t rbp;
	struct amd64_frame *frame;
	vm_offset_t callpc;
	pc_t caller = (pc_t) solaris_cpu[curcpu].cpu_dtrace_caller;

	if (intrpc != 0)
		pcstack[depth++] = (pc_t) intrpc;

	aframes++;

	__asm __volatile("movq %%rbp,%0" : "=r" (rbp));

	frame = (struct amd64_frame *)rbp;
	td = curthread;
	while (depth < pcstack_limit) {
		kmsan_mark(frame, sizeof(*frame), KMSAN_STATE_INITED);

		if (!kstack_contains(curthread, (vm_offset_t)frame,
		    sizeof(*frame)))
			break;

		callpc = frame->f_retaddr;

		if (!INKERNEL(callpc))
			break;

		if (aframes > 0) {
			aframes--;
			if ((aframes == 0) && (caller != 0)) {
				pcstack[depth++] = caller;
			}
		} else {
			pcstack[depth++] = callpc;
		}

		if ((vm_offset_t)frame->f_frame <= (vm_offset_t)frame)
			break;
		frame = frame->f_frame;
	}

	for (; depth < pcstack_limit; depth++) {
		pcstack[depth] = 0;
	}
	kmsan_check(pcstack, pcstack_limit * sizeof(*pcstack), "dtrace");
}

static int
dtrace_getustack_common(uint64_t *pcstack, int pcstack_limit, uintptr_t pc,
    uintptr_t sp)
{
	uintptr_t oldsp;
	volatile uint16_t *flags =
	    (volatile uint16_t *)&cpu_core[curcpu].cpuc_dtrace_flags;
	int ret = 0;

	ASSERT(pcstack == NULL || pcstack_limit > 0);
	ASSERT(dtrace_ustackdepth_max > 0);

	while (pc != 0) {
		/*
		 * We limit the number of times we can go around this
		 * loop to account for a circular stack.
		 */
		if (ret++ >= dtrace_ustackdepth_max) {
			*flags |= CPU_DTRACE_BADSTACK;
			cpu_core[curcpu].cpuc_dtrace_illval = sp;
			break;
		}

		if (pcstack != NULL) {
			*pcstack++ = (uint64_t)pc;
			pcstack_limit--;
			if (pcstack_limit <= 0)
				break;
		}

		if (sp == 0)
			break;

		oldsp = sp;

		pc = dtrace_fuword64((void *)(sp +
			offsetof(struct amd64_frame, f_retaddr)));
		sp = dtrace_fuword64((void *)sp);

		if (sp == oldsp) {
			*flags |= CPU_DTRACE_BADSTACK;
			cpu_core[curcpu].cpuc_dtrace_illval = sp;
			break;
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
	proc_t *p = curproc;
	struct trapframe *tf;
	uintptr_t pc, sp, fp;
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
	if (p == NULL || (tf = curthread->td_frame) == NULL)
		goto zero;

	*pcstack++ = (uint64_t)p->p_pid;
	pcstack_limit--;

	if (pcstack_limit <= 0)
		return;

	pc = tf->tf_rip;
	fp = tf->tf_rbp;
	sp = tf->tf_rsp;

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_ENTRY)) {
		/* 
		 * In an entry probe.  The frame pointer has not yet been
		 * pushed (that happens in the function prologue).  The
		 * best approach is to add the current pc as a missing top
		 * of stack and back the pc up to the caller, which is stored
		 * at the current stack pointer address since the call 
		 * instruction puts it there right before the branch.
		 */

		*pcstack++ = (uint64_t)pc;
		pcstack_limit--;
		if (pcstack_limit <= 0)
			return;

		pc = dtrace_fuword64((void *) sp);
	}

	n = dtrace_getustack_common(pcstack, pcstack_limit, pc, fp);
	ASSERT(n >= 0);
	ASSERT(n <= pcstack_limit);

	pcstack += n;
	pcstack_limit -= n;

zero:
	while (pcstack_limit-- > 0)
		*pcstack++ = 0;
}

int
dtrace_getustackdepth(void)
{
	proc_t *p = curproc;
	struct trapframe *tf;
	uintptr_t pc, fp, sp;
	int n = 0;

	if (p == NULL || (tf = curthread->td_frame) == NULL)
		return (0);

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_FAULT))
		return (-1);

	pc = tf->tf_rip;
	fp = tf->tf_rbp;
	sp = tf->tf_rsp;

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_ENTRY)) {
		/* 
		 * In an entry probe.  The frame pointer has not yet been
		 * pushed (that happens in the function prologue).  The
		 * best approach is to add the current pc as a missing top
		 * of stack and back the pc up to the caller, which is stored
		 * at the current stack pointer address since the call 
		 * instruction puts it there right before the branch.
		 */

		pc = dtrace_fuword64((void *) sp);
		n++;
	}

	n += dtrace_getustack_common(NULL, 0, pc, fp);

	return (n);
}

void
dtrace_getufpstack(uint64_t *pcstack, uint64_t *fpstack, int pcstack_limit)
{
	proc_t *p = curproc;
	struct trapframe *tf;
	uintptr_t pc, sp, fp;
	volatile uint16_t *flags =
	    (volatile uint16_t *)&cpu_core[curcpu].cpuc_dtrace_flags;
#ifdef notyet	/* XXX signal stack */
	uintptr_t oldcontext;
	size_t s1, s2;
#endif

	if (*flags & CPU_DTRACE_FAULT)
		return;

	if (pcstack_limit <= 0)
		return;

	/*
	 * If there's no user context we still need to zero the stack.
	 */
	if (p == NULL || (tf = curthread->td_frame) == NULL)
		goto zero;

	*pcstack++ = (uint64_t)p->p_pid;
	pcstack_limit--;

	if (pcstack_limit <= 0)
		return;

	pc = tf->tf_rip;
	sp = tf->tf_rsp;
	fp = tf->tf_rbp;

#ifdef notyet /* XXX signal stack */
	oldcontext = lwp->lwp_oldcontext;
	s1 = sizeof (struct xframe) + 2 * sizeof (long);
	s2 = s1 + sizeof (siginfo_t);
#endif

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_ENTRY)) {
		*pcstack++ = (uint64_t)pc;
		*fpstack++ = 0;
		pcstack_limit--;
		if (pcstack_limit <= 0)
			return;

		pc = dtrace_fuword64((void *)sp);
	}

	while (pc != 0) {
		*pcstack++ = (uint64_t)pc;
		*fpstack++ = fp;
		pcstack_limit--;
		if (pcstack_limit <= 0)
			break;

		if (fp == 0)
			break;

#ifdef notyet /* XXX signal stack */
		if (oldcontext == sp + s1 || oldcontext == sp + s2) {
			ucontext_t *ucp = (ucontext_t *)oldcontext;
			greg_t *gregs = ucp->uc_mcontext.gregs;

			sp = dtrace_fulword(&gregs[REG_FP]);
			pc = dtrace_fulword(&gregs[REG_PC]);

			oldcontext = dtrace_fulword(&ucp->uc_link);
		} else
#endif /* XXX */
		{
			pc = dtrace_fuword64((void *)(fp +
				offsetof(struct amd64_frame, f_retaddr)));
			fp = dtrace_fuword64((void *)fp);
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
		*pcstack++ = 0;
}

/*ARGSUSED*/
uint64_t
dtrace_getarg(int arg, int aframes)
{
	struct thread *td;
	uintptr_t val;
	struct amd64_frame *fp = (struct amd64_frame *)dtrace_getfp();
	uintptr_t *stack;
	int i;

	/*
	 * A total of 6 arguments are passed via registers; any argument with
	 * index of 5 or lower is therefore in a register.
	 */
	int inreg = 5;

	/*
	 * Did we arrive here via dtrace_invop()?  We can simply fetch arguments
	 * from the trap frame if so.
	 */
	td = curthread;
	if (td->t_dtrace_trapframe != NULL) {
		struct trapframe *tf = td->t_dtrace_trapframe;

		if (arg <= inreg) {
			switch (arg) {
			case 0:
				return (tf->tf_rdi);
			case 1:
				return (tf->tf_rsi);
			case 2:
				return (tf->tf_rdx);
			case 3:
				return (tf->tf_rcx);
			case 4:
				return (tf->tf_r8);
			case 5:
				return (tf->tf_r9);
			}
		}

		arg -= inreg;
		stack = (uintptr_t *)tf->tf_rsp;
		goto load;
	}

	for (i = 1; i <= aframes; i++) {
		kmsan_mark(fp, sizeof(*fp), KMSAN_STATE_INITED);
		fp = fp->f_frame;
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

	if (arg <= inreg) {
		/*
		 * This shouldn't happen.  If the argument is passed in a
		 * register then it should have been, well, passed in a
		 * register...
		 */
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return (0);
	}

	arg -= (inreg + 1);
	stack = (uintptr_t *)&fp[1];

load:
	DTRACE_CPUFLAG_SET(CPU_DTRACE_NOFAULT);
	val = stack[arg];
	DTRACE_CPUFLAG_CLEAR(CPU_DTRACE_NOFAULT);

	kmsan_mark(&val, sizeof(val), KMSAN_STATE_INITED);

	return (val);
}

int
dtrace_getstackdepth(int aframes)
{
	int depth = 0;
	struct amd64_frame *frame;
	vm_offset_t rbp;

	aframes++;
	rbp = dtrace_getfp();
	frame = (struct amd64_frame *)rbp;
	depth++;
	for (;;) {
		kmsan_mark(frame, sizeof(*frame), KMSAN_STATE_INITED);

		if (!kstack_contains(curthread, (vm_offset_t)frame,
		    sizeof(*frame)))
			break;

		depth++;
		if (frame->f_frame <= frame)
			break;
		frame = frame->f_frame;
	}
	if (depth < aframes)
		return 0;
	else
		return depth - aframes;
}

ulong_t
dtrace_getreg(struct trapframe *frame, uint_t reg)
{
	/* This table is dependent on reg.d. */
	int regmap[] = {
		REG_GS,		/* 0  GS */
		REG_FS,		/* 1  FS */
		REG_ES,		/* 2  ES */
		REG_DS,		/* 3  DS */
		REG_RDI,	/* 4  EDI */
		REG_RSI,	/* 5  ESI */
		REG_RBP,	/* 6  EBP, REG_FP */
		REG_RSP,	/* 7  ESP */
		REG_RBX,	/* 8  EBX, REG_R1 */
		REG_RDX,	/* 9  EDX */
		REG_RCX,	/* 10 ECX */
		REG_RAX,	/* 11 EAX, REG_R0 */
		REG_TRAPNO,	/* 12 TRAPNO */
		REG_ERR,	/* 13 ERR */
		REG_RIP,	/* 14 EIP, REG_PC */
		REG_CS,		/* 15 CS */
		REG_RFL,	/* 16 EFL, REG_PS */
		REG_RSP,	/* 17 UESP, REG_SP */
		REG_SS		/* 18 SS */
	};

	if (reg <= GS) {
		if (reg >= sizeof (regmap) / sizeof (int)) {
			DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
			return (0);
		}

		reg = regmap[reg];
	} else {
		/* This is dependent on reg.d. */
		reg -= GS + 1;
	}

	switch (reg) {
	case REG_RDI:
		return (frame->tf_rdi);
	case REG_RSI:
		return (frame->tf_rsi);
	case REG_RDX:
		return (frame->tf_rdx);
	case REG_RCX:
		return (frame->tf_rcx);
	case REG_R8:
		return (frame->tf_r8);
	case REG_R9:
		return (frame->tf_r9);
	case REG_RAX:
		return (frame->tf_rax);
	case REG_RBX:
		return (frame->tf_rbx);
	case REG_RBP:
		return (frame->tf_rbp);
	case REG_R10:
		return (frame->tf_r10);
	case REG_R11:
		return (frame->tf_r11);
	case REG_R12:
		return (frame->tf_r12);
	case REG_R13:
		return (frame->tf_r13);
	case REG_R14:
		return (frame->tf_r14);
	case REG_R15:
		return (frame->tf_r15);
	case REG_DS:
		return (frame->tf_ds);
	case REG_ES:
		return (frame->tf_es);
	case REG_FS:
		return (frame->tf_fs);
	case REG_GS:
		return (frame->tf_gs);
	case REG_TRAPNO:
		return (frame->tf_trapno);
	case REG_ERR:
		return (frame->tf_err);
	case REG_RIP:
		return (frame->tf_rip);
	case REG_CS:
		return (frame->tf_cs);
	case REG_SS:
		return (frame->tf_ss);
	case REG_RFL:
		return (frame->tf_rflags);
	case REG_RSP:
		return (frame->tf_rsp);
	default:
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return (0);
	}
}

static int
dtrace_copycheck(uintptr_t uaddr, uintptr_t kaddr, size_t size)
{
	ASSERT(INKERNEL(kaddr) && kaddr + size >= kaddr);

	if (uaddr + size > VM_MAXUSER_ADDRESS || uaddr + size < uaddr) {
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
	if (dtrace_copycheck(uaddr, kaddr, size)) {
		dtrace_copy(uaddr, kaddr, size);
		kmsan_mark((void *)kaddr, size, KMSAN_STATE_INITED);
	}
}

void
dtrace_copyout(uintptr_t kaddr, uintptr_t uaddr, size_t size,
    volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size)) {
		kmsan_check((void *)kaddr, size, "dtrace_copyout");
		dtrace_copy(kaddr, uaddr, size);
	}
}

void
dtrace_copyinstr(uintptr_t uaddr, uintptr_t kaddr, size_t size,
    volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size)) {
		dtrace_copystr(uaddr, kaddr, size, flags);
		kmsan_mark((void *)kaddr, size, KMSAN_STATE_INITED);
	}
}

void
dtrace_copyoutstr(uintptr_t kaddr, uintptr_t uaddr, size_t size,
    volatile uint16_t *flags)
{
	if (dtrace_copycheck(uaddr, kaddr, size)) {
		kmsan_check((void *)kaddr, size, "dtrace_copyoutstr");
		dtrace_copystr(kaddr, uaddr, size, flags);
	}
}

uint8_t
dtrace_fuword8(void *uaddr)
{
	uint8_t val;

	if ((uintptr_t)uaddr > VM_MAXUSER_ADDRESS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}
	val = dtrace_fuword8_nocheck(uaddr);
	kmsan_mark(&val, sizeof(val), KMSAN_STATE_INITED);
	return (val);
}

uint16_t
dtrace_fuword16(void *uaddr)
{
	uint16_t val;

	if ((uintptr_t)uaddr > VM_MAXUSER_ADDRESS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}
	val = dtrace_fuword16_nocheck(uaddr);
	kmsan_mark(&val, sizeof(val), KMSAN_STATE_INITED);
	return (val);
}

uint32_t
dtrace_fuword32(void *uaddr)
{
	uint32_t val;

	if ((uintptr_t)uaddr > VM_MAXUSER_ADDRESS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}
	val = dtrace_fuword32_nocheck(uaddr);
	kmsan_mark(&val, sizeof(val), KMSAN_STATE_INITED);
	return (val);
}

uint64_t
dtrace_fuword64(void *uaddr)
{
	uint64_t val;

	if ((uintptr_t)uaddr > VM_MAXUSER_ADDRESS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}
	val = dtrace_fuword64_nocheck(uaddr);
	kmsan_mark(&val, sizeof(val), KMSAN_STATE_INITED);
	return (val);
}

/*
 * ifunc resolvers for SMAP support
 */
void dtrace_copy_nosmap(uintptr_t, uintptr_t, size_t);
void dtrace_copy_smap(uintptr_t, uintptr_t, size_t);
DEFINE_IFUNC(, void, dtrace_copy, (uintptr_t, uintptr_t, size_t))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    dtrace_copy_smap : dtrace_copy_nosmap);
}

void dtrace_copystr_nosmap(uintptr_t, uintptr_t, size_t, volatile uint16_t *);
void dtrace_copystr_smap(uintptr_t, uintptr_t, size_t, volatile uint16_t *);
DEFINE_IFUNC(, void, dtrace_copystr, (uintptr_t, uintptr_t, size_t,
    volatile uint16_t *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    dtrace_copystr_smap : dtrace_copystr_nosmap);
}

uintptr_t dtrace_fulword_nosmap(void *);
uintptr_t dtrace_fulword_smap(void *);
DEFINE_IFUNC(, uintptr_t, dtrace_fulword, (void *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    dtrace_fulword_smap : dtrace_fulword_nosmap);
}

uint8_t dtrace_fuword8_nocheck_nosmap(void *);
uint8_t dtrace_fuword8_nocheck_smap(void *);
DEFINE_IFUNC(, uint8_t, dtrace_fuword8_nocheck, (void *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    dtrace_fuword8_nocheck_smap : dtrace_fuword8_nocheck_nosmap);
}

uint16_t dtrace_fuword16_nocheck_nosmap(void *);
uint16_t dtrace_fuword16_nocheck_smap(void *);
DEFINE_IFUNC(, uint16_t, dtrace_fuword16_nocheck, (void *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    dtrace_fuword16_nocheck_smap : dtrace_fuword16_nocheck_nosmap);
}

uint32_t dtrace_fuword32_nocheck_nosmap(void *);
uint32_t dtrace_fuword32_nocheck_smap(void *);
DEFINE_IFUNC(, uint32_t, dtrace_fuword32_nocheck, (void *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    dtrace_fuword32_nocheck_smap : dtrace_fuword32_nocheck_nosmap);
}

uint64_t dtrace_fuword64_nocheck_nosmap(void *);
uint64_t dtrace_fuword64_nocheck_smap(void *);
DEFINE_IFUNC(, uint64_t, dtrace_fuword64_nocheck, (void *))
{

	return ((cpu_stdext_feature & CPUID_STDEXT_SMAP) != 0 ?
	    dtrace_fuword64_nocheck_smap : dtrace_fuword64_nocheck_nosmap);
}
