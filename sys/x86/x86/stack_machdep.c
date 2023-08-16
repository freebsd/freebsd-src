/*-
 * Copyright (c) 2015 EMC Corporation
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
#include "opt_stack.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/stack.h>

#include <machine/pcb.h>
#include <machine/smp.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <machine/stack.h>

#ifdef __i386__
#define	PCB_FP(pcb)	((pcb)->pcb_ebp)
#define	TF_FLAGS(tf)	((tf)->tf_eflags)
#define	TF_FP(tf)	((tf)->tf_ebp)
#define	TF_PC(tf)	((tf)->tf_eip)

typedef struct i386_frame *x86_frame_t;
#else
#define	PCB_FP(pcb)	((pcb)->pcb_rbp)
#define	TF_FLAGS(tf)	((tf)->tf_rflags)
#define	TF_FP(tf)	((tf)->tf_rbp)
#define	TF_PC(tf)	((tf)->tf_rip)

typedef struct amd64_frame *x86_frame_t;
#endif

#ifdef SMP
static struct stack *stack_intr_stack;
static struct thread *stack_intr_td;
static struct mtx intr_lock;
MTX_SYSINIT(intr_lock, &intr_lock, "stack intr", MTX_DEF);
#endif

static void __nosanitizeaddress __nosanitizememory
stack_capture(struct thread *td, struct stack *st, register_t fp)
{
	x86_frame_t frame;
	vm_offset_t callpc;

	stack_zero(st);
	frame = (x86_frame_t)fp;
	while (1) {
		if (!kstack_contains(td, (vm_offset_t)frame, sizeof(*frame)))
			break;
		callpc = frame->f_retaddr;
		if (!INKERNEL(callpc))
			break;
		if (stack_put(st, callpc) == -1)
			break;
		if (frame->f_frame <= frame)
			break;
		frame = frame->f_frame;
	}
}

#ifdef SMP
void
stack_capture_intr(void)
{
	struct thread *td;

	td = curthread;
	stack_capture(td, stack_intr_stack, TF_FP(td->td_intr_frame));
	atomic_store_rel_ptr((void *)&stack_intr_td, (uintptr_t)td);
}
#endif

int
stack_save_td(struct stack *st, struct thread *td)
{
	int cpuid, error;
	bool done;

	THREAD_LOCK_ASSERT(td, MA_OWNED);
	KASSERT(!TD_IS_SWAPPED(td),
	    ("stack_save_td: thread %p is swapped", td));
	if (TD_IS_RUNNING(td) && td != curthread)
		PROC_LOCK_ASSERT(td->td_proc, MA_OWNED);

	if (td == curthread) {
		stack_save(st);
		return (0);
	}

	for (done = false, error = 0; !done;) {
		if (!TD_IS_RUNNING(td)) {
			/*
			 * The thread will not start running so long as we hold
			 * its lock.
			 */
			stack_capture(td, st, PCB_FP(td->td_pcb));
			error = 0;
			break;
		}

#ifdef SMP
		thread_unlock(td);
		cpuid = atomic_load_int(&td->td_oncpu);
		if (cpuid == NOCPU) {
			cpu_spinwait();
		} else {
			mtx_lock(&intr_lock);
			stack_intr_td = NULL;
			stack_intr_stack = st;
			ipi_cpu(cpuid, IPI_TRACE);
			while (atomic_load_acq_ptr((void *)&stack_intr_td) ==
			    (uintptr_t)NULL)
				cpu_spinwait();
			if (stack_intr_td == td) {
				done = true;
				error = st->depth > 0 ? 0 : EBUSY;
			}
			stack_intr_td = NULL;
			mtx_unlock(&intr_lock);
		}
		thread_lock(td);
#else
		(void)cpuid;
		KASSERT(0, ("%s: multiple running threads", __func__));
#endif
	}

	return (error);
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
