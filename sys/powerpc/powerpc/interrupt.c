/*
 * Copyright 2002 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Interrupts are dispatched to here from locore asm
 */

#include <sys/cdefs.h>                  /* RCS ID & Copyright macro defns */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/unistd.h>
#include <sys/vmmeter.h>

#include <machine/cpu.h>
#include <machine/db_machdep.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/spr.h>
#include <machine/sr.h>
#include <machine/interruptvar.h>

void powerpc_interrupt(struct trapframe *);

/*
 * External interrupt install routines
 */
static void (*powerpc_extintr_handler)(void);

void
ext_intr_install(void (*new_extint)(void))
{
	powerpc_extintr_handler = new_extint;
}

extern void	decr_intr(struct clockframe *);
extern void	trap(struct trapframe *);

/*
 * A very short dispatch, to try and maximise assembler code use
 * between all exception types. Maybe 'true' interrupts should go
 * here, and the trap code can come in separately
 */
void
powerpc_interrupt(struct trapframe *framep)
{
        struct thread *td;
	struct clockframe ckframe;

	td = curthread;

	switch (framep->exc) {
	case EXC_EXI:
		atomic_add_int(&td->td_intr_nesting_level, 1);
		(*powerpc_extintr_handler)();
		atomic_subtract_int(&td->td_intr_nesting_level, 1);	
		break;

	case EXC_DECR:
		atomic_add_int(&td->td_intr_nesting_level, 1);
		ckframe.srr0 = framep->srr0;
		ckframe.srr1 = framep->srr1;
		decr_intr(&ckframe);
		atomic_subtract_int(&td->td_intr_nesting_level, 1);	
		break;

	default:
		/*
		 * Re-enable interrupts and call the generic trap code
		 */
#if 0
		printf("powerpc_interrupt: got trap\n");
		mtmsr(mfmsr() | PSL_EE);
		isync();
#endif
		trap(framep);
	}	        
}
