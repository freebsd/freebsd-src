/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/ia64/include/db_machdep.h,v 1.12 2005/07/02 23:52:36 marcel Exp $
 */

#ifndef	_MACHINE_DB_MACHDEP_H_
#define	_MACHINE_DB_MACHDEP_H_

#include <machine/ia64_cpu.h>

/* We define some of our own commands. */
#define	DB_MACHINE_COMMANDS

/* We use Elf64 symbols in DDB. */
#define	DB_ELFSIZE	64

/* Pretty arbitrary. */
#define	DB_SMALL_VALUE_MAX	0x7fffffff
#define	DB_SMALL_VALUE_MIN	(-0x400001)

typedef	vm_offset_t	db_addr_t;	/* address - unsigned */
typedef	long		db_expr_t;	/* expression - signed */

#define	PC_REGS()	((kdb_thrctx->pcb_special.__spare == 0) ?	\
	kdb_thrctx->pcb_special.rp :					\
	kdb_thrctx->pcb_special.iip + ((kdb_thrctx->pcb_special.psr>>41) & 3))

#define BKPT_WRITE(addr, storage)	db_bkpt_write(addr, storage)
#define BKPT_CLEAR(addr, storage)	db_bkpt_clear(addr, storage)
#define BKPT_SKIP			db_bkpt_skip()
#define BKPT_INST_TYPE			uint64_t

void db_bkpt_write(db_addr_t, BKPT_INST_TYPE *storage);
void db_bkpt_clear(db_addr_t, uint64_t *storage);
void db_bkpt_skip(void);

#define db_clear_single_step		kdb_cpu_clear_singlestep
#define db_set_single_step		kdb_cpu_set_singlestep

#define	IS_BREAKPOINT_TRAP(type, code)	(type == IA64_VEC_BREAK)
#define	IS_WATCHPOINT_TRAP(type, code)	0

#define	inst_trap_return(ins)	(ins & 0)
#define	inst_return(ins)	(ins & 0)
#define	inst_call(ins)		(ins & 0)
#define	inst_branch(ins)	(ins & 0)
#define	inst_load(ins)		(ins & 0)
#define	inst_store(ins)		(ins & 0)
#define	inst_unconditional_flow_transfer(ins) (ins & 0)

#define	branch_taken(ins, pc, regs) pc

/* Function call support. */
#define	DB_MAXARGS	8	/* Only support arguments in registers. */
#define	DB_CALL		db_fncall_ia64

#endif	/* _MACHINE_DB_MACHDEP_H_ */
