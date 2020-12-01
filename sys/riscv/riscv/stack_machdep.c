/*-
 * Copyright (c) 2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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

#include <machine/vmparam.h>
#include <machine/pcb.h>
#include <machine/stack.h>

static void
stack_capture(struct thread *td, struct stack *st, struct unwind_state *frame)
{

	stack_zero(st);

	while (1) {
		if (!kstack_contains(td, (vm_offset_t)frame->fp -
		    (sizeof(uintptr_t) * 2), sizeof(uintptr_t) * 2))
			break;
		unwind_frame(frame);
		if (!INKERNEL((vm_offset_t)frame->pc))
			break;
		if (stack_put(st, frame->pc) == -1)
			break;
	}
}

int
stack_save_td(struct stack *st, struct thread *td)
{
	struct unwind_state frame;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(!TD_IS_SWAPPED(td),
	    ("stack_save_td: thread %p is swapped", td));

	if (TD_IS_RUNNING(td))
		return (EOPNOTSUPP);

	frame.sp = td->td_pcb->pcb_sp;
	frame.fp = td->td_pcb->pcb_s[0];
	frame.pc = td->td_pcb->pcb_ra;

	stack_capture(td, st, &frame);
	return (0);
}

void
stack_save(struct stack *st)
{
	struct unwind_state frame;
	uintptr_t sp;

	__asm __volatile("mv %0, sp" : "=&r" (sp));

	frame.sp = sp;
	frame.fp = (uintptr_t)__builtin_frame_address(0);
	frame.pc = (uintptr_t)stack_save;

	stack_capture(curthread, st, &frame);
}
