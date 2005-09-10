/* $NetBSD: db_trace.c,v 1.9 2000/12/13 03:16:36 mycroft Exp $ */

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Ross Harvey.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
/*__KERNEL_RCSID(0, "$NetBSD: db_trace.c,v 1.9 2000/12/13 03:16:36 mycroft Exp $");*/
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/linker.h>
#include <sys/proc.h>
#include <sys/stack.h>
#include <sys/sysent.h>

#include <machine/db_machdep.h>
#include <machine/md_var.h>
#include <machine/pcb.h>

#include <ddb/ddb.h>
#include <ddb/db_sym.h> 
#include <ddb/db_access.h>
#include <ddb/db_variables.h>
#include <ddb/db_output.h>
#include <alpha/alpha/db_instruction.h>

/*
 * Information about the `standard' Alpha function prologue.
 */
struct prologue_info {
	int	pi_reg_offset[32]; /* offset of registers in stack frame */
	u_int32_t pi_regmask;	   /* which registers are in frame */
	int	pi_frame_size;	   /* frame size */
};

/*
 * We use several symbols to take special action:
 *
 *	Trap vectors, which use a different (fixed-size) stack frame:
 *
 *		XentArith
 *		XentIF
 *		XentInt
 *		XentMM
 *		XentSys
 *		XentUna
 */

static struct special_symbol {
	uintptr_t ss_val;
	const char *ss_note;
} special_symbols[] = {
	{ (uintptr_t)&XentArith,	"arithmetic trap" },
	{ (uintptr_t)&XentIF,		"instruction fault" },
	{ (uintptr_t)&XentInt,		"interrupt" },
	{ (uintptr_t)&XentMM,		"memory management fault" },
	{ (uintptr_t)&XentSys,		"syscall" },
	{ (uintptr_t)&XentUna,		"unaligned access fault" },
	{ (uintptr_t)&XentRestart,	"console restart" },
	{ 0, NULL }
};


/*
 * Decode the function prologue for the function we're in, and note
 * which registers are stored where, and how large the stack frame is.
 */
static int
decode_prologue(db_addr_t callpc, db_addr_t func,
    struct prologue_info *pi)
{
	long signed_immediate;
	alpha_instruction ins;
	db_expr_t pc;

	pi->pi_regmask = 0;
	pi->pi_frame_size = 0;

#define	CHECK_FRAMESIZE							\
do {									\
	if (pi->pi_frame_size != 0) {					\
		db_printf("frame size botch: adjust register offsets?\n"); \
		return (1);						\
	}								\
} while (0)

	for (pc = func; pc < callpc; pc += sizeof(alpha_instruction)) {
		ins.bits = *(unsigned int *)pc;

		if (ins.memory_format.opcode == op_lda &&
		    ins.memory_format.ra == 30 &&
		    ins.memory_format.rb == 30) {
			/*
			 * GCC 2.7-style stack adjust:
			 *
			 *	lda	sp, -64(sp)
			 */
			signed_immediate = (long)ins.mem_format.displacement;
#if 1
			if (signed_immediate > 0) {
				db_printf("prologue botch: displacement %ld\n",
				    signed_immediate);
				return (1);
			}
#endif
			CHECK_FRAMESIZE;
			pi->pi_frame_size += -signed_immediate;
		} else if (ins.operate_lit_format.opcode == op_arit &&
			   ins.operate_lit_format.function == op_subq &&
			   ins.operate_lit_format.rs == 30 &&
			   ins.operate_lit_format.rd == 30) {
			/*
			 * EGCS-style stack adjust:
			 *
			 *	subq	sp, 64, sp
			 */
			CHECK_FRAMESIZE;
			pi->pi_frame_size += ins.operate_lit_format.literal;
		} else if (ins.mem_format.opcode == op_stq &&
			   ins.mem_format.rs == 30 &&
			   ins.mem_format.rd != 31) {
			/* Store of (non-zero) register onto the stack. */
			signed_immediate = (long)ins.mem_format.displacement;
			pi->pi_regmask |= 1 << ins.mem_format.rd;
			pi->pi_reg_offset[ins.mem_format.rd] = signed_immediate;
		}
	}
	return (0);
}

static int
sym_is_trapsymbol(uintptr_t v)
{
	int i;

	for (i = 0; special_symbols[i].ss_val != 0; ++i)
		if (v == special_symbols[i].ss_val)
			return 1;
	return 0;
}

static void
decode_syscall(int number, struct thread *td)
{
	struct proc *p;
	c_db_sym_t sym;
	db_expr_t diff;
	sy_call_t *f;
	const char *symname;

	p = (td != NULL) ? td->td_proc : NULL;
	db_printf(" (%d", number);
	if (p != NULL && 0 <= number && number < p->p_sysent->sv_size) {
		f = p->p_sysent->sv_table[number].sy_call;
		sym = db_search_symbol((db_addr_t)f, DB_STGY_ANY, &diff);
		if (sym != DB_SYM_NULL && diff == 0) {
			db_symbol_values(sym, &symname, NULL);
			db_printf(", %s, %s", p->p_sysent->sv_name, symname);
		}
	}
	db_printf(")");	
}

static int
db_backtrace(struct thread *td, db_addr_t frame, db_addr_t pc, int count)
{
	struct prologue_info pi;
	struct trapframe *tf;
	const char *symname;
	c_db_sym_t sym;
	db_expr_t diff;
	db_addr_t symval;
	u_long last_ipl, tfps;
	int i, quit;

	if (count == -1)
		count = 1024;

	last_ipl = ~0L;
	tf = NULL;
	quit = 0;
	db_setup_paging(db_simple_pager, &quit, db_lines_per_page);
	while (count-- && !quit) {
		sym = db_search_symbol(pc, DB_STGY_ANY, &diff);
		if (sym == DB_SYM_NULL)
			return (ENOENT);

		db_symbol_values(sym, &symname, (db_expr_t *)&symval);

		if (pc < symval) {
			db_printf("symbol botch: pc 0x%lx < "
			    "func 0x%lx (%s)\n", pc, symval, symname);
			return (0);
		}

		/*
		 * XXX Printing out arguments is Hard.  We'd have to
		 * keep lots of state as we traverse the frame, figuring
		 * out where the arguments to the function are stored
		 * on the stack.
		 *
		 * Even worse, they may be stored to the stack _after_
		 * being modified in place; arguments are passed in
		 * registers.
		 *
		 * So, in order for this to work reliably, we pretty much
		 * have to have a kernel built with `cc -g':
		 *
		 *	- The debugging symbols would tell us where the
		 *	  arguments are, how many there are, if there were
		 *	  any passed on the stack, etc.
		 *
		 *	- Presumably, the compiler would be careful to
		 *	  store the argument registers on the stack before
		 *	  modifying the registers, so that a debugger could
		 *	  know what those values were upon procedure entry.
		 *
		 * Because of this, we don't bother.  We've got most of the
		 * benefit of back tracking without the arguments, and we
		 * could get the arguments if we use a remote source-level
		 * debugger (for serious debugging).
		 */
		db_printf("%s() at ", symname);
		db_printsym(pc, DB_STGY_PROC);
		db_printf("\n");

		/*
		 * If we are in a trap vector, frame points to a
		 * trapframe.
		 */
		if (sym_is_trapsymbol(symval)) {
			tf = (struct trapframe *)frame;
			for (i = 0; special_symbols[i].ss_val != 0; ++i)
				if (symval == special_symbols[i].ss_val)
					db_printf("--- %s",
					    special_symbols[i].ss_note);

			tfps = tf->tf_regs[FRAME_PS];
			if (symval == (uintptr_t)&XentSys)
				decode_syscall(tf->tf_regs[FRAME_V0], td);
			if ((tfps & ALPHA_PSL_IPL_MASK) != last_ipl) {
				last_ipl = tfps & ALPHA_PSL_IPL_MASK;
				if (symval != (uintptr_t)&XentSys)
					db_printf(" (from ipl %ld)", last_ipl);
			}
			db_printf(" ---\n");
			if (tfps & ALPHA_PSL_USERMODE) {
				db_printf("--- user mode ---\n");
				break;	/* Terminate search.  */
			}
			frame = (db_addr_t)(tf + 1);
			pc = tf->tf_regs[FRAME_PC];
			continue;
		}

		/*
		 * This is a bit trickier; we must decode the function
		 * prologue to find the saved RA.
		 *
		 * XXX How does this interact w/ alloca()?!
		 */
		if (decode_prologue(pc, symval, &pi))
			return (0);
		if ((pi.pi_regmask & (1 << 26)) == 0) {
			/*
			 * No saved RA found.  We might have RA from
			 * the trap frame, however (e.g trap occurred
			 * in a leaf call).  If not, we've found the
			 * root of the call graph.
			 */
			if (tf)
				pc = tf->tf_regs[FRAME_RA];
			else {
				db_printf("--- root of call graph ---\n");
				break;
			}
		} else
			pc = *(u_long *)(frame + pi.pi_reg_offset[26]);
		frame += pi.pi_frame_size;
		tf = NULL;
	}

	return (0);
}

void
db_trace_self(void)
{
	register_t pc, sp;

	__asm __volatile(
		"	mov $30,%0 \n"
		"	lda %1,1f \n"
		"1:\n"
		: "=r" (sp), "=r" (pc));
	db_backtrace(curthread, sp, pc, -1);
}

int
db_trace_thread(struct thread *thr, int count)
{
	struct pcb *ctx;

	ctx = kdb_thr_ctx(thr);
	return (db_backtrace(thr, ctx->pcb_hw.apcb_ksp, ctx->pcb_context[7],
		    count));
}

void
stack_save(struct stack *st)
{
	struct prologue_info pi;
	linker_symval_t symval;
	c_linker_sym_t sym;
	vm_offset_t callpc, frame;
	long offset;
	register_t pc, sp;

	stack_zero(st);
	__asm __volatile(
		"	mov $30,%0 \n"
		"	lda %1,1f \n"
		"1:\n"
		: "=r" (sp), "=r" (pc));
	callpc = (vm_offset_t)pc;
	frame = (vm_offset_t)sp;
	while (1) {
		/*
		 * search_symbol/symbol_values are slow
		 */
		if (linker_ddb_search_symbol((caddr_t)callpc, &sym, &offset) != 0)
			break;
		if (linker_ddb_symbol_values(sym, &symval) != 0)
			break;
		if (callpc < (vm_offset_t)symval.value)
			break;
		if (stack_put(st, callpc) == -1)
			break;
		if (decode_prologue(callpc, (db_addr_t)symval.value, &pi))
			break;
		if ((pi.pi_regmask & (1 << 26)) == 0)
			break;
		callpc = *(vm_offset_t *)(frame + pi.pi_reg_offset[26]);
		frame += pi.pi_frame_size;
	}
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

