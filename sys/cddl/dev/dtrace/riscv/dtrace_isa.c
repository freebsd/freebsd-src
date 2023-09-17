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
 * Portions Copyright 2016 Ruslan Bukin <br@bsdpad.com>
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

#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/encoding.h>
#include <machine/riscvreg.h>

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

#include "regset.h"

#define	MAX_USTACK_DEPTH  2048

uint8_t dtrace_fuword8_nocheck(void *);
uint16_t dtrace_fuword16_nocheck(void *);
uint32_t dtrace_fuword32_nocheck(void *);
uint64_t dtrace_fuword64_nocheck(void *);

int dtrace_match_opcode(uint32_t, int, int);
int dtrace_instr_sdsp(uint32_t **);
int dtrace_instr_ret(uint32_t **);
int dtrace_instr_c_sdsp(uint32_t **);
int dtrace_instr_c_ret(uint32_t **);

void
dtrace_getpcstack(pc_t *pcstack, int pcstack_limit, int aframes,
    uint32_t *intrpc)
{
	struct unwind_state state;
	uintptr_t caller;
	register_t sp;
	int scp_offset;
	int depth;

	depth = 0;
	caller = solaris_cpu[curcpu].cpu_dtrace_caller;

	if (intrpc != 0) {
		pcstack[depth++] = (pc_t)intrpc;
	}

	/*
	 * Construct the unwind state, starting from this function. This frame,
	 * and 'aframes' others will be skipped.
	 */
	__asm __volatile("mv %0, sp" : "=&r" (sp));

	state.fp = (uintptr_t)__builtin_frame_address(0);
	state.sp = (uintptr_t)sp;
	state.pc = (uintptr_t)dtrace_getpcstack;

	while (depth < pcstack_limit) {
		if (!unwind_frame(curthread, &state))
			break;

		if (!INKERNEL(state.pc) || !kstack_contains(curthread,
		    (vm_offset_t)state.fp, sizeof(uintptr_t)))
			break;

		if (aframes > 0) {
			aframes--;

			/*
			 * fbt_invop() records the return address at the time
			 * the FBT probe fires. We need to insert this into the
			 * backtrace manually, since the stack frame state at
			 * the time of the probe does not capture it.
			 */
			if (aframes == 0 && caller != 0)
				pcstack[depth++] = caller;
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
	volatile uint16_t *flags;
	uintptr_t oldfp;
	int ret;

	oldfp = fp;
	ret = 0;
	flags = (volatile uint16_t *)&cpu_core[curcpu].cpuc_dtrace_flags;

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

		pc = dtrace_fuword64((void *)(fp - 1 * sizeof(uint64_t)));
		fp = dtrace_fuword64((void *)(fp - 2 * sizeof(uint64_t)));

		if (fp == oldfp) {
			*flags |= CPU_DTRACE_BADSTACK;
			cpu_core[curcpu].cpuc_dtrace_illval = fp;
			break;
		}
		oldfp = fp;
	}

	return (ret);
}

void
dtrace_getupcstack(uint64_t *pcstack, int pcstack_limit)
{
	volatile uint16_t *flags;
	struct trapframe *tf;
	uintptr_t pc, fp;
	proc_t *p;
	int n;

	p = curproc;
	flags = (volatile uint16_t *)&cpu_core[curcpu].cpuc_dtrace_flags;

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

	pc = tf->tf_sepc;
	fp = tf->tf_s[0];

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

		pc = tf->tf_ra;
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
	struct trapframe *tf;
	uintptr_t pc, fp;
	int n = 0;

	if (curproc == NULL || (tf = curthread->td_frame) == NULL)
		return (0);

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_FAULT))
		return (-1);

	pc = tf->tf_sepc;
	fp = tf->tf_s[0];

	if (DTRACE_CPUFLAG_ISSET(CPU_DTRACE_ENTRY)) {
		/*
		 * In an entry probe.  The frame pointer has not yet been
		 * pushed (that happens in the function prologue).  The
		 * best approach is to add the current pc as a missing top
		 * of stack and back the pc up to the caller, which is stored
		 * at the current stack pointer address since the call
		 * instruction puts it there right before the branch.
		 */
		pc = tf->tf_ra;
		n++;
	}

	n += dtrace_getustack_common(NULL, 0, pc, fp);

	return (0);
}

void
dtrace_getufpstack(uint64_t *pcstack, uint64_t *fpstack, int pcstack_limit)
{

	printf("IMPLEMENT ME: %s\n", __func__);
}

/*ARGSUSED*/
uint64_t
dtrace_getarg(int arg, int aframes)
{

	printf("IMPLEMENT ME: %s\n", __func__);

	return (0);
}

int
dtrace_getstackdepth(int aframes)
{
	struct unwind_state state;
	int scp_offset;
	register_t sp;
	int depth;
	bool done;

	depth = 1;
	done = false;

	__asm __volatile("mv %0, sp" : "=&r" (sp));

	state.fp = (uintptr_t)__builtin_frame_address(0);
	state.sp = sp;
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
	case REG_ZERO:
		return (0);
	case REG_RA:
		return (frame->tf_ra);
	case REG_SP:
		return (frame->tf_sp);
	case REG_GP:
		return (frame->tf_gp);
	case REG_TP:
		return (frame->tf_tp);
	case REG_T0 ... REG_T2:
		return (frame->tf_t[reg - REG_T0]);
	case REG_S0 ... REG_S1:
		return (frame->tf_s[reg - REG_S0]);
	case REG_A0 ... REG_A7:
		return (frame->tf_a[reg - REG_A0]);
	case REG_S2 ... REG_S11:
		return (frame->tf_s[reg - REG_S2 + 2]);
	case REG_T3 ... REG_T6:
		return (frame->tf_t[reg - REG_T3 + 3]);
	case REG_PC:
		return (frame->tf_sepc);
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

int
dtrace_match_opcode(uint32_t insn, int match, int mask)
{
	if (((insn ^ match) & mask) == 0)
		return (1);

	return (0);
}

int
dtrace_instr_sdsp(uint32_t **instr)
{
	if (dtrace_match_opcode(**instr, (MATCH_SD | RS2_RA | RS1_SP),
	    (MASK_SD | RS2_MASK | RS1_MASK)))
		return (1);

	return (0);
}

int
dtrace_instr_c_sdsp(uint32_t **instr)
{
	uint16_t *instr1;
	int i;

	for (i = 0; i < 2; i++) {
		instr1 = (uint16_t *)(*instr) + i;
		if (dtrace_match_opcode(*instr1, (MATCH_C_SDSP | RS2_C_RA),
		    (MASK_C_SDSP | RS2_C_MASK))) {
			*instr = (uint32_t *)instr1;
			return (1);
		}
	}

	return (0);
}

int
dtrace_instr_ret(uint32_t **instr)
{
	if (dtrace_match_opcode(**instr, (MATCH_JALR | (X_RA << RS1_SHIFT)),
	    (MASK_JALR | RD_MASK | RS1_MASK | IMM_MASK)))
		return (1);

	return (0);
}

int
dtrace_instr_c_ret(uint32_t **instr)
{
	uint16_t *instr1;
	int i;

	for (i = 0; i < 2; i++) {
		instr1 = (uint16_t *)(*instr) + i;
		if (dtrace_match_opcode(*instr1,
		    (MATCH_C_JR | (X_RA << RD_SHIFT)), (MASK_C_JR | RD_MASK))) {
			*instr = (uint32_t *)instr1;
			return (1);
		}
	}

	return (0);
}
