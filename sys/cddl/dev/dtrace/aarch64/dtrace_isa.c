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
#include <sys/stack.h>
#include <sys/pcpu.h>

#include <machine/frame.h>
#include <machine/md_var.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/atomic.h>
#include <machine/db_machdep.h>
#include <machine/md_var.h>
#include <machine/stack.h>
#include <ddb/db_sym.h>
#include <ddb/ddb.h>
#include <sys/kdb.h>

#include <cddl/dev/dtrace/dtrace_cddl.h>

#include "regset.h"

#define	MAX_USTACK_DEPTH  2048

uint8_t dtrace_fuword8_nocheck(void *);
uint16_t dtrace_fuword16_nocheck(void *);
uint32_t dtrace_fuword32_nocheck(void *);
uint64_t dtrace_fuword64_nocheck(void *);

void
dtrace_getpcstack(pc_t *pcstack, int pcstack_limit, int aframes,
    uint32_t *intrpc)
{
	struct unwind_state state;
	int scp_offset;
	int depth;

	depth = 0;

	if (intrpc != 0) {
		pcstack[depth++] = (pc_t) intrpc;
	}

	aframes++;

	state.fp = (uintptr_t)__builtin_frame_address(0);
	state.pc = (uintptr_t)dtrace_getpcstack;

	while (depth < pcstack_limit) {
		if (!unwind_frame(curthread, &state))
			break;
		if (!INKERNEL(state.pc))
			break;

		/*
		 * NB: Unlike some other architectures, we don't need to
		 * explicitly insert cpu_dtrace_caller as it appears in the
		 * normal kernel stack trace rather than a special trap frame.
		 */
		if (aframes > 0) {
			aframes--;
		} else {
			pcstack[depth++] = state.pc;
		}

	}

	for (; depth < pcstack_limit; depth++) {
		pcstack[depth] = 0;
	}
}

static int
dtrace_getustack_common(uint64_t *pcstack, int pcstack_limit, uintptr_t pc,
    uintptr_t fp)
{
	volatile uint16_t *flags =
	    (volatile uint16_t *)&cpu_core[curcpu].cpuc_dtrace_flags;
	int ret = 0;
	uintptr_t oldfp = fp;

	ASSERT(pcstack == NULL || pcstack_limit > 0);

	while (pc != 0) {
		/*
		 * We limit the number of times we can go around this
		 * loop to account for a circular stack.
		 */
		if (ret++ >= MAX_USTACK_DEPTH) {
			*flags |= CPU_DTRACE_BADSTACK;
			cpu_core[curcpu].cpuc_dtrace_illval = fp;
			break;
		}

		if (pcstack != NULL) {
			*pcstack++ = (uint64_t)pc;
			pcstack_limit--;
			if (pcstack_limit <= 0)
				break;
		}

		if (fp == 0)
			break;

		pc = dtrace_fuword64((void *)(fp +
		    offsetof(struct unwind_state, pc)));
		fp = dtrace_fuword64((void *)fp);

		if (fp == oldfp) {
			*flags |= CPU_DTRACE_BADSTACK;
			cpu_core[curcpu].cpuc_dtrace_illval = fp;
			break;
		}

		/*
		 * ARM64TODO:
		 *     This workaround might not be necessary. It needs to be
		 *     revised and removed from all architectures if found
		 *     unwanted. Leaving the original x86 comment for reference.
		 *
		 * This is totally bogus:  if we faulted, we're going to clear
		 * the fault and break.  This is to deal with the apparently
		 * broken Java stacks on x86.
		 */
		if (*flags & CPU_DTRACE_FAULT) {
			*flags &= ~CPU_DTRACE_FAULT;
			break;
		}

		oldfp = fp;
	}

	return (ret);
}

void
dtrace_getupcstack(uint64_t *pcstack, int pcstack_limit)
{
	proc_t *p = curproc;
	struct trapframe *tf;
	uintptr_t pc, fp;
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

	pc = tf->tf_elr;
	fp = tf->tf_x[29];

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

		pc = tf->tf_lr;
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

	printf("IMPLEMENT ME: %s\n", __func__);

	return (0);
}

void
dtrace_getufpstack(uint64_t *pcstack, uint64_t *fpstack, int pcstack_limit)
{

	printf("IMPLEMENT ME: %s\n", __func__);
}

uint64_t
dtrace_getarg(int arg, int aframes __unused)
{
	struct trapframe *tf;

	/*
	 * We only handle invop providers here.
	 */
	if ((tf = curthread->t_dtrace_trapframe) == NULL) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return (0);
	} else if (arg < 8) {
		return (tf->tf_x[arg]);
	} else {
		uintptr_t p;
		uint64_t val;

		p = (tf->tf_sp + (arg - 8) * sizeof(uint64_t));
		if ((p & 7) != 0) {
			DTRACE_CPUFLAG_SET(CPU_DTRACE_BADALIGN);
			cpu_core[curcpu].cpuc_dtrace_illval = p;
			return (0);
		}
		if (!kstack_contains(curthread, p, sizeof(uint64_t))) {
			DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
			cpu_core[curcpu].cpuc_dtrace_illval = p;
			return (0);
		}
		memcpy(&val, (void *)p, sizeof(uint64_t));
		return (val);
	}
}

int
dtrace_getstackdepth(int aframes)
{
	struct unwind_state state;
	int scp_offset;
	int depth;
	bool done;

	depth = 1;
	done = false;

	state.fp = (uintptr_t)__builtin_frame_address(0);
	state.pc = (uintptr_t)dtrace_getstackdepth;

	do {
		done = !unwind_frame(curthread, &state);
		if (!INKERNEL(state.pc) || !INKERNEL(state.fp))
			break;
		depth++;
	} while (!done);

	if (depth < aframes)
		return (0);
	else
		return (depth - aframes);
}

ulong_t
dtrace_getreg(struct trapframe *frame, uint_t reg)
{
	switch (reg) {
	case REG_X0 ... REG_X29:
		return (frame->tf_x[reg]);
	case REG_LR:
		return (frame->tf_lr);
	case REG_SP:
		return (frame->tf_sp);
	case REG_PC:
		return (frame->tf_elr);
	default:
		DTRACE_CPUFLAG_SET(CPU_DTRACE_ILLOP);
		return (0);
	}
	/* NOTREACHED */
}

static int
dtrace_copycheck(uintptr_t uaddr, uintptr_t kaddr, size_t size)
{

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

	if ((uintptr_t)uaddr > VM_MAXUSER_ADDRESS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}

	return (dtrace_fuword8_nocheck(uaddr));
}

uint16_t
dtrace_fuword16(void *uaddr)
{

	if ((uintptr_t)uaddr > VM_MAXUSER_ADDRESS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}

	return (dtrace_fuword16_nocheck(uaddr));
}

uint32_t
dtrace_fuword32(void *uaddr)
{

	if ((uintptr_t)uaddr > VM_MAXUSER_ADDRESS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}

	return (dtrace_fuword32_nocheck(uaddr));
}

uint64_t
dtrace_fuword64(void *uaddr)
{

	if ((uintptr_t)uaddr > VM_MAXUSER_ADDRESS) {
		DTRACE_CPUFLAG_SET(CPU_DTRACE_BADADDR);
		cpu_core[curcpu].cpuc_dtrace_illval = (uintptr_t)uaddr;
		return (0);
	}

	return (dtrace_fuword64_nocheck(uaddr));
}
