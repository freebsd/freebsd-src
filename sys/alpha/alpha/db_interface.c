/*-
 * Mach Operating System
 * Copyright (c) 1992,1991,1990 Carnegie Mellon University
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
 *
 *	db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Parts of this file are derived from Mach 3:
 *
 *	File: alpha_instruction.c
 *	Author: Alessandro Forin, Carnegie Mellon University
 *	Date:	6/92
 */

/*
 * Interface to DDB.
 *
 * Modified for NetBSD/alpha by:
 *
 *	Christopher G. Demetriou, Carnegie Mellon University
 *
 *	Jason R. Thorpe, Numerical Aerospace Simulation Facility,
 *	NASA Ames Research Center
 */

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */
/* __KERNEL_RCSID(0, "$NetBSD: db_interface.c,v 1.2 1997/09/16 19:07:19 thorpej Exp $"); */
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/proc.h>
#include <sys/smp.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/pal.h>
#include <machine/prom.h>

#include <alpha/alpha/db_instruction.h>

#include <ddb/ddb.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

static db_varfcn_t db_frame;

struct db_variable db_regs[] = {
	{ "v0",		(db_expr_t *)FRAME_V0,	db_frame },
	{ "t0",		(db_expr_t *)FRAME_T0,	db_frame },
	{ "t1",		(db_expr_t *)FRAME_T1,	db_frame },
	{ "t2",		(db_expr_t *)FRAME_T2,	db_frame },
	{ "t3",		(db_expr_t *)FRAME_T3,	db_frame },
	{ "t4",		(db_expr_t *)FRAME_T4,	db_frame },
	{ "t5",		(db_expr_t *)FRAME_T5,	db_frame },
	{ "t6",		(db_expr_t *)FRAME_T6,	db_frame },
	{ "t7",		(db_expr_t *)FRAME_T7,	db_frame },
	{ "s0",		(db_expr_t *)FRAME_S0,	db_frame },
	{ "s1",		(db_expr_t *)FRAME_S1,	db_frame },
	{ "s2",		(db_expr_t *)FRAME_S2,	db_frame },
	{ "s3",		(db_expr_t *)FRAME_S3,	db_frame },
	{ "s4",		(db_expr_t *)FRAME_S4,	db_frame },
	{ "s5",		(db_expr_t *)FRAME_S5,	db_frame },
	{ "s6",		(db_expr_t *)FRAME_S6,	db_frame },
	{ "a0",		(db_expr_t *)FRAME_A0,	db_frame },
	{ "a1",		(db_expr_t *)FRAME_A1,	db_frame },
	{ "a2",		(db_expr_t *)FRAME_A2,	db_frame },
	{ "a3",		(db_expr_t *)FRAME_A3,	db_frame },
	{ "a4",		(db_expr_t *)FRAME_A4,	db_frame },
	{ "a5",		(db_expr_t *)FRAME_A5,	db_frame },
	{ "t8",		(db_expr_t *)FRAME_T8,	db_frame },
	{ "t9",		(db_expr_t *)FRAME_T9,	db_frame },
	{ "t10",	(db_expr_t *)FRAME_T10,	db_frame },
	{ "t11",	(db_expr_t *)FRAME_T11,	db_frame },
	{ "ra",		(db_expr_t *)FRAME_RA,	db_frame },
	{ "t12",	(db_expr_t *)FRAME_T12,	db_frame },
	{ "at",		(db_expr_t *)FRAME_AT,	db_frame },
	{ "gp",		(db_expr_t *)FRAME_GP,	db_frame },
	{ "sp",		(db_expr_t *)FRAME_SP,	db_frame },
	{ "pc",		(db_expr_t *)FRAME_PC,	db_frame },
	{ "ps",		(db_expr_t *)FRAME_PS,	db_frame },
	{ "ai",		(db_expr_t *)FRAME_T11,	db_frame },
	{ "pv",		(db_expr_t *)FRAME_T12,	db_frame },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

static int
db_frame(struct db_variable *vp, db_expr_t *valuep, int op)
{

	if (kdb_frame == NULL)
		return (0);
	if (op == DB_VAR_GET)
		*valuep = kdb_frame->tf_regs[(uintptr_t)vp->valuep];
	else
		kdb_frame->tf_regs[(uintptr_t)vp->valuep] = *valuep;
	return (1);
}

/*
 * Read bytes from kernel address space for debugger.
 */
int
db_read_bytes(vm_offset_t addr, size_t size, char *data)
{
	jmp_buf jb;
	void *prev_jb;
	char *src;
	int ret;

	prev_jb = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		src = (char *)addr;
		while (size-- > 0)
			*data++ = *src++;
	}
	(void)kdb_jmpbuf(prev_jb);
	return (ret);
}

/*
 * Write bytes to kernel address space for debugger.
 */
int
db_write_bytes(vm_offset_t addr, size_t size, char *data)
{
	jmp_buf jb;
	void *prev_jb;
	char *dst;
	int ret;

	prev_jb = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		dst = (char *)addr;
		while (size-- > 0)
			*dst++ = *data++;
		alpha_pal_imb();
	}
	(void)kdb_jmpbuf(prev_jb);
	return (ret);
}

/*
 * Alpha-specific ddb commands:
 *
 *	halt		set halt bit in rpb and halt
 *	reboot		set reboot bit in rpb and halt
 */

DB_COMMAND(halt, db_mach_halt)
{

	prom_halt(1);
}

DB_COMMAND(reboot, db_mach_reboot)
{
	prom_halt(0);
}

/*
 * Map Alpha register numbers to trapframe/db_regs_t offsets.
 */
static int reg_to_frame[32] = {
	FRAME_V0,
	FRAME_T0,
	FRAME_T1,
	FRAME_T2,
	FRAME_T3,
	FRAME_T4,
	FRAME_T5,
	FRAME_T6,
	FRAME_T7,

	FRAME_S0,
	FRAME_S1,
	FRAME_S2,
	FRAME_S3,
	FRAME_S4,
	FRAME_S5,
	FRAME_S6,

	FRAME_A0,
	FRAME_A1,
	FRAME_A2,
	FRAME_A3,
	FRAME_A4,
	FRAME_A5,

	FRAME_T8,
	FRAME_T9,
	FRAME_T10,
	FRAME_T11,
	FRAME_RA,
	FRAME_T12,
	FRAME_AT,
	FRAME_GP,
	FRAME_SP,
	-1,		/* zero */
};

u_long
db_register_value(int regno)
{

	if (regno > 31 || regno < 0) {
		db_printf(" **** STRANGE REGISTER NUMBER %d **** ", regno);
		return (0);
	}

	if (regno == 31)
		return (0);

	return (kdb_frame->tf_regs[reg_to_frame[regno]]);
}

/*
 * Support functions for software single-step.
 */

boolean_t
db_inst_call(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	return ((insn.branch_format.opcode == op_bsr) ||
	    ((insn.jump_format.opcode == op_j) &&
	     (insn.jump_format.action & 1)));
}

boolean_t
db_inst_return(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	return ((insn.jump_format.opcode == op_j) &&
	    (insn.jump_format.action == op_ret));
}

boolean_t
db_inst_trap_return(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	return ((insn.pal_format.opcode == op_pal) &&
	    (insn.pal_format.function == PAL_OSF1_rti));
}

boolean_t
db_inst_branch(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	switch (insn.branch_format.opcode) {
	case op_j:
	case op_br:
	case op_fbeq:
	case op_fblt:
	case op_fble:
	case op_fbne:
	case op_fbge:
	case op_fbgt:
	case op_blbc:
	case op_beq:
	case op_blt:
	case op_ble:
	case op_blbs:
	case op_bne:
	case op_bge:
	case op_bgt:
		return (TRUE);
	}

	return (FALSE);
}

boolean_t
db_inst_unconditional_flow_transfer(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	switch (insn.branch_format.opcode) {
	case op_j:
	case op_br:
		return (TRUE);

	case op_pal:
		switch (insn.pal_format.function) {
		case PAL_OSF1_retsys:
		case PAL_OSF1_rti:
		case PAL_OSF1_callsys:
			return (TRUE);
		}
	}

	return (FALSE);
}

boolean_t
db_inst_load(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;
	
	/* Loads. */
	if (insn.mem_format.opcode == op_ldbu ||
	    insn.mem_format.opcode == op_ldq_u ||
	    insn.mem_format.opcode == op_ldwu)
		return (TRUE);
	if ((insn.mem_format.opcode >= op_ldf) &&
	    (insn.mem_format.opcode <= op_ldt))
		return (TRUE);
	if ((insn.mem_format.opcode >= op_ldl) &&
	    (insn.mem_format.opcode <= op_ldq_l))
		return (TRUE);

	/* Prefetches. */
	if (insn.mem_format.opcode == op_special) {
		/* Note: MB is treated as a store. */
		if ((insn.mem_format.displacement == (short)op_fetch) ||
		    (insn.mem_format.displacement == (short)op_fetch_m))
			return (TRUE);
	}

	return (FALSE);
}

boolean_t
db_inst_store(ins)
	int ins;
{
	alpha_instruction insn;

	insn.bits = ins;

	/* Stores. */
	if (insn.mem_format.opcode == op_stw ||
	    insn.mem_format.opcode == op_stb ||
	    insn.mem_format.opcode == op_stq_u)
		return (TRUE);
	if ((insn.mem_format.opcode >= op_stf) &&
	    (insn.mem_format.opcode <= op_stt))
		return (TRUE);
	if ((insn.mem_format.opcode >= op_stl) &&
	    (insn.mem_format.opcode <= op_stq_c))
		return (TRUE);

	/* Barriers. */
	if (insn.mem_format.opcode == op_special) {
		if (insn.mem_format.displacement == op_mb)
			return (TRUE);
	}

	return (FALSE);
}

db_addr_t
db_branch_taken(int ins, db_addr_t pc)
{
	alpha_instruction insn;
	db_addr_t newpc;

	insn.bits = ins;
	switch (insn.branch_format.opcode) {
	/*
	 * Jump format: target PC is (contents of instruction's "RB") & ~3.
	 */
	case op_j:
		newpc = db_register_value(insn.jump_format.rs) & ~3;
		break;

	/*
	 * Branch format: target PC is
	 *	(new PC) + (4 * sign-ext(displacement)).
	 */
	case op_br:
	case op_fbeq:
	case op_fblt:
	case op_fble:
	case op_bsr:
	case op_fbne:
	case op_fbge:
	case op_fbgt:
	case op_blbc:
	case op_beq:
	case op_blt:
	case op_ble:
	case op_blbs:
	case op_bne:
	case op_bge:
	case op_bgt:
		newpc = (insn.branch_format.displacement << 2) + (pc + 4);
		break;

	default:
		printf("DDB: db_inst_branch_taken on non-branch!\n");
		newpc = pc;	/* XXX */
	}

	return (newpc);
}

void
db_show_mdpcpu(struct pcpu *pc)
{

	db_printf("ipis         = 0x%lx\n", pc->pc_pending_ipis);
	db_printf("next ASN     = %d\n", pc->pc_next_asn);
}
