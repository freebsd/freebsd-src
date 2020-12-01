/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2005 Antoine Brodin
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/stack.h>

#include <machine/mips_opcode.h>

#include <machine/pcb.h>
#include <machine/regnum.h>

#define	VALID_PC(addr)		((addr) >= (uintptr_t)btext && (addr) % 4 == 0)

static void
stack_capture(struct stack *st, struct thread *td, uintptr_t pc, uintptr_t sp)
{
	u_register_t ra;
	uintptr_t i, ra_addr;
	int ra_stack_pos, stacksize;
	InstFmt insn;

	stack_zero(st);

	for (;;) {
		if (!VALID_PC(pc))
			break;

		/*
		 * Walk backward from the PC looking for the function
		 * start.  Assume a subtraction from SP is the start
		 * of a function.  Hope that we find the store of RA
		 * into the stack frame along the way and save the
		 * offset of the saved RA relative to SP.
		 */
		ra_stack_pos = -1;
		stacksize = 0;
		for (i = pc; VALID_PC(i); i -= sizeof(insn)) {
			bcopy((void *)i, &insn, sizeof(insn));
			switch (insn.IType.op) {
			case OP_ADDI:
			case OP_ADDIU:
			case OP_DADDI:
			case OP_DADDIU:
				if (insn.IType.rs != SP || insn.IType.rt != SP)
					break;

				/*
				 * Ignore stack fixups in "early"
				 * returns in a function, or if the
				 * call was from an unlikely branch
				 * moved after the end of the normal
				 * return.
				 */
				if ((short)insn.IType.imm > 0)
					break;

				stacksize = -(short)insn.IType.imm;
				break;

			case OP_SW:
			case OP_SD:
				if (insn.IType.rs != SP || insn.IType.rt != RA)
					break;
				ra_stack_pos = (short)insn.IType.imm;
				break;
			default:
				break;
			}

			if (stacksize != 0)
				break;
		}

		if (stack_put(st, pc) == -1)
			break;

		if (ra_stack_pos == -1)
			break;

		/*
		 * Walk forward from the PC to find the function end
		 * (jr RA).  If eret is hit instead, stop unwinding.
		 */
		ra_addr = sp + ra_stack_pos;
		ra = 0;
		for (i = pc; VALID_PC(i); i += sizeof(insn)) {
			bcopy((void *)i, &insn, sizeof(insn));

			switch (insn.IType.op) {
			case OP_SPECIAL:
				if (insn.RType.func == OP_JR) {
					if (insn.RType.rs != RA)
						break;
					if (!kstack_contains(td, ra_addr,
					    sizeof(ra)))
						goto done;
					ra = *(u_register_t *)ra_addr;
					if (ra == 0)
						goto done;
					ra -= 8;
				}
				break;
			default:
				break;
			}

			/* eret */
			if (insn.word == 0x42000018)
				goto done;

			if (ra != 0)
				break;
		}

		if (pc == ra && stacksize == 0)
			break;

		sp += stacksize;
		pc = ra;
	}
done:
	return;
}

int
stack_save_td(struct stack *st, struct thread *td)
{
	uintptr_t pc, sp;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(!TD_IS_SWAPPED(td),
	    ("stack_save_td: thread %p is swapped", td));

	if (TD_IS_RUNNING(td))
		return (EOPNOTSUPP);

	pc = td->td_pcb->pcb_context[PCB_REG_RA];
	sp = td->td_pcb->pcb_context[PCB_REG_SP];
	stack_capture(st, td, pc, sp);
	return (0);
}

void
stack_save(struct stack *st)
{
	uintptr_t pc, sp;

	pc = (uintptr_t)&&here;
	sp = (uintptr_t)__builtin_frame_address(0);
here:
	stack_capture(st, curthread, pc, sp);
}
