/*
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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 *	from: FreeBSD: src/sys/i386/include/db_machdep.h,v 1.16 1999/10/04
 * $FreeBSD$
 */

#ifndef	_MACHINE_DB_MACHDEP_H_
#define	_MACHINE_DB_MACHDEP_H_

#include <machine/frame.h>
#include <machine/trap.h>

#define	BYTE_MSF	(1)

typedef vm_offset_t	db_addr_t;
typedef long		db_expr_t;

typedef struct trapframe db_regs_t;
extern db_regs_t ddb_regs;
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_tpc)

#define	BKPT_INST	(0)
#define	BKPT_SIZE	(4)
#define	BKPT_SET(inst)	(BKPT_INST)

#define	BKPT_SKIP do {							\
	ddb_regs.tf_tpc = ddb_regs.tf_tnpc + 4;				\
	ddb_regs.tf_tnpc += 8;						\
} while (0)

#define	db_clear_single_step(regs)
#define	db_set_single_step(regs)

#define	IS_BREAKPOINT_TRAP(type, code)	(type == T_BREAKPOINT)
#define	IS_WATCHPOINT_TRAP(type, code)	(0)

#define	inst_trap_return(ins)	(0)
#define	inst_return(ins)	(0)
#define	inst_call(ins)		(0)
#define	inst_load(ins)		(0)
#define	inst_store(ins)		(0)

#define	DB_SMALL_VALUE_MAX	(0x7fffffff)
#define	DB_SMALL_VALUE_MIN	(-0x40001)

#define	DB_ELFSIZE		64

#endif /* !_MACHINE_DB_MACHDEP_H_ */
