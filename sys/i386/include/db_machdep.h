/*-
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
 * $FreeBSD$
 */

#ifndef _MACHINE_DB_MACHDEP_H_
#define	_MACHINE_DB_MACHDEP_H_

#include <machine/frame.h>
#include <machine/trap.h>

typedef	vm_offset_t	db_addr_t;	/* address - unsigned */
typedef	int		db_expr_t;	/* expression - signed */

#define	PC_REGS()	((db_addr_t)(kdb_frame->tf_eflags & PSL_VM ?	\
			    (kdb_frame->tf_eip & 0xffff) +		\
			    ((kdb_frame->tf_cs & 0xffff) << 4) :	\
			    kdb_frame->tf_eip))

#define	BKPT_INST	0xcc		/* breakpoint instruction */
#define	BKPT_SIZE	(1)		/* size of breakpoint inst */
#define	BKPT_SET(inst)	(BKPT_INST)

#define BKPT_SKIP				\
do {						\
	kdb_frame->tf_eip += 1;			\
	kdb_thrctx->pcb_eip += 1;		\
} while(0)

#define	FIXUP_PC_AFTER_BREAK			\
do {						\
	kdb_frame->tf_eip -= 1;			\
	kdb_thrctx->pcb_eip -= 1;		\
} while(0);

#define	db_clear_single_step	kdb_cpu_clear_singlestep
#define	db_set_single_step	kdb_cpu_set_singlestep

/*
 * The debug exception type is copied from %dr6 to 'code' and used to
 * disambiguate single step traps.  Watchpoints have no special support.
 * Our hardware breakpoints are not well integrated with ddb and are too
 * different from watchpoints.  ddb treats them as unknown traps with
 * unknown addresses and doesn't turn them off while it is running.
 */
#define	IS_BREAKPOINT_TRAP(type, code)	((type) == T_BPTFLT)
#define	IS_SSTEP_TRAP(type, code)	((type) == T_TRCTRAP && (code) & 0x4000)
#define	IS_WATCHPOINT_TRAP(type, code)	0

#define	I_CALL		0xe8
#define	I_CALLI		0xff
#define	I_RET		0xc3
#define	I_IRET		0xcf

#define	inst_trap_return(ins)	(((ins)&0xff) == I_IRET)
#define	inst_return(ins)	(((ins)&0xff) == I_RET)
#define	inst_call(ins)		(((ins)&0xff) == I_CALL || \
				 (((ins)&0xff) == I_CALLI && \
				  ((ins)&0x3800) == 0x1000))
#define inst_load(ins)		0
#define inst_store(ins)		0

/*
 * There no interesting addresses below _kstack = 0xefbfe000.  There
 * are small absolute values for GUPROF, but we don't want to see them.
 * Treat "negative" addresses below _kstack as non-small to allow for
 * future reductions of _kstack and to avoid sign extension problems.
 *
 * There is one interesting symbol above -db_maxoff = 0xffff0000,
 * namely _APTD = 0xfffff000.  Accepting this would mess up the
 * printing of small negative offsets.  The next largest symbol is
 * _APTmap = 0xffc00000.  Accepting this is OK (unless db_maxoff is
 * set to >= 0x400000 - (max stack offset)).
 */
#define	DB_SMALL_VALUE_MAX	0x7fffffff
#define	DB_SMALL_VALUE_MIN	(-0x400001)

#endif /* !_MACHINE_DB_MACHDEP_H_ */
