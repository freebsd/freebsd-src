/* $NetBSD: db_interface.c,v 1.2 1997/09/16 19:07:19 thorpej Exp $ */
/* $FreeBSD$ */

/* 
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

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/cons.h>
#include <sys/ktr.h>
#include <sys/mutex.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/pal.h>
#include <machine/prom.h>
#include <machine/smp.h>

#include <alpha/alpha/db_instruction.h>

#include <ddb/ddb.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <setjmp.h>

static jmp_buf *db_nofault = 0;
extern jmp_buf	db_jmpbuf;

extern void	gdb_handle_exception __P((db_regs_t *, int, int));

#if 0
extern char *trap_type[];
extern int trap_types;
#endif

int	db_active;

void	ddbprinttrap __P((unsigned long, unsigned long, unsigned long,
	    unsigned long));

struct db_variable db_regs[] = {
	{	"v0",	&ddb_regs.tf_regs[FRAME_V0],	FCN_NULL	},
	{	"t0",	&ddb_regs.tf_regs[FRAME_T0],	FCN_NULL	},
	{	"t1",	&ddb_regs.tf_regs[FRAME_T1],	FCN_NULL	},
	{	"t2",	&ddb_regs.tf_regs[FRAME_T2],	FCN_NULL	},
	{	"t3",	&ddb_regs.tf_regs[FRAME_T3],	FCN_NULL	},
	{	"t4",	&ddb_regs.tf_regs[FRAME_T4],	FCN_NULL	},
	{	"t5",	&ddb_regs.tf_regs[FRAME_T5],	FCN_NULL	},
	{	"t6",	&ddb_regs.tf_regs[FRAME_T6],	FCN_NULL	},
	{	"t7",	&ddb_regs.tf_regs[FRAME_T7],	FCN_NULL	},
	{	"s0",	&ddb_regs.tf_regs[FRAME_S0],	FCN_NULL	},
	{	"s1",	&ddb_regs.tf_regs[FRAME_S1],	FCN_NULL	},
	{	"s2",	&ddb_regs.tf_regs[FRAME_S2],	FCN_NULL	},
	{	"s3",	&ddb_regs.tf_regs[FRAME_S3],	FCN_NULL	},
	{	"s4",	&ddb_regs.tf_regs[FRAME_S4],	FCN_NULL	},
	{	"s5",	&ddb_regs.tf_regs[FRAME_S5],	FCN_NULL	},
	{	"s6",	&ddb_regs.tf_regs[FRAME_S6],	FCN_NULL	},
	{	"a0",	&ddb_regs.tf_regs[FRAME_A0],	FCN_NULL	},
	{	"a1",	&ddb_regs.tf_regs[FRAME_A1],	FCN_NULL	},
	{	"a2",	&ddb_regs.tf_regs[FRAME_A2],	FCN_NULL	},
	{	"a3",	&ddb_regs.tf_regs[FRAME_A3],	FCN_NULL	},
	{	"a4",	&ddb_regs.tf_regs[FRAME_A4],	FCN_NULL	},
	{	"a5",	&ddb_regs.tf_regs[FRAME_A5],	FCN_NULL	},
	{	"t8",	&ddb_regs.tf_regs[FRAME_T8],	FCN_NULL	},
	{	"t9",	&ddb_regs.tf_regs[FRAME_T9],	FCN_NULL	},
	{	"t10",	&ddb_regs.tf_regs[FRAME_T10],	FCN_NULL	},
	{	"t11",	&ddb_regs.tf_regs[FRAME_T11],	FCN_NULL	},
	{	"ra",	&ddb_regs.tf_regs[FRAME_RA],	FCN_NULL	},
	{	"t12",	&ddb_regs.tf_regs[FRAME_T12],	FCN_NULL	},
	{	"at",	&ddb_regs.tf_regs[FRAME_AT],	FCN_NULL	},
	{	"gp",	&ddb_regs.tf_regs[FRAME_GP],	FCN_NULL	},
	{	"sp",	&ddb_regs.tf_regs[FRAME_SP],	FCN_NULL	},
	{	"pc",	&ddb_regs.tf_regs[FRAME_PC],	FCN_NULL	},
	{	"ps",	&ddb_regs.tf_regs[FRAME_PS],	FCN_NULL	},
	{	"ai",	&ddb_regs.tf_regs[FRAME_T11],	FCN_NULL	},
	{	"pv",	&ddb_regs.tf_regs[FRAME_T12],	FCN_NULL	},
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

/*
 * Print trap reason.
 */
void
ddbprinttrap(a0, a1, a2, entry)
	unsigned long a0, a1, a2, entry;
{

	/* XXX Implement. */

	printf("ddbprinttrap(0x%lx, 0x%lx, 0x%lx, 0x%lx)\n", a0, a1, a2,
	    entry);
}

/*
 *  ddb_trap - field a kernel trap
 */
int
kdb_trap(a0, a1, a2, entry, regs)
	unsigned long a0, a1, a2, entry;
	db_regs_t *regs;
{
	int ddb_mode = !(boothowto & RB_GDB);
	critical_t s;

	/*
	 * Don't bother checking for usermode, since a benign entry
	 * by the kernel (call to Debugger() or a breakpoint) has
	 * already checked for usermode.  If neither of those
	 * conditions exist, something Bad has happened.
	 */

	if (entry != ALPHA_KENTRY_IF ||
	    (a0 != ALPHA_IF_CODE_BUGCHK && a0 != ALPHA_IF_CODE_BPT
		&& a0 != ALPHA_IF_CODE_GENTRAP)) {
#if 0
		if (ddb_mode) {
			db_printf("ddbprinttrap from 0x%lx\n",	/* XXX */
				  regs->tf_regs[FRAME_PC]);
			ddbprinttrap(a0, a1, a2, entry);
			/*
			 * Tell caller "We did NOT handle the trap."
			 * Caller should panic, or whatever.
			 */
			return (0);
		}
#endif
		if (db_nofault) {
			jmp_buf *no_fault = db_nofault;
			db_nofault = 0;
			longjmp(*no_fault, 1);
		}
	}

	/*
	 * XXX Should switch to DDB's own stack, here.
	 */

	ddb_regs = *regs;

	s = critical_enter();

#if 0
	db_printf("stopping %x\n", PCPU_GET(other_cpus));
	stop_cpus(PCPU_GET(other_cpus));
	db_printf("stopped_cpus=%x\n", stopped_cpus);
#endif

	db_active++;

	if (ddb_mode) {
	    cndbctl(TRUE);	/* DDB active, unblank video */
	    db_trap(entry, a0);	/* Where the work happens */
	    cndbctl(FALSE);	/* DDB inactive */
	} else
	    gdb_handle_exception(&ddb_regs, entry, a0);

	db_active--;

#if 0
	restart_cpus(stopped_cpus);
#endif

	critical_exit(s);

	*regs = ddb_regs;

	/*
	 * Tell caller "We HAVE handled the trap."
	 */
	return (1);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register size_t	size;
	register char	*data;
{
	register char	*src;

	db_nofault = &db_jmpbuf;

	src = (char *)addr;
	while (size-- > 0)
		*data++ = *src++;

	db_nofault = 0;
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register size_t	size;
	register char	*data;
{
	register char	*dst;

	db_nofault = &db_jmpbuf;

	dst = (char *)addr;
	while (size-- > 0)
		*dst++ = *data++;
	alpha_pal_imb();

	db_nofault = 0;
}

void
Debugger(const char* msg)
{
	u_int	saveintr;

	printf("%s\n", msg);
	saveintr = alpha_pal_swpipl(ALPHA_PSL_IPL_HIGH);
	__asm("call_pal 0x81");		/* XXX bugchk */
	alpha_pal_swpipl(saveintr);
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
db_register_value(regs, regno)
	db_regs_t *regs;
	int regno;
{

	if (regno > 31 || regno < 0) {
		db_printf(" **** STRANGE REGISTER NUMBER %d **** ", regno);
		return (0);
	}

	if (regno == 31)
		return (0);

	return (regs->tf_regs[reg_to_frame[regno]]);
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

#if 0
boolean_t
db_inst_spill(ins, regn)
	int ins, regn;
{
	alpha_instruction insn;

	insn.bits = ins;
	return ((insn.mem_format.opcode == op_stq) &&
	    (insn.mem_format.rd == regn));
}
#endif

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
db_branch_taken(ins, pc, regs)
	int ins;
	db_addr_t pc;
	db_regs_t *regs;
{
	alpha_instruction insn;
	db_addr_t newpc;

	insn.bits = ins;
	switch (insn.branch_format.opcode) {
	/*
	 * Jump format: target PC is (contents of instruction's "RB") & ~3.
	 */
	case op_j:
		newpc = db_register_value(regs, insn.jump_format.rs) & ~3;
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
