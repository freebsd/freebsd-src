/* $FreeBSD$ */
/* $NetBSD: db_machdep.h,v 1.6 1997/09/06 02:02:25 thorpej Exp $ */

/*
 * Copyright (c) 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
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

#ifndef	_ALPHA_DB_MACHDEP_H_
#define	_ALPHA_DB_MACHDEP_H_

/*
 * Machine-dependent defines for new kernel debugger.
 */

#include "opt_simos.h"

#include <sys/param.h>
#include <vm/vm.h>
#include <machine/frame.h>

#define DB_NO_AOUT

typedef	vm_offset_t	db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */

typedef struct trapframe db_regs_t;
db_regs_t		ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_regs[FRAME_PC])

#ifdef SIMOS
#define	BKPT_INST	0x000000aa	/* gentrap instruction */
#else
#define	BKPT_INST	0x00000080	/* breakpoint instruction */
#endif
#define	BKPT_SIZE	(4)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#define	FIXUP_PC_AFTER_BREAK \
	(ddb_regs.tf_regs[FRAME_PC] -= BKPT_SIZE);

#define	SOFTWARE_SSTEP	1		/* no hardware support */
#ifdef SIMOS
#define	IS_BREAKPOINT_TRAP(type, code)	((type) == ALPHA_KENTRY_IF && \
					 (code) == ALPHA_IF_CODE_GENTRAP)
#else
#define	IS_BREAKPOINT_TRAP(type, code)	((type) == ALPHA_KENTRY_IF && \
					 (code) == ALPHA_IF_CODE_BPT)
#endif
#define	IS_WATCHPOINT_TRAP(type, code)	0

/*
 * Functions needed for software single-stepping.
 */

boolean_t	db_inst_trap_return __P((int inst));
boolean_t	db_inst_return __P((int inst));
boolean_t	db_inst_call __P((int inst));
boolean_t	db_inst_branch __P((int inst));
boolean_t	db_inst_load __P((int inst));
boolean_t	db_inst_store __P((int inst));
boolean_t	db_inst_unconditional_flow_transfer __P((int inst));
db_addr_t	db_branch_taken __P((int inst, db_addr_t pc, db_regs_t *regs));

#define	inst_trap_return(ins)	db_inst_trap_return(ins)
#define	inst_return(ins)	db_inst_return(ins)
#define	inst_call(ins)		db_inst_call(ins)
#define	inst_branch(ins)	db_inst_branch(ins)
#define	inst_load(ins)		db_inst_load(ins)
#define	inst_store(ins)		db_inst_store(ins)
#define	inst_unconditional_flow_transfer(ins) \
				db_inst_unconditional_flow_transfer(ins)
#define	branch_taken(ins, pc, regs) \
				db_branch_taken((ins), (pc), (regs))

/* No delay slots on Alpha. */
#define	next_instr_address(v, b) ((db_addr_t) ((b) ? (v) : ((v) + 4)))

u_long	db_register_value __P((db_regs_t *, int));
int	kdb_trap __P((unsigned long, unsigned long, unsigned long,
	    unsigned long, struct trapframe *));

/*
 * Pretty arbitrary
 */
#define	DB_SMALL_VALUE_MAX	0x7fffffff
#define	DB_SMALL_VALUE_MIN	(-0x400001)

/*
 * We define some of our own commands.
 */
#define	DB_MACHINE_COMMANDS

/*
 * We use Elf64 symbols in DDB.
 */
#define	DB_ELFSIZE	64

#endif	/* _ALPHA_DB_MACHDEP_H_ */
