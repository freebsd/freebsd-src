/*-
 * Copyright (c) 2003, 2004 Marcel Moolenaar
 * Copyright (c) 2000-2001 Doug Rabson
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
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/proc.h>
#include <sys/stack.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/unwind.h>
#include <machine/vmparam.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h> 
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>

static int
db_backtrace(struct thread *td, struct pcb *pcb, int count)
{
	struct unw_regstate rs;
	struct trapframe *tf;
	const char *name;
	db_expr_t offset;
	uint64_t bsp, cfm, ip, pfs, reg, sp;
	c_db_sym_t sym;
	int args, error, i, quit;

	quit = 0;
	db_setup_paging(db_simple_pager, &quit, db_lines_per_page);
	error = unw_create_from_pcb(&rs, pcb);
	while (!error && count-- && !quit) {
		error = unw_get_cfm(&rs, &cfm);
		if (!error)
			error = unw_get_bsp(&rs, &bsp);
		if (!error)
			error = unw_get_ip(&rs, &ip);
		if (!error)
			error = unw_get_sp(&rs, &sp);
		if (error)
			break;

		args = IA64_CFM_SOL(cfm);
		if (args > 8)
			args = 8;

		error = unw_step(&rs);
		if (!error) {
			if (!unw_get_cfm(&rs, &pfs)) {
				i = IA64_CFM_SOF(pfs) - IA64_CFM_SOL(pfs);
				if (args > i)
					args = i;
			}
		}

		sym = db_search_symbol(ip, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);
		db_printf("%s(", name);
		if (bsp >= IA64_RR_BASE(5)) {
			for (i = 0; i < args; i++) {
				if ((bsp & 0x1ff) == 0x1f8)
					bsp += 8;
				db_read_bytes(bsp, sizeof(reg), (void*)&reg);
				if (i > 0)
					db_printf(", ");
				db_printf("0x%lx", reg);
				bsp += 8;
			}
		} else
			db_printf("...");
		db_printf(") at ");

		db_printsym(ip, DB_STGY_PROC);
		db_printf("\n");

		if (error != ERESTART)
			continue;
		if (sp < IA64_RR_BASE(5))
			break;

		tf = (struct trapframe *)(sp + 16);
		if ((tf->tf_flags & FRAME_SYSCALL) != 0 ||
		    tf->tf_special.iip < IA64_RR_BASE(5))
			break;

		/* XXX ask if we should unwind across the trapframe. */
		db_printf("--- trapframe at %p\n", tf);
		unw_delete(&rs);
		error = unw_create_from_frame(&rs, tf);
	}

	unw_delete(&rs);
	/*
	 * EJUSTRETURN and ERESTART signal the end of a trace and
	 * are not really errors.
	 */
	return ((error > 0) ? error : 0);
}

void
db_trace_self(void)
{
	struct pcb pcb;

	savectx(&pcb);
	db_backtrace(curthread, &pcb, -1);
}

int
db_trace_thread(struct thread *td, int count)
{
	struct pcb *ctx;

	ctx = kdb_thr_ctx(td);
	return (db_backtrace(td, ctx, count));
}

void
stack_save(struct stack *st)
{

	stack_zero(st);
	/*
	 * Nothing for now.
	 * Is libuwx reentrant?
	 * Can unw_create* sleep?
	 */
}

int
db_md_set_watchpoint(addr, size)
	db_expr_t addr;
	db_expr_t size;
{
	return (-1);
}


int
db_md_clr_watchpoint(addr, size)
	db_expr_t addr;
	db_expr_t size;
{
	return (-1);
}


void
db_md_list_watchpoints()
{
	return;
}
