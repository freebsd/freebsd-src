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

#include <sys/param.h>
#include <vm/vm.h>
#include <machine/frame.h>

#define DB_NO_AOUT

struct ia64_bundle;

typedef	vm_offset_t	db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */
typedef struct trapframe db_regs_t;
extern db_regs_t	ddb_regs;	/* register state */
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_cr_iip \
			 + (((regs)->tf_cr_ipsr >> 41) & 3))

#define BKPT_WRITE(addr, storage)	db_write_breakpoint(addr, storage)
#define BKPT_CLEAR(addr, storage)	db_clear_breakpoint(addr, storage)
#define BKPT_INST_TYPE			u_int64_t

#define BKPT_SKIP			db_skip_breakpoint()

#define db_clear_single_step(regs)	ddb_regs.tf_cr_ipsr &= ~IA64_PSR_SS
#define db_set_single_step(regs)	ddb_regs.tf_cr_ipsr |= IA64_PSR_SS

#define	IS_BREAKPOINT_TRAP(type, code)	(type == IA64_VEC_BREAK)
#define	IS_WATCHPOINT_TRAP(type, code)	0

#define	inst_trap_return(ins)	0
#define	inst_return(ins)	0
#define	inst_call(ins)		0
#define	inst_branch(ins)	0
#define	inst_load(ins)		0
#define	inst_store(ins)		0
#define	inst_unconditional_flow_transfer(ins) 0
				
#define	branch_taken(ins, pc, regs) pc

/*
 * Functions needed for software single-stepping.
 */

/* No delay slots on Alpha. */
#define	next_instr_address(v, b) ((db_addr_t) ((b) ? (v) : ((v) + 4)))

u_long	db_register_value(db_regs_t *, int);
int	kdb_trap(int vector, struct trapframe *regs);

u_int64_t *db_rse_current_frame(void);
u_int64_t *db_rse_previous_frame(u_int64_t *bsp, int sof);
u_int64_t *db_rse_register_address(u_int64_t *bsp, int regno);

void	db_read_bundle(db_addr_t addr, struct ia64_bundle *bp);
void	db_write_bundle(db_addr_t addr, struct ia64_bundle *bp);
void	db_write_breakpoint(db_addr_t addr, u_int64_t *storage);
void	db_clear_breakpoint(db_addr_t addr, u_int64_t *storage);
void	db_skip_breakpoint(void);

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
