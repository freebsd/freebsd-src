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
#include <sys/cons.h>
#include <sys/ktr.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/mutex.h>
#include <machine/smp.h>

#include <machine/inst.h>

#include <ddb/ddb.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>
#include <setjmp.h>

static jmp_buf *db_nofault = 0;
extern jmp_buf	db_jmpbuf;

extern void	gdb_handle_exception __P((db_regs_t *, int));

#if 0
extern char *trap_type[];
extern int trap_types;
#endif

int	db_active;
static u_int64_t zero;
static int db_get_rse_reg(struct db_variable *vp, db_expr_t *valuep, int op);

struct db_variable db_regs[] = {
	/* Misc control/app registers */
#define DB_MISC_REGS	14	/* make sure this is correct */

	{"ip",		(db_expr_t*) &ddb_regs.tf_cr_iip,	FCN_NULL},
	{"psr",		(db_expr_t*) &ddb_regs.tf_cr_ipsr,	FCN_NULL},
	{"cr.isr",	(db_expr_t*) &ddb_regs.tf_cr_isr,	FCN_NULL},
	{"cr.ifa",	(db_expr_t*) &ddb_regs.tf_cr_ifa,	FCN_NULL},
	{"pr",		(db_expr_t*) &ddb_regs.tf_pr,		FCN_NULL},
	{"ar.rsc",	(db_expr_t*) &ddb_regs.tf_ar_rsc,	FCN_NULL},
	{"ar.pfs",	(db_expr_t*) &ddb_regs.tf_ar_pfs,	FCN_NULL},
	{"cr.ifs",	(db_expr_t*) &ddb_regs.tf_cr_ifs,	FCN_NULL},
	{"ar.bspstore",	(db_expr_t*) &ddb_regs.tf_ar_bspstore,	FCN_NULL},
	{"ar.rnat",	(db_expr_t*) &ddb_regs.tf_ar_rnat,	FCN_NULL},
	{"ar.bsp",	(db_expr_t*) &ddb_regs.tf_ar_bsp,	FCN_NULL},
	{"ar.unat",	(db_expr_t*) &ddb_regs.tf_ar_unat,	FCN_NULL},
	{"ar.ccv",	(db_expr_t*) &ddb_regs.tf_ar_ccv,	FCN_NULL},
	{"ar.fpsr",	(db_expr_t*) &ddb_regs.tf_ar_fpsr,	FCN_NULL},

	/* Static registers */
	{"r0",		(db_expr_t*) &zero,			FCN_NULL},
	{"gp",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R1],	FCN_NULL},
	{"r2",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R2],	FCN_NULL},
	{"r3",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R3],	FCN_NULL},
	{"r4",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R4],	FCN_NULL},
	{"r5",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R5],	FCN_NULL},
	{"r6",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R6],	FCN_NULL},
	{"r7",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R7],	FCN_NULL},
	{"r8",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R8],	FCN_NULL},
	{"r9",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R9],	FCN_NULL},
	{"r10",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R10],	FCN_NULL},
	{"r11",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R11],	FCN_NULL},
	{"sp",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R12],	FCN_NULL},
	{"r13",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R13],	FCN_NULL},
	{"r14",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R14],	FCN_NULL},
	{"r15",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R15],	FCN_NULL},
	{"r16",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R16],	FCN_NULL},
	{"r17",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R17],	FCN_NULL},
	{"r18",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R18],	FCN_NULL},
	{"r19",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R19],	FCN_NULL},
	{"r20",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R20],	FCN_NULL},
	{"r21",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R21],	FCN_NULL},
	{"r22",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R22],	FCN_NULL},
	{"r23",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R23],	FCN_NULL},
	{"r24",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R24],	FCN_NULL},
	{"r25",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R25],	FCN_NULL},
	{"r26",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R26],	FCN_NULL},
	{"r27",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R27],	FCN_NULL},
	{"r28",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R28],	FCN_NULL},
	{"r29",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R29],	FCN_NULL},
	{"r30",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R30],	FCN_NULL},
	{"r31",		(db_expr_t*) &ddb_regs.tf_r[FRAME_R31],	FCN_NULL},

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
rse_slot(u_int64_t *bsp)
{
	return ((u_int64_t) bsp >> 3) & 0x3f;
}

/*
 * Return the address of register regno (regno >= 32) given that bsp
 * points at the base of the register stack frame.
 */
static u_int64_t *
rse_register_address(u_int64_t *bsp, int regno)
{
	int off = regno - 32;
	u_int64_t rnats = (rse_slot(bsp) + off) / 63;
	u_int64_t *p = bsp + off + rnats;
	return p;
}

static u_int64_t *
rse_previous_frame(u_int64_t *bsp, int sof)
{
	int slot = rse_slot(bsp);
	int rnats = 0;
	int count = sof;

	while (count > slot) {
		count -= 63;
		rnats++;
		slot = 63;
	}
	return bsp - sof - rnats;
}

static int
db_get_rse_reg(struct db_variable *vp, db_expr_t *valuep, int op)
{
	int sof = ddb_regs.tf_cr_ifs & 0xff;
	int regno = (db_expr_t) vp->valuep;
	u_int64_t *bsp = (u_int64_t *) ddb_regs.tf_ar_bsp;
	u_int64_t *reg;

	if (regno - 32 >= sof) {
		if (op == DB_VAR_GET)
			*valuep = 0xdeadbeefdeadbeef;
	} else {
		bsp = rse_previous_frame(bsp, sof);
		reg = rse_register_address(bsp, regno);
		if (op == DB_VAR_GET)
			*valuep = *reg;
		else
			*reg = *valuep;
	}

	return 0;
}

/*
 * Print trap reason.
 */
static void
ddbprinttrap(int vector)
{

	/* XXX Implement. */

	printf("ddbprinttrap(%d)\n", vector);
}

/*
 *  ddb_trap - field a kernel trap
 */
int
kdb_trap(int vector, struct trapframe *regs)
{
	int ddb_mode = !(boothowto & RB_GDB);
	int s;

	/*
	 * Don't bother checking for usermode, since a benign entry
	 * by the kernel (call to Debugger() or a breakpoint) has
	 * already checked for usermode.  If neither of those
	 * conditions exist, something Bad has happened.
	 */

	if (vector != IA64_VEC_BREAK) {
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

	/*
	 * XXX pretend that registers outside the current frame don't exist.
	 */
	db_eregs = db_regs + DB_MISC_REGS + 32 + (ddb_regs.tf_cr_ifs & 0xff);

	__asm __volatile("flushrs"); /* so we can look at them */

	s = splhigh();

#if 0
	db_printf("stopping %x\n", PCPU_GET(other_cpus));
	stop_cpus(PCPU_GET(other_cpus));
	db_printf("stopped_cpus=%x\n", stopped_cpus);
#endif

	db_active++;

	if (ddb_mode) {
	    cndbctl(TRUE);	/* DDB active, unblank video */
	    db_trap(vector, 0);	/* Where the work happens */
	    cndbctl(FALSE);	/* DDB inactive */
	} else
	    gdb_handle_exception(&ddb_regs, vector);

	db_active--;

#if 0
	restart_cpus(stopped_cpus);
#endif

	splx(s);

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

	if (regno > 127 || regno < 0) {
		db_printf(" **** STRANGE REGISTER NUMBER %d **** ", regno);
		return (0);
	}

	if (regno == 0)
		return (0);

	if (regno < 32) {
		return (regs->tf_r[regno - 1]);
	} else {
		int sof = ddb_regs.tf_cr_ifs & 0xff;
		u_int64_t *bsp = (u_int64_t *) ddb_regs.tf_ar_bsp;
		u_int64_t *reg;

		if (regno - 32 >= sof) {
			return 0xdeadbeefdeadbeef;
		} else {
			bsp = rse_previous_frame(bsp, sof);
			reg = rse_register_address(bsp, regno);
			return *reg;
		}
	}
}

#if defined(KTR)

struct tstate {
	int	cur;
	int	first;
};
static struct tstate tstate;
static int db_mach_vtrace(void);

DB_COMMAND(tbuf, db_mach_tbuf)
{

	/*
	 * We know that ktr_idx is the oldest entry, so don't go futzing
	 * around with timespecs unnecessarily.
	 */
	tstate.first = ktr_idx;
	if ((tstate.cur = ktr_idx - 1) < 0)
		tstate.cur = KTR_ENTRIES - 1;
	
	db_mach_vtrace();

	return;
}

DB_COMMAND(tall, db_mach_tall)
{
	int	c;

	db_mach_tbuf(addr, have_addr, count, modif);
	while (db_mach_vtrace()) {
		c = cncheckc();
		if (c != -1)
			break;
	}

	return;
}

DB_COMMAND(tnext, db_mach_tnext)
{
	db_mach_vtrace();
}

static int
db_mach_vtrace(void)
{
	struct ktr_entry	*kp;

	if (tstate.cur == tstate.first) {
		db_printf("--- End of trace buffer ---\n");
		return (0);
	}

	kp = &ktr_buf[tstate.cur];

	db_printf("%d: %4d.%06ld: ", tstate.cur, kp->ktr_tv.tv_sec,
	    kp->ktr_tv.tv_nsec / 1000);
#ifdef KTR_EXTEND
	db_printf("cpu%d %s.%d\t%s", kp->ktr_cpu, kp->ktr_filename,
	    kp->ktr_line, kp->ktr_desc);
#else
	db_printf(kp->ktr_desc, kp->ktr_parm1, kp->ktr_parm2, kp->ktr_parm3,
	    kp->ktr_parm4, kp->ktr_parm5);
#endif
	db_printf("\n");
	tstate.first &= ~0x80000000;
	if (--tstate.cur < 0)
		tstate.cur = KTR_ENTRIES - 1;

	return (1);
}

#endif
