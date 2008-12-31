/*-
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
__FBSDID("$FreeBSD: src/sys/i386/i386/stack_machdep.c,v 1.2.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/stack.h>

#include <machine/pcb.h>
#include <machine/stack.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

static void
stack_capture(struct stack *st, register_t ebp)
{
	struct i386_frame *frame;
	vm_offset_t callpc;

	stack_zero(st);
	frame = (struct i386_frame *)ebp;
	while (1) {
		if (!INKERNEL(frame))
			break;
		callpc = frame->f_retaddr;
		if (!INKERNEL(callpc))
			break;
		if (stack_put(st, callpc) == -1)
			break;
		if (frame->f_frame <= frame ||
		    (vm_offset_t)frame->f_frame >=
		    (vm_offset_t)ebp + KSTACK_PAGES * PAGE_SIZE)
			break;
		frame = frame->f_frame;
	}
}

void
stack_save_td(struct stack *st, struct thread *td)
{
	register_t ebp;

	if (TD_IS_SWAPPED(td))
		panic("stack_save_td: swapped");
	if (TD_IS_RUNNING(td))
		panic("stack_save_td: running");

	ebp = td->td_pcb->pcb_ebp;
	stack_capture(st, ebp);
}

void
stack_save(struct stack *st)
{
	register_t ebp;

	__asm __volatile("movl %%ebp,%0" : "=r" (ebp));
	stack_capture(st, ebp);
}
