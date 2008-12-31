/*-
 * Copyright (c) 2003-2005 Marcel Moolenaar
 * Copyright (c) 2000-2001 Doug Rabson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/ia64/ia64/db_machdep.c,v 1.4.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <opt_xtrace.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/cons.h>
#include <sys/kdb.h>
#include <sys/ktr.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/smp.h>
#include <sys/stack.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>
#include <machine/kdb.h>
#include <machine/md_var.h>
#include <machine/mutex.h>
#include <machine/pcb.h>
#include <machine/setjmp.h>
#include <machine/unwind.h>
#include <machine/vmparam.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

#include <ia64/disasm/disasm.h>

#define	TMPL_BITS	5
#define	TMPL_MASK	((1 << TMPL_BITS) - 1)
#define	SLOT_BITS	41
#define	SLOT_COUNT	3
#define	SLOT_MASK	((1ULL << SLOT_BITS) - 1ULL)
#define	SLOT_SHIFT(i)	(TMPL_BITS+((i)<<3)+(i))

typedef db_expr_t __db_f(db_expr_t, db_expr_t, db_expr_t, db_expr_t, db_expr_t,
    db_expr_t, db_expr_t, db_expr_t);

register uint64_t __db_gp __asm__("gp");

static db_varfcn_t db_frame;
static db_varfcn_t db_getip;
static db_varfcn_t db_getrse;

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
db_backtrace(struct thread *td, struct pcb *pcb, int count)
{
	struct unw_regstate rs;
	struct trapframe *tf;
	const char *name;
	db_expr_t offset;
	uint64_t bsp, cfm, ip, pfs, reg, sp;
	c_db_sym_t sym;
	int args, error, i;

	error = unw_create_from_pcb(&rs, pcb);
	while (!error && count-- && !db_pager_quit) {
		error = unw_get_cfm(&rs, &cfm);
		if (!error)
			error = unw_get_bsp(&rs, &bsp);
		if (!error)
			error = unw_get_ip(&rs, &ip);
		if (!error)
			error = unw_get_sp(&rs, &sp);
		if (error)
			break;

		args = IA64_CFM_SOL(cfm);
		if (args > 8)
			args = 8;

		error = unw_step(&rs);
		if (!error) {
			if (!unw_get_cfm(&rs, &pfs)) {
				i = IA64_CFM_SOF(pfs) - IA64_CFM_SOL(pfs);
				if (args > i)
					args = i;
			}
		}

		sym = db_search_symbol(ip, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);
		db_printf("%s(", name);
		if (bsp >= IA64_RR_BASE(5)) {
			for (i = 0; i < args; i++) {
				if ((bsp & 0x1ff) == 0x1f8)
					bsp += 8;
				db_read_bytes(bsp, sizeof(reg), (void*)&reg);
				if (i > 0)
					db_printf(", ");
				db_printf("0x%lx", reg);
				bsp += 8;
			}
		} else
			db_printf("...");
		db_printf(") at ");

		db_printsym(ip, DB_STGY_PROC);
		db_printf("\n");

		if (error != ERESTART)
			continue;
		if (sp < IA64_RR_BASE(5))
			break;

		tf = (struct trapframe *)(sp + 16);
		if ((tf->tf_flags & FRAME_SYSCALL) != 0 ||
		    tf->tf_special.iip < IA64_RR_BASE(5))
			break;

		/* XXX ask if we should unwind across the trapframe. */
		db_printf("--- trapframe at %p\n", tf);
		unw_delete(&rs);
		error = unw_create_from_frame(&rs, tf);
	}

	unw_delete(&rs);
	/*
	 * EJUSTRETURN and ERESTART signal the end of a trace and
	 * are not really errors.
	 */
	return ((error > 0) ? error : 0);
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

int
db_md_clr_watchpoint(db_expr_t addr, db_expr_t size)
{

	return (-1);
}

void
db_md_list_watchpoints()
{

	return;
}

int
db_md_set_watchpoint(db_expr_t addr, db_expr_t size)
{

	return (-1);
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
	size_t cnt;
	char *dst;
	int ret;

	prev_jb = kdb_jmpbuf(jb);
	ret = setjmp(jb);
	if (ret == 0) {
		dst = (char *)addr;
		cnt = size;
		while (cnt-- > 0)
			*dst++ = *data++;
		kdb_cpu_sync_icache((void *)addr, size);
	}
	(void)kdb_jmpbuf(prev_jb);
	return (ret);
}

void
db_show_mdpcpu(struct pcpu *pc)
{
}

void
db_trace_self(void)
{
	struct pcb pcb;

	savectx(&pcb);
	db_backtrace(curthread, &pcb, -1);
}

int
db_trace_thread(struct thread *td, int count)
{
	struct pcb *ctx;

	ctx = kdb_thr_ctx(td);
	return (db_backtrace(td, ctx, count));
}

#ifdef EXCEPTION_TRACING

extern long xtrace[];
extern long *xhead;

DB_COMMAND(xtrace, db_xtrace)
{
	long *p;

	p = (*xhead == 0) ? xtrace : xhead;

	db_printf("ITC\t\t IVT\t\t  IIP\t\t   IFA\t\t    ISR\n");
	if (*p == 0)
		return;

	do {
		db_printf("%016lx %016lx %016lx %016lx %016lx\n", p[0], p[1],
		    p[2], p[3], p[4]);
		p += 5;
		if (p == (void *)&xhead)
			p = xtrace;
	} while (p != xhead);
}

#endif
