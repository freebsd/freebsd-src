/*-
 * Copyright (c) 2004-2005, Juniper Networks, Inc.
 * All rights reserved.
 *
 *	JNPR: db_trace.c,v 1.8 2007/08/09 11:23:32 katta
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/proc.h>
#include <sys/stack.h>
#include <sys/sysent.h>

#include <machine/db_machdep.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

#include <ddb/ddb.h>

int
db_md_set_watchpoint(db_expr_t addr, db_expr_t size)
{

	return(0);
}


int
db_md_clr_watchpoint( db_expr_t addr, db_expr_t size)
{

	return(0);
}


void
db_md_list_watchpoints()
{
}

static int
db_backtrace(struct thread *td, db_addr_t frame, int count)
{
	stacktrace_subr((struct trapframe *)frame,
	    (int (*) (const char *, ...))db_printf);
	return (0);
}

void
db_trace_self(void)
{
	db_trace_thread (curthread, -1);
	return;
}

int
db_trace_thread(struct thread *thr, int count)
{
	struct pcb *ctx;

	ctx = kdb_thr_ctx(thr);
	return (db_backtrace(thr, (db_addr_t) &ctx->pcb_regs, count));
}

void
db_show_mdpcpu(struct pcpu *pc)
{

	db_printf("ipis	    = 0x%x\n", pc->pc_pending_ipis);
	db_printf("next ASID    = %d\n", pc->pc_next_asid);
	db_printf("GENID	    = %d\n", pc->pc_asid_generation);
	return;
}
