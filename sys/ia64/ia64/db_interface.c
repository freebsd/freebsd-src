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
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/smp.h>
#include <sys/cons.h>
#include <sys/ktr.h>

#include <vm/vm.h>

#include <machine/inst.h>
#include <machine/db_machdep.h>
#include <machine/mutex.h>

#include <ddb/ddb.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <machine/setjmp.h>

static jmp_buf *db_nofault = 0;
extern jmp_buf	db_jmpbuf;

extern void	gdb_handle_exception(db_regs_t *, int);

int	db_active;
db_regs_t ddb_regs;

static int db_get_rse_reg(struct db_variable *vp, db_expr_t *valuep, int op);
static int db_get_ip_reg(struct db_variable *vp, db_expr_t *valuep, int op);

struct db_variable db_regs[] = {
	/* Misc control/app registers */
#define DB_MISC_REGS	13	/* make sure this is correct */

	{"ip",		NULL,			db_get_ip_reg},
	{"psr",		(db_expr_t*) &ddb_regs.tf_special.psr,	FCN_NULL},
	{"cr.isr",	(db_expr_t*) &ddb_regs.tf_special.isr,	FCN_NULL},
	{"cr.ifa",	(db_expr_t*) &ddb_regs.tf_special.ifa,	FCN_NULL},
	{"pr",		(db_expr_t*) &ddb_regs.tf_special.pr,	FCN_NULL},
	{"ar.rsc",	(db_expr_t*) &ddb_regs.tf_special.rsc,	FCN_NULL},
	{"ar.pfs",	(db_expr_t*) &ddb_regs.tf_special.pfs,	FCN_NULL},
	{"cr.ifs",	(db_expr_t*) &ddb_regs.tf_special.cfm,	FCN_NULL},
	{"ar.bspstore",	(db_expr_t*) &ddb_regs.tf_special.bspstore, FCN_NULL},
	{"ndirty",	(db_expr_t*) &ddb_regs.tf_special.ndirty, FCN_NULL},
	{"ar.rnat",	(db_expr_t*) &ddb_regs.tf_special.rnat,	FCN_NULL},
	{"ar.unat",	(db_expr_t*) &ddb_regs.tf_special.unat,	FCN_NULL},
	{"ar.fpsr",	(db_expr_t*) &ddb_regs.tf_special.fpsr,	FCN_NULL},

	/* Branch registers */
	{"rp",		(db_expr_t*) &ddb_regs.tf_special.rp,	FCN_NULL},
	/* b1, b2, b3, b4, b5 are preserved */
	{"b6",		(db_expr_t*) &ddb_regs.tf_scratch.br6,	FCN_NULL},
	{"b7",		(db_expr_t*) &ddb_regs.tf_scratch.br7,	FCN_NULL},

	/* Static registers */
	{"gp",		(db_expr_t*) &ddb_regs.tf_special.gp,	FCN_NULL},
	{"r2",		(db_expr_t*) &ddb_regs.tf_scratch.gr2,	FCN_NULL},
	{"r3",		(db_expr_t*) &ddb_regs.tf_scratch.gr3,	FCN_NULL},
	{"r8",		(db_expr_t*) &ddb_regs.tf_scratch.gr8,	FCN_NULL},
	{"r9",		(db_expr_t*) &ddb_regs.tf_scratch.gr9,	FCN_NULL},
	{"r10",		(db_expr_t*) &ddb_regs.tf_scratch.gr10,	FCN_NULL},
	{"r11",		(db_expr_t*) &ddb_regs.tf_scratch.gr11,	FCN_NULL},
	{"sp",		(db_expr_t*) &ddb_regs.tf_special.sp,	FCN_NULL},
	{"tp",		(db_expr_t*) &ddb_regs.tf_special.tp,	FCN_NULL},
	{"r14",		(db_expr_t*) &ddb_regs.tf_scratch.gr14,	FCN_NULL},
	{"r15",		(db_expr_t*) &ddb_regs.tf_scratch.gr15,	FCN_NULL},
	{"r16",		(db_expr_t*) &ddb_regs.tf_scratch.gr16,	FCN_NULL},
	{"r17",		(db_expr_t*) &ddb_regs.tf_scratch.gr17,	FCN_NULL},
	{"r18",		(db_expr_t*) &ddb_regs.tf_scratch.gr18,	FCN_NULL},
	{"r19",		(db_expr_t*) &ddb_regs.tf_scratch.gr19,	FCN_NULL},
	{"r20",		(db_expr_t*) &ddb_regs.tf_scratch.gr20,	FCN_NULL},
	{"r21",		(db_expr_t*) &ddb_regs.tf_scratch.gr21,	FCN_NULL},
	{"r22",		(db_expr_t*) &ddb_regs.tf_scratch.gr22,	FCN_NULL},
	{"r23",		(db_expr_t*) &ddb_regs.tf_scratch.gr23,	FCN_NULL},
	{"r24",		(db_expr_t*) &ddb_regs.tf_scratch.gr24,	FCN_NULL},
	{"r25",		(db_expr_t*) &ddb_regs.tf_scratch.gr25,	FCN_NULL},
	{"r26",		(db_expr_t*) &ddb_regs.tf_scratch.gr26,	FCN_NULL},
	{"r27",		(db_expr_t*) &ddb_regs.tf_scratch.gr27,	FCN_NULL},
	{"r28",		(db_expr_t*) &ddb_regs.tf_scratch.gr28,	FCN_NULL},
	{"r29",		(db_expr_t*) &ddb_regs.tf_scratch.gr29,	FCN_NULL},
	{"r30",		(db_expr_t*) &ddb_regs.tf_scratch.gr30,	FCN_NULL},
	{"r31",		(db_expr_t*) &ddb_regs.tf_scratch.gr31,	FCN_NULL},

	/* Stacked registers */
	{"r32",		(db_expr_t*) 32,	db_get_rse_reg},
	{"r33",		(db_expr_t*) 33,	db_get_rse_reg},
	{"r34",		(db_expr_t*) 34,	db_get_rse_reg},
	{"r35",		(db_expr_t*) 35,	db_get_rse_reg},
	{"r36",		(db_expr_t*) 36,	db_get_rse_reg},
	{"r37",		(db_expr_t*) 37,	db_get_rse_reg},
	{"r38",		(db_expr_t*) 38,	db_get_rse_reg},
	{"r39",		(db_expr_t*) 39,	db_get_rse_reg},
	{"r40",		(db_expr_t*) 40,	db_get_rse_reg},
	{"r41",		(db_expr_t*) 41,	db_get_rse_reg},
	{"r42",		(db_expr_t*) 42,	db_get_rse_reg},
	{"r43",		(db_expr_t*) 43,	db_get_rse_reg},
	{"r44",		(db_expr_t*) 44,	db_get_rse_reg},
	{"r45",		(db_expr_t*) 45,	db_get_rse_reg},
	{"r46",		(db_expr_t*) 46,	db_get_rse_reg},
	{"r47",		(db_expr_t*) 47,	db_get_rse_reg},
	{"r48",		(db_expr_t*) 48,	db_get_rse_reg},
	{"r49",		(db_expr_t*) 49,	db_get_rse_reg},
	{"r50",		(db_expr_t*) 50,	db_get_rse_reg},
	{"r51",		(db_expr_t*) 51,	db_get_rse_reg},
	{"r52",		(db_expr_t*) 52,	db_get_rse_reg},
	{"r53",		(db_expr_t*) 53,	db_get_rse_reg},
	{"r54",		(db_expr_t*) 54,	db_get_rse_reg},
	{"r55",		(db_expr_t*) 55,	db_get_rse_reg},
	{"r56",		(db_expr_t*) 56,	db_get_rse_reg},
	{"r57",		(db_expr_t*) 57,	db_get_rse_reg},
	{"r58",		(db_expr_t*) 58,	db_get_rse_reg},
	{"r59",		(db_expr_t*) 59,	db_get_rse_reg},
	{"r60",		(db_expr_t*) 60,	db_get_rse_reg},
	{"r61",		(db_expr_t*) 61,	db_get_rse_reg},
	{"r62",		(db_expr_t*) 62,	db_get_rse_reg},
	{"r63",		(db_expr_t*) 63,	db_get_rse_reg},
	{"r64",		(db_expr_t*) 64,	db_get_rse_reg},
	{"r65",		(db_expr_t*) 65,	db_get_rse_reg},
	{"r66",		(db_expr_t*) 66,	db_get_rse_reg},
	{"r67",		(db_expr_t*) 67,	db_get_rse_reg},
	{"r68",		(db_expr_t*) 68,	db_get_rse_reg},
	{"r69",		(db_expr_t*) 69,	db_get_rse_reg},
	{"r70",		(db_expr_t*) 70,	db_get_rse_reg},
	{"r71",		(db_expr_t*) 71,	db_get_rse_reg},
	{"r72",		(db_expr_t*) 72,	db_get_rse_reg},
	{"r73",		(db_expr_t*) 73,	db_get_rse_reg},
	{"r74",		(db_expr_t*) 74,	db_get_rse_reg},
	{"r75",		(db_expr_t*) 75,	db_get_rse_reg},
	{"r76",		(db_expr_t*) 76,	db_get_rse_reg},
	{"r77",		(db_expr_t*) 77,	db_get_rse_reg},
	{"r78",		(db_expr_t*) 78,	db_get_rse_reg},
	{"r79",		(db_expr_t*) 79,	db_get_rse_reg},
	{"r80",		(db_expr_t*) 80,	db_get_rse_reg},
	{"r81",		(db_expr_t*) 81,	db_get_rse_reg},
	{"r82",		(db_expr_t*) 82,	db_get_rse_reg},
	{"r83",		(db_expr_t*) 83,	db_get_rse_reg},
	{"r84",		(db_expr_t*) 84,	db_get_rse_reg},
	{"r85",		(db_expr_t*) 85,	db_get_rse_reg},
	{"r86",		(db_expr_t*) 86,	db_get_rse_reg},
	{"r87",		(db_expr_t*) 87,	db_get_rse_reg},
	{"r88",		(db_expr_t*) 88,	db_get_rse_reg},
	{"r89",		(db_expr_t*) 89,	db_get_rse_reg},
	{"r90",		(db_expr_t*) 90,	db_get_rse_reg},
	{"r91",		(db_expr_t*) 91,	db_get_rse_reg},
	{"r92",		(db_expr_t*) 92,	db_get_rse_reg},
	{"r93",		(db_expr_t*) 93,	db_get_rse_reg},
	{"r94",		(db_expr_t*) 94,	db_get_rse_reg},
	{"r95",		(db_expr_t*) 95,	db_get_rse_reg},
	{"r96",		(db_expr_t*) 96,	db_get_rse_reg},
	{"r97",		(db_expr_t*) 97,	db_get_rse_reg},
	{"r98",		(db_expr_t*) 98,	db_get_rse_reg},
	{"r99",		(db_expr_t*) 99,	db_get_rse_reg},
	{"r100",	(db_expr_t*) 100,	db_get_rse_reg},
	{"r101",	(db_expr_t*) 101,	db_get_rse_reg},
	{"r102",	(db_expr_t*) 102,	db_get_rse_reg},
	{"r103",	(db_expr_t*) 103,	db_get_rse_reg},
	{"r104",	(db_expr_t*) 104,	db_get_rse_reg},
	{"r105",	(db_expr_t*) 105,	db_get_rse_reg},
	{"r106",	(db_expr_t*) 106,	db_get_rse_reg},
	{"r107",	(db_expr_t*) 107,	db_get_rse_reg},
	{"r108",	(db_expr_t*) 108,	db_get_rse_reg},
	{"r109",	(db_expr_t*) 109,	db_get_rse_reg},
	{"r110",	(db_expr_t*) 110,	db_get_rse_reg},
	{"r111",	(db_expr_t*) 111,	db_get_rse_reg},
	{"r112",	(db_expr_t*) 112,	db_get_rse_reg},
	{"r113",	(db_expr_t*) 113,	db_get_rse_reg},
	{"r114",	(db_expr_t*) 114,	db_get_rse_reg},
	{"r115",	(db_expr_t*) 115,	db_get_rse_reg},
	{"r116",	(db_expr_t*) 116,	db_get_rse_reg},
	{"r117",	(db_expr_t*) 117,	db_get_rse_reg},
	{"r118",	(db_expr_t*) 118,	db_get_rse_reg},
	{"r119",	(db_expr_t*) 119,	db_get_rse_reg},
	{"r120",	(db_expr_t*) 120,	db_get_rse_reg},
	{"r121",	(db_expr_t*) 121,	db_get_rse_reg},
	{"r122",	(db_expr_t*) 122,	db_get_rse_reg},
	{"r123",	(db_expr_t*) 123,	db_get_rse_reg},
	{"r124",	(db_expr_t*) 124,	db_get_rse_reg},
	{"r125",	(db_expr_t*) 125,	db_get_rse_reg},
	{"r126",	(db_expr_t*) 126,	db_get_rse_reg},
	{"r127",	(db_expr_t*) 127,	db_get_rse_reg},
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

static int
db_get_rse_reg(struct db_variable *vp, db_expr_t *valuep, int op)
{
	u_int64_t *reg;
	uint64_t bsp;
	int nats, regno, sof;

	bsp = ddb_regs.tf_special.bspstore + ddb_regs.tf_special.ndirty;
	regno = (db_expr_t)vp->valuep - 32;
	sof = (int)(ddb_regs.tf_special.cfm & 0x7f);
	nats = (sof - regno + 63 - ((int)(bsp >> 3) & 0x3f)) / 63;

	reg = (void*)(bsp - ((sof - regno + nats) << 3));

	if (regno < sof) {
		if (op == DB_VAR_GET)
			*valuep = *reg;
		else
			*reg = *valuep;
	} else {
		if (op == DB_VAR_GET)
			*valuep = 0xdeadbeefdeadbeef;
	}

	return (0);
}

static int
db_get_ip_reg(struct db_variable *vp, db_expr_t *valuep, int op)
{
	/* Read only */
	if (op == DB_VAR_GET)
		*valuep = PC_REGS(DDB_REGS);
	return 0;
}

#if 0
/*
 * Print trap reason.
 */
static void
ddbprinttrap(int vector)
{

	/* XXX Implement. */

	printf("ddbprinttrap(%d)\n", vector);
}
#endif

#define CPUSTOP_ON_DDBBREAK
#define VERBOSE_CPUSTOP_ON_DDBBREAK

/*
 *  ddb_trap - field a kernel trap
 */
int
kdb_trap(int vector, struct trapframe *regs)
{
	int ddb_mode = !(boothowto & RB_GDB);
	register_t s;

	/*
	 * Don't bother checking for usermode, since a benign entry
	 * by the kernel (call to Debugger() or a breakpoint) has
	 * already checked for usermode.  If neither of those
	 * conditions exist, something Bad has happened.
	 */

	if (vector != IA64_VEC_BREAK
	    && vector != IA64_VEC_SINGLE_STEP_TRAP) {
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

	s = intr_disable();

#ifdef SMP
#ifdef CPUSTOP_ON_DDBBREAK

#if defined(VERBOSE_CPUSTOP_ON_DDBBREAK)
	db_printf("CPU%d stopping CPUs: 0x%08x...", PCPU_GET(cpuid),
	    PCPU_GET(other_cpus));
#endif /* VERBOSE_CPUSTOP_ON_DDBBREAK */

	/* We stop all CPUs except ourselves (obviously) */
	stop_cpus(PCPU_GET(other_cpus));

#if defined(VERBOSE_CPUSTOP_ON_DDBBREAK)
	db_printf(" stopped.\n");
#endif /* VERBOSE_CPUSTOP_ON_DDBBREAK */

#endif /* CPUSTOP_ON_DDBBREAK */
#endif /* SMP */

	ddb_regs = *regs;

	/*
	 * XXX pretend that registers outside the current frame don't exist.
	 */
	db_eregs = db_regs + DB_MISC_REGS + 3 + 27 +
	    (ddb_regs.tf_special.cfm & 0x7f);

	__asm __volatile("flushrs"); /* so we can look at them */

	db_active++;

	if (ddb_mode) {
	    cndbctl(TRUE);	/* DDB active, unblank video */
	    db_trap(vector, 0);	/* Where the work happens */
	    cndbctl(FALSE);	/* DDB inactive */
	} else
	    gdb_handle_exception(&ddb_regs, vector);

	db_active--;

#ifdef SMP
#ifdef CPUSTOP_ON_DDBBREAK

#if defined(VERBOSE_CPUSTOP_ON_DDBBREAK)
	db_printf("CPU%d restarting CPUs: 0x%08x...", PCPU_GET(cpuid),
	    stopped_cpus);
#endif /* VERBOSE_CPUSTOP_ON_DDBBREAK */

	/* Restart all the CPUs we previously stopped */
	if (stopped_cpus != PCPU_GET(other_cpus) && smp_started != 0) {
		db_printf("whoa, other_cpus: 0x%08x, stopped_cpus: 0x%08x\n",
			  PCPU_GET(other_cpus), stopped_cpus);
		panic("stop_cpus() failed");
	}
	restart_cpus(stopped_cpus);

#if defined(VERBOSE_CPUSTOP_ON_DDBBREAK)
	db_printf(" restarted.\n");
#endif /* VERBOSE_CPUSTOP_ON_DDBBREAK */

#endif /* CPUSTOP_ON_DDBBREAK */
#endif /* SMP */

	*regs = ddb_regs;

	intr_restore(s);


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

	db_nofault = &db_jmpbuf;

	if (addr < VM_MAX_ADDRESS)
		copyin((char *)addr, data, size);
	else
		bcopy((char *)addr, data, size);

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

	db_nofault = &db_jmpbuf;

	if (addr < VM_MAX_ADDRESS)
		copyout(data, (char *)addr, size);
	else
		bcopy(data, (char *)addr, size);

	db_nofault = 0;
}

void
Debugger(const char* msg)
{
	printf("%s\n", msg);
	__asm("break 0x80100");
}

u_long
db_register_value(regs, regno)
	db_regs_t *regs;
	int regno;
{
	uint64_t *rsp;
	uint64_t bsp;
	int nats, sof;

	if (regno == 0)
		return (0);
	if (regno == 1)
		return (regs->tf_special.gp);
	if (regno >= 2 && regno <= 3)
		return ((&regs->tf_scratch.gr2)[regno - 2]);
	if (regno >= 8 && regno <= 11)
		return ((&regs->tf_scratch.gr8)[regno - 8]);
	if (regno == 12)
		return (regs->tf_special.sp);
	if (regno == 13)
		return (regs->tf_special.tp);
	if (regno >= 14 && regno <= 31)
		return ((&regs->tf_scratch.gr14)[regno - 14]);

	sof = (int)(regs->tf_special.cfm & 0x7f);
	if (regno >= 32 && regno < sof + 32) {
		bsp = regs->tf_special.bspstore + regs->tf_special.ndirty;
		regno -= 32;
		nats = (sof - regno + 63 - ((int)(bsp >> 3) & 0x3f)) / 63;
		rsp = (void*)(bsp - ((sof - regno + nats) << 3));
		return (*rsp);
	}

	db_printf(" **** STRANGE REGISTER NUMBER %d **** ", regno);
	return (0);
}

void
db_read_bundle(db_addr_t addr, struct ia64_bundle *bp)
{
	u_int64_t low, high;

	db_read_bytes(addr, 8, (caddr_t) &low);
	db_read_bytes(addr+8, 8, (caddr_t) &high);

	ia64_unpack_bundle(low, high, bp);
}

void
db_write_bundle(db_addr_t addr, struct ia64_bundle *bp)
{
	u_int64_t low, high;

	ia64_pack_bundle(&low, &high, bp);

	db_write_bytes(addr, 8, (caddr_t) &low);
	db_write_bytes(addr+8, 8, (caddr_t) &high);

	ia64_fc(addr);
	ia64_sync_i();
}

void
db_write_breakpoint(vm_offset_t addr, u_int64_t *storage)
{
	struct ia64_bundle b;
	int slot;

	slot = addr & 15;
	addr &= ~15;
	db_read_bundle(addr, &b);
	*storage = b.slot[slot];
	b.slot[slot] = 0x80100 << 6; /* break.* 0x80100 */
	db_write_bundle(addr, &b);
}

void
db_clear_breakpoint(vm_offset_t addr, u_int64_t *storage)
{
	struct ia64_bundle b;
	int slot;

	slot = addr & 15;
	addr &= ~15;
	db_read_bundle(addr, &b);
	b.slot[slot] = *storage;
	db_write_bundle(addr, &b);
}

void
db_skip_breakpoint(void)
{
	/*
	 * Skip past the break instruction.
	 */
	ddb_regs.tf_special.psr += IA64_PSR_RI_1;
	if ((ddb_regs.tf_special.psr & IA64_PSR_RI) > IA64_PSR_RI_2) {
		ddb_regs.tf_special.psr &= ~IA64_PSR_RI;
		ddb_regs.tf_special.iip += 16;
	}
}

void
db_show_mdpcpu(struct pcpu *pc)
{
}
