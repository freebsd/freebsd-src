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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/stack.h>

#include <x86/stack.h>

#include <machine/pcb.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#ifdef __i386__
#define	PCB_FP(pcb)	((pcb)->pcb_ebp)
#define	TF_FP(tf)	((tf)->tf_ebp)
#define	TF_PC(tf)	((tf)->tf_eip)

typedef struct i386_frame *x86_frame_t;
#else
#define	PCB_FP(pcb)	((pcb)->pcb_rbp)
#define	TF_FP(tf)	((tf)->tf_rbp)
#define	TF_PC(tf)	((tf)->tf_rip)

typedef struct amd64_frame *x86_frame_t;
#endif

static void
stack_capture(struct thread *td, struct stack *st, register_t fp)
{
	x86_frame_t frame;
	vm_offset_t callpc;

	stack_zero(st);
	frame = (x86_frame_t)fp;
	while (1) {
		if (!INKERNEL((long)frame))
			break;
		callpc = frame->f_retaddr;
		if (!INKERNEL(callpc))
			break;
		if (stack_put(st, callpc) == -1)
			break;
		if (frame->f_frame <= frame ||
		    (vm_offset_t)frame->f_frame >= td->td_kstack +
		    td->td_kstack_pages * PAGE_SIZE)
			break;
		frame = frame->f_frame;
	}
}

void
stack_save_td(struct stack *st, struct thread *td)
{

	if (TD_IS_SWAPPED(td))
		panic("stack_save_td: swapped");
	if (TD_IS_RUNNING(td))
		panic("stack_save_td: running");

	stack_capture(td, st, PCB_FP(td->td_pcb));
}

void
stack_save(struct stack *st)
{
	register_t fp;

#ifdef __i386__
	__asm __volatile("movl %%ebp,%0" : "=g" (fp));
#else
	__asm __volatile("movq %%rbp,%0" : "=g" (fp));
#endif
	stack_capture(curthread, st, fp);
}
