/*-
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
#include <sys/proc.h>
#include <machine/inst.h>
#include <machine/db_machdep.h>
#include <machine/unwind.h>
#include <machine/rse.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h> 
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>


int  db_md_set_watchpoint   __P((db_expr_t addr, db_expr_t size));
int  db_md_clr_watchpoint   __P((db_expr_t addr, db_expr_t size));
void db_md_list_watchpoints __P((void));

extern char ia64_vector_table[], do_syscall[], do_syscall_end[];

void
db_stack_trace_cmd(db_expr_t addr, boolean_t have_addr, db_expr_t count, char *modif)
{
	struct ia64_unwind_state *us;

	if (count == -1)
		count = 65535;

	if (!have_addr) {
		us = ia64_create_unwind_state(&ddb_regs);
	} else {
		return;		/* XXX */
	}

	if (!us) {
		db_printf("db_stack_trace_cmd: can't create unwind state\n");
		return;
	}

	while (count--) {
		const char *	name;
		db_expr_t	ip;
		db_expr_t	offset;
		c_db_sym_t	sym;
		int		cfm, sof, sol, nargs, i;
		u_int64_t	*bsp;
		u_int64_t	*p, reg;

		ip = ia64_unwind_state_get_ip(us);
		cfm = ia64_unwind_state_get_cfm(us);
		bsp = ia64_unwind_state_get_bsp(us);
		sof = cfm & 0x7f;
		sol = (cfm >> 7) & 0x7f;

		sym = db_search_symbol(ip, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);

		db_printf("%s(", name);

		nargs = sof - sol;
		if (nargs > 8)
			nargs = 8;
		if (bsp >= (u_int64_t *)IA64_RR_BASE(5)) {
			for (i = 0; i < nargs; i++) {
				p = ia64_rse_register_address(bsp, 32 + i);
				db_read_bytes((vm_offset_t) p, sizeof(reg),
					      (caddr_t) &reg);
				if (i > 0)
					db_printf(", ");
				db_printf("0x%lx", reg);
			}
		}
		db_printf(") at ");

		db_printsym(ip, DB_STGY_PROC);
		db_printf("\n");

		/*
		 * Was this an exception? If so, we can keep unwinding
		 * based on the interrupted trapframe. We could do
		 * this by constructing funky unwind records in
		 * exception.s but this is easier.
		 */
		if (ip >= (u_int64_t) &ia64_vector_table[0]
		    && ip < (u_int64_t) &ia64_vector_table[32768]) {
			u_int64_t sp = ia64_unwind_state_get_sp(us);
			ia64_free_unwind_state(us);
			us = ia64_create_unwind_state((struct trapframe *)
						      (sp + 16));
		} else if (ip >= (u_int64_t) &do_syscall[0]
		      && ip < (u_int64_t) &do_syscall_end[0]) {
			u_int64_t sp = ia64_unwind_state_get_sp(us);
			ia64_free_unwind_state(us);
			us = ia64_create_unwind_state((struct trapframe *)
						      (sp + 16 + 8*8));
		} else {
			if (ia64_unwind_state_previous_frame(us))
				break;
		}

		ip = ia64_unwind_state_get_ip(us);
		if (!ip)
			break;
	}

	ia64_free_unwind_state(us);
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

