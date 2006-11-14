/*	$NetBSD: undefined.c,v 1.22 2003/11/29 22:21:29 bjh21 Exp $	*/

/*-
 * Copyright (c) 2001 Ben Harris.
 * Copyright (c) 1995 Mark Brinicombe.
 * Copyright (c) 1995 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Brini.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * undefined.c
 *
 * Fault handler
 *
 * Created      : 06/01/95
 */


#include "opt_ddb.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/malloc.h>
#include <sys/queue.h>
#include <sys/signal.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/syslog.h>
#include <sys/vmmeter.h>
#include <sys/types.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/signalvar.h>
#include <sys/ptrace.h>
#ifdef KDB
#include <sys/kdb.h>
#endif
#ifdef FAST_FPE
#include <sys/acct.h>
#endif

#include <vm/vm.h>
#include <vm/vm_extern.h>

#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/undefined.h>
#include <machine/trap.h>

#include <machine/disassem.h>

#ifdef DDB
#include <ddb/db_output.h>
#include <machine/db_machdep.h>
#endif

#ifdef acorn26
#include <machine/machdep.h>
#endif

static int gdb_trapper(u_int, u_int, struct trapframe *, int);
#ifdef FAST_FPE
extern int want_resched;
#endif

LIST_HEAD(, undefined_handler) undefined_handlers[MAX_COPROCS];


void *
install_coproc_handler(int coproc, undef_handler_t handler)
{
	struct undefined_handler *uh;

	KASSERT(coproc >= 0 && coproc < MAX_COPROCS, ("bad coproc"));
	KASSERT(handler != NULL, ("handler is NULL")); /* Used to be legal. */

	/* XXX: M_TEMP??? */
	MALLOC(uh, struct undefined_handler *, sizeof(*uh), M_TEMP, M_WAITOK);
	uh->uh_handler = handler;
	install_coproc_handler_static(coproc, uh);
	return uh;
}

void
install_coproc_handler_static(int coproc, struct undefined_handler *uh)
{

	LIST_INSERT_HEAD(&undefined_handlers[coproc], uh, uh_link);
}

void
remove_coproc_handler(void *cookie)
{
	struct undefined_handler *uh = cookie;

	LIST_REMOVE(uh, uh_link);
	FREE(uh, M_TEMP);
}


static int
gdb_trapper(u_int addr, u_int insn, struct trapframe *frame, int code)
{
	struct thread *td;
	td = (curthread == NULL) ? &thread0 : curthread;

	if (insn == GDB_BREAKPOINT || insn == GDB5_BREAKPOINT) {
		if (code == FAULT_USER) {
			trapsignal(td, SIGTRAP, 0);
			return 0;
		}
#if 0
#ifdef KGDB
		return !kgdb_trap(T_BREAKPOINT, frame);
#endif
#endif
	}
	return 1;
}

static struct undefined_handler gdb_uh;

void
undefined_init()
{
	int loop;

	/* Not actually necessary -- the initialiser is just NULL */
	for (loop = 0; loop < MAX_COPROCS; ++loop)
		LIST_INIT(&undefined_handlers[loop]);

	/* Install handler for GDB breakpoints */
	gdb_uh.uh_handler = gdb_trapper;
	install_coproc_handler_static(0, &gdb_uh);
}


void
undefinedinstruction(trapframe_t *frame)
{
	struct thread *td;
	u_int fault_pc;
	int fault_instruction;
	int fault_code;
	int coprocessor;
	struct undefined_handler *uh;
#ifdef VERBOSE_ARM32
	int s;
#endif

	/* Enable interrupts if they were enabled before the exception. */
	if (!(frame->tf_spsr & I32_bit))
		enable_interrupts(I32_bit);

	frame->tf_pc -= INSN_SIZE;
	PCPU_LAZY_INC(cnt.v_trap);

	fault_pc = frame->tf_pc;

	/* 
	 * Get the current thread/proc structure or thread0/proc0 if there is 
	 * none.
	 */
	td = curthread == NULL ? &thread0 : curthread;

	/*
	 * Make sure the program counter is correctly aligned so we
	 * don't take an alignment fault trying to read the opcode.
	 */
	if (__predict_false((fault_pc & 3) != 0)) {
		trapsignal(td, SIGILL, 0);
		userret(td, frame, 0);
		return;
	}

	/*
	 * Should use fuword() here .. but in the interests of squeezing every
	 * bit of speed we will just use ReadWord(). We know the instruction
	 * can be read as was just executed so this will never fail unless the
	 * kernel is screwed up in which case it does not really matter does
	 * it ?
	 */

	fault_instruction = *(u_int32_t *)fault_pc;

	/* Update vmmeter statistics */
#if 0
	uvmexp.traps++;
#endif
	/* Check for coprocessor instruction */

	/*
	 * According to the datasheets you only need to look at bit 27 of the
	 * instruction to tell the difference between and undefined
	 * instruction and a coprocessor instruction following an undefined
	 * instruction trap.
	 */

	if ((fault_instruction & (1 << 27)) != 0)
		coprocessor = (fault_instruction >> 8) & 0x0f;
	else
		coprocessor = 0;

	if ((frame->tf_spsr & PSR_MODE) == PSR_USR32_MODE) {
		/*
		 * Modify the fault_code to reflect the USR/SVC state at
		 * time of fault.
		 */
		fault_code = FAULT_USER;
		td->td_frame = frame;
	} else
		fault_code = 0;

	/* OK this is were we do something about the instruction. */
	LIST_FOREACH(uh, &undefined_handlers[coprocessor], uh_link)
	    if (uh->uh_handler(fault_pc, fault_instruction, frame,
			       fault_code) == 0)
		    break;

	if (fault_code & FAULT_USER && fault_instruction == PTRACE_BREAKPOINT) {
		PROC_LOCK(td->td_proc);
		_PHOLD(td->td_proc);
		ptrace_clear_single_step(td);
		_PRELE(td->td_proc);
		PROC_UNLOCK(td->td_proc);
		return;
	}

	if (uh == NULL && (fault_code & FAULT_USER)) {
		/* Fault has not been handled */
		trapsignal(td, SIGILL, 0);
	}

	if ((fault_code & FAULT_USER) == 0) {
		if (fault_instruction == KERNEL_BREAKPOINT) {
#ifdef KDB
		kdb_trap(T_BREAKPOINT, 0, frame);
#else
		printf("No debugger in kernel.\n");
#endif
		return;
		} else
			panic("Undefined instruction in kernel.\n");
	}

#ifdef FAST_FPE
	/* Optimised exit code */
	{

		/*
		 * Check for reschedule request, at the moment there is only
		 * 1 ast so this code should always be run
		 */

		if (want_resched) {
			/*
			 * We are being preempted.
			 */
			preempt(0);
		}

		/* Invoke MI userret code */
		mi_userret(td);

#if 0
		l->l_priority = l->l_usrpri;

		curcpu()->ci_schedstate.spc_curpriority = l->l_priority;
#endif
	}

#else
	userret(td, frame, 0);
#endif
}
