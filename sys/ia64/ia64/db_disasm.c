/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <machine/db_machdep.h>
#include <machine/inst.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>

#define sign_extend(imm, w) (((int64_t)(imm) << (64 - (w))) >> (64 - (w)))

typedef void (*ia64_print_slot)(db_addr_t loc, u_int64_t slot, boolean_t showregs);

static void ia64_print_M(db_addr_t, u_int64_t, boolean_t);
static void ia64_print_I(db_addr_t, u_int64_t, boolean_t);
static void ia64_print_X(db_addr_t, u_int64_t, boolean_t);
static void ia64_print_B(db_addr_t, u_int64_t, boolean_t);
static void ia64_print_F(db_addr_t, u_int64_t, boolean_t);
static void ia64_print_bad(db_addr_t, u_int64_t, boolean_t);

#define M	ia64_print_M
#define I	ia64_print_I
#define X	ia64_print_X
#define B	ia64_print_B
#define F	ia64_print_F
#define _	ia64_print_bad

/*
 * Convert template+slot into a function to disassemble that slot.
 */
static ia64_print_slot prints[][3] = {
	{ M, I, I },		/* 00 */
	{ M, I, I },		/* 01 */
	{ M, I, I },		/* 02 */
	{ M, I, I },		/* 03 */
	{ M, _, X },		/* 04 */
	{ M, _, X,},		/* 05 */
	{ _, _, _ },		/* 06 */
	{ _, _, _ },		/* 07 */
	{ M, M, I },		/* 08 */
	{ M, M, I },		/* 09 */
	{ M, M, I },		/* 0a */
	{ M, M, I },		/* 0b */
	{ M, F, I },		/* 0c */
	{ M, F, I },		/* 0d */
	{ M, M, F },		/* 0e */
	{ M, M, F },		/* 0f */
	{ M, I, B },		/* 10 */
	{ M, I, B },		/* 11 */
	{ M, B, B },		/* 12 */
	{ M, B, B },		/* 13 */
	{ _, _, _ },		/* 14 */
	{ _, _, _ },		/* 15 */
	{ B, B, B },		/* 16 */
	{ B, B, B },		/* 17 */
	{ M, M, B },		/* 18 */
	{ M, M, B },		/* 19 */
	{ _, _, _ },		/* 1a */
	{ _, _, _ },		/* 1b */
	{ M, F, B },		/* 1c */
	{ M, F, B },		/* 1d */
	{ _, _, _ },		/* 1e */
	{ _, _, _ },		/* 1f */
};

#undef M
#undef I
#undef X
#undef B
#undef F
#undef _

/*
 * Nonzero if template+slot has a following stop
 */
static char stops[][3] = {
	{ 0, 0, 0 },		/* 00 */
	{ 0, 0, 1 },		/* 01 */
	{ 0, 1, 0 },		/* 02 */
	{ 0, 1, 1 },		/* 03 */
	{ 0, 0, 0 },		/* 04 */
	{ 0, 0, 1 },		/* 05 */
	{ 0, 0, 0 },		/* 06 */
	{ 0, 0, 0 },		/* 07 */
	{ 0, 0, 0 },		/* 08 */
	{ 0, 0, 1 },		/* 09 */
	{ 1, 0, 0 },		/* 0a */
	{ 1, 0, 1 },		/* 0b */
	{ 0, 0, 0 },		/* 0c */
	{ 0, 0, 1 },		/* 0d */
	{ 0, 0, 0 },		/* 0e */
	{ 0, 0, 1 },		/* 0f */
	{ 0, 0, 0 },		/* 10 */
	{ 0, 0, 1 },		/* 11 */
	{ 0, 0, 0 },		/* 12 */
	{ 0, 0, 1 },		/* 13 */
	{ 0, 0, 0 },		/* 14 */
	{ 0, 0, 0 },		/* 15 */
	{ 0, 0, 0 },		/* 16 */
	{ 0, 0, 1 },		/* 17 */
	{ 0, 0, 0 },		/* 18 */
	{ 0, 0, 1 },		/* 19 */
	{ 0, 0, 0 },		/* 1a */
	{ 0, 0, 0 },		/* 1b */
	{ 0, 0, 0 },		/* 1c */
	{ 0, 0, 1 },		/* 1d */
	{ 0, 0, 0 },		/* 1e */
	{ 0, 0, 0 },		/* 1f */
};

const char *register_names[] = {
	"r0",	"gp",	"r2",	"r3",
	"r4",	"r5",	"r6",	"r7",
	"r8",	"r9",	"r10",	"r11",
	"sp",	"r13",	"r14",	"r15",
	"r16",	"r17",	"r18",	"r19",
	"r20",	"r21",	"r22",	"r23",
	"r24",	"r25",	"r26",	"r27",
	"r28",	"r29",	"r30",	"r31",
	"r32",	"r33",	"r34",	"r35",
	"r36",	"r37",	"r38",	"r39",
	"r40",	"r41",	"r42",	"r43",
	"r44",	"r45",	"r46",	"r47",
	"r48",	"r49",	"r50",	"r51",
	"r52",	"r53",	"r54",	"r55",
	"r56",	"r57",	"r58",	"r59",
	"r60",	"r61",	"r62",	"r63",
	"r64",	"r65",	"r66",	"r67",
	"r68",	"r69",	"r70",	"r71",
	"r72",	"r73",	"r74",	"r75",
	"r76",	"r77",	"r78",	"r79",
	"r80",	"r81",	"r82",	"r83",
	"r84",	"r85",	"r86",	"r87",
	"r88",	"r89",	"r90",	"r91",
	"r92",	"r93",	"r94",	"r95",
	"r96",	"r97",	"r98",	"r99",
	"r100",	"r101",	"r102",	"r103",
	"r104",	"r105",	"r106",	"r107",
	"r108",	"r109",	"r110",	"r111",
	"r112",	"r113",	"r114",	"r115",
	"r116",	"r117",	"r118",	"r119",
	"r120",	"r121",	"r122",	"r123",
	"r124",	"r125",	"r126",	"r127",
};

const char *branch_names[] = {
	"rp", "b1", "b2", "b3", "b4", "b5", "b6", "b7"
};

const char *appreg_names[] = {
	"ar.k0",	"ar.k1",	"ar.k2",	"ar.k3",
	"ar.k4",	"ar.k5",	"ar.k6",	"ar.k7",
	"ar8",		"ar9",		"ar10",		"ar11",
	"ar12",		"ar13",		"ar14",		"ar15",
	"ar.rsc",	"ar.bsp",	"ar.bspstore",	"ar.rnat",
	"ar20",		"ar.fcr",	"ar22",		"ar23",
	"ar.eflag",	"ar.csd",	"ar.ssd",	"ar.cflg",
	"ar.fsr",	"ar.fir",	"ar.fdr",	"ar31",
	"ar.ccv",	"ar33",		"ar34",		"ar35",
	"ar.unat",	"ar37",		"ar38",		"ar39",
	"ar.fpsr",	"ar41",		"ar42",		"ar43",
	"ar.itc",	"ar45",		"ar46",		"ar47",
	"ar48",		"ar49",		"ar50",		"ar51",
	"ar52",		"ar53",		"ar54",		"ar55",
	"ar56",		"ar57",		"ar58",		"ar59",
	"ar60",		"ar61",		"ar62",		"ar63",
	"ar.pfs",	"ar.lc",	"ar.ec",	"ar67",
	"ar68",		"ar69",		"ar70",		"ar71",
	"ar72",		"ar73",		"ar74",		"ar75",
	"ar76",		"ar77",		"ar78",		"ar79",
	"ar80",		"ar81",		"ar82",		"ar83",
	"ar84",		"ar85",		"ar86",		"ar87",
	"ar88",		"ar89",		"ar90",		"ar91",
	"ar92",		"ar93",		"ar94",		"ar95",
	"ar96",		"ar97",		"ar98",		"ar99",
	"ar100",	"ar101",	"ar102",	"ar103",
	"ar104",	"ar105",	"ar106",	"ar107",
	"ar108",	"ar109",	"ar110",	"ar111",
	"ar112",	"ar113",	"ar114",	"ar115",
	"ar116",	"ar117",	"ar118",	"ar119",
	"ar120",	"ar121",	"ar122",	"ar123",
	"ar124",	"ar125",	"ar126",	"ar127",
};

const char *control_names[] = {
	"cr.dcr",	"cr.itm",	"cr.iva",	"cr3",
	"cr4",		"cr5",		"cr6",		"cr7",
	"cr.pta",	"cr9",		"cr10",		"cr11",
	"cr12",		"cr13",		"cr14",		"cr15",
	"cr.ipsr",	"cr.isr",	"cr18",		"cr.iip",
	"cr.ifa",	"cr.itir",	"cr.iipa",	"cr.ifs",
	"cr.iim",	"cr.iha",	"cr26",		"cr27",
	"cr28",		"cr29",		"cr30",		"cr31",
	"cr32",		"cr33",		"cr34",		"cr35",
	"cr36",		"cr37",		"cr38",		"cr39",
	"cr40",		"cr41",		"cr42",		"cr43",
	"cr44",		"cr45",		"cr46",		"cr47",
	"cr48",		"cr49",		"cr50",		"cr51",
	"cr52",		"cr53",		"cr54",		"cr55",
	"cr56",		"cr57",		"cr58",		"cr59",
	"cr60",		"cr61",		"cr62",		"cr63",
	"cr.lid",	"cr.ivr",	"cr.tpr",	"cr.eoi",
	"cr.irr0",	"cr.irr1",	"cr.irr2",	"cr.irr3",
	"cr.itv",	"cr.pmv",	"cr.cmcv",	"cr75",
	"cr76",		"cr77",		"cr78",		"cr79",
	"cr.lrr0",	"cr.lrr1",	"cr82",		"cr83",
	"cr84",		"cr85",		"cr86",		"cr87",
	"cr88",		"cr89",		"cr90",		"cr91",
	"cr92",		"cr93",		"cr94",		"cr95",
	"cr96",		"cr97",		"cr98",		"cr99",
	"cr100",	"cr101",	"cr102",	"cr103",
	"cr104",	"cr105",	"cr106",	"cr107",
	"cr108",	"cr109",	"cr110",	"cr111",
	"cr112",	"cr113",	"cr114",	"cr115",
	"cr116",	"cr117",	"cr118",	"cr119",
	"cr120",	"cr121",	"cr122",	"cr123",
	"cr124",	"cr125",	"cr126",	"cr127",
};

static void
ia64_print_ill(const char *name, u_int64_t ins, db_addr_t loc)
{
	db_printf("%s %lx", name, ins);
}

static void
ia64_print_A1(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%s",
		  name,
		  register_names[u.A1.r1],
		  register_names[u.A1.r2],
		  register_names[u.A1.r3]);
}

static void
ia64_print_A1_comma1(const char *name, u_int64_t ins, db_addr_t loc)
{
	ia64_print_A1(name, ins, loc);
	db_printf(",1");
}

static void
ia64_print_A2(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%d,%s",
		  name,
		  register_names[u.A2.r1],
		  register_names[u.A2.r2],
		  u.A2.ct2d + 1,
		  register_names[u.A2.r3]);
}

static void
ia64_print_A3(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	u.ins = ins;
	db_printf("%s %s=%ld,%s",
		  name,
		  register_names[u.A3.r1],
		  sign_extend((u.A3.s << 7) | u.A3.imm7b, 8),
		  register_names[u.A3.r3]);
}

static void
ia64_print_A4(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%ld,%s",
		  name,
		  register_names[u.A4.r1],
		  sign_extend((u.A4.s << 13) | (u.A4.imm6d << 7) | u.A4.imm7b, 14),
		  register_names[u.A4.r3]);
}

static void
ia64_print_A4_mov(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  register_names[u.A4.r1],
		  register_names[u.A4.r3]);
}

static void
ia64_print_A5(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%ld,%s",
		  name,
		  register_names[u.A5.r1],
		  sign_extend((u.A5.s << 21) | (u.A5.imm5c << 16)
			      | (u.A5.imm9d << 7) | u.A5.imm7b, 22),
		  register_names[u.A5.r3]);
}

static void
ia64_print_A6(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s p%d,p%d=%s,%s",
		  name,
		  u.A6.p1,
		  u.A6.p2,
		  register_names[u.A6.r2],
		  register_names[u.A6.r3]);
}

static void
ia64_print_A7(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s p%d,p%d=r0,%s",
		  name,
		  u.A7.p1,
		  u.A7.p2,
		  register_names[u.A7.r3]);
}

static void
ia64_print_A8(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s p%d,p%d=%ld,%s",
		  name,
		  u.A8.p1,
		  u.A8.p2,
		  sign_extend((u.A8.s << 7) | u.A8.imm7b, 8),
		  register_names[u.A8.r3]);
}

static void
ia64_print_A9(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%s",
		  name,
		  register_names[u.A9.r1],
		  register_names[u.A9.r2],
		  register_names[u.A9.r3]);
}

static void
ia64_print_A10(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%d,%s",
		  name,
		  register_names[u.A10.r1],
		  register_names[u.A10.r2],
		  u.A10.ct2d + 1,
		  register_names[u.A10.r3]);
}

static void
ia64_print_I1(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	static int count2[] = { 0, 7, 15, 16 };
	u.ins = ins;
	db_printf("%s %s=%s,%s,%d",
		  name,
		  register_names[u.I1.r1],
		  register_names[u.I1.r2],
		  register_names[u.I1.r3],
		  count2[u.I1.ct2d]);
}

static void
ia64_print_I2(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%s",
		  name,
		  register_names[u.I2.r1],
		  register_names[u.I2.r2],
		  register_names[u.I2.r3]);
}

static void
ia64_print_I3(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	static const char *mbtype4[] = {
		"@brcst",	/* 0 */
		"@reserved",	/* 1 */
		"@reserved",	/* 2 */
		"@reserved",	/* 3 */
		"@reserved",	/* 4 */
		"@reserved",	/* 5 */
		"@reserved",	/* 6 */
		"@reserved",	/* 7 */
		"@mix",		/* 8 */
		"@shuf",	/* 9 */
		"@alt",		/* 10 */
		"@rev",		/* 11 */
		"@reserved",	/* 12 */
		"@reserved",	/* 13 */
		"@reserved",	/* 14 */
		"@reserved",	/* 15 */
	};
	u.ins = ins;
	db_printf("%s %s=%s,%s",
		  name,
		  register_names[u.I3.r1],
		  register_names[u.I3.r2],
		  mbtype4[u.I3.mbt4c]);
}

static void
ia64_print_I4(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%d",
		  name,
		  register_names[u.I4.r1],
		  register_names[u.I4.r2],
		  u.I4.mht8c);
}

static void
ia64_print_I5(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%s",
		  name,
		  register_names[u.I5.r1],
		  register_names[u.I5.r3],
		  register_names[u.I5.r2]);
}

static void
ia64_print_I6(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%d",
		  name,
		  register_names[u.I6.r1],
		  register_names[u.I6.r3],
		  u.I6.count5b);
}

static void
ia64_print_I7(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%s",
		  name,
		  register_names[u.I7.r1],
		  register_names[u.I7.r2],
		  register_names[u.I7.r3]);
}

static void
ia64_print_I8(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%d",
		  name,
		  register_names[u.I8.r1],
		  register_names[u.I8.r2],
		  31 - u.I8.count5c);
}

static void
ia64_print_I9(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  register_names[u.I9.r1],
		  register_names[u.I9.r3]);
}

static void
ia64_print_I10(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%s,%d",
		  name,
		  register_names[u.I10.r1],
		  register_names[u.I10.r2],
		  register_names[u.I10.r3],
		  u.I10.count6d);
}

static void
ia64_print_I11(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%d,%d",
		  name,
		  register_names[u.I11.r1],
		  register_names[u.I11.r3],
		  u.I11.pos6b,
		  u.I11.len6d + 1);
}

static void
ia64_print_I12(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%d,%d",
		  name,
		  register_names[u.I12.r1],
		  register_names[u.I12.r2],
		  63 - u.I12.cpos6c,
		  u.I12.len6d + 1);
}

static void
ia64_print_I13(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%ld,%d,%d",
		  name,
		  register_names[u.I13.r1],
		  sign_extend((u.I13.s << 7) | u.I13.imm7b, 8),
		  63 - u.I13.cpos6c,
		  u.I13.len6d + 1);
}

static void
ia64_print_I14(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%ld,%s,%d,%d",
		  name,
		  register_names[u.I14.r1],
		  sign_extend(u.I14.s, 1),
		  register_names[u.I14.r3],
		  63 - u.I14.cpos6b,
		  u.I14.len6d + 1);
}

static void
ia64_print_I15(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%s,%d,%d",
		  name,
		  register_names[u.I15.r1],
		  register_names[u.I15.r2],
		  register_names[u.I15.r3],
		  63 - u.I15.cpos6d,
		  u.I15.len4d + 1);
}

static void
ia64_print_I16(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s p%d,p%d=%s,%d",
		  name,
		  u.I16.p1,
		  u.I16.p2,
		  register_names[u.I16.r3],
		  u.I16.pos6b);
}

static void
ia64_print_I17(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s p%d,p%d=%s",
		  name,
		  u.I17.p1,
		  u.I17.p2,
		  register_names[u.I17.r3]);
}

static void
ia64_print_I19(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s 0x%x",
		  name,
		  (u.I19.i << 20) | u.I19.imm20a);
}

static void
ia64_print_I20(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s,",
		  name,
		  register_names[u.I20.r2]);
	db_printsym(loc + (sign_extend((u.I20.s << 20)
				       | (u.I20.imm13c << 7)
				       | u.I20.imm7a, 21) << 4),
		    DB_STGY_PROC);
}

static void
ia64_print_I21(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	static const char *ih[] = { "", ".imp" };
	static const char *wh[] = { ".sptk", "", ".dptk", ".ill" };
	static const char *ret[] = { "", ".ret" };
	u.ins = ins;

	db_printf("%s%s%s%s %s=%s",
		  name,
		  ret[u.I21.x],
		  wh[u.I21.wh],
		  ih[u.I21.ih],
		  branch_names[u.I21.b1],
		  register_names[u.I21.r2]);
	if (u.I21.timm9c)
		db_printf(",%lx",
			  loc + (sign_extend(u.I21.timm9c, 9) << 4));
}

static void
ia64_print_I22(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  register_names[u.I22.r1],
		  branch_names[u.I22.b2]);
}

static void
ia64_print_I23(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s pr=%s,0x%lx",
		  name,
		  register_names[u.I23.r2],
		  sign_extend((u.I23.s << 16)
			      | (u.I23.mask8c << 8)
			      | (u.I23.mask7a << 1), 17));
}

static void
ia64_print_I24(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s pr.rot=%lx",
		  name,
		  sign_extend(((u_int64_t) u.I24.s << 43)
			      | (u.I24.imm27a << 16), 44));
}

static void
ia64_print_I25(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	if (u.I25.x6 == 0x30)
		db_printf("%s %s=ip",
			  name,
			  register_names[u.I25.r1]);
	else
		db_printf("%s %s=pr",
			  name,
			  register_names[u.I25.r1]);
}

static void
ia64_print_I26(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  appreg_names[u.I26.ar3],
		  register_names[u.I26.r2]);
}

static void
ia64_print_I27(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%lx",
		  name,
		  appreg_names[u.I27.ar3],
		  sign_extend((u.I27.s << 7) | u.I27.imm7b, 8));
}

static void
ia64_print_I28(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  register_names[u.I28.r1],
		  appreg_names[u.I28.ar3]);
}

static void
ia64_print_I29(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  register_names[u.I29.r1],
		  register_names[u.I29.r3]);
}

static void
ia64_print_M1(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=[%s]",
		  name,
		  register_names[u.M1.r1],
		  register_names[u.M1.r3]);
}

static void
ia64_print_M2(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=[%s],%s",
		  name,
		  register_names[u.M2.r1],
		  register_names[u.M2.r3],
		  register_names[u.M2.r2]);
}

static void
ia64_print_M3(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=[%s],%ld",
		  name,
		  register_names[u.M3.r1],
		  register_names[u.M3.r3],
		  sign_extend((u.M3.s << 8)
			      | (u.M3.i << 7)
			      | u.M3.imm7b, 9));
}

static void
ia64_print_M4(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s [%s]=%s",
		  name,
		  register_names[u.M4.r3],
		  register_names[u.M4.r2]);
}

static void
ia64_print_M5(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s [%s]=%s,%ld",
		  name,
		  register_names[u.M5.r3],
		  register_names[u.M5.r2],
		  sign_extend((u.M5.s << 8)
			      | (u.M5.i << 7)
			      | u.M5.imm7a, 9));
}

static void
ia64_print_M6(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d=[%s]",
		  name,
		  u.M6.f1,
		  register_names[u.M6.r3]);
}

static void
ia64_print_M7(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d=[%s],%s",
		  name,
		  u.M7.f1,
		  register_names[u.M7.r3],
		  register_names[u.M7.r2]);
}

static void
ia64_print_M8(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d=[%s],%ld",
		  name,
		  u.M8.f1,
		  register_names[u.M8.r3],
		  sign_extend((u.M8.s << 8)
			      | (u.M8.i << 7)
			      | u.M8.imm7b, 9));
}

static void
ia64_print_M9(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s [%s]=f%d",
		  name,
		  register_names[u.M9.r3],
		  u.M9.f2);
}

static void
ia64_print_M10(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s [%s]=f%d,%ld",
		  name,
		  register_names[u.M10.r3],
		  u.M10.f2,
		  sign_extend((u.M10.s << 8)
			      | (u.M10.i << 7)
			      | u.M10.imm7a, 9));
}

static void
ia64_print_M11(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d,f%d=[%s]",
		  name,
		  u.M11.f1,
		  u.M11.f2,
		  register_names[u.M11.r3]);
}

static void
ia64_print_M12(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	int imm;
	u.ins = ins;
	if ((u.M12.x6 & 3) == 2)
	    imm = 8;
	else
	    imm = 16;
	db_printf("%s f%d,f%d=[%s],%d",
		  name,
		  u.M12.f1,
		  u.M12.f2,
		  register_names[u.M12.r3],
		  imm);
}

static void
ia64_print_M13(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s [%s]",
		  name,
		  register_names[u.M14.r3]);
}

static void
ia64_print_M14(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s [%s],%s",
		  name,
		  register_names[u.M14.r3],
		  register_names[u.M14.r2]);
}

static void
ia64_print_M15(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s [%s],%ld",
		  name,
		  register_names[u.M15.r3],
		  sign_extend((u.M15.s << 8)
			      | (u.M15.i << 7)
			      | u.M15.imm7b, 9));
}

static void
ia64_print_M16(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	if (u.M16.x6 < 0x08)
		db_printf("%s %s=[%s],%s,ar.ccv",
			  name,
			  register_names[u.M16.r1],
			  register_names[u.M16.r3],
			  register_names[u.M16.r2]);
	else
		db_printf("%s %s=[%s],%s",
			  name,
			  register_names[u.M16.r1],
			  register_names[u.M16.r3],
			  register_names[u.M16.r2]);
}

static void
ia64_print_M17(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=[%s],%ld",
		  name,
		  register_names[u.M17.r1],
		  register_names[u.M17.r3],
		  sign_extend((u.M17.s << 2) | u.M17.i2b, 3));
}

static void
ia64_print_M18(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d=%s",
		  name,
		  u.M18.f1,
		  register_names[u.M18.r2]);
}

static void
ia64_print_M19(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=f%d",
		  name,
		  register_names[u.M19.r1],
		  u.M19.f2);
}

static void
ia64_print_M20(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s,",
		  name,
		  register_names[u.M20.r2]);
	db_printsym(loc + (sign_extend((u.M20.s << 20)
				       | (u.M20.imm13c << 7)
				       | u.M20.imm7a, 21) << 4),
		    DB_STGY_PROC);
}

static void
ia64_print_M21(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d,",
		  name,
		  u.M21.f2);
	db_printsym(loc + (sign_extend((u.M21.s << 20)
				       | (u.M21.imm13c << 7)
				       | u.M21.imm7a, 21) << 4),
		    DB_STGY_PROC);
}

static void
ia64_print_M22(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s,",
		  name,
		  register_names[u.M22.r1]);
	db_printsym(loc + (sign_extend((u.M22.s << 20)
				       | u.M22.imm20b, 21) << 4),
		    DB_STGY_PROC);
}

static void
ia64_print_M23(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d,",
		  name,
		  u.M23.f1);
	db_printsym(loc + (sign_extend((u.M23.s << 20)
				       | u.M23.imm20b, 21) << 4),
		    DB_STGY_PROC);
}

/* Also M25 */
static void
ia64_print_M24(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s", name);
}

static void
ia64_print_M26(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s",
		  name,
		  register_names[u.M26.r1]);
}

static void
ia64_print_M27(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d",
		  name,
		  u.M27.f1);
}

static void
ia64_print_M28(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s",
		  name,
		  register_names[u.M28.r3]);
}

static void
ia64_print_M29(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  appreg_names[u.M29.ar3],
		  register_names[u.M29.r2]);
}

static void
ia64_print_M30(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%ld",
		  name,
		  appreg_names[u.M30.ar3],
		  sign_extend((u.M30.s << 7) | u.M30.imm7b, 8));
}

static void
ia64_print_M31(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  register_names[u.M31.r1],
		  appreg_names[u.M31.ar3]);
}

static void
ia64_print_M32(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  control_names[u.M32.cr3],
		  register_names[u.M32.r2]);
}

static void
ia64_print_M33(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  register_names[u.M33.r1],
		  control_names[u.M33.cr3]);
}

static void
ia64_print_M34(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=ar.pfs,0,%d,%d,%d",
		  name,
		  register_names[u.M34.r1],
		  u.M34.sol,
		  u.M34.sof - u.M34.sol,
		  u.M34.sor << 3);
}

static void
ia64_print_M35(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	if (u.M35.x6 == 0x2D)
		db_printf("%s psr.l=%s",
			  name,
			  register_names[u.M35.r2]);
	else
		db_printf("%s psr.um=%s",
			  name,
			  register_names[u.M35.r2]);
}

static void
ia64_print_M36(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	if (u.M35.x6 == 0x25)
		db_printf("%s %s=psr",
			  name,
			  register_names[u.M36.r1]);
	else
		db_printf("%s %s=psr.um",
			  name,
			  register_names[u.M36.r1]);
}

static void
ia64_print_M37(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s 0x%x",
		  name,
		  (u.M37.i << 20) | u.M37.imm20a);
}

static void
ia64_print_M38(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%s",
		  name,
		  register_names[u.M38.r1],
		  register_names[u.M38.r3],
		  register_names[u.M38.r2]);
}

static void
ia64_print_M39(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s,%d",
		  name,
		  register_names[u.M39.r1],
		  register_names[u.M39.r3],
		  u.M39.i2b);
}

static void
ia64_print_M40(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s,%d",
		  name,
		  register_names[u.M40.r3],
		  u.M40.i2b);
}

static void
ia64_print_M41(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s",
		  name,
		  register_names[u.M41.r2]);
}

static void
ia64_print_M42(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	static const char *base[] = {
		"rr", "dbr", "ibr", "pkr", "pmc", "pmd", "dtr", "itr"
	};
	u.ins = ins;
	db_printf("%s %s[%s]=%s",
		  name,
		  base[u.M42.x6],
		  register_names[u.M42.r3],
		  register_names[u.M42.r2]);
}

static void
ia64_print_M43(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	static const char *base[] = {
		"rr", "dbr", "ibr", "pkr", "pmc", "pmd", "dtr", "itr"
	};
	u.ins = ins;
	db_printf("%s %s=%s[%s]",
		  name,
		  register_names[u.M43.r1],
		  base[u.M43.x6 & 0x7],
		  register_names[u.M43.r3]);
}

static void
ia64_print_M44(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s 0x%x",
		  name,
		  (u.M44.i << 23) | (u.M44.i2d << 21) | u.M44.imm21a);
}

static void
ia64_print_M45(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  register_names[u.M45.r3],
		  register_names[u.M45.r2]);
}

static void
ia64_print_M46(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s %s=%s",
		  name,
		  register_names[u.M46.r1],
		  register_names[u.M46.r3]);
}

static const char *ptable[] = { ".few", ".many" };
static const char *whtable[] = { ".sptk", ".spnt", ".dptk", ".dpnt" };
static const char *dtable[] = { "", ".clr" };
static const char *ihtable[] = { "", ".imp" };

static void
ia64_print_B1(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s%s%s ",
		  name,
		  whtable[u.B1.wh],
		  ptable[u.B1.p],
		  dtable[u.B1.d]);
	db_printsym(loc + (sign_extend((u.B1.s << 20)
				       | u.B1.imm20b, 21) << 4),
		    DB_STGY_PROC);
}

static void
ia64_print_B3(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s%s%s %s=",
		  name,
		  whtable[u.B3.wh],
		  ptable[u.B3.p],
		  dtable[u.B3.d],
		  branch_names[u.B3.b1]);
	db_printsym(loc + (sign_extend((u.B3.s << 20)
				       | u.B3.imm20b, 21) << 4),
		    DB_STGY_PROC);
}

static void
ia64_print_B4(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s%s%s %s",
		  name,
		  whtable[u.B4.wh],
		  ptable[u.B4.p],
		  dtable[u.B4.d],
		  branch_names[u.B4.b2]);
}

static void
ia64_print_B5(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s%s%s %s=%s",
		  name,
		  whtable[u.B5.wh],
		  ptable[u.B5.p],
		  dtable[u.B5.d],
		  branch_names[u.B5.b1],
		  branch_names[u.B5.b2]);
}

static void
ia64_print_B6(const char *name, u_int64_t ins, db_addr_t loc)
{
	static const char *whtable[] = { ".sptk", ".loop", ".dptk", ".exit" };
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s%s ",
		  name,
		  whtable[u.B6.wh],
		  ihtable[u.B6.ih]);
	db_printsym(loc + (sign_extend((u.B6.s << 20)
				       | u.B6.imm20b, 21) << 4),
		    DB_STGY_PROC);
	db_printf("%x", (u.B6.t2e << 7) | u.B6.timm7a);
}

static void
ia64_print_B7(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s%s %s,%x",
		  name,
		  whtable[u.B7.wh],
		  ihtable[u.B7.ih],
		  branch_names[u.B7.b2],
		  (u.B7.t2e << 7) | u.B7.timm7a);
}

static void
ia64_print_B8(const char *name, u_int64_t ins, db_addr_t loc)
{
	db_printf("%s", name);
}

static void
ia64_print_B9(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s 0x%x",
		  name,
		  (u.B9.i << 20) | u.B9.imm20a);
}

static const char *sftable[] = { ".s0", ".s1", ".s2", ".s3" };

static void
ia64_print_F1(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s f%d=f%d,f%d,f%d",
		  name,
		  sftable[u.F1.sf],
		  u.F1.f1,
		  u.F1.f3,
		  u.F1.f4,
		  u.F1.f2);
}

static void
ia64_print_F2(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d=f%d,f%d,f%d",
		  name,
		  u.F2.f1,
		  u.F2.f3,
		  u.F2.f4,
		  u.F2.f2);
}

static void
ia64_print_F3(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d=f%d,f%d,f%d",
		  name,
		  u.F3.f1,
		  u.F3.f3,
		  u.F3.f4,
		  u.F3.f2);
}

static void
ia64_print_F4(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s p%d,p%d=f%d,f%d",
		  name,
		  sftable[u.F4.sf],
		  u.F4.p1,
		  u.F4.p2,
		  u.F4.f2,
		  u.F4.f3);
}

static void
ia64_print_F5(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s p%d,p%d=f%d,0x%x",
		  name,
		  sftable[u.F4.sf],
		  u.F5.p1,
		  u.F5.p2,
		  u.F5.f2,
		  (u.F5.fclass7c << 2) | u.F5.fc2);
}

static void
ia64_print_F6(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s f%d,p%d=f%d,f%d",
		  name,
		  sftable[u.F6.sf],
		  u.F6.f1,
		  u.F6.p2,
		  u.F6.f2,
		  u.F6.f3);
}

static void
ia64_print_F7(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s f%d,p%d=f%d",
		  name,
		  sftable[u.F7.sf],
		  u.F7.f1,
		  u.F7.p2,
		  u.F7.f3);
}

static void
ia64_print_F8(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s f%d=f%d,f%d",
		  name,
		  sftable[u.F8.sf],
		  u.F8.f1,
		  u.F8.f2,
		  u.F8.f3);
}

static void
ia64_print_F9(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d=f%d,f%d",
		  name,
		  u.F9.f1,
		  u.F9.f2,
		  u.F9.f3);
}

static void
ia64_print_F10(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s f%d=f%d",
		  name,
		  sftable[u.F10.sf],
		  u.F10.f1,
		  u.F10.f2);
}

static void
ia64_print_F11(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s f%d=f%d",
		  name,
		  u.F11.f1,
		  u.F11.f2);
}

static void
ia64_print_F12(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s 0x%x,0x%x",
		  name,
		  sftable[u.F12.sf],
		  u.F12.amask7b,
		  u.F12.omask7c);
}

static void
ia64_print_F13(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s",
		  name,
		  sftable[u.F13.sf]);
}

static void
ia64_print_F14(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s%s ",
		  name,
		  sftable[u.F14.sf]);
	db_printsym(loc + (sign_extend((u.F14.s << 20)
				       | u.F14.imm20a, 21) << 4),
		    DB_STGY_PROC);
}

static void
ia64_print_F15(const char *name, u_int64_t ins, db_addr_t loc)
{
	union ia64_instruction u;
	u.ins = ins;
	db_printf("%s 0x%x",
		  name,
		  (u.F15.i << 20) | u.F15.imm20a);
}

static void
ia64_print_X1(const char *name, u_int64_t ins, db_addr_t loc)
{
	struct ia64_bundle b;
	union ia64_instruction u;
	u.ins = ins;
	db_read_bundle(loc, &b);
	db_printf("%s %lx",
		  name,
		  (b.slot[1] << 21) | (u.X1.i << 20) | u.X1.imm20a);
}

static void
ia64_print_X2(const char *name, u_int64_t ins, db_addr_t loc)
{
	struct ia64_bundle b;
	union ia64_instruction u;
	u.ins = ins;
	db_read_bundle(loc, &b);
	db_printf("%s %s=%lx",
		  name,
		  register_names[u.X2.r1],
		  (((long)u.X2.i << 63) | (b.slot[1] << 22) | (u.X2.ic << 21)
		   | (u.X2.imm5c << 16) | (u.X2.imm9d << 7) | u.X2.imm7b));
}

struct ia64_opcode {
	const char*	name;
	u_int64_t	value;
	u_int64_t	mask;
	void		(*print)(const char*, u_int64_t, db_addr_t loc);
};

#define field(n,s,e)	(((u_int64_t)(n) & ((1 << (e-s+1))-1)) << s)

#define OP(n)		field(n,37,40)
#define Za(n)		field(n,36,36)
#define Tb(n)		field(n,36,36)
#define S(n)		field(n,36,36)
#define X2a(n)		field(n,34,35)
#define X2(n)		field(n,34,35)
#define Ve(n)		field(n,33,33)
#define Zb(n)		field(n,33,33)
#define Ta(n)		field(n,33,33)
#define X4(n)		field(n,29,32)
#define X2b(n)		field(n,27,28)
#define I(n)		field(n,13,19)
#define C(n)		field(n,12,12)

#define mOP		OP(~0)
#define mZa		Za(~0)
#define mTb		Tb(~0)
#define mS		S(~0)
#define mX2a		X2a(~0)
#define mX2		X2(~0)
#define mVe		Ve(~0)
#define mZb		Zb(~0)
#define mTa		Ta(~0)
#define mX4		X4(~0)
#define mX2b		X2b(~0)
#define mI		I(~0)
#define mC		C(~0)

#define OPX2aVe(a,b,c) \
	OP(a)|X2a(b)|Ve(c),		mOP|mX2a|mVe
#define OPX2aVeSI(a,b,c,d,e) \
	OP(a)|X2a(b)|Ve(c)|S(d)|I(e),	mOP|mX2a|mVe|mS|mI
#define OPX2aVeX4X2b(a,b,c,d,e) \
	OP(a)|X2a(b)|Ve(c)|X4(d)|X2b(e), mOP|mX2a|mVe|mX4|mX2b
#define OPX2aVeX4(a,b,c,d) \
	OP(a)|X2a(b)|Ve(c)|X4(d),	mOP|mX2a|mVe|mX4
#define OPX2TbTaC(a,b,c,d,e) \
	OP(a)|X2(b)|Tb(c)|Ta(d)|C(e),	mOP|mX2|mTb|mTa|mC
#define OPX2TaC(a,b,c,d) \
	OP(a)|X2(b)|Ta(c)|C(d),		mOP|mX2|mTa|mC
#define OPX2aZaZbX4X2b(a,b,c,d,e,f) \
	OP(a)|X2a(b)|Za(c)|Zb(d)|X4(e)|X2b(f), mOP|mX2a|mZa|mZb|mX4|mX2b
#define OPX2aZaZbX4(a,b,c,d,e) \
	OP(a)|X2a(b)|Za(c)|Zb(d)|X4(e), mOP|mX2a|mZa|mZb|mX4
#define OPZaZbVeX2aX2bX2c(a,b,c,d,e,f,g) \
	OP(a)|Za(b)|Zb(c)|Ve(d)|X2a(e)|X2b(f)|X2c(g), \
	mOP|mZa|mZb|mVe|mX2a|mX2b|mX2c
#define OPZaZbVeX2aX2b(a,b,c,d,e,f) \
	OP(a)|Za(b)|Zb(c)|Ve(d)|X2a(e)|X2b(f), \
	mOP|mZa|mZb|mVe|mX2a|mX2b

static struct ia64_opcode A_opcodes[] = {

	/* Table 4-8 */
	{"mov",		OPX2aVeSI(8,2,0,0,0),		ia64_print_A4_mov},
	{"adds",	OPX2aVe(8,2,0),			ia64_print_A4},
	{"addp4",	OPX2aVe(8,3,0),			ia64_print_A4},

	/* Table 4-9 */
	{"add",		OPX2aVeX4X2b(8,0,0,0,0),	ia64_print_A1},
	{"add",		OPX2aVeX4X2b(8,0,0,0,1),	ia64_print_A1_comma1},
	{"sub",		OPX2aVeX4X2b(8,0,0,1,0),	ia64_print_A1_comma1},
	{"sub",		OPX2aVeX4X2b(8,0,0,1,1),	ia64_print_A1},
	{"addp4",	OPX2aVeX4X2b(8,0,0,2,0),	ia64_print_A1},
	{"and",		OPX2aVeX4X2b(8,0,0,3,0),	ia64_print_A1},
	{"andcm",	OPX2aVeX4X2b(8,0,0,3,1),	ia64_print_A1},
	{"or",		OPX2aVeX4X2b(8,0,0,3,2),	ia64_print_A1},
	{"xor",		OPX2aVeX4X2b(8,0,0,3,3),	ia64_print_A1},
	{"shladd",	OPX2aVeX4(8,0,0,4),		ia64_print_A2},
	{"shladdp4",	OPX2aVeX4(8,0,0,6),		ia64_print_A2},
	{"sub",		OPX2aVeX4X2b(8,0,0,9,1),	ia64_print_A3},
	{"and",		OPX2aVeX4X2b(8,0,0,11,0),	ia64_print_A3},
	{"andcm",	OPX2aVeX4X2b(8,0,0,11,1),	ia64_print_A3},
	{"or",		OPX2aVeX4X2b(8,0,0,11,2),	ia64_print_A3},
	{"xor",		OPX2aVeX4X2b(8,0,0,11,3),	ia64_print_A3},

	/* Section 4.2.1.5 */
	{"addl",	OP(9),mOP,			ia64_print_A5},

	/* Table 4-10 */
	{"cmp.lt",	OPX2TbTaC(12,0,0,0,0),		ia64_print_A6},
	{"cmp.lt.unc",	OPX2TbTaC(12,0,0,0,1),		ia64_print_A6},
	{"cmp.eq.and",	OPX2TbTaC(12,0,0,1,0),		ia64_print_A6},
	{"cmp.ne.and",	OPX2TbTaC(12,0,0,1,1),		ia64_print_A6},
	{"cmp.gt.and",	OPX2TbTaC(12,0,1,0,0),		ia64_print_A7},
	{"cmp.le.and",	OPX2TbTaC(12,0,1,0,1),		ia64_print_A7},
	{"cmp.ge.and",	OPX2TbTaC(12,0,1,1,0),		ia64_print_A7},
	{"cmp.lt.and",	OPX2TbTaC(12,0,1,1,1),		ia64_print_A7},
	{"cmp4.lt",	OPX2TbTaC(12,1,0,0,0),		ia64_print_A6},
	{"cmp4.lt.unc",	OPX2TbTaC(12,1,0,0,1),		ia64_print_A6},
	{"cmp4.eq.and",	OPX2TbTaC(12,1,0,1,0),		ia64_print_A6},
	{"cmp4.ne.and",	OPX2TbTaC(12,1,0,1,1),		ia64_print_A6},
	{"cmp4.gt.and",	OPX2TbTaC(12,1,1,0,0),		ia64_print_A7},
	{"cmp4.le.and",	OPX2TbTaC(12,1,1,0,1),		ia64_print_A7},
	{"cmp4.ge.and",	OPX2TbTaC(12,1,1,1,0),		ia64_print_A7},
	{"cmp4.lt.and",	OPX2TbTaC(12,1,1,1,1),		ia64_print_A7},
	{"cmp.ltu",	OPX2TbTaC(13,0,0,0,0),		ia64_print_A6},
	{"cmp.ltu.unc",	OPX2TbTaC(13,0,0,0,1),		ia64_print_A6},
	{"cmp.eq.or",	OPX2TbTaC(13,0,0,1,0),		ia64_print_A6},
	{"cmp.ne.or",	OPX2TbTaC(13,0,0,1,1),		ia64_print_A6},
	{"cmp.gt.or",	OPX2TbTaC(13,0,1,0,0),		ia64_print_A7},
	{"cmp.le.or",	OPX2TbTaC(13,0,1,0,1),		ia64_print_A7},
	{"cmp.ge.or",	OPX2TbTaC(13,0,1,1,0),		ia64_print_A7},
	{"cmp.lt.or",	OPX2TbTaC(13,0,1,1,1),		ia64_print_A7},
	{"cmp4.ltu",	OPX2TbTaC(13,1,0,0,0),		ia64_print_A6},
	{"cmp4.ltu.unc", OPX2TbTaC(13,1,0,0,1),		ia64_print_A6},
	{"cmp4.eq.or",	OPX2TbTaC(13,1,0,1,0),		ia64_print_A6},
	{"cmp4.ne.or",	OPX2TbTaC(13,1,0,1,1),		ia64_print_A6},
	{"cmp4.gt.or",	OPX2TbTaC(13,1,1,0,0),		ia64_print_A7},
	{"cmp4.le.or",	OPX2TbTaC(13,1,1,0,1),		ia64_print_A7},
	{"cmp4.ge.or",	OPX2TbTaC(13,1,1,1,0),		ia64_print_A7},
	{"cmp4.lt.or",	OPX2TbTaC(13,1,1,1,1),		ia64_print_A7},
	{"cmp.eq",	OPX2TbTaC(14,0,0,0,0),		ia64_print_A6},
	{"cmp.eq.unc",	OPX2TbTaC(14,0,0,0,1),		ia64_print_A6},
	{"cmp.eq.andcm", OPX2TbTaC(14,0,0,1,0),		ia64_print_A6},
	{"cmp.ne.andcm", OPX2TbTaC(14,0,0,1,1),		ia64_print_A6},
	{"cmp.gt.andcm", OPX2TbTaC(14,0,1,0,0),		ia64_print_A7},
	{"cmp.le.andcm", OPX2TbTaC(14,0,1,0,1),		ia64_print_A7},
	{"cmp.ge.andcm", OPX2TbTaC(14,0,1,1,0),		ia64_print_A7},
	{"cmp.lt.andcm", OPX2TbTaC(14,0,1,1,1),		ia64_print_A7},
	{"cmp4.eq",	OPX2TbTaC(14,1,0,0,0),		ia64_print_A6},
	{"cmp4.eq.unc",	OPX2TbTaC(14,1,0,0,1),		ia64_print_A6},
	{"cmp4.eq.andcm", OPX2TbTaC(14,1,0,1,0),	ia64_print_A6},
	{"cmp4.ne.andcm", OPX2TbTaC(14,1,0,1,1),	ia64_print_A6},
	{"cmp4.gt.andcm", OPX2TbTaC(14,1,1,0,0),	ia64_print_A7},
	{"cmp4.le.andcm", OPX2TbTaC(14,1,1,0,1),	ia64_print_A7},
	{"cmp4.ge.andcm", OPX2TbTaC(14,1,1,1,0),	ia64_print_A7},
	{"cmp4.lt.andcm", OPX2TbTaC(14,1,1,1,1),	ia64_print_A7},

	/* Table 4-11 */
	{"cmp.lt",	OPX2TaC(12,2,0,0),		ia64_print_A8},
	{"cmp.lt.unc",	OPX2TaC(12,2,0,1),		ia64_print_A8},
	{"cmp.eq.and",	OPX2TaC(12,2,1,0),		ia64_print_A8},
	{"cmp.ne.and",	OPX2TaC(12,2,1,1),		ia64_print_A8},
	{"cmp4.lt",	OPX2TaC(12,3,0,0),		ia64_print_A8},
	{"cmp4.lt.unc",	OPX2TaC(12,3,0,1),		ia64_print_A8},
	{"cmp4.eq.and",	OPX2TaC(12,3,1,0),		ia64_print_A8},
	{"cmp4.ne.and",	OPX2TaC(12,3,1,1),		ia64_print_A8},
	{"cmp.ltu",	OPX2TaC(13,2,0,0),		ia64_print_A8},
	{"cmp.ltu.unc",	OPX2TaC(13,2,0,1),		ia64_print_A8},
	{"cmp.eq.or",	OPX2TaC(13,2,1,0),		ia64_print_A8},
	{"cmp.ne.or",	OPX2TaC(13,2,1,1),		ia64_print_A8},
	{"cmp4.ltu",	OPX2TaC(13,3,0,0),		ia64_print_A8},
	{"cmp4.ltu.unc", OPX2TaC(13,3,0,1),		ia64_print_A8},
	{"cmp4.eq.or",	OPX2TaC(13,3,1,0),		ia64_print_A8},
	{"cmp4.ne.or",	OPX2TaC(13,3,1,1),		ia64_print_A8},
	{"cmp.eq",	OPX2TaC(14,2,0,0),		ia64_print_A8},
	{"cmp.eq.unc",	OPX2TaC(14,2,0,1),		ia64_print_A8},
	{"cmp.eq.or.andcm", OPX2TaC(14,2,1,0),		ia64_print_A8},
	{"cmp.ne.or.andcm", OPX2TaC(14,2,1,1),		ia64_print_A8},
	{"cmp4.eq",	OPX2TaC(14,3,0,0),		ia64_print_A8},
	{"cmp4.eq.unc", OPX2TaC(14,3,0,1),		ia64_print_A8},
	{"cmp4.eq.or.andcm", OPX2TaC(14,3,1,0),		ia64_print_A8},
	{"cmp4.ne.or.andcm", OPX2TaC(14,3,1,1),		ia64_print_A8},

	/* Table 4-13 */
	{"padd1",	OPX2aZaZbX4X2b(8,1,0,0,0,0),	ia64_print_A9},
	{"padd1.sss",	OPX2aZaZbX4X2b(8,1,0,0,0,1),	ia64_print_A9},
	{"padd1.uuu",	OPX2aZaZbX4X2b(8,1,0,0,0,2),	ia64_print_A9},
	{"padd1.uus",	OPX2aZaZbX4X2b(8,1,0,0,0,3),	ia64_print_A9},
	{"psub1",	OPX2aZaZbX4X2b(8,1,0,0,1,0),	ia64_print_A9},
	{"psub1.sss",	OPX2aZaZbX4X2b(8,1,0,0,1,1),	ia64_print_A9},
	{"psub1.uuu",	OPX2aZaZbX4X2b(8,1,0,0,1,2),	ia64_print_A9},
	{"psub1.uus",	OPX2aZaZbX4X2b(8,1,0,0,1,3),	ia64_print_A9},
	{"pavg1",	OPX2aZaZbX4X2b(8,1,0,0,2,2),	ia64_print_A9},
	{"pavg1.raz",	OPX2aZaZbX4X2b(8,1,0,0,2,3),	ia64_print_A9},
	{"pavgsub1",	OPX2aZaZbX4X2b(8,1,0,0,3,2),	ia64_print_A9},
	{"pcmp1.eq",	OPX2aZaZbX4X2b(8,1,0,0,9,0),	ia64_print_A9},
	{"pcmp1.gt",	OPX2aZaZbX4X2b(8,1,0,0,9,1),	ia64_print_A9},

	/* Table 4-14 */
	{"padd2",	OPX2aZaZbX4X2b(8,1,0,1,0,0),	ia64_print_A9},
	{"padd2.sss",	OPX2aZaZbX4X2b(8,1,0,1,0,1),	ia64_print_A9},
	{"padd2.uuu",	OPX2aZaZbX4X2b(8,1,0,1,0,2),	ia64_print_A9},
	{"padd2.uus",	OPX2aZaZbX4X2b(8,1,0,1,0,3),	ia64_print_A9},
	{"psub2",	OPX2aZaZbX4X2b(8,1,0,1,1,0),	ia64_print_A9},
	{"psub2.sss",	OPX2aZaZbX4X2b(8,1,0,1,1,1),	ia64_print_A9},
	{"psub2.uuu",	OPX2aZaZbX4X2b(8,1,0,1,1,2),	ia64_print_A9},
	{"psub2.uus",	OPX2aZaZbX4X2b(8,1,0,1,1,3),	ia64_print_A9},
	{"pavg2",	OPX2aZaZbX4X2b(8,1,0,1,2,2),	ia64_print_A9},
	{"pavg2.raz",	OPX2aZaZbX4X2b(8,1,0,1,2,3),	ia64_print_A9},
	{"pavgsub2",	OPX2aZaZbX4X2b(8,1,0,1,3,2),	ia64_print_A9},
	{"pshladd2",	OPX2aZaZbX4(8,1,0,1,4),		ia64_print_A10},
	{"pshradd2",	OPX2aZaZbX4(8,1,0,1,6),		ia64_print_A10},
	{"pcmp2.eq",	OPX2aZaZbX4X2b(8,1,0,1,9,0),	ia64_print_A9},
	{"pcmp2.gt",	OPX2aZaZbX4X2b(8,1,0,1,9,1),	ia64_print_A9},

	/* Table 4-15 */
	{"padd4",	OPX2aZaZbX4X2b(8,1,1,0,0,0),	ia64_print_A9},
	{"psub4",	OPX2aZaZbX4X2b(8,1,1,0,1,0),	ia64_print_A9},
	{"pcmp4.eq",	OPX2aZaZbX4X2b(8,1,1,0,9,0),	ia64_print_A9},
	{"pcmp4.gt",	OPX2aZaZbX4X2b(8,1,1,0,9,1),	ia64_print_A9},

	{"illegal.a",	0,0,				ia64_print_ill},
	{0},
};

#undef OP
#undef Za
#undef Tb
#undef S
#undef X2a
#undef X2
#undef Ve
#undef Zb
#undef Ta
#undef X4
#undef X2b
#undef C

#undef mOP
#undef mZa
#undef mTb
#undef mS
#undef mX2a
#undef mX2
#undef mVe
#undef mZb
#undef mTa
#undef mX4
#undef mX2b
#undef mC

#undef OPX2aVe
#undef OPX2aVeSI
#undef OPX2aVeX4X2b
#undef OPX2aVeX4
#undef OPX2TbTaC
#undef OPX2TaC
#undef OPX2aZaZbX4X2b
#undef OPX2aZaZbX4
#undef OPZaZbVeX2aX2bX2c
#undef OPZaZbVeX2aX2b

#define OP(n)		field(n,37,40)
#define Za(n)		field(n,36,36)
#define Tb(n)		field(n,36,36)
#define X2a(n)		field(n,34,35)
#define X2(n)		field(n,34,35)
#define X3(n)		field(n,33,35)
#define X6(n)		field(n,27,32)
#define Zb(n)		field(n,33,33)
#define X(n)		field(n,33,33)
#define Ta(n)		field(n,33,33)
#define Ve(n)		field(n,32,32)
#define X2c(n)		field(n,30,31)
#define X2b(n)		field(n,28,29)
#define YY(n)		field(n,26,26)
#define Y(n)		field(n,13,13)
#define C(n)		field(n,12,12)

#define mOP		OP(~0)
#define mZa		Za(~0)
#define mTb		Tb(~0)
#define mX2a		X2a(~0)
#define mX2		X2(~0)
#define mX3		X3(~0)
#define mX6		X6(~0)
#define mZb		Zb(~0)
#define mX		X(~0)
#define mTa		Ta(~0)
#define mVe		Ve(~0)
#define mX2c		X2c(~0)
#define mX2b		X2b(~0)
#define mYY		YY(~0)
#define mY		Y(~0)
#define mC		C(~0)

#define OPZaZbVeX2aX2bX2c(a,b,c,d,e,f,g) \
	OP(a)|Za(b)|Zb(c)|Ve(d)|X2a(e)|X2b(f)|X2c(g), \
	mOP|mZa|mZb|mVe|mX2a|mX2b|mX2c
#define OPZaZbVeX2aX2b(a,b,c,d,e,f) \
	OP(a)|Za(b)|Zb(c)|Ve(d)|X2a(e)|X2b(f), \
	mOP|mZa|mZb|mVe|mX2a|mX2b
#define OPX2XY(a,b,c,d) \
	OP(a)|X2(b)|X(c)|Y(d),		mOP|mX2|mX|mY
#define OPX2XYY(a,b,c,d) \
	OP(a)|X2(b)|X(c)|YY(d),		mOP|mX2|mX|mYY
#define OPX2X(a,b,c) \
	OP(a)|X2(b)|X(c),		mOP|mX2|mX
#define OPX2TaTbCY(a,b,c,d,e,f) \
	OP(a)|X2(b)|Ta(c)|Tb(d)|C(e)|Y(f), mOP|mX2|mTa|mTb|mC|mY
#define OPX3(a,b) \
	OP(a)|X3(b),			mOP|mX3
#define OPX3X6(a,b,c) \
	OP(a)|X3(b)|X6(c),		mOP|mX3|mX6

static struct ia64_opcode I_opcodes[] = {
	/* Table 4-17 */
	{"unpack1.h",	OPZaZbVeX2aX2bX2c(7,0,0,0,2,0,1), ia64_print_I2},
	{"mix1.r",	OPZaZbVeX2aX2bX2c(7,0,0,0,2,0,2), ia64_print_I2},
	{"pmin1.u",	OPZaZbVeX2aX2bX2c(7,0,0,0,2,1,0), ia64_print_I2},
	{"pmax2.u",	OPZaZbVeX2aX2bX2c(7,0,0,0,2,1,1), ia64_print_I2},
	{"unpack1.l",	OPZaZbVeX2aX2bX2c(7,0,0,0,2,2,1), ia64_print_I2},
	{"mix1.l",	OPZaZbVeX2aX2bX2c(7,0,0,0,2,2,2), ia64_print_I2},
	{"psad1",	OPZaZbVeX2aX2bX2c(7,0,0,0,2,3,2), ia64_print_I2},
	{"mux1",	OPZaZbVeX2aX2bX2c(7,0,0,0,3,2,2), ia64_print_I3},

	/* Table 4-18 */
	{"pshr2.u",	OPZaZbVeX2aX2bX2c(7,0,1,0,0,0,0), ia64_print_I5},
	{"pshl2",	OPZaZbVeX2aX2bX2c(7,0,1,0,0,0,1), ia64_print_I7},
	{"pmpyshr2.u",	OPZaZbVeX2aX2b(7,0,1,0,0,1),	ia64_print_I1},
	{"pshr2",	OPZaZbVeX2aX2bX2c(7,0,1,0,0,2,0), ia64_print_I5},
	{"pmpyshr2",	OPZaZbVeX2aX2b(7,0,1,0,0,3),	ia64_print_I1},
	{"pshr2.u",	OPZaZbVeX2aX2bX2c(7,0,1,0,1,1,0), ia64_print_I6},
	{"popcnt",	OPZaZbVeX2aX2bX2c(7,0,1,0,1,1,2), ia64_print_I9},
	{"pshr2",	OPZaZbVeX2aX2bX2c(7,0,1,0,1,3,0), ia64_print_I6},
	{"pack2.uss",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,0,0), ia64_print_I2},
	{"unpack2.h",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,0,1), ia64_print_I2},
	{"mix2.r",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,0,2), ia64_print_I2},
	{"pmpy2.r",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,1,3), ia64_print_I2},
	{"pack2.sss",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,2,0), ia64_print_I2},
	{"unpack2.l",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,2,1), ia64_print_I2},
	{"mix2.l",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,2,2), ia64_print_I2},
	{"pmin2",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,3,0), ia64_print_I2},
	{"pmax2",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,3,1), ia64_print_I2},
	{"pmpy2.l",	OPZaZbVeX2aX2bX2c(7,0,1,0,2,3,3), ia64_print_I2},
	{"pshl2",	OPZaZbVeX2aX2bX2c(7,0,1,0,3,1,1), ia64_print_I8},
	{"mux2",	OPZaZbVeX2aX2bX2c(7,0,1,0,3,2,2), ia64_print_I4},

	/* Table 4-19 */
	{"pshr4.u",	OPZaZbVeX2aX2bX2c(7,1,0,0,0,0,0), ia64_print_I5},
	{"pshl4",	OPZaZbVeX2aX2bX2c(7,1,0,0,0,0,1), ia64_print_I7},
	{"pshr4",	OPZaZbVeX2aX2bX2c(7,1,0,0,0,2,0), ia64_print_I5},
	{"pshr4.u",	OPZaZbVeX2aX2bX2c(7,1,0,0,1,1,0), ia64_print_I6},
	{"pshr4",	OPZaZbVeX2aX2bX2c(7,1,0,0,1,3,0), ia64_print_I6},
	{"unpack4.h",	OPZaZbVeX2aX2bX2c(7,1,0,0,2,0,1), ia64_print_I2},
	{"mix4.r",	OPZaZbVeX2aX2bX2c(7,1,0,0,2,0,2), ia64_print_I2},
	{"pack4.sss",	OPZaZbVeX2aX2bX2c(7,1,0,0,2,2,0), ia64_print_I2},
	{"unpack4.l",	OPZaZbVeX2aX2bX2c(7,1,0,0,2,2,1), ia64_print_I2},
	{"mix4.l",	OPZaZbVeX2aX2bX2c(7,1,0,0,2,2,2), ia64_print_I2},
	{"pshl4",	OPZaZbVeX2aX2bX2c(7,1,0,0,3,1,1), ia64_print_I8},

	/* Table 4-20 */
	{"shr.u",	OPZaZbVeX2aX2bX2c(7,1,1,0,0,0,0), ia64_print_I5},
	{"shl",		OPZaZbVeX2aX2bX2c(7,1,1,0,0,0,1), ia64_print_I7},
	{"shr",		OPZaZbVeX2aX2bX2c(7,1,1,0,0,2,0), ia64_print_I5},

	/* Table 4-21 */
	{"extr.u",	OPX2XY(5,1,0,0),		ia64_print_I11},
	{"extr",	OPX2XY(5,1,0,1),		ia64_print_I11},
	{"shrp",	OPX2X(5,3,0),			ia64_print_I10},

	/* Table 4-22 */
	{"dep.z",	OPX2XYY(5,1,1,0),		ia64_print_I12},
	{"dep.z",	OPX2XYY(5,1,1,1),		ia64_print_I13},
	{"dep",		OPX2X(5,3,1),			ia64_print_I14},

	/* Table 4-23 */
	{"tbit.z",	OPX2TaTbCY(5,0,0,0,0,0),	ia64_print_I16},
	{"tnat.z",	OPX2TaTbCY(5,0,0,0,0,1),	ia64_print_I17},
	{"tbit.z.unc",	OPX2TaTbCY(5,0,0,0,1,0),	ia64_print_I16},
	{"tnat.z.unc",	OPX2TaTbCY(5,0,0,0,1,1),	ia64_print_I17},
	{"tbit.z.and",	OPX2TaTbCY(5,0,0,1,0,0),	ia64_print_I16},
	{"tnat.z.and",	OPX2TaTbCY(5,0,0,1,0,1),	ia64_print_I17},
	{"tbit.nz.and",	OPX2TaTbCY(5,0,0,1,1,0),	ia64_print_I16},
	{"tnat.nz.and",	OPX2TaTbCY(5,0,0,1,1,1),	ia64_print_I17},
	{"tbit.z.or",	OPX2TaTbCY(5,0,1,0,0,0),	ia64_print_I16},
	{"tnat.z.or",	OPX2TaTbCY(5,0,1,0,0,1),	ia64_print_I17},
	{"tbit.nz.or",	OPX2TaTbCY(5,0,1,0,1,0),	ia64_print_I16},
	{"tnat.nz.or",	OPX2TaTbCY(5,0,1,0,1,1),	ia64_print_I17},
	{"tbit.z.or.andcm", OPX2TaTbCY(5,0,1,1,0,0),	ia64_print_I16},
	{"tnat.z.or.andcm", OPX2TaTbCY(5,0,1,1,0,1),	ia64_print_I17},
	{"tbit.nz.or.andcm", OPX2TaTbCY(5,0,1,1,1,0),	ia64_print_I16},
	{"tnat.nz.or.andcm", OPX2TaTbCY(5,0,1,1,1,1),	ia64_print_I17},

	/* Section 4.3.2.6 */
	{"dep",		OP(4),mOP,			ia64_print_I15},

	/* Table 4-24 */
	{"chk.s.i",	OPX3(0,1),			ia64_print_I20},
	{"mov",		OPX3(0,2),			ia64_print_I24},
	{"mov",		OPX3(0,3),			ia64_print_I23},
	{"mov",		OPX3(0,7),			ia64_print_I21},

	/* Table 4-25 */
	{"break.i",	OPX3X6(0,0,0),			ia64_print_I19},
	{"nop.i",	OPX3X6(0,0,1),			ia64_print_I19},
	{"mov.i",	OPX3X6(0,0,10),			ia64_print_I27},
	{"zxt1",	OPX3X6(0,0,16),			ia64_print_I29},
	{"zxt2",	OPX3X6(0,0,17),			ia64_print_I29},
	{"zxt4",	OPX3X6(0,0,18),			ia64_print_I29},
	{"sxt1",	OPX3X6(0,0,20),			ia64_print_I29},
	{"sxt2",	OPX3X6(0,0,21),			ia64_print_I29},
	{"sxt4",	OPX3X6(0,0,22),			ia64_print_I29},
	{"czx1.l",	OPX3X6(0,0,24),			ia64_print_I29},
	{"czx2.l",	OPX3X6(0,0,25),			ia64_print_I29},
	{"czx1.r",	OPX3X6(0,0,28),			ia64_print_I29},
	{"czx2.r",	OPX3X6(0,0,29),			ia64_print_I29},
	{"mov.i",	OPX3X6(0,0,42),			ia64_print_I26},
	{"mov",		OPX3X6(0,0,48),			ia64_print_I25},
	{"mov",		OPX3X6(0,0,49),			ia64_print_I22},
	{"mov.i",	OPX3X6(0,0,50),			ia64_print_I28},
	{"mov",		OPX3X6(0,0,51),			ia64_print_I25},

	{"illegal.i",	0,0,				ia64_print_ill},
	{0},
};

#undef OP
#undef Za
#undef Tb
#undef X2a
#undef X2
#undef X3
#undef X6
#undef Zb
#undef X
#undef Ta
#undef Ve
#undef X2c
#undef X2b
#undef YY
#undef Y
#undef C

#undef mOP
#undef mZa
#undef mTb
#undef mX2a
#undef mX2
#undef mX3
#undef mX6
#undef mZb
#undef mX
#undef mTa
#undef mVe
#undef mX2c
#undef mX2b
#undef mYY
#undef mY
#undef mC

#undef OPZaZbVeX2aX2bX2c
#undef OPZaZbVeX2aX2b
#undef OPX2XY
#undef OPX2XYY
#undef OPX2X
#undef OPX2TaTbCY
#undef OPX3
#undef OPX3X6

#define OP(n)		field(n,37,40)
#define M(n)		field(n,36,36)
#define X3(n)		field(n,33,35)
#define X6(n)		field(n,30,35)
#define X2(n)		field(n,31,32)
#define X4(n)		field(n,27,30)
#define X6a(n)		field(n,27,32)
#define X(n)		field(n,27,27)

#define mOP		OP(~0)
#define mM		M(~0)
#define mX3		X3(~0)
#define mX6		X6(~0)
#define mX2		X2(~0)
#define mX4		X4(~0)
#define mX6a		X6a(~0)
#define mX		X(~0)

#define OPMXX6(a,b,c,d) \
	OP(a)|M(b)|X(c)|X6(d),		mOP|mM|mX|mX6
#define OPX6(a,b) \
	OP(a)|X6(b),			mOP|mX6			
#define OPX3(a,b) \
	OP(a)|X3(b),			mOP|mX3
#define OPX3(a,b) \
	OP(a)|X3(b),			mOP|mX3
#define OPX3X4(a,b,c) \
	OP(a)|X3(b)|X4(c),		mOP|mX3|mX4
#define OPX3X4X2(a,b,c,d) \
	OP(a)|X3(b)|X4(c)|X2(d),	mOP|mX3|mX4|mX2
#define OPX3X6(a,b,c) \
	OP(a)|X3(b)|X6a(c),		mOP|mX3|mX6a

static struct ia64_opcode M_opcodes[] = {

	/* Table 4-29 */
	{"ld1",		OPMXX6(4,0,0,0x00),	ia64_print_M1},
	{"ld2",		OPMXX6(4,0,0,0x01),	ia64_print_M1},
	{"ld4",		OPMXX6(4,0,0,0x02),	ia64_print_M1},
	{"ld8",		OPMXX6(4,0,0,0x03),	ia64_print_M1},
	{"ld1.s",	OPMXX6(4,0,0,0x04),	ia64_print_M1},
	{"ld2.s",	OPMXX6(4,0,0,0x05),	ia64_print_M1},
	{"ld4.s",	OPMXX6(4,0,0,0x06),	ia64_print_M1},
	{"ld8.s",	OPMXX6(4,0,0,0x07),	ia64_print_M1},
	{"ld1.a",	OPMXX6(4,0,0,0x08),	ia64_print_M1},
	{"ld2.a",	OPMXX6(4,0,0,0x09),	ia64_print_M1},
	{"ld4.a",	OPMXX6(4,0,0,0x0a),	ia64_print_M1},
	{"ld8.a",	OPMXX6(4,0,0,0x0b),	ia64_print_M1},
	{"ld1.sa",	OPMXX6(4,0,0,0x0c),	ia64_print_M1},
	{"ld2.sa",	OPMXX6(4,0,0,0x0d),	ia64_print_M1},
	{"ld4.sa",	OPMXX6(4,0,0,0x0e),	ia64_print_M1},
	{"ld8.sa",	OPMXX6(4,0,0,0x0f),	ia64_print_M1},
	{"ld1.bias",	OPMXX6(4,0,0,0x10),	ia64_print_M1},
	{"ld2.bias",	OPMXX6(4,0,0,0x11),	ia64_print_M1},
	{"ld4.bias",	OPMXX6(4,0,0,0x12),	ia64_print_M1},
	{"ld8.bias",	OPMXX6(4,0,0,0x13),	ia64_print_M1},
	{"ld1.acq",	OPMXX6(4,0,0,0x14),	ia64_print_M1},
	{"ld2.acq",	OPMXX6(4,0,0,0x15),	ia64_print_M1},
	{"ld4.acq",	OPMXX6(4,0,0,0x16),	ia64_print_M1},
	{"ld8.acq",	OPMXX6(4,0,0,0x17),	ia64_print_M1},
	{"ld8.fill",	OPMXX6(4,0,0,0x1b),	ia64_print_M1},
	{"ld1.c.clr",	OPMXX6(4,0,0,0x20),	ia64_print_M1},
	{"ld2.c.clr",	OPMXX6(4,0,0,0x21),	ia64_print_M1},
	{"ld4.c.clr",	OPMXX6(4,0,0,0x22),	ia64_print_M1},
	{"ld8.c.clr",	OPMXX6(4,0,0,0x23),	ia64_print_M1},
	{"ld1.c.nc",	OPMXX6(4,0,0,0x24),	ia64_print_M1},
	{"ld2.c.nc",	OPMXX6(4,0,0,0x25),	ia64_print_M1},
	{"ld4.c.nc",	OPMXX6(4,0,0,0x26),	ia64_print_M1},
	{"ld8.c.nc",	OPMXX6(4,0,0,0x27),	ia64_print_M1},
	{"ld1.c.clr.acq", OPMXX6(4,0,0,0x28),	ia64_print_M1},
	{"ld2.c.clr.acq", OPMXX6(4,0,0,0x29),	ia64_print_M1},
	{"ld4.c.clr.acq", OPMXX6(4,0,0,0x2a),	ia64_print_M1},
	{"ld8.c.clr.acq", OPMXX6(4,0,0,0x2b),	ia64_print_M1},
	{"st1",		OPMXX6(4,0,0,0x30),	ia64_print_M4},
	{"st2",		OPMXX6(4,0,0,0x31),	ia64_print_M4},
	{"st4",		OPMXX6(4,0,0,0x32),	ia64_print_M4},
	{"st8",		OPMXX6(4,0,0,0x33),	ia64_print_M4},
	{"st1.rel",	OPMXX6(4,0,0,0x34),	ia64_print_M4},
	{"st2.rel",	OPMXX6(4,0,0,0x35),	ia64_print_M4},
	{"st4.rel",	OPMXX6(4,0,0,0x36),	ia64_print_M4},
	{"st8.rel",	OPMXX6(4,0,0,0x37),	ia64_print_M4},
	{"st8.spill",	OPMXX6(4,0,0,0x3b),	ia64_print_M4},

	/* Table 4-30 */
	{"ld1",		OPMXX6(4,1,0,0x00),	ia64_print_M2},
	{"ld2",		OPMXX6(4,1,0,0x01),	ia64_print_M2},
	{"ld4",		OPMXX6(4,1,0,0x02),	ia64_print_M2},
	{"ld8",		OPMXX6(4,1,0,0x03),	ia64_print_M2},
	{"ld1.s",	OPMXX6(4,1,0,0x04),	ia64_print_M2},
	{"ld2.s",	OPMXX6(4,1,0,0x05),	ia64_print_M2},
	{"ld4.s",	OPMXX6(4,1,0,0x06),	ia64_print_M2},
	{"ld8.s",	OPMXX6(4,1,0,0x07),	ia64_print_M2},
	{"ld1.a",	OPMXX6(4,1,0,0x08),	ia64_print_M2},
	{"ld2.a",	OPMXX6(4,1,0,0x09),	ia64_print_M2},
	{"ld4.a",	OPMXX6(4,1,0,0x0a),	ia64_print_M2},
	{"ld8.a",	OPMXX6(4,1,0,0x0b),	ia64_print_M2},
	{"ld1.sa",	OPMXX6(4,1,0,0x0c),	ia64_print_M2},
	{"ld2.sa",	OPMXX6(4,1,0,0x0d),	ia64_print_M2},
	{"ld4.sa",	OPMXX6(4,1,0,0x0e),	ia64_print_M2},
	{"ld8.sa",	OPMXX6(4,1,0,0x0f),	ia64_print_M2},
	{"ld1.bias",	OPMXX6(4,1,0,0x10),	ia64_print_M2},
	{"ld2.bias",	OPMXX6(4,1,0,0x11),	ia64_print_M2},
	{"ld4.bias",	OPMXX6(4,1,0,0x12),	ia64_print_M2},
	{"ld8.bias",	OPMXX6(4,1,0,0x13),	ia64_print_M2},
	{"ld1.acq",	OPMXX6(4,1,0,0x14),	ia64_print_M2},
	{"ld2.acq",	OPMXX6(4,1,0,0x15),	ia64_print_M2},
	{"ld4.acq",	OPMXX6(4,1,0,0x16),	ia64_print_M2},
	{"ld8.acq",	OPMXX6(4,1,0,0x17),	ia64_print_M2},
	{"ld8.fill",	OPMXX6(4,1,0,0x1b),	ia64_print_M2},
	{"ld1.c.clr",	OPMXX6(4,1,0,0x20),	ia64_print_M2},
	{"ld2.c.clr",	OPMXX6(4,1,0,0x21),	ia64_print_M2},
	{"ld4.c.clr",	OPMXX6(4,1,0,0x22),	ia64_print_M2},
	{"ld8.c.clr",	OPMXX6(4,1,0,0x23),	ia64_print_M2},
	{"ld1.c.nc",	OPMXX6(4,1,0,0x24),	ia64_print_M2},
	{"ld2.c.nc",	OPMXX6(4,1,0,0x25),	ia64_print_M2},
	{"ld4.c.nc",	OPMXX6(4,1,0,0x26),	ia64_print_M2},
	{"ld8.c.nc",	OPMXX6(4,1,0,0x27),	ia64_print_M2},
	{"ld1.c.clr.acq", OPMXX6(4,1,0,0x28),	ia64_print_M2},
	{"ld2.c.clr.acq", OPMXX6(4,1,0,0x29),	ia64_print_M2},
	{"ld4.c.clr.acq", OPMXX6(4,1,0,0x2a),	ia64_print_M2},
	{"ld8.c.clr.acq", OPMXX6(4,1,0,0x2b),	ia64_print_M2},

	/* Table 4-31 */
	{"ld1",		OPX6(5,0x00),		ia64_print_M3},
	{"ld2",		OPX6(5,0x01),		ia64_print_M3},
	{"ld4",		OPX6(5,0x02),		ia64_print_M3},
	{"ld8",		OPX6(5,0x03),		ia64_print_M3},
	{"ld1.s",	OPX6(5,0x04),		ia64_print_M3},
	{"ld2.s",	OPX6(5,0x05),		ia64_print_M3},
	{"ld4.s",	OPX6(5,0x06),		ia64_print_M3},
	{"ld8.s",	OPX6(5,0x07),		ia64_print_M3},
	{"ld1.a",	OPX6(5,0x08),		ia64_print_M3},
	{"ld2.a",	OPX6(5,0x09),		ia64_print_M3},
	{"ld4.a",	OPX6(5,0x0a),		ia64_print_M3},
	{"ld8.a",	OPX6(5,0x0b),		ia64_print_M3},
	{"ld1.sa",	OPX6(5,0x0c),		ia64_print_M3},
	{"ld2.sa",	OPX6(5,0x0d),		ia64_print_M3},
	{"ld4.sa",	OPX6(5,0x0e),		ia64_print_M3},
	{"ld8.sa",	OPX6(5,0x0f),		ia64_print_M3},
	{"ld1.bias",	OPX6(5,0x10),		ia64_print_M3},
	{"ld2.bias",	OPX6(5,0x11),		ia64_print_M3},
	{"ld4.bias",	OPX6(5,0x12),		ia64_print_M3},
	{"ld8.bias",	OPX6(5,0x13),		ia64_print_M3},
	{"ld1.acq",	OPX6(5,0x14),		ia64_print_M3},
	{"ld2.acq",	OPX6(5,0x15),		ia64_print_M3},
	{"ld4.acq",	OPX6(5,0x16),		ia64_print_M3},
	{"ld8.acq",	OPX6(5,0x17),		ia64_print_M3},
	{"ld8.fill",	OPX6(5,0x1b),		ia64_print_M3},
	{"ld1.c.clr",	OPX6(5,0x20),		ia64_print_M3},
	{"ld2.c.clr",	OPX6(5,0x21),		ia64_print_M3},
	{"ld4.c.clr",	OPX6(5,0x22),		ia64_print_M3},
	{"ld8.c.clr",	OPX6(5,0x23),		ia64_print_M3},
	{"ld1.c.nc",	OPX6(5,0x24),		ia64_print_M3},
	{"ld2.c.nc",	OPX6(5,0x25),		ia64_print_M3},
	{"ld4.c.nc",	OPX6(5,0x26),		ia64_print_M3},
	{"ld8.c.nc",	OPX6(5,0x27),		ia64_print_M3},
	{"ld1.c.clr.acq", OPX6(5,0x28),		ia64_print_M3},
	{"ld2.c.clr.acq", OPX6(5,0x29),		ia64_print_M3},
	{"ld4.c.clr.acq", OPX6(5,0x2a),		ia64_print_M3},
	{"ld8.c.clr.acq", OPX6(5,0x2b),		ia64_print_M3},
	{"st1",		OPX6(5,0x30),		ia64_print_M5},
	{"st2",		OPX6(5,0x31),		ia64_print_M5},
	{"st4",		OPX6(5,0x32),		ia64_print_M5},
	{"st8",		OPX6(5,0x33),		ia64_print_M5},
	{"st1.rel",	OPX6(5,0x34),		ia64_print_M5},
	{"st2.rel",	OPX6(5,0x35),		ia64_print_M5},
	{"st4.rel",	OPX6(5,0x36),		ia64_print_M5},
	{"st8.rel",	OPX6(5,0x37),		ia64_print_M5},
	{"st8.spill",	OPX6(5,0x3b),		ia64_print_M5},

	/* Table 4-32 */
	{"cmpchg1.acq",	OPMXX6(4,0,1,0x00),	ia64_print_M16},
	{"cmpchg2.acq",	OPMXX6(4,0,1,0x01),	ia64_print_M16},
	{"cmpchg4.acq",	OPMXX6(4,0,1,0x02),	ia64_print_M16},
	{"cmpchg8.acq",	OPMXX6(4,0,1,0x03),	ia64_print_M16},
	{"cmpchg1.rel",	OPMXX6(4,0,1,0x04),	ia64_print_M16},
	{"cmpchg2.rel",	OPMXX6(4,0,1,0x05),	ia64_print_M16},
	{"cmpchg4.rel",	OPMXX6(4,0,1,0x06),	ia64_print_M16},
	{"cmpchg8.rel",	OPMXX6(4,0,1,0x07),	ia64_print_M16},
	{"xchg1",	OPMXX6(4,0,1,0x08),	ia64_print_M16},
	{"xchg2",	OPMXX6(4,0,1,0x09),	ia64_print_M16},
	{"xchg4",	OPMXX6(4,0,1,0x0a),	ia64_print_M16},
	{"xchg8",	OPMXX6(4,0,1,0x0b),	ia64_print_M16},
	{"fetchadd4.acq", OPMXX6(4,0,1,0x12),	ia64_print_M17},
	{"fetchadd8.acq", OPMXX6(4,0,1,0x13),	ia64_print_M17},
	{"fetchadd4.rel", OPMXX6(4,0,1,0x16),	ia64_print_M17},
	{"fetchadd8.rel", OPMXX6(4,0,1,0x17),	ia64_print_M17},
	{"getf.sig",	OPMXX6(4,0,1,0x1c),	ia64_print_M19},
	{"getf.exp",	OPMXX6(4,0,1,0x1d),	ia64_print_M19},
	{"getf.s",	OPMXX6(4,0,1,0x1e),	ia64_print_M19},
	{"getf.d",	OPMXX6(4,0,1,0x1f),	ia64_print_M19},

	/* Table 4-33 */
	{"ldfe",	OPMXX6(6,0,0,0x00),	ia64_print_M6},
	{"ldf8",	OPMXX6(6,0,0,0x01),	ia64_print_M6},
	{"ldfs",	OPMXX6(6,0,0,0x02),	ia64_print_M6},
	{"ldfd",	OPMXX6(6,0,0,0x03),	ia64_print_M6},
	{"ldfe.s",	OPMXX6(6,0,0,0x04),	ia64_print_M6},
	{"ldf8.s",	OPMXX6(6,0,0,0x05),	ia64_print_M6},
	{"ldfs.s",	OPMXX6(6,0,0,0x06),	ia64_print_M6},
	{"ldfd.s",	OPMXX6(6,0,0,0x07),	ia64_print_M6},
	{"ldfe.a",	OPMXX6(6,0,0,0x08),	ia64_print_M6},
	{"ldf8.a",	OPMXX6(6,0,0,0x09),	ia64_print_M6},
	{"ldfs.a",	OPMXX6(6,0,0,0x0a),	ia64_print_M6},
	{"ldfd.a",	OPMXX6(6,0,0,0x0b),	ia64_print_M6},
	{"ldfe.sa",	OPMXX6(6,0,0,0x0c),	ia64_print_M6},
	{"ldf8.sa",	OPMXX6(6,0,0,0x0d),	ia64_print_M6},
	{"ldfs.sa",	OPMXX6(6,0,0,0x0e),	ia64_print_M6},
	{"ldfd.sa",	OPMXX6(6,0,0,0x0f),	ia64_print_M6},
	{"ldf.fill",	OPMXX6(6,0,0,0x1b),	ia64_print_M6},
	{"ldfe.c.clr",	OPMXX6(6,0,0,0x20),	ia64_print_M6},
	{"ldf8.c.clr",	OPMXX6(6,0,0,0x21),	ia64_print_M6},
	{"ldfs.c.clr",	OPMXX6(6,0,0,0x22),	ia64_print_M6},
	{"ldfd.c.clr",	OPMXX6(6,0,0,0x23),	ia64_print_M6},
	{"ldfe.c.nc",	OPMXX6(6,0,0,0x24),	ia64_print_M6},
	{"ldf8.c.nc",	OPMXX6(6,0,0,0x25),	ia64_print_M6},
	{"ldfs.c.nc",	OPMXX6(6,0,0,0x26),	ia64_print_M6},
	{"ldfd.c.nc",	OPMXX6(6,0,0,0x27),	ia64_print_M6},
	{"lfetch",	OPMXX6(6,0,0,0x2c),	ia64_print_M13},
	{"lfetch.excl",	OPMXX6(6,0,0,0x2d),	ia64_print_M13},
	{"lfetch.fault", OPMXX6(6,0,0,0x2e),	ia64_print_M13},
	{"lfetch.fault.excl", OPMXX6(6,0,0,0x2f), ia64_print_M13},
	{"stfe",	OPMXX6(6,0,0,0x30),	ia64_print_M9},
	{"stf8",	OPMXX6(6,0,0,0x31),	ia64_print_M9},
	{"stfs",	OPMXX6(6,0,0,0x32),	ia64_print_M9},
	{"stfd",	OPMXX6(6,0,0,0x33),	ia64_print_M9},
	{"stf.spill",	OPMXX6(6,0,0,0x3b),	ia64_print_M9},

	/* Table 4-34 */
	{"ldfe",	OPMXX6(6,1,0,0x00),	ia64_print_M7},
	{"ldf8",	OPMXX6(6,1,0,0x01),	ia64_print_M7},
	{"ldfs",	OPMXX6(6,1,0,0x02),	ia64_print_M7},
	{"ldfd",	OPMXX6(6,1,0,0x03),	ia64_print_M7},
	{"ldfe.s",	OPMXX6(6,1,0,0x04),	ia64_print_M7},
	{"ldf8.s",	OPMXX6(6,1,0,0x05),	ia64_print_M7},
	{"ldfs.s",	OPMXX6(6,1,0,0x06),	ia64_print_M7},
	{"ldfd.s",	OPMXX6(6,1,0,0x07),	ia64_print_M7},
	{"ldfe.a",	OPMXX6(6,1,0,0x08),	ia64_print_M7},
	{"ldf8.a",	OPMXX6(6,1,0,0x09),	ia64_print_M7},
	{"ldfs.a",	OPMXX6(6,1,0,0x0a),	ia64_print_M7},
	{"ldfd.a",	OPMXX6(6,1,0,0x0b),	ia64_print_M7},
	{"ldfe.sa",	OPMXX6(6,1,0,0x0c),	ia64_print_M7},
	{"ldf8.sa",	OPMXX6(6,1,0,0x0d),	ia64_print_M7},
	{"ldfs.sa",	OPMXX6(6,1,0,0x0e),	ia64_print_M7},
	{"ldfd.sa",	OPMXX6(6,1,0,0x0f),	ia64_print_M7},
	{"ldf.fill",	OPMXX6(6,1,0,0x1b),	ia64_print_M7},
	{"ldfe.c.clr",	OPMXX6(6,1,0,0x20),	ia64_print_M7},
	{"ldf8.c.clr",	OPMXX6(6,1,0,0x21),	ia64_print_M7},
	{"ldfs.c.clr",	OPMXX6(6,1,0,0x22),	ia64_print_M7},
	{"ldfd.c.clr",	OPMXX6(6,1,0,0x23),	ia64_print_M7},
	{"ldfe.c.nc",	OPMXX6(6,1,0,0x24),	ia64_print_M7},
	{"ldf8.c.nc",	OPMXX6(6,1,0,0x25),	ia64_print_M7},
	{"ldfs.c.nc",	OPMXX6(6,1,0,0x26),	ia64_print_M7},
	{"ldfd.c.nc",	OPMXX6(6,1,0,0x27),	ia64_print_M7},
	{"lfetch",	OPMXX6(6,1,0,0x2c),	ia64_print_M14},
	{"lfetch.excl",	OPMXX6(6,1,0,0x2d),	ia64_print_M14},
	{"lfetch.fault", OPMXX6(6,1,0,0x2e),	ia64_print_M14},
	{"lfetch.fault.excl", OPMXX6(6,1,0,0x2f), ia64_print_M14},

	/* Table 4-35 */
	{"ldfe",	OPX6(7,0x00),		ia64_print_M8},
	{"ldf8",	OPX6(7,0x01),		ia64_print_M8},
	{"ldfs",	OPX6(7,0x02),		ia64_print_M8},
	{"ldfd",	OPX6(7,0x03),		ia64_print_M8},
	{"ldfe.s",	OPX6(7,0x04),		ia64_print_M8},
	{"ldf8.s",	OPX6(7,0x05),		ia64_print_M8},
	{"ldfs.s",	OPX6(7,0x06),		ia64_print_M8},
	{"ldfd.s",	OPX6(7,0x07),		ia64_print_M8},
	{"ldfe.a",	OPX6(7,0x08),		ia64_print_M8},
	{"ldf8.a",	OPX6(7,0x09),		ia64_print_M8},
	{"ldfs.a",	OPX6(7,0x0a),		ia64_print_M8},
	{"ldfd.a",	OPX6(7,0x0b),		ia64_print_M8},
	{"ldfe.sa",	OPX6(7,0x0c),		ia64_print_M8},
	{"ldf8.sa",	OPX6(7,0x0d),		ia64_print_M8},
	{"ldfs.sa",	OPX6(7,0x0e),		ia64_print_M8},
	{"ldfd.sa",	OPX6(7,0x0f),		ia64_print_M8},
	{"ldf.fill",	OPX6(7,0x1b),		ia64_print_M8},
	{"ldfe.c.clr",	OPX6(7,0x20),		ia64_print_M8},
	{"ldf8.c.clr",	OPX6(7,0x21),		ia64_print_M8},
	{"ldfs.c.clr",	OPX6(7,0x22),		ia64_print_M8},
	{"ldfd.c.clr",	OPX6(7,0x23),		ia64_print_M8},
	{"ldfe.c.nc",	OPX6(7,0x24),		ia64_print_M8},
	{"ldf8.c.nc",	OPX6(7,0x25),		ia64_print_M8},
	{"ldfs.c.nc",	OPX6(7,0x26),		ia64_print_M8},
	{"ldfd.c.nc",	OPX6(7,0x27),		ia64_print_M8},
	{"lfetch",	OPX6(7,0x2c),		ia64_print_M15},
	{"lfetch.excl",	OPX6(7,0x2d),		ia64_print_M15},
	{"lfetch.fault", OPX6(7,0x2e),		ia64_print_M15},
	{"lfetch.fault.excl", OPX6(7,0x2f),	ia64_print_M15},
	{"stfe",	OPX6(7,0x30),		ia64_print_M10},
	{"stf8",	OPX6(7,0x31),		ia64_print_M10},
	{"stfs",	OPX6(7,0x32),		ia64_print_M10},
	{"stfd",	OPX6(7,0x33),		ia64_print_M10},
	{"stf.spill",	OPX6(7,0x3b),		ia64_print_M10},

	/* Table 4-36 */
	{"ldfp8",	OPMXX6(6,0,1,0x01),	ia64_print_M11},
	{"ldfps",	OPMXX6(6,0,1,0x02),	ia64_print_M11},
	{"ldfpd",	OPMXX6(6,0,1,0x03),	ia64_print_M11},
	{"ldfp8.s",	OPMXX6(6,0,1,0x05),	ia64_print_M11},
	{"ldfps.s",	OPMXX6(6,0,1,0x06),	ia64_print_M11},
	{"ldfpd.s",	OPMXX6(6,0,1,0x07),	ia64_print_M11},
	{"ldfp8.a",	OPMXX6(6,0,1,0x09),	ia64_print_M11},
	{"ldfps.a",	OPMXX6(6,0,1,0x0a),	ia64_print_M11},
	{"ldfpd.a",	OPMXX6(6,0,1,0x0b),	ia64_print_M11},
	{"ldfp8.sa",	OPMXX6(6,0,1,0x0d),	ia64_print_M11},
	{"ldfps.sa",	OPMXX6(6,0,1,0x0e),	ia64_print_M11},
	{"ldfpd.sa",	OPMXX6(6,0,1,0x0f),	ia64_print_M11},
	{"setf.sig",	OPMXX6(6,0,1,0x1c),	ia64_print_M18},
	{"setf.exp",	OPMXX6(6,0,1,0x1d),	ia64_print_M18},
	{"setf.s",	OPMXX6(6,0,1,0x1e),	ia64_print_M18},
	{"setf.d",	OPMXX6(6,0,1,0x1f),	ia64_print_M18},
	{"ldfp8.c.clr",	OPMXX6(6,0,1,0x21),	ia64_print_M11},
	{"ldfps.c.clr",	OPMXX6(6,0,1,0x22),	ia64_print_M11},
	{"ldfpd.c.clr",	OPMXX6(6,0,1,0x23),	ia64_print_M11},
	{"ldfp8.c.nc",	OPMXX6(6,0,1,0x25),	ia64_print_M11},
	{"ldfps.c.nc",	OPMXX6(6,0,1,0x26),	ia64_print_M11},
	{"ldfpd.c.nc",	OPMXX6(6,0,1,0x27),	ia64_print_M11},

	/* Table 4-37 */
	{"ldfp8",	OPMXX6(6,1,1,0x01),	ia64_print_M12},
	{"ldfps",	OPMXX6(6,1,1,0x02),	ia64_print_M12},
	{"ldfpd",	OPMXX6(6,1,1,0x03),	ia64_print_M12},
	{"ldfp8.s",	OPMXX6(6,1,1,0x05),	ia64_print_M12},
	{"ldfps.s",	OPMXX6(6,1,1,0x06),	ia64_print_M12},
	{"ldfpd.s",	OPMXX6(6,1,1,0x07),	ia64_print_M12},
	{"ldfp8.a",	OPMXX6(6,1,1,0x09),	ia64_print_M12},
	{"ldfps.a",	OPMXX6(6,1,1,0x0a),	ia64_print_M12},
	{"ldfpd.a",	OPMXX6(6,1,1,0x0b),	ia64_print_M12},
	{"ldfp8.sa",	OPMXX6(6,1,1,0x0d),	ia64_print_M12},
	{"ldfps.sa",	OPMXX6(6,1,1,0x0e),	ia64_print_M12},
	{"ldfpd.sa",	OPMXX6(6,1,1,0x0f),	ia64_print_M12},
	{"ldfp8.c.clr",	OPMXX6(6,1,1,0x21),	ia64_print_M12},
	{"ldfps.c.clr",	OPMXX6(6,1,1,0x22),	ia64_print_M12},
	{"ldfpd.c.clr",	OPMXX6(6,1,1,0x23),	ia64_print_M12},
	{"ldfp8.c.nc",	OPMXX6(6,1,1,0x25),	ia64_print_M12},
	{"ldfps.c.nc",	OPMXX6(6,1,1,0x26),	ia64_print_M12},
	{"ldfpd.c.nc",	OPMXX6(6,1,1,0x27),	ia64_print_M12},

	/* Table 4-41 */
	{"chk.a.nc",	OPX3(0,4),		ia64_print_M22},
	{"chk.a.clr",	OPX3(0,4),		ia64_print_M22},
	{"chk.a.nc",	OPX3(0,4),		ia64_print_M23},
	{"chk.a.clr",	OPX3(0,4),		ia64_print_M23},

	/* Table 4-42 */
	{"break.m",	OPX3X4X2(0,0,0,0),	ia64_print_M37},
	{"invala",	OPX3X4X2(0,0,0,1),	ia64_print_M24},
	{"fwb",		OPX3X4X2(0,0,0,2),	ia64_print_M24},
	{"srlz.d",	OPX3X4X2(0,0,0,3),	ia64_print_M24},
	{"nop.m",	OPX3X4X2(0,0,1,0),	ia64_print_M37},
	{"srlz.i",	OPX3X4X2(0,0,1,3),	ia64_print_M24},
	{"invala.e",	OPX3X4X2(0,0,2,1),	ia64_print_M26},
	{"mf",		OPX3X4X2(0,0,2,2),	ia64_print_M24},
	{"invala.e",	OPX3X4X2(0,0,3,1),	ia64_print_M27},
	{"mf.a",	OPX3X4X2(0,0,3,2),	ia64_print_M24},
	{"sync.i",	OPX3X4X2(0,0,3,3),	ia64_print_M24},
	{"sum",		OPX3X4(0,0,4),		ia64_print_M44},
	{"rum",		OPX3X4(0,0,5),		ia64_print_M44},
	{"ssm",		OPX3X4(0,0,6),		ia64_print_M44},
	{"rsm",		OPX3X4(0,0,7),		ia64_print_M44},
	{"mov.m",	OPX3X4X2(0,0,8,2),	ia64_print_M30},
	{"loadrs",	OPX3X4X2(0,0,10,0),	ia64_print_M24}, /* M25 */
	{"flushrs",	OPX3X4X2(0,0,12,0),	ia64_print_M24}, /* M25 */

	/* Table 4-43 */
	{"chk.s.m",	OPX3(1,1),		ia64_print_M20},
	{"chk.s",	OPX3(1,3),		ia64_print_M21},
	{"alloc",	OPX3(1,6),		ia64_print_M34},

	/* Table 4-44 */
	{"mov",		OPX3X6(1,0,0x00),	ia64_print_M42},
	{"mov",		OPX3X6(1,0,0x01),	ia64_print_M42},
	{"mov",		OPX3X6(1,0,0x02),	ia64_print_M42},
	{"mov",		OPX3X6(1,0,0x03),	ia64_print_M42},
	{"mov",		OPX3X6(1,0,0x04),	ia64_print_M42},
	{"mov",		OPX3X6(1,0,0x05),	ia64_print_M42},
	{"ptc.l",	OPX3X6(1,0,0x09),	ia64_print_M45},
	{"ptc.g",	OPX3X6(1,0,0x0a),	ia64_print_M45},
	{"ptc.ga",	OPX3X6(1,0,0x0b),	ia64_print_M45},
	{"ptr.d",	OPX3X6(1,0,0x0c),	ia64_print_M45},
	{"ptr.i",	OPX3X6(1,0,0x0d),	ia64_print_M45},
	{"itr.d",	OPX3X6(1,0,0x0e),	ia64_print_M42},
	{"itr.i",	OPX3X6(1,0,0x0f),	ia64_print_M41},
	{"mov",		OPX3X6(1,0,0x10),	ia64_print_M43},
	{"mov",		OPX3X6(1,0,0x11),	ia64_print_M43},
	{"mov",		OPX3X6(1,0,0x12),	ia64_print_M43},
	{"mov",		OPX3X6(1,0,0x13),	ia64_print_M43},
	{"mov",		OPX3X6(1,0,0x14),	ia64_print_M43},
	{"mov",		OPX3X6(1,0,0x15),	ia64_print_M43},
	{"mov",		OPX3X6(1,0,0x17),	ia64_print_M43},
	{"probe.r",	OPX3X6(1,0,0x18),	ia64_print_M39},
	{"probe.w",	OPX3X6(1,0,0x19),	ia64_print_M39},
	{"thash",	OPX3X6(1,0,0x1a),	ia64_print_M46},
	{"ttag",	OPX3X6(1,0,0x1b),	ia64_print_M46},
	{"tpa",		OPX3X6(1,0,0x1e),	ia64_print_M46},
	{"tak",		OPX3X6(1,0,0x1f),	ia64_print_M46},
	{"mov",		OPX3X6(1,0,0x21),	ia64_print_M36},
	{"mov.m",	OPX3X6(1,0,0x22),	ia64_print_M31},
	{"mov",		OPX3X6(1,0,0x24),	ia64_print_M33},
	{"mov",		OPX3X6(1,0,0x25),	ia64_print_M36},
	{"mov",		OPX3X6(1,0,0x29),	ia64_print_M35},
	{"mov.m",	OPX3X6(1,0,0x2a),	ia64_print_M29},
	{"mov",		OPX3X6(1,0,0x2c),	ia64_print_M32},
	{"mov",		OPX3X6(1,0,0x2d),	ia64_print_M35},
	{"mov",		OPX3X6(1,0,0x2e),	ia64_print_M41},
	{"mov",		OPX3X6(1,0,0x2f),	ia64_print_M41},
	{"fc",		OPX3X6(1,0,0x30),	ia64_print_M28},
	{"probe.rw.fault", OPX3X6(1,0,0x31),	ia64_print_M40},
	{"probe.r.fault", OPX3X6(1,0,0x32),	ia64_print_M40},
	{"probe.w.fault", OPX3X6(1,0,0x33),	ia64_print_M40},
	{"ptc.e",	OPX3X6(1,0,0x34),	ia64_print_M28},
	{"probe.r",	OPX3X6(1,0,0x38),	ia64_print_M38},
	{"probe.w",	OPX3X6(1,0,0x39),	ia64_print_M38},

	{"illegal.m",	0,0,			ia64_print_ill},
	{0},
};

#undef OP
#undef M
#undef X
#undef X3
#undef X6
#undef X2
#undef X4

#undef mOP
#undef mM
#undef mX
#undef mX3
#undef mX6
#undef mX2
#undef mX4

#undef OPMXX6
#undef OPX3
#undef OPX6
#undef OPX3
#undef OPX3X4
#undef OPX3X4X2
#undef OPX3X6

#define OP(n)		field(n,37,40)
#define Q(n)		field(n,36,36)
#define Rb(n)		field(n,36,36)
#define X2(n)		field(n,34,35)
#define X(n)		field(n,33,33)
#define Ra(n)		field(n,33,33)
#define X6(n)		field(n,27,32)
#define Ta(n)		field(n,12,12)

#define mOP		OP(~0)
#define mQ		Q(~0)
#define mRb		Rb(~0)
#define mX2		X2(~0)
#define mX		X(~0)
#define mRa		Ra(~0)
#define mX6		X6(~0)
#define mTa		Ta(~0)

#define OPXX6(a,b,c) \
	OP(a)|X(b)|X6(c),		mOP|mX|mX6
#define OPXQ(a,b,c) \
	OP(a)|X(b)|Q(c),		mOP|mX|mQ
#define OPX(a,b) \
	OP(a)|X(b),			mOP|mX
#define OPXX2(a,b,c) \
	OP(a)|X(b)|X2(c),		mOP|mX|mX2
#define OPRaRbTa(a,b,c,d) \
	OP(a)|Ra(b)|Rb(c)|Ta(d),	mOP|mRa|mRb|mTa
#define OPTa(a,b) \
	OP(a)|Ta(b),			mOP|mTa

static struct ia64_opcode F_opcodes[] = {

	/* Table 4-58 */
	{"break.f",	OPXX6(0,0,0x00),	ia64_print_F15},
	{"nop.f",	OPXX6(0,0,0x01),	ia64_print_F15},
	{"fsetc",	OPXX6(0,0,0x04),	ia64_print_F12},
	{"fclrf",	OPXX6(0,0,0x05),	ia64_print_F13},
	{"fchkf",	OPXX6(0,0,0x08),	ia64_print_F14},
	{"fmerge.s",	OPXX6(0,0,0x10),	ia64_print_F9},
	{"fmerge.ns",	OPXX6(0,0,0x10),	ia64_print_F9},
	{"fmerge.se",	OPXX6(0,0,0x10),	ia64_print_F9},
	{"fmin",	OPXX6(0,0,0x14),	ia64_print_F8},
	{"fmax",	OPXX6(0,0,0x15),	ia64_print_F8},
	{"famin",	OPXX6(0,0,0x16),	ia64_print_F8},
	{"famax",	OPXX6(0,0,0x17),	ia64_print_F8},
	{"fcvt.fx",	OPXX6(0,0,0x18),	ia64_print_F10},
	{"fcvt.fxu",	OPXX6(0,0,0x19),	ia64_print_F10},
	{"fcvt.fx.trunc", OPXX6(0,0,0x1a),	ia64_print_F10},
	{"fcvt.fxu.trunc", OPXX6(0,0,0x1b),	ia64_print_F10},
	{"fcvt.xf",	OPXX6(0,0,0x1c),	ia64_print_F11},
	{"fpack",	OPXX6(0,0,0x28),	ia64_print_F9},
	{"fand",	OPXX6(0,0,0x2c),	ia64_print_F9},
	{"fandcm",	OPXX6(0,0,0x2d),	ia64_print_F9},
	{"for",		OPXX6(0,0,0x2e),	ia64_print_F9},
	{"fxor",	OPXX6(0,0,0x2f),	ia64_print_F9},
	{"fswap",	OPXX6(0,0,0x34),	ia64_print_F9},
	{"fswap.nl",	OPXX6(0,0,0x35),	ia64_print_F9},
	{"fswap.nr",	OPXX6(0,0,0x36),	ia64_print_F9},
	{"fmix.lr",	OPXX6(0,0,0x39),	ia64_print_F9},
	{"fmix.r",	OPXX6(0,0,0x3a),	ia64_print_F9},
	{"fmix.l",	OPXX6(0,0,0x3b),	ia64_print_F9},
	{"fsxt.r",	OPXX6(0,0,0x3c),	ia64_print_F9},
	{"fsxt.l",	OPXX6(0,0,0x3d),	ia64_print_F9},

	/* Table 4-59 */
	{"fpmerge.s",	OPXX6(1,0,0x10),	ia64_print_F9},
	{"fpmerge.ns",	OPXX6(1,0,0x11),	ia64_print_F9},
	{"fpmerge.se",	OPXX6(1,0,0x12),	ia64_print_F9},
	{"fpmin",	OPXX6(1,0,0x14),	ia64_print_F8},
	{"fpmax",	OPXX6(1,0,0x15),	ia64_print_F8},
	{"fpamin",	OPXX6(1,0,0x16),	ia64_print_F8},
	{"fpamax",	OPXX6(1,0,0x17),	ia64_print_F8},
	{"fpcvt.fx",	OPXX6(1,0,0x18),	ia64_print_F10},
	{"fpcvt.fxu",	OPXX6(1,0,0x19),	ia64_print_F10},
	{"fpcvt.fx.trunc", OPXX6(1,0,0x1a),	ia64_print_F10},
	{"fpcvt.fxu.trunc", OPXX6(1,0,0x1b),	ia64_print_F10},
	{"fpcmp.eq",	OPXX6(1,0,0x30),	ia64_print_F8},
	{"fpcmp.lt",	OPXX6(1,0,0x31),	ia64_print_F8},
	{"fpcmp.le",	OPXX6(1,0,0x32),	ia64_print_F8},
	{"fpcmp.unord",	OPXX6(1,0,0x33),	ia64_print_F8},
	{"fpcmp.neq",	OPXX6(1,0,0x34),	ia64_print_F8},
	{"fpcmp.nlt",	OPXX6(1,0,0x35),	ia64_print_F8},
	{"fpcmp.nle",	OPXX6(1,0,0x36),	ia64_print_F8},
	{"fpcmp.ord",	OPXX6(1,0,0x37),	ia64_print_F8},

	/* Table 4-60 */
	{"frcpa",	OPXQ(0,1,0),		ia64_print_F6},
	{"frsqrta",	OPXQ(0,1,1),		ia64_print_F7},
	{"fprcpa",	OPXQ(1,1,0),		ia64_print_F6},
	{"fprsqrta",	OPXQ(1,1,1),		ia64_print_F7},

	/* Table 4-62 */
	{"fma",		OPX(8,0),		ia64_print_F1},
	{"fma.s",	OPX(8,1),		ia64_print_F1},
	{"fma.d",	OPX(9,0),		ia64_print_F1},
	{"fpma",	OPX(9,1),		ia64_print_F1},
	{"fms",		OPX(10,0),		ia64_print_F1},
	{"fms.s",	OPX(10,1),		ia64_print_F1},
	{"fms.d",	OPX(11,0),		ia64_print_F1},
	{"fpms",	OPX(11,1),		ia64_print_F1},
	{"fnma",	OPX(12,0),		ia64_print_F1},
	{"fnma.s",	OPX(12,1),		ia64_print_F1},
	{"fnma.d",	OPX(13,0),		ia64_print_F1},
	{"fpnma",	OPX(13,1),		ia64_print_F1},

	/* Table 4-63 */
	{"fselect",	OPX(14,0),		ia64_print_F3},
	{"xma.l",	OPXX2(14,1,0),		ia64_print_F2},
	{"xma.hu",	OPXX2(14,1,2),		ia64_print_F2},
	{"xma.h",	OPXX2(14,1,3),		ia64_print_F2},

	/* Table 4-64 */
	{"fcmp.eq",	OPRaRbTa(4,0,0,0),	ia64_print_F4},
	{"fcmp.eq.unc",	OPRaRbTa(4,0,0,1),	ia64_print_F4},
	{"fcmp.lt",	OPRaRbTa(4,0,1,0),	ia64_print_F4},
	{"fcmp.lt.unc",	OPRaRbTa(4,0,1,1),	ia64_print_F4},
	{"fcmp.le",	OPRaRbTa(4,1,0,0),	ia64_print_F4},
	{"fcmp.le.unc",	OPRaRbTa(4,1,0,1),	ia64_print_F4},
	{"fcmp.unord",	OPRaRbTa(4,1,1,0),	ia64_print_F4},
	{"fcmp.unord.unc", OPRaRbTa(4,1,1,1),	ia64_print_F4},

	/* Table 4-65 */
	{"fclass.m",	OPTa(5,0),		ia64_print_F5},
	{"fclass.m.unc", OPTa(5,1),		ia64_print_F5},

	{"illegal.f",	0,0,			ia64_print_ill},
	{0},
};

#undef OP
#undef Q
#undef X2
#undef X
#undef X6

#undef mOP
#undef mQ
#undef mX2
#undef mX
#undef mX6

#undef OPXX6
#undef OPXQ
#undef OPX
#undef OPXX2
#undef OPRaRbTa
#undef OPTa

#define OP(n)		field(n,37,40)
#define X3(n)		field(n,33,35)
#define X6(n)		field(n,27,32)
#define BT(n)		field(n,6,8)

#define mOP		OP(~0)
#define mX3		X3(~0)
#define mX6		X6(~0)
#define mBT		BT(~0)

#define OPX6(a,b) \
	OP(a)|X6(b),		mOP|mX6
#define OPBT(a,b) \
	OP(a)|BT(b),		mOP|mBT
#define OPX6BT(a,b,c) \
	OP(a)|X6(b)|BT(c),	mOP|mX6|mBT

static struct ia64_opcode B_opcodes[] = {

	/* Table 4-45 */
	{"br.cond",	OPBT(4, 0),		ia64_print_B1},
	{"br.wexit",	OPBT(4, 2),		ia64_print_B1},
	{"br.wtop",	OPBT(4, 3),		ia64_print_B1},
	{"br.cloop",	OPBT(4, 5),		ia64_print_B1},	/* B2 */
	{"br.cexit",	OPBT(4, 6),		ia64_print_B1},	/* B2 */
	{"br.ctop",	OPBT(4, 7),		ia64_print_B1},	/* B2 */
	
	/* Table 4-46 */
	{"break.b",	OPX6(0,0x00),		ia64_print_B9},
	{"cover",	OPX6(0,0x02),		ia64_print_B8},
	{"clrrb",	OPX6(0,0x04),		ia64_print_B8},
	{"clrrb.pr",	OPX6(0,0x05),		ia64_print_B8},
	{"rfi",		OPX6(0,0x08),		ia64_print_B8},
	{"bsw.0",	OPX6(0,0x0c),		ia64_print_B8},
	{"bsw.1",	OPX6(0,0x0d),		ia64_print_B8},
	{"epc",		OPX6(0,0x10),		ia64_print_B8},

	/* Table 4-47 */
	{"br.cond",	OPX6BT(0,0x20,0),	ia64_print_B4},
	{"br.ia",	OPX6BT(0,0x20,1),	ia64_print_B4},

	/* Table 4-48 */
	{"br.ret",	OPX6BT(0,0x21,4),	ia64_print_B4},

	/* Section 4.5.1.3 */
	{"br.call",	OP(5),mOP,		ia64_print_B3},

	/* Section 4.5.1.5 */
	{"br.call",	OP(1),mOP,		ia64_print_B5},
	
	/* Table 4-53 */
	{"nop.b",	OPX6(2,0x00),		ia64_print_B9},
	{"brp",		OPX6(2,0x10),		ia64_print_B7},
	{"brp.ret",	OPX6(2,0x11),		ia64_print_B7},

	/* Section 4.5.2.1 */
	{"brp",		OP(7),mOP,		ia64_print_B6},

	{"illegal.b",	0,0,			ia64_print_ill},
	{0},
};

#define OPX3X6(a,b,c) \
	OP(a)|X3(b)|X6(c),	mOP|mX3|mX6

static struct ia64_opcode X_opcodes[] = {

	/* Table 4-67 */
	{"break.x",	OPX3X6(0,0,0),		ia64_print_X1},
	{"nop.x",	OPX3X6(0,0,1),		ia64_print_X1},

	/* Table 4-68 */
	{"movl",	OP(6),mOP,		ia64_print_X2},

	{"illegal.x",	0,0,			ia64_print_ill},
	{0},
};

static void
ia64_print_table(struct ia64_opcode *table, u_int64_t ins, db_addr_t loc)
{
	while (table->name) {
		if ((ins & table->mask) == table->value) {
			table->print(table->name, ins, loc);
			return;
		}
		table++;
	}
	ia64_print_bad(0, ins, 0);
}

static void
ia64_print_M(db_addr_t loc, u_int64_t ins, boolean_t showregs)
{
	if ((ins >> 37) >= 8)
		ia64_print_table(A_opcodes, ins, loc);
	else
		ia64_print_table(M_opcodes, ins, loc);
}

static void
ia64_print_I(db_addr_t loc, u_int64_t ins, boolean_t showregs)
{
	if ((ins >> 37) >= 8)
		ia64_print_table(A_opcodes, ins, loc);
	else
		ia64_print_table(I_opcodes, ins, loc);

}

static void
ia64_print_X(db_addr_t loc, u_int64_t ins, boolean_t showregs)
{
	ia64_print_table(X_opcodes, ins, loc);
}

static void
ia64_print_B(db_addr_t loc, u_int64_t ins, boolean_t showregs)
{
	ia64_print_table(B_opcodes, ins, loc);
}

static void
ia64_print_F(db_addr_t loc, u_int64_t ins, boolean_t showregs)
{
	ia64_print_table(F_opcodes, ins, loc);
}

static void
ia64_print_bad(db_addr_t loc, u_int64_t ins, boolean_t showregs)
{
	db_printf("illegal %lx", ins);
}

db_addr_t
db_disasm(db_addr_t loc, boolean_t altfmt)
{
	struct ia64_bundle b;
	int slot;

	/*
	 * We encode the slot number into the low bits of the address.
	 */
	slot = loc & 15;
	loc &= ~15;
	db_read_bundle(loc, &b);

	/*
	 * The uses a Restart Instruction value of one to represent
	 * the L+X slot of an MLX template bundle but the opcode is
	 * actually in slot two.
	 */
	if ((b.template == 4 || b.template == 5) && slot == 1)
		slot = 2;

	if (b.slot[slot] & 63)
		db_printf("(p%ld) ", b.slot[slot] & 63);
	(prints[b.template][slot])(loc, b.slot[slot], altfmt);
	if (stops[b.template][slot])
		db_printf(";;\n");
	else
		db_printf("\n");

	/*
	 * Handle MLX bundles by advancing from slot one to the
	 * following bundle.
	 */
	if (b.template == 4 || b.template == 5) {
		if (slot == 0)
			loc += 1;
		else
			loc += 16;
	} else {
		if (slot == 2)
			loc += 16;
		else
			loc += slot+1;
	}

	return loc;
}
