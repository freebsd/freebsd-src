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
__FBSDID("$FreeBSD$");
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

#ifdef __ARM_EABI__
/*
 * Definitions for the instruction interpreter.
 *
 * The ARM EABI specifies how to perform the frame unwinding in the
 * Exception Handling ABI for the ARM Architecture document. To perform
 * the unwind we need to know the initial frame pointer, stack pointer,
 * link register and program counter. We then find the entry within the
 * index table that points to the function the program counter is within.
 * This gives us either a list of three instructions to process, a 31-bit
 * relative offset to a table of instructions, or a value telling us
 * we can't unwind any further.
 *
 * When we have the instructions to process we need to decode them
 * following table 4 in section 9.3. This describes a collection of bit
 * patterns to encode that steps to take to update the stack pointer and
 * link register to the correct values at the start of the function.
 */

/* A special case when we are unable to unwind past this function */
#define	EXIDX_CANTUNWIND	1

/* The register names */
#define	FP	11
#define	SP	13
#define	LR	14
#define	PC	15

/*
 * These are set in the linker script. Their addresses will be
 * either the start or end of the exception table or index.
 */
extern int extab_start, extab_end, exidx_start, exidx_end;

/*
 * Entry types.
 * These are the only entry types that have been seen in the kernel.
 */
#define	ENTRY_MASK	0xff000000
#define	ENTRY_ARM_SU16	0x80000000
#define	ENTRY_ARM_LU16	0x81000000

/* Instruction masks. */
#define	INSN_VSP_MASK		0xc0
#define	INSN_VSP_SIZE_MASK	0x3f
#define	INSN_STD_MASK		0xf0
#define	INSN_STD_DATA_MASK	0x0f
#define	INSN_POP_TYPE_MASK	0x08
#define	INSN_POP_COUNT_MASK	0x07
#define	INSN_VSP_LARGE_INC_MASK	0xff

/* Instruction definitions */
#define	INSN_VSP_INC		0x00
#define	INSN_VSP_DEC		0x40
#define	INSN_POP_MASKED		0x80
#define	INSN_VSP_REG		0x90
#define	INSN_POP_COUNT		0xa0
#define	INSN_FINISH		0xb0
#define	INSN_VSP_LARGE_INC	0xb2

/* An item in the exception index table */
struct unwind_idx {
	uint32_t offset;
	uint32_t insn;
};

/* The state of the unwind process */
struct unwind_state {
	uint32_t registers[16];
	uint32_t start_pc;
	uint32_t *insn;
	u_int entries;
	u_int byte;
	uint16_t update_mask;
};

/* Expand a 31-bit signed value to a 32-bit signed value */
static __inline int32_t
db_expand_prel31(uint32_t prel31)
{

	return ((int32_t)(prel31 & 0x7fffffffu) << 1) / 2;
}

/*
 * Perform a binary search of the index table to find the function
 * with the largest address that doesn't exceed addr.
 */
static struct unwind_idx *
db_find_index(uint32_t addr)
{
	unsigned int min, mid, max;
	struct unwind_idx *start;
	struct unwind_idx *item;
	int32_t prel31_addr;
	uint32_t func_addr;

	start = (struct unwind_idx *)&exidx_start;

	min = 0;
	max = (&exidx_end - &exidx_start) / 2;

	while (min != max) {
		mid = min + (max - min + 1) / 2;

		item = &start[mid];

	 	prel31_addr = db_expand_prel31(item->offset);
		func_addr = (uint32_t)&item->offset + prel31_addr;

		if (func_addr <= addr) {
			min = mid;
		} else {
			max = mid - 1;
		}
	}

	return &start[min];
}

/* Reads the next byte from the instruction list */
static uint8_t
db_unwind_exec_read_byte(struct unwind_state *state)
{
	uint8_t insn;

	/* Read the unwind instruction */
	insn = (*state->insn) >> (state->byte * 8);

	/* Update the location of the next instruction */
	if (state->byte == 0) {
		state->byte = 3;
		state->insn++;
		state->entries--;
	} else
		state->byte--;

	return insn;
}

/* Executes the next instruction on the list */
static int
db_unwind_exec_insn(struct unwind_state *state)
{
	unsigned int insn;
	uint32_t *vsp = (uint32_t *)state->registers[SP];
	int update_vsp = 0;

	/* This should never happen */
	if (state->entries == 0)
		return 1;

	/* Read the next instruction */
	insn = db_unwind_exec_read_byte(state);

	if ((insn & INSN_VSP_MASK) == INSN_VSP_INC) {
		state->registers[SP] += ((insn & INSN_VSP_SIZE_MASK) << 2) + 4;

	} else if ((insn & INSN_VSP_MASK) == INSN_VSP_DEC) {
		state->registers[SP] -= ((insn & INSN_VSP_SIZE_MASK) << 2) + 4;

	} else if ((insn & INSN_STD_MASK) == INSN_POP_MASKED) {
		unsigned int mask, reg;

		/* Load the mask */
		mask = db_unwind_exec_read_byte(state);
		mask |= (insn & INSN_STD_DATA_MASK) << 8;

		/* We have a refuse to unwind instruction */
		if (mask == 0)
			return 1;

		/* Update SP */
		update_vsp = 1;

		/* Load the registers */
		for (reg = 4; mask && reg < 16; mask >>= 1, reg++) {
			if (mask & 1) {
				state->registers[reg] = *vsp++;
				state->update_mask |= 1 << reg;

				/* If we have updated SP kep its value */
				if (reg == SP)
					update_vsp = 0;
			}
		}

	} else if ((insn & INSN_STD_MASK) == INSN_VSP_REG &&
	    ((insn & INSN_STD_DATA_MASK) != 13) &&
	    ((insn & INSN_STD_DATA_MASK) != 15)) {
		/* sp = register */
		state->registers[SP] =
		    state->registers[insn & INSN_STD_DATA_MASK];

	} else if ((insn & INSN_STD_MASK) == INSN_POP_COUNT) {
		unsigned int count, reg;

		/* Read how many registers to load */
		count = insn & INSN_POP_COUNT_MASK;

		/* Update sp */
		update_vsp = 1;

		/* Pop the registers */
		for (reg = 4; reg <= 4 + count; reg++) {
			state->registers[reg] = *vsp++;
			state->update_mask |= 1 << reg;
		}

		/* Check if we are in the pop r14 version */
		if ((insn & INSN_POP_TYPE_MASK) != 0) {
			state->registers[14] = *vsp++;
		}

	} else if (insn == INSN_FINISH) {
		/* Stop processing */
		state->entries = 0;

	} else if ((insn & INSN_VSP_LARGE_INC_MASK) == INSN_VSP_LARGE_INC) {
		unsigned int uleb128;

		/* Read the increment value */
		uleb128 = db_unwind_exec_read_byte(state);

		state->registers[SP] += 0x204 + (uleb128 << 2);

	} else {
		/* We hit a new instruction that needs to be implemented */
		db_printf("Unhandled instruction %.2x\n", insn);
		return 1;
	}

	if (update_vsp) {
		state->registers[SP] = (uint32_t)vsp;
	}

#if 0
	db_printf("fp = %08x, sp = %08x, lr = %08x, pc = %08x\n",
	    state->registers[FP], state->registers[SP], state->registers[LR],
	    state->registers[PC]);
#endif

	return 0;
}

/* Performs the unwind of a function */
static int
db_unwind_tab(struct unwind_state *state)
{
	uint32_t entry;

	/* Set PC to a known value */
	state->registers[PC] = 0;

	/* Read the personality */
	entry = *state->insn & ENTRY_MASK;

	if (entry == ENTRY_ARM_SU16) {
		state->byte = 2;
		state->entries = 1;
	} else if (entry == ENTRY_ARM_LU16) {
		state->byte = 1;
		state->entries = ((*state->insn >> 16) & 0xFF) + 1;
	} else {
		db_printf("Unknown entry: %x\n", entry);
		return 1;
	}

	while (state->entries > 0) {
		if (db_unwind_exec_insn(state) != 0)
			return 1;
	}

	/*
	 * The program counter was not updated, load it from the link register.
	 */
	if (state->registers[PC] == 0)
		state->registers[PC] = state->registers[LR];

	return 0;
}

static void
db_stack_trace_cmd(struct unwind_state *state)
{
	struct unwind_idx *index;
	const char *name;
	db_expr_t value;
	db_expr_t offset;
	c_db_sym_t sym;
	u_int reg, i;
	char *sep;
	uint16_t upd_mask;
	bool finished;

	finished = false;
	while (!finished) {
		/* Reset the mask of updated registers */
		state->update_mask = 0;

		/* The pc value is correct and will be overwritten, save it */
		state->start_pc = state->registers[PC];

		/* Find the item to run */
		index = db_find_index(state->start_pc);

		if (index->insn != EXIDX_CANTUNWIND) {
			if (index->insn & (1 << 31)) {
				/* The data is within the instruction */
				state->insn = &index->insn;
			} else {
				/* A prel31 offset to the unwind table */
				state->insn = (uint32_t *)
				    ((uintptr_t)&index->insn + 
				     db_expand_prel31(index->insn));
			}
			/* Run the unwind function */
			finished = db_unwind_tab(state);
		}

		/* Print the frame details */
		sym = db_search_symbol(state->start_pc, DB_STGY_ANY, &offset);
		if (sym == C_DB_SYM_NULL) {
			value = 0;
			name = "(null)";
		} else
			db_symbol_values(sym, &name, &value);
		db_printf("%s() at ", name);
		db_printsym(state->start_pc, DB_STGY_PROC);
		db_printf("\n");
		db_printf("\t pc = 0x%08x  lr = 0x%08x (", state->start_pc,
		    state->registers[LR]);
		db_printsym(state->registers[LR], DB_STGY_PROC);
		db_printf(")\n");
		db_printf("\t sp = 0x%08x  fp = 0x%08x",
		    state->registers[SP], state->registers[FP]);

		/* Don't print the registers we have already printed */
		upd_mask = state->update_mask & 
		    ~((1 << SP) | (1 << FP) | (1 << LR) | (1 << PC));
		sep = "\n\t";
		for (i = 0, reg = 0; upd_mask != 0; upd_mask >>= 1, reg++) {
			if ((upd_mask & 1) != 0) {
				db_printf("%s%sr%d = 0x%08x", sep,
				    (reg < 10) ? " " : "", reg,
				    state->registers[reg]);
				i++;
				if (i == 2) {
					sep = "\n\t";
					i = 0;
				} else
					sep = " ";
				
			}
		}
		db_printf("\n");

		/*
		 * Stop if directed to do so, or if we've unwound back to the
		 * kernel entry point, or if the unwind function didn't change
		 * anything (to avoid getting stuck in this loop forever).
		 * If the latter happens, it's an indication that the unwind
		 * information is incorrect somehow for the function named in
		 * the last frame printed before you see the unwind failure
		 * message (maybe it needs a STOP_UNWINDING).
		 */
		if (index->insn == EXIDX_CANTUNWIND) {
			db_printf("Unable to unwind further\n");
			finished = true;
		} else if (state->registers[PC] < VM_MIN_KERNEL_ADDRESS) {
			db_printf("Unable to unwind into user mode\n");
			finished = true;
		} else if (state->update_mask == 0) {
			db_printf("Unwind failure (no registers changed)\n");
			finished = true;
		}
	}
}
#endif

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

#ifndef __ARM_EABI__	/* The frame format is differend in AAPCS */
static void
db_stack_trace_cmd(db_expr_t addr, db_expr_t count, boolean_t kernel_only)
{
	u_int32_t	*frame, *lastframe;
	c_db_sym_t sym;
	const char *name;
	db_expr_t value;
	db_expr_t offset;
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
#endif

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
#ifdef __ARM_EABI__
	struct unwind_state state;
#endif
	struct pcb *ctx;

	if (thr != curthread) {
		ctx = kdb_thr_ctx(thr);

#ifdef __ARM_EABI__
		state.registers[FP] = ctx->un_32.pcb32_r11;
		state.registers[SP] = ctx->un_32.pcb32_sp;
		state.registers[LR] = ctx->un_32.pcb32_lr;
		state.registers[PC] = ctx->un_32.pcb32_pc;

		db_stack_trace_cmd(&state);
#else
		db_stack_trace_cmd(ctx->un_32.pcb32_r11, -1, TRUE);
#endif
	} else
		db_trace_self();
	return (0);
}

void
db_trace_self(void)
{
#ifdef __ARM_EABI__
	struct unwind_state state;
	uint32_t sp;

	/* Read the stack pointer */
	__asm __volatile("mov %0, sp" : "=&r" (sp));

	state.registers[FP] = (uint32_t)__builtin_frame_address(0);
	state.registers[SP] = sp;
	state.registers[LR] = (uint32_t)__builtin_return_address(0);
	state.registers[PC] = (uint32_t)db_trace_self;

	db_stack_trace_cmd(&state);
#else
	db_addr_t addr;

	addr = (db_addr_t)__builtin_frame_address(0);
	db_stack_trace_cmd(addr, -1, FALSE);
#endif
}
