/*-
 * Copyright (c) 2011-2014 Robert N. M. Watson
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

#include <machine/atomic.h>
#include <machine/cheri.h>
#include <machine/pcb.h>
#include <machine/sysarch.h>

#ifdef DDB
/*
 * Variation that prints live register state from the capability coprocessor.
 */
DB_SHOW_COMMAND(cheri, ddb_dump_cheri)
{
	register_t cause;

	db_printf("CHERI registers\n");
	DB_CHERI_REG_PRINT(0, 0);
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
	CHERI_CGETCAUSE(cause);
	db_printf("CHERI cause: ExcCode: 0x%02x RegNum: 0x%02x\n",
	    (uint8_t)((cause >> 8) & 0xff), (uint8_t)(cause & 0x1f));
}

/*
 * Variation that prints the saved userspace CHERI register frame for a
 * thread.
 */
DB_SHOW_COMMAND(cheriframe, ddb_dump_cheriframe)
{
	struct thread *td;
	struct cheri_frame *cfp;
	u_int i;

	if (have_addr)
		td = db_lookup_thread(addr, TRUE);
	else
		td = curthread;

	cfp = &td->td_pcb->pcb_cheriframe;
	db_printf("Thread %d at %p\n", td->td_tid, td);
	db_printf("CHERI frame at %p\n", cfp);

	/* Laboriously load and print each user capability. */
	for (i = 0; i < 27; i++) {
		cheri_capability_load(CHERI_CR_CTEMP0,
		    (struct chericap *)&cfp->cf_c0 + i);
		DB_CHERI_REG_PRINT(CHERI_CR_CTEMP0, i);
	}
	cheri_capability_load(CHERI_CR_CTEMP0,
	    (struct chericap *)&cfp->cf_c0 + CHERIFRAME_OFF_PCC);
	db_printf("PCC ");
	DB_CHERI_CAP_PRINT(CHERI_CR_CTEMP0);
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
		db_printf("  IDC ");
		DB_CHERI_CAP_PRINT(CHERI_CR_CTEMP0);

		CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, &csfp->csf_pcc, 0);
		db_printf("  PCC ");
		DB_CHERI_CAP_PRINT(CHERI_CR_CTEMP0);
	}
}
#endif
