/* $FreeBSD: src/sys/alpha/alpha/db_trace.c,v 1.3.2.1 2000/08/03 00:48:03 peter Exp $ */
/* $NetBSD: db_trace.c,v 1.1 1997/09/06 02:00:50 thorpej Exp $ */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

/* __KERNEL_RCSID(0, "$NetBSD: db_trace.c,v 1.1 1997/09/06 02:00:50 thorpej Exp $"); */

#include <sys/param.h>
#include <sys/proc.h>
#include <machine/db_machdep.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h> 
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>
#include <alpha/alpha/db_instruction.h>

struct alpha_proc {
	int		pcreg;		/* index of return pc register */
	int		frame_size;	/* size of stack frame */
	u_int32_t	reg_mask;
	int		regs[32];	/* offsets from sp to saved regs */
};

static void
parse_proc(db_expr_t addr, struct alpha_proc* frame)
{
	c_db_sym_t sym;
	db_expr_t func;
	db_expr_t junk, pc, limit;

	frame->pcreg = -1;
	frame->reg_mask = 0;
	frame->frame_size = 0;

	sym = db_search_symbol(addr, DB_STGY_PROC, &junk);
	if (!sym)
		return;
	db_symbol_values(sym, 0, &func);

	pc = func;
	limit = addr;
	if (limit - pc > 200)
		limit = pc + 200;
	for (; pc < limit; pc += 4) {
		alpha_instruction ins;
		ins.bits = *(u_int32_t*) pc;
		if (ins.memory_format.opcode == op_lda
		    && ins.memory_format.ra == 30) {
			/* gcc 2.7 */
			frame->frame_size += -ins.memory_format.offset;
		} else if (ins.operate_lit_format.opcode == op_arit
		    && ins.operate_lit_format.function == op_subq
		    && ins.operate_lit_format.rs == 30) {
		    	/* egcs */
			frame->frame_size += ins.operate_lit_format.literal;
		} else if (ins.memory_format.opcode == op_stq
			   && ins.memory_format.rb == 30
			   && ins.memory_format.ra != 31) {
			int reg = ins.memory_format.ra;
			frame->reg_mask |= 1 << reg;
			frame->regs[reg] = ins.memory_format.offset;
			if (frame->pcreg == -1 && reg == 26)
				frame->pcreg = reg;
		}
	}
}

void
db_stack_trace_cmd(db_expr_t addr, boolean_t have_addr, db_expr_t count, char *modif)
{
	db_addr_t callpc;
	db_addr_t frame;

	if (count == -1)
		count = 65535;

	if (!have_addr) {
		frame = (db_addr_t) ddb_regs.tf_regs[FRAME_SP];
		callpc = (db_addr_t)ddb_regs.tf_regs[FRAME_PC];
	} else {
		frame = (db_addr_t)addr;
		callpc = (db_addr_t)db_get_value(frame, 8, FALSE);
	}

	while (count--) {
		const char *	name;
		db_expr_t	offset;
		c_db_sym_t	sym;
		struct alpha_proc proc;

		sym = db_search_symbol(callpc, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);

		db_printf("%s() at ", name);
		db_printsym(callpc, DB_STGY_PROC);
		db_printf("\n");

		parse_proc(callpc, &proc);

		if (proc.pcreg == -1)
			break;

		callpc = db_get_value(frame + proc.regs[proc.pcreg], 8, FALSE);
		frame = frame + proc.frame_size;
	}
}
