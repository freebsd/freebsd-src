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
#include <machine/vmparam.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h> 
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>


int  db_md_set_watchpoint(db_expr_t addr, db_expr_t size);
int  db_md_clr_watchpoint(db_expr_t addr, db_expr_t size);
void db_md_list_watchpoints(void);

void
db_stack_trace_cmd(db_expr_t addr, boolean_t have_addr, db_expr_t count,
    char *modif)
{
	struct unw_regstate rs;
	const char *name;
	db_expr_t offset;
	uint64_t bsp, cfm, ip, pfs, reg;
	c_db_sym_t sym;
	int args, error, i;

	error = unw_create(&rs, &ddb_regs);
	while (!error && count--) {
		error = unw_get_cfm(&rs, &cfm);
		if (!error)
			error = unw_get_bsp(&rs, &bsp);
		if (!error)
			error = unw_get_ip(&rs, &ip);
		if (error)
			break;

		args = (cfm >> 7) & 0x7f;
		if (args > 8)
			args = 8;

		error = unw_step(&rs);
		if (!error) {
			error = unw_get_cfm(&rs, &pfs);
			if (!error) {
				i = (pfs & 0x7f) - ((pfs >> 7) & 0x7f);
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
	}
}

void
db_print_backtrace(void)
{
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
