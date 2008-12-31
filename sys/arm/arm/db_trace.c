/*	$NetBSD: db_trace.c,v 1.8 2003/01/17 22:28:48 thorpej Exp $	*/

/*-
 * Copyright (c) 2000, 2001 Ben Harris
 * Copyright (c) 1996 Scott K. Stevens
 *
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 * 
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 * 
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 * 
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/arm/db_trace.c,v 1.13.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");
#include <sys/param.h>
#include <sys/systm.h>


#include <sys/proc.h>
#include <sys/kdb.h>
#include <sys/stack.h>
#include <machine/armreg.h>
#include <machine/asm.h>
#include <machine/cpufunc.h>
#include <machine/db_machdep.h>
#include <machine/pcb.h>
#include <machine/stack.h>
#include <machine/vmparam.h>
#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>

/*
 * APCS stack frames are awkward beasts, so I don't think even trying to use
 * a structure to represent them is a good idea.
 *
 * Here's the diagram from the APCS.  Increasing address is _up_ the page.
 * 
 *          save code pointer       [fp]        <- fp points to here
 *          return link value       [fp, #-4]
 *          return sp value         [fp, #-8]
 *          return fp value         [fp, #-12]
 *          [saved v7 value]
 *          [saved v6 value]
 *          [saved v5 value]
 *          [saved v4 value]
 *          [saved v3 value]
 *          [saved v2 value]
 *          [saved v1 value]
 *          [saved a4 value]
 *          [saved a3 value]
 *          [saved a2 value]
 *          [saved a1 value]
 *
 * The save code pointer points twelve bytes beyond the start of the 
 * code sequence (usually a single STM) that created the stack frame.  
 * We have to disassemble it if we want to know which of the optional 
 * fields are actually present.
 */

static void
db_stack_trace_cmd(db_expr_t addr, db_expr_t count)
{
	u_int32_t	*frame, *lastframe;
	c_db_sym_t sym;
	const char *name;
	db_expr_t value;
	db_expr_t offset;
	boolean_t	kernel_only = TRUE;
	int	scp_offset;

	frame = (u_int32_t *)addr;
	lastframe = NULL;
	scp_offset = -(get_pc_str_offset() >> 2);

	while (count-- && frame != NULL && !db_pager_quit) {
		db_addr_t	scp;
		u_int32_t	savecode;
		int		r;
		u_int32_t	*rp;
		const char	*sep;

		/*
		 * In theory, the SCP isn't guaranteed to be in the function
		 * that generated the stack frame.  We hope for the best.
		 */
		scp = frame[FR_SCP];

		sym = db_search_symbol(scp, DB_STGY_ANY, &offset);
		if (sym == C_DB_SYM_NULL) {
			value = 0;
			name = "(null)";
		} else
			db_symbol_values(sym, &name, &value);
		db_printf("%s() at ", name);
		db_printsym(scp, DB_STGY_PROC);
		db_printf("\n");
#ifdef __PROG26
		db_printf("scp=0x%08x rlv=0x%08x (", scp, frame[FR_RLV] & R15_PC);
		db_printsym(frame[FR_RLV] & R15_PC, DB_STGY_PROC);
		db_printf(")\n");
#else
		db_printf("scp=0x%08x rlv=0x%08x (", scp, frame[FR_RLV]);
		db_printsym(frame[FR_RLV], DB_STGY_PROC);
		db_printf(")\n");
#endif
		db_printf("\trsp=0x%08x rfp=0x%08x", frame[FR_RSP], frame[FR_RFP]);

		savecode = ((u_int32_t *)scp)[scp_offset];
		if ((savecode & 0x0e100000) == 0x08000000) {
			/* Looks like an STM */
			rp = frame - 4;
			sep = "\n\t";
			for (r = 10; r >= 0; r--) {
				if (savecode & (1 << r)) {
					db_printf("%sr%d=0x%08x",
					    sep, r, *rp--);
					sep = (frame - rp) % 4 == 2 ?
					    "\n\t" : " ";
				}
			}
		}

		db_printf("\n");

		/*
		 * Switch to next frame up
		 */
		if (frame[FR_RFP] == 0)
			break; /* Top of stack */

		lastframe = frame;
		frame = (u_int32_t *)(frame[FR_RFP]);

		if (INKERNEL((int)frame)) {
			/* staying in kernel */
			if (frame <= lastframe) {
				db_printf("Bad frame pointer: %p\n", frame);
				break;
			}
		} else if (INKERNEL((int)lastframe)) {
			/* switch from user to kernel */
			if (kernel_only)
				break;	/* kernel stack only */
		} else {
			/* in user */
			if (frame <= lastframe) {
				db_printf("Bad user frame pointer: %p\n",
					  frame);
				break;
			}
		}
	}
}

/* XXX stubs */
void
db_md_list_watchpoints()
{
}

int
db_md_clr_watchpoint(db_expr_t addr, db_expr_t size)
{
	return (0);
}

int
db_md_set_watchpoint(db_expr_t addr, db_expr_t size)
{
	return (0);
}

int
db_trace_thread(struct thread *thr, int count)
{
	uint32_t addr;

	if (thr == curthread)
		addr = (uint32_t)__builtin_frame_address(0);
	else
		addr = thr->td_pcb->un_32.pcb32_r11;
	db_stack_trace_cmd(addr, -1);
	return (0);
}

void
db_trace_self(void)
{
	db_trace_thread(curthread, -1);
}
