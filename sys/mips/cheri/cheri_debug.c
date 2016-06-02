/*-
 * Copyright (c) 2011-2016 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/syscall.h>
#include <sys/sysctl.h>

#include <ddb/ddb.h>
#include <sys/kdb.h>

#include <cheri/cheri.h>

#include <machine/atomic.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>

#ifdef DDB
/*
 * Variation that prints live register state from the capability coprocessor.
 *
 * NB: Over time we will shift towards special registers holding values such
 * as $ddc.  As a result, we must move those values through a temporary
 * register that is hence overwritten.
 */
DB_SHOW_COMMAND(cp2, ddb_dump_cp2)
{
	register_t cause;
	uint8_t exccode, regnum;

	CHERI_CGETCAUSE(cause);
	exccode = (cause & CHERI_CAPCAUSE_EXCCODE_MASK) >>
	    CHERI_CAPCAUSE_EXCCODE_SHIFT;
	regnum = cause & CHERI_CAPCAUSE_REGNUM_MASK;
	db_printf("CHERI cause: ExcCode: 0x%02x ", exccode);
	if (regnum < 32)
		db_printf("RegNum: $c%02d ", regnum);
	else if (regnum == 255)
		db_printf("RegNum: $pcc ");
	else
		db_printf("RegNum: invalid (%d) ", regnum);
	db_printf("(%s)\n", cheri_exccode_string(exccode));

	/* Shift $ddc into $ctemp for printing. */
	CHERI_CGETDEFAULT(CHERI_CR_CTEMP0);
	DB_CHERI_REG_PRINT(CHERI_CR_CTEMP0, 0);
	DB_CHERI_REG_PRINT(1, 1);
	DB_CHERI_REG_PRINT(2, 2);
	DB_CHERI_REG_PRINT(3, 3);
	DB_CHERI_REG_PRINT(4, 4);
	DB_CHERI_REG_PRINT(5, 5);
	DB_CHERI_REG_PRINT(6, 6);
	DB_CHERI_REG_PRINT(7, 7);
	DB_CHERI_REG_PRINT(8, 8);
	DB_CHERI_REG_PRINT(9, 9);
	DB_CHERI_REG_PRINT(10, 10);
	DB_CHERI_REG_PRINT(11, 11);
	DB_CHERI_REG_PRINT(12, 12);
	DB_CHERI_REG_PRINT(13, 13);
	DB_CHERI_REG_PRINT(14, 14);
	DB_CHERI_REG_PRINT(15, 15);
	DB_CHERI_REG_PRINT(16, 16);
	DB_CHERI_REG_PRINT(17, 17);
	DB_CHERI_REG_PRINT(18, 18);
	DB_CHERI_REG_PRINT(19, 19);
	DB_CHERI_REG_PRINT(20, 20);
	DB_CHERI_REG_PRINT(21, 21);
	DB_CHERI_REG_PRINT(22, 22);
	DB_CHERI_REG_PRINT(23, 23);
	DB_CHERI_REG_PRINT(24, 24);
	DB_CHERI_REG_PRINT(25, 25);
	DB_CHERI_REG_PRINT(26, 26);
	DB_CHERI_REG_PRINT(27, 27);
	DB_CHERI_REG_PRINT(28, 28);
	DB_CHERI_REG_PRINT(29, 29);
	DB_CHERI_REG_PRINT(30, 30);
	DB_CHERI_REG_PRINT(31, 31);
}

static void
db_show_cheri_trapframe(struct trapframe *frame)
{
	register_t cause;
	uint8_t exccode, regnum;
	u_int i;

	db_printf("Trapframe at %p\n", frame);
	cause = frame->capcause;
	exccode = (cause & CHERI_CAPCAUSE_EXCCODE_MASK) >>
	    CHERI_CAPCAUSE_EXCCODE_SHIFT;
	regnum = cause & CHERI_CAPCAUSE_REGNUM_MASK;
	db_printf("CHERI cause: ExcCode: 0x%02x ", exccode);
	if (regnum < 32)
		db_printf("RegNum: $c%02d ", regnum);
	else if (regnum == 255)
		db_printf("RegNum: $pcc ");
	else
		db_printf("RegNum: invalid (%d) ", regnum);
	db_printf("(%s)\n", cheri_exccode_string(exccode));

	cheri_capability_load(CHERI_CR_CTEMP0, &frame->ddc);
	db_printf("$ddc ");
	DB_CHERI_CAP_PRINT(CHERI_CR_CTEMP0);
	cheri_capability_load(CHERI_CR_CTEMP0, &frame->pcc);
	db_printf("$pcc ");
	DB_CHERI_CAP_PRINT(CHERI_CR_CTEMP0);
	db_printf("\n");

	/* Laboriously load and print each trapframe capability. */
	for (i = 1; i < 27; i++) {
		cheri_capability_load(CHERI_CR_CTEMP0, &frame->ddc + i);
		DB_CHERI_REG_PRINT(CHERI_CR_CTEMP0, i);
	}

}

/*
 * Variation that prints register state from the trap frame provided by KDB.
 */
DB_SHOW_COMMAND(cheri, ddb_dump_cheri)
{

	db_show_cheri_trapframe(kdb_frame);
}

/*
 * Variation that prints the saved userspace CHERI register frame for a
 * thread.
 */
DB_SHOW_COMMAND(cheripcb, ddb_dump_cheripcb)
{
	struct thread *td;
	struct trapframe *frame;

	if (have_addr)
		td = db_lookup_thread(addr, TRUE);
	else
		td = curthread;

	frame = &td->td_pcb->pcb_regs;
	db_printf("Thread %d at %p\n", td->td_tid, td);
	db_show_cheri_trapframe(frame);
}

/*
 * Print out the trusted stack for the current thread, starting at the top.
 *
 * XXXRW: Would be nice to take a tid/pid argument rather than always use
 * curthread.
 */
DB_SHOW_COMMAND(cheristack, ddb_dump_cheristack)
{
	struct cheri_stack_frame *csfp;
	struct pcb *pcb = curthread->td_pcb;
	int i;

	db_printf("Trusted stack for TID %d; TSP 0x%016jx\n",
	    curthread->td_tid, (uintmax_t)pcb->pcb_cheristack.cs_tsp);
	for (i = CHERI_STACK_DEPTH - 1; i >= 0; i--) {
	    /* i > (pcb->pcb_cheristack.cs_tsp / CHERI_FRAME_SIZE); i--) { */
		csfp = &pcb->pcb_cheristack.cs_frames[i];

		db_printf("Frame %d%c\n", i,
		    (i >= (pcb->pcb_cheristack.cs_tsp / CHERI_FRAME_SIZE)) ?
		    '*' : ' ');

		CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &csfp->csf_idc, 0);
		db_printf("  $idc ");
		DB_CHERI_CAP_PRINT(CHERI_CR_CTEMP0);

		CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &csfp->csf_pcc, 0);
		db_printf("  $ppc ");
		DB_CHERI_CAP_PRINT(CHERI_CR_CTEMP0);
	}
}
#endif
