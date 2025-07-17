/*-
 * SPDX-License-Identifier: MIT-CMU
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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
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
/*
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Commands to run process.
 */

#include <sys/param.h>
#include <sys/kdb.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/systm.h>

#include <machine/kdb.h>
#include <machine/pcb.h>

#include <vm/vm.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_break.h>
#include <ddb/db_command.h>

#define	STEP_ONCE	1
#define	STEP_RETURN	2
#define	STEP_CALLT	3
#define	STEP_CONTINUE	4
#define	STEP_INVISIBLE	5
#define	STEP_COUNT	6
static int	db_run_mode = STEP_CONTINUE;

static bool		db_sstep_multiple;
static bool		db_sstep_print;
static int		db_loop_count;
static int		db_call_depth;

int		db_inst_count;
int		db_load_count;
int		db_store_count;

bool
db_stop_at_pc(int type, int code, bool *is_breakpoint, bool *is_watchpoint)
{
	db_addr_t	pc;
	db_breakpoint_t bkpt;

	*is_breakpoint = IS_BREAKPOINT_TRAP(type, code);
	*is_watchpoint = IS_WATCHPOINT_TRAP(type, code);
	pc = PC_REGS();

	db_clear_single_step();
	db_clear_breakpoints();
	db_clear_watchpoints();

#ifdef	FIXUP_PC_AFTER_BREAK
	if (*is_breakpoint) {
	    /*
	     * Breakpoint trap.  Fix up the PC if the
	     * machine requires it.
	     */
	    FIXUP_PC_AFTER_BREAK
	    pc = PC_REGS();
	}
#endif

	/*
	 * Now check for a breakpoint at this address.
	 */
	bkpt = db_find_breakpoint_here(pc);
	if (bkpt) {
	    if (--bkpt->count == 0) {
		bkpt->count = bkpt->init_count;
		*is_breakpoint = true;
		return (true);	/* stop here */
	    }
	    return (false);	/* continue the countdown */
	} else if (*is_breakpoint) {
#ifdef BKPT_SKIP
		BKPT_SKIP;
#endif
	}

	*is_breakpoint = false;	/* might be a breakpoint, but not ours */

	/*
	 * If not stepping, then silently ignore single-step traps
	 * (except for clearing the single-step-flag above).
	 *
	 * If stepping, then abort if the trap type is unexpected.
	 * Breakpoints owned by us are expected and were handled above.
	 * Single-steps are expected and are handled below.  All others
	 * are unexpected.
	 *
	 * Only do either of these if the MD layer claims to classify
	 * single-step traps unambiguously (by defining IS_SSTEP_TRAP).
	 * Otherwise, fall through to the bad historical behaviour
	 * given by turning unexpected traps into expected traps: if not
	 * stepping, then expect only breakpoints and stop, and if
	 * stepping, then expect only single-steps and step.
	 */
#ifdef IS_SSTEP_TRAP
	if (db_run_mode == STEP_CONTINUE && IS_SSTEP_TRAP(type, code))
	    return (false);
	if (db_run_mode != STEP_CONTINUE && !IS_SSTEP_TRAP(type, code)) {
	    printf("Stepping aborted\n");
	    return (true);
	}
#endif

	if (db_run_mode == STEP_INVISIBLE) {
	    db_run_mode = STEP_CONTINUE;
	    return (false);	/* continue */
	}
	if (db_run_mode == STEP_COUNT) {
	    return (false); /* continue */
	}
	if (db_run_mode == STEP_ONCE) {
	    if (--db_loop_count > 0) {
		if (db_sstep_print) {
		    db_printf("\t\t");
		    db_print_loc_and_inst(pc);
		}
		return (false);	/* continue */
	    }
	}
	if (db_run_mode == STEP_RETURN) {
	    /* continue until matching return */
	    db_expr_t ins;

	    ins = db_get_value(pc, sizeof(int), false);
	    if (!inst_trap_return(ins) &&
		(!inst_return(ins) || --db_call_depth != 0)) {
		if (db_sstep_print) {
		    if (inst_call(ins) || inst_return(ins)) {
			int i;

			db_printf("[after %6d]     ", db_inst_count);
			for (i = db_call_depth; --i > 0; )
			    db_printf("  ");
			db_print_loc_and_inst(pc);
		    }
		}
		if (inst_call(ins))
		    db_call_depth++;
		return (false);	/* continue */
	    }
	}
	if (db_run_mode == STEP_CALLT) {
	    /* continue until call or return */
	    db_expr_t ins;

	    ins = db_get_value(pc, sizeof(int), false);
	    if (!inst_call(ins) &&
		!inst_return(ins) &&
		!inst_trap_return(ins)) {
		return (false);	/* continue */
	    }
	}
	return (true);
}

void
db_restart_at_pc(bool watchpt)
{
	db_addr_t	pc = PC_REGS();

	if ((db_run_mode == STEP_COUNT) ||
	    ((db_run_mode == STEP_ONCE) && db_sstep_multiple) ||
	    (db_run_mode == STEP_RETURN) ||
	    (db_run_mode == STEP_CALLT)) {
	    /*
	     * We are about to execute this instruction,
	     * so count it now.
	     */
	    db_get_value(pc, sizeof(int), false);
	    db_inst_count++;
	    db_load_count += inst_load(ins);
	    db_store_count += inst_store(ins);
	}

	if (db_run_mode == STEP_CONTINUE) {
	    if (watchpt || db_find_breakpoint_here(pc)) {
		/*
		 * Step over breakpoint/watchpoint.
		 */
		db_run_mode = STEP_INVISIBLE;
		db_set_single_step();
	    } else {
		db_set_breakpoints();
		db_set_watchpoints();
	    }
	} else {
	    db_set_single_step();
	}
}

/* single-step */
/*ARGSUSED*/
void
db_single_step_cmd(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	bool		print = false;

	if (count == -1)
	    count = 1;

	if (modif[0] == 'p')
	    print = true;

	db_run_mode = STEP_ONCE;
	db_loop_count = count;
	db_sstep_multiple = (count != 1);
	db_sstep_print = print;
	db_inst_count = 0;
	db_load_count = 0;
	db_store_count = 0;

	db_cmd_loop_done = 1;
}

/* trace and print until call/return */
/*ARGSUSED*/
void
db_trace_until_call_cmd(db_expr_t addr, bool have_addr, db_expr_t count,
    char *modif)
{
	bool	print = false;

	if (modif[0] == 'p')
	    print = true;

	db_run_mode = STEP_CALLT;
	db_sstep_print = print;
	db_inst_count = 0;
	db_load_count = 0;
	db_store_count = 0;

	db_cmd_loop_done = 1;
}

/*ARGSUSED*/
void
db_trace_until_matching_cmd(db_expr_t addr, bool have_addr, db_expr_t count,
    char *modif)
{
	bool	print = false;

	if (modif[0] == 'p')
	    print = true;

	db_run_mode = STEP_RETURN;
	db_call_depth = 1;
	db_sstep_print = print;
	db_inst_count = 0;
	db_load_count = 0;
	db_store_count = 0;

	db_cmd_loop_done = 1;
}

/* continue */
/*ARGSUSED*/
void
db_continue_cmd(db_expr_t addr, bool have_addr, db_expr_t count, char *modif)
{
	if (modif[0] == 'c')
	    db_run_mode = STEP_COUNT;
	else
	    db_run_mode = STEP_CONTINUE;
	db_inst_count = 0;
	db_load_count = 0;
	db_store_count = 0;

	db_cmd_loop_done = 1;
}
