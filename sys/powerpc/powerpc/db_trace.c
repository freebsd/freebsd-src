/*	$FreeBSD$ */
/*	$NetBSD: db_trace.c,v 1.20 2002/05/13 20:30:09 matt Exp $	*/
/*	$OpenBSD: db_trace.c,v 1.3 1997/03/21 02:10:48 niklas Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 * any improvements or extensions that they make and grant Carnegie Mellon 
 * the rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/db_machdep.h>
#include <machine/spr.h>
#include <machine/trap.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

struct db_variable db_regs[] = {
	{ "r0",  &ddb_regs.r[0],  FCN_NULL },
	{ "r1",  &ddb_regs.r[1],  FCN_NULL },
	{ "r2",  &ddb_regs.r[2],  FCN_NULL },
	{ "r3",  &ddb_regs.r[3],  FCN_NULL },
	{ "r4",  &ddb_regs.r[4],  FCN_NULL },
	{ "r5",  &ddb_regs.r[5],  FCN_NULL },
	{ "r6",  &ddb_regs.r[6],  FCN_NULL },
	{ "r7",  &ddb_regs.r[7],  FCN_NULL },
	{ "r8",  &ddb_regs.r[8],  FCN_NULL },
	{ "r9",  &ddb_regs.r[9],  FCN_NULL },
	{ "r10", &ddb_regs.r[10], FCN_NULL },
	{ "r11", &ddb_regs.r[11], FCN_NULL },
	{ "r12", &ddb_regs.r[12], FCN_NULL },
	{ "r13", &ddb_regs.r[13], FCN_NULL },
	{ "r14", &ddb_regs.r[14], FCN_NULL },
	{ "r15", &ddb_regs.r[15], FCN_NULL },
	{ "r16", &ddb_regs.r[16], FCN_NULL },
	{ "r17", &ddb_regs.r[17], FCN_NULL },
	{ "r18", &ddb_regs.r[18], FCN_NULL },
	{ "r19", &ddb_regs.r[19], FCN_NULL },
	{ "r20", &ddb_regs.r[20], FCN_NULL },
	{ "r21", &ddb_regs.r[21], FCN_NULL },
	{ "r22", &ddb_regs.r[22], FCN_NULL },
	{ "r23", &ddb_regs.r[23], FCN_NULL },
	{ "r24", &ddb_regs.r[24], FCN_NULL },
	{ "r25", &ddb_regs.r[25], FCN_NULL },
	{ "r26", &ddb_regs.r[26], FCN_NULL },
	{ "r27", &ddb_regs.r[27], FCN_NULL },
	{ "r28", &ddb_regs.r[28], FCN_NULL },
	{ "r29", &ddb_regs.r[29], FCN_NULL },
	{ "r30", &ddb_regs.r[30], FCN_NULL },
	{ "r31", &ddb_regs.r[31], FCN_NULL },
	{ "iar", &ddb_regs.iar,   FCN_NULL },
	{ "msr", &ddb_regs.msr,   FCN_NULL },
	{ "lr",  &ddb_regs.lr,    FCN_NULL },
	{ "ctr", &ddb_regs.ctr,   FCN_NULL },
	{ "cr",  &ddb_regs.cr,    FCN_NULL },
	{ "xer", &ddb_regs.xer,   FCN_NULL },
#ifdef PPC_IBM4XX
	{ "dear", &ddb_regs.dear, FCN_NULL },
	{ "esr", &ddb_regs.esr,   FCN_NULL },
	{ "pid", &ddb_regs.pid,   FCN_NULL },
#endif
};
struct db_variable *db_eregs = db_regs + sizeof (db_regs)/sizeof (db_regs[0]);

extern int trapexit[];
extern int end[];

/*
 *	Frame tracing.
 */
static void
db_stack_trace_print(db_expr_t addr, int have_addr, db_expr_t count, 
		     char *modif, void (*pr)(const char *, ...))
{
	db_addr_t frame, lr, caller, *args;
	db_addr_t fakeframe[2];
	db_expr_t diff;
	c_db_sym_t sym;
	const char *symname;
	boolean_t kernel_only = TRUE;
	boolean_t trace_thread = FALSE;
	boolean_t full = FALSE;

	{
		register char *cp = modif;
		register char c;

		while ((c = *cp++) != 0) {
			if (c == 't')
				trace_thread = TRUE;
			if (c == 'u')
				kernel_only = FALSE;
			if (c == 'f')
				full = TRUE;
		}
	}

	if (have_addr) {
#if 0
		if (trace_thread) {
			struct proc *p;
			struct user *u;

			(*pr)("trace: pid %d ", (int)addr);
			p = pfind(addr);
			if (p == NULL) {
				(*pr)("not found\n");
				return;
			}	
			if (!(p->p_flag&P_INMEM)) {
				(*pr)("swapped out\n");
				return;
			}
			u = p->p_addr;
			frame = (db_addr_t)u->u_pcb.pcb_sp;
			(*pr)("at %p\n", frame);
		} else
#endif
			frame = (db_addr_t)addr;
	} else {
		frame = (db_addr_t)ddb_regs.r[1];
	}
	for (;;) {
		if (frame < PAGE_SIZE)
			break;
#ifdef PPC_MPC6XX
		if (kernel_only &&
		    ((frame > (db_addr_t) end &&
		      frame < VM_MIN_KERNEL_ADDRESS) ||
		     frame >= VM_MAX_KERNEL_ADDRESS))
			break;
#endif 
		frame = *(db_addr_t *)frame;
	    next_frame:
		args = (db_addr_t *)(frame + 8);
		if (frame < PAGE_SIZE)
			break;
#ifdef PPC_MPC6XX
		if (kernel_only &&
		    ((frame > (db_addr_t) end &&
		      frame < VM_MIN_KERNEL_ADDRESS) ||
		     frame >= VM_MAX_KERNEL_ADDRESS))
			break;
#endif
	        if (count-- == 0)
			break;

		lr = *(db_addr_t *)(frame + 4) - 4;
		if ((lr & 3) || (lr < 0x100)) {
			(*pr)("saved LR(0x%x) is invalid.", lr);
			break;
		}
		if ((caller = (db_addr_t)vtophys(lr)) == 0)
			caller = lr;

		if (frame != (db_addr_t) fakeframe) {
			(*pr)("0x%08lx: ", frame);
		} else {
			(*pr)("     <?>  : ");
		}
		if (caller + 4 == (db_addr_t) &trapexit) {
			const char *trapstr;
			struct trapframe *tf = (struct trapframe *) (frame+8);
			(*pr)("%s ", tf->srr1 & PSL_PR ? "user" : "kernel");
			switch (tf->exc) {
			case EXC_DSI:
#ifdef PPC_MPC6XX
				(*pr)("DSI %s trap @ %#x by ",
				    tf->dsisr & DSISR_STORE ? "write" : "read",
				    tf->dar);
#endif
#ifdef PPC_IBM4XX
				(*pr)("DSI %s trap @ %#x by ",
				    tf->esr & ESR_DST ? "write" : "read",
				    tf->dear);
#endif
				goto print_trap;
			case EXC_ISI: trapstr = "ISI"; break;
			case EXC_PGM: trapstr = "PGM"; break;
			case EXC_SC: trapstr = "SC"; break;
			case EXC_EXI: trapstr = "EXI"; break;
			case EXC_MCHK: trapstr = "MCHK"; break;
			case EXC_VEC: trapstr = "VEC"; break;
			case EXC_FPU: trapstr = "FPU"; break;
			case EXC_FPA: trapstr = "FPA"; break;
			case EXC_DECR: trapstr = "DECR"; break;
			case EXC_ALI: trapstr = "ALI"; break;
			case EXC_BPT: trapstr = "BPT"; break;
			case EXC_TRC: trapstr = "TRC"; break;
			case EXC_RUNMODETRC: trapstr = "RUNMODETRC"; break;
			case EXC_PERF: trapstr = "PERF"; break;
			case EXC_SMI: trapstr = "SMI"; break;
			case EXC_RST: trapstr = "RST"; break;
			default: trapstr = NULL; break;
			}
			if (trapstr != NULL) {
				(*pr)("%s trap by ", trapstr);
			} else {
				(*pr)("trap %#x by ", tf->exc);
			}
		   print_trap:	
			lr = (db_addr_t) tf->srr0;
			if ((caller = (db_addr_t)vtophys(lr)) == 0)
				caller = lr;
			diff = 0;
			symname = NULL;
			sym = db_search_symbol(caller, DB_STGY_ANY, &diff);
			db_symbol_values(sym, &symname, 0);
			if (symname == NULL || !strcmp(symname, "end")) {
				(*pr)("%p: srr1=%#x\n", caller, tf->srr1);
			} else {
				(*pr)("%s+%x: srr1=%#x\n", symname, diff,
				    tf->srr1);
			}
			(*pr)("%-10s  r1=%#x cr=%#x xer=%#x ctr=%#x",
			    "", tf->fixreg[1], tf->cr, tf->xer, tf->ctr);
#ifdef PPC_MPC6XX
			if (tf->exc == EXC_DSI)
				(*pr)(" dsisr=%#x", tf->dsisr);
#endif
#ifdef PPC_IBM4XX
			if (tf->exc == EXC_DSI)
				(*pr)(" dear=%#x", tf->dear);
			(*pr)(" esr=%#x pid=%#x", tf->esr, tf->pid);
#endif
			(*pr)("\n");
			fakeframe[0] = (db_addr_t) tf->fixreg[1];
			fakeframe[1] = (db_addr_t) tf->lr;
			frame = (db_addr_t) fakeframe;
			if (kernel_only && (tf->srr1 & PSL_PR))
				break;
			goto next_frame;
		}

		diff = 0;
		symname = NULL;
		sym = db_search_symbol(caller, DB_STGY_ANY, &diff);
		db_symbol_values(sym, &symname, 0);
		if (symname == NULL || !strcmp(symname, "end"))
			(*pr)("at %p", caller);
		else
			(*pr)("at %s+%#x", symname, diff);
		if (full)
			/* Print all the args stored in that stackframe. */
			(*pr)("(%lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx)",
				args[0], args[1], args[2], args[3],
				args[4], args[5], args[6], args[7]);
		(*pr)("\n");
	}
}

void
db_stack_trace_cmd(db_expr_t addr, boolean_t have_addr, db_expr_t count,
    char *modif)
{

	db_stack_trace_print(addr, have_addr, count, modif, db_printf);
}

void
db_print_backtrace(void)
{
}
