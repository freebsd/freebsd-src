/*-
 * Copyright (C) 2006 Semihalf, Rafal Jaworowski <raj@semihalf.com>
 * Copyright 2002 by Peter Grehan.
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
 */

/*
 * Interrupts are dispatched to here from locore asm
 */

#include <sys/cdefs.h>                  /* RCS ID & Copyright macro defns */
__FBSDID("$FreeBSD$");

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
#include <machine/frame.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/trap.h>

#include "pic_if.h"

extern void decr_intr(struct trapframe *);

void powerpc_decr_interrupt(struct trapframe *);
void powerpc_extr_interrupt(struct trapframe *);
void powerpc_crit_interrupt(struct trapframe *);
void powerpc_mchk_interrupt(struct trapframe *);

static void dump_frame(struct trapframe *framep);

static void
dump_frame(struct trapframe *frame)
{
	int i;

	printf("\n*** *** STACK FRAME DUMP *** ***\n");
	printf("  exc  = 0x%x\n", frame->exc);
	printf("  srr0 = 0x%08x\n", frame->srr0);
	printf("  srr1 = 0x%08x\n", frame->srr1);
	printf("  dear = 0x%08x\n", frame->cpu.booke.dear);
	printf("  esr  = 0x%08x\n", frame->cpu.booke.esr);
	printf("  lr   = 0x%08x\n", frame->lr);
	printf("  cr   = 0x%08x\n", frame->cr);
	printf("  sp   = 0x%08x\n", frame->fixreg[1]);

	for (i = 0; i < 32; i++) {
		printf("  R%02d = 0x%08x", i, frame->fixreg[i]);
		if ((i & 0x3) == 3)
			printf("\n");
	}
	printf("\n");
}

void powerpc_crit_interrupt(struct trapframe *framep)
{

	printf("powerpc_crit_interrupt: critical interrupt!\n");
	dump_frame(framep);
	trap(framep);
}

void powerpc_mchk_interrupt(struct trapframe *framep)
{

	printf("powerpc_mchk_interrupt: machine check interrupt!\n");
	dump_frame(framep);
	trap(framep);
}

/*
 * Decrementer interrupt routine
 */
void
powerpc_decr_interrupt(struct trapframe *framep)
{
	struct thread *td;
	struct trapframe *oldframe;

	td = curthread;
	critical_enter();
	atomic_add_int(&td->td_intr_nesting_level, 1);
	oldframe = td->td_intr_frame;
	td->td_intr_frame = framep;
	decr_intr(framep);
	td->td_intr_frame = oldframe;
	atomic_subtract_int(&td->td_intr_nesting_level, 1);
	critical_exit();
	framep->srr1 &= ~PSL_WE;
}

/*
 * External input interrupt routine
 */
void
powerpc_extr_interrupt(struct trapframe *framep)
{

	critical_enter();
	PIC_DISPATCH(root_pic, framep);
	critical_exit();
	framep->srr1 &= ~PSL_WE;
}
