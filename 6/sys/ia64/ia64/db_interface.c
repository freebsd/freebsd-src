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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Interface to DDB.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/smp.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/mutex.h>
#include <machine/setjmp.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

#include <ia64/disasm/disasm.h>

#define	TMPL_BITS	5
#define	TMPL_MASK	((1 << TMPL_BITS) - 1)
#define	SLOT_BITS	41
#define	SLOT_COUNT	3
#define	SLOT_MASK	((1ULL << SLOT_BITS) - 1ULL)
#define	SLOT_SHIFT(i)	(TMPL_BITS+((i)<<3)+(i))

static db_varfcn_t db_frame;
static db_varfcn_t db_getrse;
static db_varfcn_t db_getip;

#define	DB_OFFSET(x)	(db_expr_t *)offsetof(struct trapframe, x)
struct db_variable db_regs[] = {
	{"ip",		NULL,				db_getip},
	{"cr.ifs",	DB_OFFSET(tf_special.cfm),	db_frame},
	{"cr.ifa",	DB_OFFSET(tf_special.ifa),	db_frame},
	{"ar.bspstore",	DB_OFFSET(tf_special.bspstore),	db_frame},
	{"ndirty",	DB_OFFSET(tf_special.ndirty),	db_frame},
	{"rp",		DB_OFFSET(tf_special.rp),	db_frame},
	{"ar.pfs",	DB_OFFSET(tf_special.pfs),	db_frame},
	{"psr",		DB_OFFSET(tf_special.psr),	db_frame},
	{"cr.isr",	DB_OFFSET(tf_special.isr),	db_frame},
	{"pr",		DB_OFFSET(tf_special.pr),	db_frame},
	{"ar.rsc",	DB_OFFSET(tf_special.rsc),	db_frame},
	{"ar.rnat",	DB_OFFSET(tf_special.rnat),	db_frame},
	{"ar.unat",	DB_OFFSET(tf_special.unat),	db_frame},
	{"ar.fpsr",	DB_OFFSET(tf_special.fpsr),	db_frame},
	{"gp",		DB_OFFSET(tf_special.gp),	db_frame},
	{"sp",		DB_OFFSET(tf_special.sp),	db_frame},
	{"tp",		DB_OFFSET(tf_special.tp),	db_frame},
	{"b6",		DB_OFFSET(tf_scratch.br6),	db_frame},
	{"b7",		DB_OFFSET(tf_scratch.br7),	db_frame},
	{"r2",		DB_OFFSET(tf_scratch.gr2),	db_frame},
	{"r3",		DB_OFFSET(tf_scratch.gr3),	db_frame},
	{"r8",		DB_OFFSET(tf_scratch.gr8),	db_frame},
	{"r9",		DB_OFFSET(tf_scratch.gr9),	db_frame},
	{"r10",		DB_OFFSET(tf_scratch.gr10),	db_frame},
	{"r11",		DB_OFFSET(tf_scratch.gr11),	db_frame},
	{"r14",		DB_OFFSET(tf_scratch.gr14),	db_frame},
	{"r15",		DB_OFFSET(tf_scratch.gr15),	db_frame},
	{"r16",		DB_OFFSET(tf_scratch.gr16),	db_frame},
	{"r17",		DB_OFFSET(tf_scratch.gr17),	db_frame},
	{"r18",		DB_OFFSET(tf_scratch.gr18),	db_frame},
	{"r19",		DB_OFFSET(tf_scratch.gr19),	db_frame},
	{"r20",		DB_OFFSET(tf_scratch.gr20),	db_frame},
	{"r21",		DB_OFFSET(tf_scratch.gr21),	db_frame},
	{"r22",		DB_OFFSET(tf_scratch.gr22),	db_frame},
	{"r23",		DB_OFFSET(tf_scratch.gr23),	db_frame},
	{"r24",		DB_OFFSET(tf_scratch.gr24),	db_frame},
	{"r25",		DB_OFFSET(tf_scratch.gr25),	db_frame},
	{"r26",		DB_OFFSET(tf_scratch.gr26),	db_frame},
	{"r27",		DB_OFFSET(tf_scratch.gr27),	db_frame},
	{"r28",		DB_OFFSET(tf_scratch.gr28),	db_frame},
	{"r29",		DB_OFFSET(tf_scratch.gr29),	db_frame},
	{"r30",		DB_OFFSET(tf_scratch.gr30),	db_frame},
	{"r31",		DB_OFFSET(tf_scratch.gr31),	db_frame},
	{"r32",		(db_expr_t*)0,			db_getrse},
	{"r33",		(db_expr_t*)1,			db_getrse},
	{"r34",		(db_expr_t*)2,			db_getrse},
	{"r35",		(db_expr_t*)3,			db_getrse},
	{"r36",		(db_expr_t*)4,			db_getrse},
	{"r37",		(db_expr_t*)5,			db_getrse},
	{"r38",		(db_expr_t*)6,			db_getrse},
	{"r39",		(db_expr_t*)7,			db_getrse},
	{"r40",		(db_expr_t*)8,			db_getrse},
	{"r41",		(db_expr_t*)9,			db_getrse},
	{"r42",		(db_expr_t*)10,			db_getrse},
	{"r43",		(db_expr_t*)11,			db_getrse},
	{"r44",		(db_expr_t*)12,			db_getrse},
	{"r45",		(db_expr_t*)13,			db_getrse},
	{"r46",		(db_expr_t*)14,			db_getrse},
	{"r47",		(db_expr_t*)15,			db_getrse},
	{"r48",		(db_expr_t*)16,			db_getrse},
	{"r49",		(db_expr_t*)17,			db_getrse},
	{"r50",		(db_expr_t*)18,			db_getrse},
	{"r51",		(db_expr_t*)19,			db_getrse},
	{"r52",		(db_expr_t*)20,			db_getrse},
	{"r53",		(db_expr_t*)21,			db_getrse},
	{"r54",		(db_expr_t*)22,			db_getrse},
	{"r55",		(db_expr_t*)23,			db_getrse},
	{"r56",		(db_expr_t*)24,			db_getrse},
	{"r57",		(db_expr_t*)25,			db_getrse},
	{"r58",		(db_expr_t*)26,			db_getrse},
	{"r59",		(db_expr_t*)27,			db_getrse},
	{"r60",		(db_expr_t*)28,			db_getrse},
	{"r61",		(db_expr_t*)29,			db_getrse},
	{"r62",		(db_expr_t*)30,			db_getrse},
	{"r63",		(db_expr_t*)31,			db_getrse},
	{"r64",		(db_expr_t*)32,			db_getrse},
	{"r65",		(db_expr_t*)33,			db_getrse},
	{"r66",		(db_expr_t*)34,			db_getrse},
	{"r67",		(db_expr_t*)35,			db_getrse},
	{"r68",		(db_expr_t*)36,			db_getrse},
	{"r69",		(db_expr_t*)37,			db_getrse},
	{"r70",		(db_expr_t*)38,			db_getrse},
	{"r71",		(db_expr_t*)39,			db_getrse},
	{"r72",		(db_expr_t*)40,			db_getrse},
	{"r73",		(db_expr_t*)41,			db_getrse},
	{"r74",		(db_expr_t*)42,			db_getrse},
	{"r75",		(db_expr_t*)43,			db_getrse},
	{"r76",		(db_expr_t*)44,			db_getrse},
	{"r77",		(db_expr_t*)45,			db_getrse},
	{"r78",		(db_expr_t*)46,			db_getrse},
	{"r79",		(db_expr_t*)47,			db_getrse},
	{"r80",		(db_expr_t*)48,			db_getrse},
	{"r81",		(db_expr_t*)49,			db_getrse},
	{"r82",		(db_expr_t*)50,			db_getrse},
	{"r83",		(db_expr_t*)51,			db_getrse},
	{"r84",		(db_expr_t*)52,			db_getrse},
	{"r85",		(db_expr_t*)53,			db_getrse},
	{"r86",		(db_expr_t*)54,			db_getrse},
	{"r87",		(db_expr_t*)55,			db_getrse},
	{"r88",		(db_expr_t*)56,			db_getrse},
	{"r89",		(db_expr_t*)57,			db_getrse},
	{"r90",		(db_expr_t*)58,			db_getrse},
	{"r91",		(db_expr_t*)59,			db_getrse},
	{"r92",		(db_expr_t*)60,			db_getrse},
	{"r93",		(db_expr_t*)61,			db_getrse},
	{"r94",		(db_expr_t*)62,			db_getrse},
	{"r95",		(db_expr_t*)63,			db_getrse},
	{"r96",		(db_expr_t*)64,			db_getrse},
	{"r97",		(db_expr_t*)65,			db_getrse},
	{"r98",		(db_expr_t*)66,			db_getrse},
	{"r99",		(db_expr_t*)67,			db_getrse},
	{"r100",	(db_expr_t*)68,			db_getrse},
	{"r101",	(db_expr_t*)69,			db_getrse},
	{"r102",	(db_expr_t*)70,			db_getrse},
	{"r103",	(db_expr_t*)71,			db_getrse},
	{"r104",	(db_expr_t*)72,			db_getrse},
	{"r105",	(db_expr_t*)73,			db_getrse},
	{"r106",	(db_expr_t*)74,			db_getrse},
	{"r107",	(db_expr_t*)75,			db_getrse},
	{"r108",	(db_expr_t*)76,			db_getrse},
	{"r109",	(db_expr_t*)77,			db_getrse},
	{"r110",	(db_expr_t*)78,			db_getrse},
	{"r111",	(db_expr_t*)79,			db_getrse},
	{"r112",	(db_expr_t*)80,			db_getrse},
	{"r113",	(db_expr_t*)81,			db_getrse},
	{"r114",	(db_expr_t*)82,			db_getrse},
	{"r115",	(db_expr_t*)83,			db_getrse},
	{"r116",	(db_expr_t*)84,			db_getrse},
	{"r117",	(db_expr_t*)85,			db_getrse},
	{"r118",	(db_expr_t*)86,			db_getrse},
	{"r119",	(db_expr_t*)87,			db_getrse},
	{"r120",	(db_expr_t*)88,			db_getrse},
	{"r121",	(db_expr_t*)89,			db_getrse},
	{"r122",	(db_expr_t*)90,			db_getrse},
	{"r123",	(db_expr_t*)91,			db_getrse},
	{"r124",	(db_expr_t*)92,			db_getrse},
	{"r125",	(db_expr_t*)93,			db_getrse},
	{"r126",	(db_expr_t*)94,			db_getrse},
	{"r127",	(db_expr_t*)95,			db_getrse},
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

static int
db_frame(struct db_variable *vp, db_expr_t *valuep, int op)
{
	uint64_t *reg;

	if (kdb_frame == NULL)
		return (0);
	reg = (uint64_t*)((uintptr_t)kdb_frame + (uintptr_t)vp->valuep);
	if (op == DB_VAR_GET)
		*valuep = *reg;
	else
		*reg = *valuep;
	return (1);
}

static int
db_getrse(struct db_variable *vp, db_expr_t *valuep, int op)
{
	u_int64_t *reg;
	uint64_t bsp;
	int nats, regno, sof;

	if (kdb_frame == NULL)
		return (0);

	regno = (int)(intptr_t)valuep;
	bsp = kdb_frame->tf_special.bspstore + kdb_frame->tf_special.ndirty;
	sof = (int)(kdb_frame->tf_special.cfm & 0x7f);

	if (regno >= sof)
		return (0);

	nats = (sof - regno + 63 - ((int)(bsp >> 3) & 0x3f)) / 63;
	reg = (void*)(bsp - ((sof - regno + nats) << 3));
	if (op == DB_VAR_GET)
		*valuep = *reg;
	else
		*reg = *valuep;
	return (1);
}

static int
db_getip(struct db_variable *vp, db_expr_t *valuep, int op)
{
	u_long iip, slot;

	if (kdb_frame == NULL)
		return (0);

	if (op == DB_VAR_GET) {
		iip = kdb_frame->tf_special.iip;
		slot = (kdb_frame->tf_special.psr >> 41) & 3;
		*valuep = iip + slot;
	} else {
		iip = *valuep & ~0xf;
		slot = *valuep & 0xf;
		if (slot > 2)
			return (0);
		kdb_frame->tf_special.iip = iip;
		kdb_frame->tf_special.psr &= ~IA64_PSR_RI;
		kdb_frame->tf_special.psr |= slot << 41;
	}
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
	}
	(void)kdb_jmpbuf(prev_jb);
	return (ret);
}

void
db_bkpt_write(db_addr_t addr, BKPT_INST_TYPE *storage)
{
	BKPT_INST_TYPE tmp;
	db_addr_t loc;
	int slot;

	slot = addr & 0xfUL;
	if (slot >= SLOT_COUNT)
		return;
	loc = (addr & ~0xfUL) + (slot << 2);

	db_read_bytes(loc, sizeof(BKPT_INST_TYPE), (char *)&tmp);
	*storage = (tmp >> SLOT_SHIFT(slot)) & SLOT_MASK;

	tmp &= ~(SLOT_MASK << SLOT_SHIFT(slot));
	tmp |= (0x84000 << 6) << SLOT_SHIFT(slot);
	db_write_bytes(loc, sizeof(BKPT_INST_TYPE), (char *)&tmp);
}

void
db_bkpt_clear(db_addr_t addr, BKPT_INST_TYPE *storage)
{
	BKPT_INST_TYPE tmp;
	db_addr_t loc;
	int slot;

	slot = addr & 0xfUL;
	if (slot >= SLOT_COUNT)
		return;
	loc = (addr & ~0xfUL) + (slot << 2);

	db_read_bytes(loc, sizeof(BKPT_INST_TYPE), (char *)&tmp);
	tmp &= ~(SLOT_MASK << SLOT_SHIFT(slot));
	tmp |= *storage << SLOT_SHIFT(slot);
	db_write_bytes(loc, sizeof(BKPT_INST_TYPE), (char *)&tmp);
}

void
db_bkpt_skip(void)
{

	if (kdb_frame == NULL)
		return;

	kdb_frame->tf_special.psr += IA64_PSR_RI_1;
	if ((kdb_frame->tf_special.psr & IA64_PSR_RI) > IA64_PSR_RI_2) {
		kdb_frame->tf_special.psr &= ~IA64_PSR_RI;
		kdb_frame->tf_special.iip += 16;
	}
}

db_addr_t
db_disasm(db_addr_t loc, boolean_t altfmt)
{
	char buf[32];
	struct asm_bundle bundle;
	const struct asm_inst *i;
	const char *tmpl;
	int n, slot;

	slot = loc & 0xf;
	loc &= ~0xful;
	db_read_bytes(loc, 16, buf);
	if (asm_decode((uintptr_t)buf, &bundle)) {
		i = bundle.b_inst + slot;
		tmpl = bundle.b_templ + slot;
		if (*tmpl == ';' || (slot == 2 && bundle.b_templ[1] == ';'))
			tmpl++;
		if (*tmpl == 'L' || i->i_op == ASM_OP_NONE) {
			db_printf("\n");
			goto out;
		}

		/* Unit + slot. */
		db_printf("[%c%d] ", *tmpl, slot);

		/* Predicate. */
		if (i->i_oper[0].o_value != 0) {
			asm_operand(i->i_oper+0, buf, loc);
			db_printf("(%s) ", buf);
		} else
			db_printf("   ");

		/* Mnemonic & completers. */
		asm_mnemonic(i->i_op, buf);
		db_printf(buf);
		n = 0;
		while (n < i->i_ncmpltrs) {
			asm_completer(i->i_cmpltr + n, buf);
			db_printf(buf);
			n++;
		}
		db_printf(" ");

		/* Operands. */
		n = 1;
		while (n < 7 && i->i_oper[n].o_type != ASM_OPER_NONE) {
			if (n > 1) {
				if (n == i->i_srcidx)
					db_printf("=");
				else
					db_printf(",");
			}
			asm_operand(i->i_oper + n, buf, loc);
			db_printf(buf);
			n++;
		}
	} else {
		tmpl = NULL;
		slot = 2;
	}
	db_printf("\n");

out:
	slot++;
	if (slot == 1 && tmpl[1] == 'L')
		slot++;
	if (slot > 2)
		slot = 16;
	return (loc + slot);
}

typedef db_expr_t __db_f(db_expr_t, db_expr_t, db_expr_t, db_expr_t, db_expr_t,
    db_expr_t, db_expr_t, db_expr_t);

register uint64_t __db_gp __asm__("gp");

int
db_fncall_ia64(db_expr_t addr, db_expr_t *rv, int nargs, db_expr_t args[])
{
	struct ia64_fdesc fdesc;
	__db_f *f;

	f = (__db_f *)&fdesc;
	fdesc.func = addr;
	fdesc.gp = __db_gp;	/* XXX doesn't work for modules. */
	*rv = (*f)(args[0], args[1], args[2], args[3], args[4], args[5],
	    args[6], args[7]);
	return (1);
}

void
db_show_mdpcpu(struct pcpu *pc)
{
}
