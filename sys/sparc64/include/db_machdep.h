/*-
 * Copyright (c) 2001 Jake Burkholder.
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
 * $FreeBSD$
 */

#ifndef	_MACHINE_DB_MACHDEP_H_
#define	_MACHINE_DB_MACHDEP_H_

#include <machine/frame.h>
#include <machine/trap.h>

#define	BYTE_MSF	(1)

typedef vm_offset_t	db_addr_t;
typedef u_long		db_expr_t;

struct db_regs {
	u_long	dr_global[8];
};

typedef struct trapframe db_regs_t;
extern db_regs_t ddb_regs;
#define	DDB_REGS	(&ddb_regs)

#define	PC_REGS(regs)	((db_addr_t)(regs)->tf_tpc)

#define	BKPT_INST	(0)
#define	BKPT_SIZE	(4)
#define	BKPT_SET(inst)	(BKPT_INST)

#define	FIXUP_PC_AFTER_BREAK do {					\
	ddb_regs.tf_tpc = ddb_regs.tf_tnpc;				\
	ddb_regs.tf_tnpc += BKPT_SIZE;					\
} while (0);

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
