/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ian Lepore <ian@freebsd.org>
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

#include <sys/types.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/stack.h>
#include <machine/pcb.h>
#include <machine/stack.h>

static void
stack_capture(struct stack *st, struct unwind_state *state)
{

	stack_zero(st);
	while (unwind_stack_one(state, 0) == 0) {
		if (stack_put(st, state->registers[PC]) == -1)
			break;
	}
}

void
stack_save(struct stack *st)
{
	struct unwind_state state;
	uint32_t sp;

	/* Read the stack pointer */
	__asm __volatile("mov %0, sp" : "=&r" (sp));

	state.registers[FP] = (uint32_t)__builtin_frame_address(0);
	state.registers[SP] = sp;
	state.registers[LR] = (uint32_t)__builtin_return_address(0);
	state.registers[PC] = (uint32_t)stack_save;

	stack_capture(st, &state);
}

void
stack_save_td(struct stack *st, struct thread *td)
{
	struct unwind_state state;

	KASSERT(!TD_IS_SWAPPED(td), ("stack_save_td: swapped"));
	KASSERT(!TD_IS_RUNNING(td), ("stack_save_td: running"));

	state.registers[FP] = td->td_pcb->pcb_regs.sf_r11;
	state.registers[SP] = td->td_pcb->pcb_regs.sf_sp;
	state.registers[LR] = td->td_pcb->pcb_regs.sf_lr;
	state.registers[PC] = td->td_pcb->pcb_regs.sf_pc;

	stack_capture(st, &state);
}

int
stack_save_td_running(struct stack *st, struct thread *td)
{

	if (td == curthread) {
		stack_save(st);
		return (0);
	}
	return (EOPNOTSUPP);
}
