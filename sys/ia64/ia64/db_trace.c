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
#include <machine/db_machdep.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h> 
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>

void
db_stack_trace_cmd(db_expr_t addr, boolean_t have_addr, db_expr_t count, char *modif)
{
	db_addr_t callpc;
	u_int64_t *bsp;
	int sof, sol;

	if (count == -1)
		count = 65535;

	if (!have_addr) {
		callpc = (db_addr_t)ddb_regs.tf_cr_iip;
		bsp = db_rse_current_frame();
		sof = ddb_regs.tf_cr_ifs & 0x7f;
		sol = (ddb_regs.tf_cr_ifs >> 7) & 0x7f;
	} else {
		callpc = 0;	/* XXX */
		bsp = 0;	/* XXX */
		sof = 0;	/* XXX */
		sol = 0;	/* XXX */
	}

	while (count--) {
		const char *	name;
		db_expr_t	offset;
		c_db_sym_t	sym;
		u_int64_t	ar_pfs;
		u_int64_t	newpc;
		int		newsof, newsol, i;

		/*
		 * XXX this assumes the simplistic stack frames used
		 * by the old toolchain.
		 */
		ar_pfs = *db_rse_register_address(bsp, 32 + sol - 1);
		newpc = *db_rse_register_address(bsp, 32 + sol - 2);
		newsof = ar_pfs & 0x7f;
		newsol = (ar_pfs >> 7) & 0x7f;

		sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);

		db_printf("%s(", name);

		for (i = 0; i < newsof - newsol; i++) {
			if (i > 0)
				db_printf(", ");
			db_printf("0x%lx",
				  *db_rse_register_address(bsp, 32 + i));
		}
		db_printf(") at ");

		db_printsym(callpc, DB_STGY_PROC);
		db_printf("\n");

		bsp = db_rse_previous_frame(bsp, newsol);
		callpc = newpc;
		sol = newsol;
		sof = newsof;
		if (!callpc)
			break;
	}
}
