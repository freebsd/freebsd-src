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
 */

#include <sys/cdefs.h>
/*
 * Instruction disassembler.
 */
#include <sys/param.h>
#include <sys/kdb.h>

#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>

/*
 * Size attributes
 */
#define	BYTE	0
#define	WORD	1
#define	LONG	2
#define	QUAD	3
#define	SNGL	4
#define	DBLR	5
#define	EXTR	6
#define	SDEP	7
#define	NONE	8

/*
 * Addressing modes
 */
#define	E	1			/* general effective address */
#define	Eind	2			/* indirect address (jump, call) */
#define	Ew	3			/* address, word size */
#define	Eb	4			/* address, byte size */
#define	R	5			/* register, in 'reg' field */
#define	Rw	6			/* word register, in 'reg' field */
#define	Ri	7			/* register in instruction */
#define	S	8			/* segment reg, in 'reg' field */
#define	Si	9			/* segment reg, in instruction */
#define	A	10			/* accumulator */
#define	BX	11			/* (bx) */
#define	CL	12			/* cl, for shifts */
#define	DX	13			/* dx, for IO */
#define	SI	14			/* si */
#define	DI	15			/* di */
#define	CR	16			/* control register */
#define	DR	17			/* debug register */
#define	TR	18			/* test register */
#define	I	19			/* immediate, unsigned */
#define	Is	20			/* immediate, signed */
#define	Ib	21			/* byte immediate, unsigned */
#define	Ibs	22			/* byte immediate, signed */
#define	Iw	23			/* word immediate, unsigned */
#define	O	25			/* direct address */
#define	Db	26			/* byte displacement from EIP */
#define	Dl	27			/* long displacement from EIP */
#define	o1	28			/* constant 1 */
#define	o3	29			/* constant 3 */
#define	OS	30			/* immediate offset/segment */
#define	ST	31			/* FP stack top */
#define	STI	32			/* FP stack */
#define	X	33			/* extended FP op */
#define	XA	34			/* for 'fstcw %ax' */
#define	El	35			/* address, long size */
#define	Ril	36			/* long register in instruction */
#define	Iba	37			/* byte immediate, don't print if 0xa */

struct inst {
	const char *	i_name;		/* name */
	bool	i_has_modrm;		/* has regmodrm byte */
	short	i_size;			/* operand size */
	int	i_mode;			/* addressing modes */
	const void *	i_extra;	/* pointer to extra opcode table */
};

#define	op1(x)		(x)
#define	op2(x,y)	((x)|((y)<<8))
#define	op3(x,y,z)	((x)|((y)<<8)|((z)<<16))

struct finst {
	const char *	f_name;		/* name for memory instruction */
	int	f_size;			/* size for memory instruction */
	int	f_rrmode;		/* mode for rr instruction */
	const void *	f_rrname;	/* name for rr instruction
					   (or pointer to table) */
};

static const char * const db_Grp6[] = {
	"sldt",
	"str",
	"lldt",
	"ltr",
	"verr",
	"verw",
	"",
	""
};

static const char * const db_Grp7[] = {
	"sgdt",
	"sidt",
	"lgdt",
	"lidt",
	"smsw",
	"",
	"lmsw",
	"invlpg"
};

static const char * const db_Grp8[] = {
	"",
	"",
	"",
	"",
	"bt",
	"bts",
	"btr",
	"btc"
};

static const char * const db_Grp9[] = {
	"",
	"cmpxchg8b",
	"",
	"",
	"",
	"",
	"",
	""
};

static const char * const db_Grp15[] = {
	"fxsave",
	"fxrstor",
	"ldmxcsr",
	"stmxcsr",
	"",
	"",
	"",
	"clflush"
};

static const char * const db_Grp15b[] = {
	"",
	"",
	"",
	"",
	"",
	"lfence",
	"mfence",
	"sfence"
};

static const struct inst db_inst_0f0x[] = {
/*00*/	{ "",	   true,  NONE,  op1(Ew),     db_Grp6 },
/*01*/	{ "",	   true,  NONE,  op1(Ew),     db_Grp7 },
/*02*/	{ "lar",   true,  LONG,  op2(E,R),    0 },
/*03*/	{ "lsl",   true,  LONG,  op2(E,R),    0 },
/*04*/	{ "",      false, NONE,  0,	      0 },
/*05*/	{ "syscall",false,NONE,  0,	      0 },
/*06*/	{ "clts",  false, NONE,  0,	      0 },
/*07*/	{ "sysret",false, NONE,  0,	      0 },

/*08*/	{ "invd",  false, NONE,  0,	      0 },
/*09*/	{ "wbinvd",false, NONE,  0,	      0 },
/*0a*/	{ "",      false, NONE,  0,	      0 },
/*0b*/	{ "",      false, NONE,  0,	      0 },
/*0c*/	{ "",      false, NONE,  0,	      0 },
/*0d*/	{ "",      false, NONE,  0,	      0 },
/*0e*/	{ "",      false, NONE,  0,	      0 },
/*0f*/	{ "",      false, NONE,  0,	      0 },
};

static const struct inst db_inst_0f1x[] = {
/*10*/	{ "",      false, NONE,  0,	      0 },
/*11*/	{ "",      false, NONE,  0,	      0 },
/*12*/	{ "",      false, NONE,  0,	      0 },
/*13*/	{ "",      false, NONE,  0,	      0 },
/*14*/	{ "",      false, NONE,  0,	      0 },
/*15*/	{ "",      false, NONE,  0,	      0 },
/*16*/	{ "",      false, NONE,  0,	      0 },
/*17*/	{ "",      false, NONE,  0,	      0 },

/*18*/	{ "",      false, NONE,  0,	      0 },
/*19*/	{ "",      false, NONE,  0,	      0 },
/*1a*/	{ "",      false, NONE,  0,	      0 },
/*1b*/	{ "",      false, NONE,  0,	      0 },
/*1c*/	{ "",      false, NONE,  0,	      0 },
/*1d*/	{ "",      false, NONE,  0,	      0 },
/*1e*/	{ "",      false, NONE,  0,	      0 },
/*1f*/	{ "nopl",  true,  SDEP,  0,	      "nopw" },
};

static const struct inst db_inst_0f2x[] = {
/*20*/	{ "mov",   true,  LONG,  op2(CR,El),  0 },
/*21*/	{ "mov",   true,  LONG,  op2(DR,El),  0 },
/*22*/	{ "mov",   true,  LONG,  op2(El,CR),  0 },
/*23*/	{ "mov",   true,  LONG,  op2(El,DR),  0 },
/*24*/	{ "mov",   true,  LONG,  op2(TR,El),  0 },
/*25*/	{ "",      false, NONE,  0,	      0 },
/*26*/	{ "mov",   true,  LONG,  op2(El,TR),  0 },
/*27*/	{ "",      false, NONE,  0,	      0 },

/*28*/	{ "",      false, NONE,  0,	      0 },
/*29*/	{ "",      false, NONE,  0,	      0 },
/*2a*/	{ "",      false, NONE,  0,	      0 },
/*2b*/	{ "",      false, NONE,  0,	      0 },
/*2c*/	{ "",      false, NONE,  0,	      0 },
/*2d*/	{ "",      false, NONE,  0,	      0 },
/*2e*/	{ "",      false, NONE,  0,	      0 },
/*2f*/	{ "",      false, NONE,  0,	      0 },
};

static const struct inst db_inst_0f3x[] = {
/*30*/	{ "wrmsr", false, NONE,  0,	      0 },
/*31*/	{ "rdtsc", false, NONE,  0,	      0 },
/*32*/	{ "rdmsr", false, NONE,  0,	      0 },
/*33*/	{ "rdpmc", false, NONE,  0,	      0 },
/*34*/	{ "sysenter",false,NONE,  0,	      0 },
/*35*/	{ "sysexit",false,NONE,  0,	      0 },
/*36*/	{ "",	   false, NONE,  0,	      0 },
/*37*/	{ "getsec",false, NONE,  0,	      0 },

/*38*/	{ "",	   false, NONE,  0,	      0 },
/*39*/	{ "",	   false, NONE,  0,	      0 },
/*3a*/	{ "",	   false, NONE,  0,	      0 },
/*3b*/	{ "",	   false, NONE,  0,	      0 },
/*3c*/	{ "",	   false, NONE,  0,	      0 },
/*3d*/	{ "",	   false, NONE,  0,	      0 },
/*3e*/	{ "",	   false, NONE,  0,	      0 },
/*3f*/	{ "",	   false, NONE,  0,	      0 },
};

static const struct inst db_inst_0f4x[] = {
/*40*/	{ "cmovo",  true, NONE,  op2(E, R),   0 },
/*41*/	{ "cmovno", true, NONE,  op2(E, R),   0 },
/*42*/	{ "cmovb",  true, NONE,  op2(E, R),   0 },
/*43*/	{ "cmovnb", true, NONE,  op2(E, R),   0 },
/*44*/	{ "cmovz",  true, NONE,  op2(E, R),   0 },
/*45*/	{ "cmovnz", true, NONE,  op2(E, R),   0 },
/*46*/	{ "cmovbe", true, NONE,  op2(E, R),   0 },
/*47*/	{ "cmovnbe",true, NONE,  op2(E, R),   0 },

/*48*/	{ "cmovs",  true, NONE,  op2(E, R),   0 },
/*49*/	{ "cmovns", true, NONE,  op2(E, R),   0 },
/*4a*/	{ "cmovp",  true, NONE,  op2(E, R),   0 },
/*4b*/	{ "cmovnp", true, NONE,  op2(E, R),   0 },
/*4c*/	{ "cmovl",  true, NONE,  op2(E, R),   0 },
/*4d*/	{ "cmovnl", true, NONE,  op2(E, R),   0 },
/*4e*/	{ "cmovle", true, NONE,  op2(E, R),   0 },
/*4f*/	{ "cmovnle",true, NONE,  op2(E, R),   0 },
};

static const struct inst db_inst_0f8x[] = {
/*80*/	{ "jo",    false, NONE,  op1(Dl),     0 },
/*81*/	{ "jno",   false, NONE,  op1(Dl),     0 },
/*82*/	{ "jb",    false, NONE,  op1(Dl),     0 },
/*83*/	{ "jnb",   false, NONE,  op1(Dl),     0 },
/*84*/	{ "jz",    false, NONE,  op1(Dl),     0 },
/*85*/	{ "jnz",   false, NONE,  op1(Dl),     0 },
/*86*/	{ "jbe",   false, NONE,  op1(Dl),     0 },
/*87*/	{ "jnbe",  false, NONE,  op1(Dl),     0 },

/*88*/	{ "js",    false, NONE,  op1(Dl),     0 },
/*89*/	{ "jns",   false, NONE,  op1(Dl),     0 },
/*8a*/	{ "jp",    false, NONE,  op1(Dl),     0 },
/*8b*/	{ "jnp",   false, NONE,  op1(Dl),     0 },
/*8c*/	{ "jl",    false, NONE,  op1(Dl),     0 },
/*8d*/	{ "jnl",   false, NONE,  op1(Dl),     0 },
/*8e*/	{ "jle",   false, NONE,  op1(Dl),     0 },
/*8f*/	{ "jnle",  false, NONE,  op1(Dl),     0 },
};

static const struct inst db_inst_0f9x[] = {
/*90*/	{ "seto",  true,  NONE,  op1(Eb),     0 },
/*91*/	{ "setno", true,  NONE,  op1(Eb),     0 },
/*92*/	{ "setb",  true,  NONE,  op1(Eb),     0 },
/*93*/	{ "setnb", true,  NONE,  op1(Eb),     0 },
/*94*/	{ "setz",  true,  NONE,  op1(Eb),     0 },
/*95*/	{ "setnz", true,  NONE,  op1(Eb),     0 },
/*96*/	{ "setbe", true,  NONE,  op1(Eb),     0 },
/*97*/	{ "setnbe",true,  NONE,  op1(Eb),     0 },

/*98*/	{ "sets",  true,  NONE,  op1(Eb),     0 },
/*99*/	{ "setns", true,  NONE,  op1(Eb),     0 },
/*9a*/	{ "setp",  true,  NONE,  op1(Eb),     0 },
/*9b*/	{ "setnp", true,  NONE,  op1(Eb),     0 },
/*9c*/	{ "setl",  true,  NONE,  op1(Eb),     0 },
/*9d*/	{ "setnl", true,  NONE,  op1(Eb),     0 },
/*9e*/	{ "setle", true,  NONE,  op1(Eb),     0 },
/*9f*/	{ "setnle",true,  NONE,  op1(Eb),     0 },
};

static const struct inst db_inst_0fax[] = {
/*a0*/	{ "push",  false, NONE,  op1(Si),     0 },
/*a1*/	{ "pop",   false, NONE,  op1(Si),     0 },
/*a2*/	{ "cpuid", false, NONE,  0,	      0 },
/*a3*/	{ "bt",    true,  LONG,  op2(R,E),    0 },
/*a4*/	{ "shld",  true,  LONG,  op3(Ib,R,E), 0 },
/*a5*/	{ "shld",  true,  LONG,  op3(CL,R,E), 0 },
/*a6*/	{ "",      false, NONE,  0,	      0 },
/*a7*/	{ "",      false, NONE,  0,	      0 },

/*a8*/	{ "push",  false, NONE,  op1(Si),     0 },
/*a9*/	{ "pop",   false, NONE,  op1(Si),     0 },
/*aa*/	{ "rsm",   false, NONE,  0,	      0 },
/*ab*/	{ "bts",   true,  LONG,  op2(R,E),    0 },
/*ac*/	{ "shrd",  true,  LONG,  op3(Ib,R,E), 0 },
/*ad*/	{ "shrd",  true,  LONG,  op3(CL,R,E), 0 },
/*ae*/	{ "",      true,  LONG,  op1(E),      db_Grp15 },
/*af*/	{ "imul",  true,  LONG,  op2(E,R),    0 },
};

static const struct inst db_inst_0fbx[] = {
/*b0*/	{ "cmpxchg",true, BYTE,	 op2(R, E),   0 },
/*b0*/	{ "cmpxchg",true, LONG,	 op2(R, E),   0 },
/*b2*/	{ "lss",   true,  LONG,  op2(E, R),   0 },
/*b3*/	{ "btr",   true,  LONG,  op2(R, E),   0 },
/*b4*/	{ "lfs",   true,  LONG,  op2(E, R),   0 },
/*b5*/	{ "lgs",   true,  LONG,  op2(E, R),   0 },
/*b6*/	{ "movzb", true,  LONG,  op2(Eb, R),  0 },
/*b7*/	{ "movzw", true,  LONG,  op2(Ew, R),  0 },

/*b8*/	{ "",      false, NONE,  0,	      0 },
/*b9*/	{ "",      false, NONE,  0,	      0 },
/*ba*/	{ "",      true,  LONG,  op2(Ib, E),  db_Grp8 },
/*bb*/	{ "btc",   true,  LONG,  op2(R, E),   0 },
/*bc*/	{ "bsf",   true,  LONG,  op2(E, R),   0 },
/*bd*/	{ "bsr",   true,  LONG,  op2(E, R),   0 },
/*be*/	{ "movsb", true,  LONG,  op2(Eb, R),  0 },
/*bf*/	{ "movsw", true,  LONG,  op2(Ew, R),  0 },
};

static const struct inst db_inst_0fcx[] = {
/*c0*/	{ "xadd",  true,  BYTE,	 op2(R, E),   0 },
/*c1*/	{ "xadd",  true,  LONG,	 op2(R, E),   0 },
/*c2*/	{ "",	   false, NONE,	 0,	      0 },
/*c3*/	{ "",	   false, NONE,	 0,	      0 },
/*c4*/	{ "",	   false, NONE,	 0,	      0 },
/*c5*/	{ "",	   false, NONE,	 0,	      0 },
/*c6*/	{ "",	   false, NONE,	 0,	      0 },
/*c7*/	{ "",	   true,  NONE,  op1(E),      db_Grp9 },
/*c8*/	{ "bswap", false, LONG,  op1(Ril),    0 },
/*c9*/	{ "bswap", false, LONG,  op1(Ril),    0 },
/*ca*/	{ "bswap", false, LONG,  op1(Ril),    0 },
/*cb*/	{ "bswap", false, LONG,  op1(Ril),    0 },
/*cc*/	{ "bswap", false, LONG,  op1(Ril),    0 },
/*cd*/	{ "bswap", false, LONG,  op1(Ril),    0 },
/*ce*/	{ "bswap", false, LONG,  op1(Ril),    0 },
/*cf*/	{ "bswap", false, LONG,  op1(Ril),    0 },
};

static const struct inst * const db_inst_0f[] = {
	db_inst_0f0x,
	db_inst_0f1x,
	db_inst_0f2x,
	db_inst_0f3x,
	db_inst_0f4x,
	0,
	0,
	0,
	db_inst_0f8x,
	db_inst_0f9x,
	db_inst_0fax,
	db_inst_0fbx,
	db_inst_0fcx,
	0,
	0,
	0
};

static const char * const db_Esc92[] = {
	"fnop",	"",	"",	"",	"",	"",	"",	""
};
static const char * const db_Esc94[] = {
	"fchs",	"fabs",	"",	"",	"ftst",	"fxam",	"",	""
};
static const char * const db_Esc95[] = {
	"fld1",	"fldl2t","fldl2e","fldpi","fldlg2","fldln2","fldz",""
};
static const char * const db_Esc96[] = {
	"f2xm1","fyl2x","fptan","fpatan","fxtract","fprem1","fdecstp",
	"fincstp"
};
static const char * const db_Esc97[] = {
	"fprem","fyl2xp1","fsqrt","fsincos","frndint","fscale","fsin","fcos"
};

static const char * const db_Esca5[] = {
	"",	"fucompp","",	"",	"",	"",	"",	""
};

static const char * const db_Escb4[] = {
	"fneni","fndisi",	"fnclex","fninit","fsetpm",	"",	"",	""
};

static const char * const db_Esce3[] = {
	"",	"fcompp","",	"",	"",	"",	"",	""
};

static const char * const db_Escf4[] = {
	"fnstsw","",	"",	"",	"",	"",	"",	""
};

static const struct finst db_Esc8[] = {
/*0*/	{ "fadd",   SNGL,  op2(STI,ST),	0 },
/*1*/	{ "fmul",   SNGL,  op2(STI,ST),	0 },
/*2*/	{ "fcom",   SNGL,  op2(STI,ST),	0 },
/*3*/	{ "fcomp",  SNGL,  op2(STI,ST),	0 },
/*4*/	{ "fsub",   SNGL,  op2(STI,ST),	0 },
/*5*/	{ "fsubr",  SNGL,  op2(STI,ST),	0 },
/*6*/	{ "fdiv",   SNGL,  op2(STI,ST),	0 },
/*7*/	{ "fdivr",  SNGL,  op2(STI,ST),	0 },
};

static const struct finst db_Esc9[] = {
/*0*/	{ "fld",    SNGL,  op1(STI),	0 },
/*1*/	{ "",       NONE,  op1(STI),	"fxch" },
/*2*/	{ "fst",    SNGL,  op1(X),	db_Esc92 },
/*3*/	{ "fstp",   SNGL,  0,		0 },
/*4*/	{ "fldenv", NONE,  op1(X),	db_Esc94 },
/*5*/	{ "fldcw",  NONE,  op1(X),	db_Esc95 },
/*6*/	{ "fnstenv",NONE,  op1(X),	db_Esc96 },
/*7*/	{ "fnstcw", NONE,  op1(X),	db_Esc97 },
};

static const struct finst db_Esca[] = {
/*0*/	{ "fiadd",  LONG,  0,		0 },
/*1*/	{ "fimul",  LONG,  0,		0 },
/*2*/	{ "ficom",  LONG,  0,		0 },
/*3*/	{ "ficomp", LONG,  0,		0 },
/*4*/	{ "fisub",  LONG,  0,		0 },
/*5*/	{ "fisubr", LONG,  op1(X),	db_Esca5 },
/*6*/	{ "fidiv",  LONG,  0,		0 },
/*7*/	{ "fidivr", LONG,  0,		0 }
};

static const struct finst db_Escb[] = {
/*0*/	{ "fild",   LONG,  0,		0 },
/*1*/	{ "",       NONE,  0,		0 },
/*2*/	{ "fist",   LONG,  0,		0 },
/*3*/	{ "fistp",  LONG,  0,		0 },
/*4*/	{ "",       WORD,  op1(X),	db_Escb4 },
/*5*/	{ "fld",    EXTR,  0,		0 },
/*6*/	{ "",       WORD,  0,		0 },
/*7*/	{ "fstp",   EXTR,  0,		0 },
};

static const struct finst db_Escc[] = {
/*0*/	{ "fadd",   DBLR,  op2(ST,STI),	0 },
/*1*/	{ "fmul",   DBLR,  op2(ST,STI),	0 },
/*2*/	{ "fcom",   DBLR,  0,		0 },
/*3*/	{ "fcomp",  DBLR,  0,		0 },
/*4*/	{ "fsub",   DBLR,  op2(ST,STI),	"fsubr" },
/*5*/	{ "fsubr",  DBLR,  op2(ST,STI),	"fsub" },
/*6*/	{ "fdiv",   DBLR,  op2(ST,STI),	"fdivr" },
/*7*/	{ "fdivr",  DBLR,  op2(ST,STI),	"fdiv" },
};

static const struct finst db_Escd[] = {
/*0*/	{ "fld",    DBLR,  op1(STI),	"ffree" },
/*1*/	{ "",       NONE,  0,		0 },
/*2*/	{ "fst",    DBLR,  op1(STI),	0 },
/*3*/	{ "fstp",   DBLR,  op1(STI),	0 },
/*4*/	{ "frstor", NONE,  op1(STI),	"fucom" },
/*5*/	{ "",       NONE,  op1(STI),	"fucomp" },
/*6*/	{ "fnsave", NONE,  0,		0 },
/*7*/	{ "fnstsw", NONE,  0,		0 },
};

static const struct finst db_Esce[] = {
/*0*/	{ "fiadd",  WORD,  op2(ST,STI),	"faddp" },
/*1*/	{ "fimul",  WORD,  op2(ST,STI),	"fmulp" },
/*2*/	{ "ficom",  WORD,  0,		0 },
/*3*/	{ "ficomp", WORD,  op1(X),	db_Esce3 },
/*4*/	{ "fisub",  WORD,  op2(ST,STI),	"fsubrp" },
/*5*/	{ "fisubr", WORD,  op2(ST,STI),	"fsubp" },
/*6*/	{ "fidiv",  WORD,  op2(ST,STI),	"fdivrp" },
/*7*/	{ "fidivr", WORD,  op2(ST,STI),	"fdivp" },
};

static const struct finst db_Escf[] = {
/*0*/	{ "fild",   WORD,  0,		0 },
/*1*/	{ "",       NONE,  0,		0 },
/*2*/	{ "fist",   WORD,  0,		0 },
/*3*/	{ "fistp",  WORD,  0,		0 },
/*4*/	{ "fbld",   NONE,  op1(XA),	db_Escf4 },
/*5*/	{ "fild",   QUAD,  0,		0 },
/*6*/	{ "fbstp",  NONE,  0,		0 },
/*7*/	{ "fistp",  QUAD,  0,		0 },
};

static const struct finst * const db_Esc_inst[] = {
	db_Esc8, db_Esc9, db_Esca, db_Escb,
	db_Escc, db_Escd, db_Esce, db_Escf
};

static const char * const db_Grp1[] = {
	"add",
	"or",
	"adc",
	"sbb",
	"and",
	"sub",
	"xor",
	"cmp"
};

static const char * const db_Grp2[] = {
	"rol",
	"ror",
	"rcl",
	"rcr",
	"shl",
	"shr",
	"shl",
	"sar"
};

static const struct inst db_Grp3[] = {
	{ "test",  true, NONE, op2(I,E), 0 },
	{ "test",  true, NONE, op2(I,E), 0 },
	{ "not",   true, NONE, op1(E),   0 },
	{ "neg",   true, NONE, op1(E),   0 },
	{ "mul",   true, NONE, op2(E,A), 0 },
	{ "imul",  true, NONE, op2(E,A), 0 },
	{ "div",   true, NONE, op2(E,A), 0 },
	{ "idiv",  true, NONE, op2(E,A), 0 },
};

static const struct inst db_Grp4[] = {
	{ "inc",   true, BYTE, op1(E),   0 },
	{ "dec",   true, BYTE, op1(E),   0 },
	{ "",      true, NONE, 0,	 0 },
	{ "",      true, NONE, 0,	 0 },
	{ "",      true, NONE, 0,	 0 },
	{ "",      true, NONE, 0,	 0 },
	{ "",      true, NONE, 0,	 0 },
	{ "",      true, NONE, 0,	 0 }
};

static const struct inst db_Grp5[] = {
	{ "inc",   true, LONG, op1(E),   0 },
	{ "dec",   true, LONG, op1(E),   0 },
	{ "call",  true, LONG, op1(Eind),0 },
	{ "lcall", true, LONG, op1(Eind),0 },
	{ "jmp",   true, LONG, op1(Eind),0 },
	{ "ljmp",  true, LONG, op1(Eind),0 },
	{ "push",  true, LONG, op1(E),   0 },
	{ "",      true, NONE, 0,	 0 }
};

static const struct inst db_inst_table[256] = {
/*00*/	{ "add",   true,  BYTE,  op2(R, E),  0 },
/*01*/	{ "add",   true,  LONG,  op2(R, E),  0 },
/*02*/	{ "add",   true,  BYTE,  op2(E, R),  0 },
/*03*/	{ "add",   true,  LONG,  op2(E, R),  0 },
/*04*/	{ "add",   false, BYTE,  op2(I, A),  0 },
/*05*/	{ "add",   false, LONG,  op2(Is, A), 0 },
/*06*/	{ "push",  false, NONE,  op1(Si),    0 },
/*07*/	{ "pop",   false, NONE,  op1(Si),    0 },

/*08*/	{ "or",    true,  BYTE,  op2(R, E),  0 },
/*09*/	{ "or",    true,  LONG,  op2(R, E),  0 },
/*0a*/	{ "or",    true,  BYTE,  op2(E, R),  0 },
/*0b*/	{ "or",    true,  LONG,  op2(E, R),  0 },
/*0c*/	{ "or",    false, BYTE,  op2(I, A),  0 },
/*0d*/	{ "or",    false, LONG,  op2(I, A),  0 },
/*0e*/	{ "push",  false, NONE,  op1(Si),    0 },
/*0f*/	{ "",      false, NONE,  0,	     0 },

/*10*/	{ "adc",   true,  BYTE,  op2(R, E),  0 },
/*11*/	{ "adc",   true,  LONG,  op2(R, E),  0 },
/*12*/	{ "adc",   true,  BYTE,  op2(E, R),  0 },
/*13*/	{ "adc",   true,  LONG,  op2(E, R),  0 },
/*14*/	{ "adc",   false, BYTE,  op2(I, A),  0 },
/*15*/	{ "adc",   false, LONG,  op2(Is, A), 0 },
/*16*/	{ "push",  false, NONE,  op1(Si),    0 },
/*17*/	{ "pop",   false, NONE,  op1(Si),    0 },

/*18*/	{ "sbb",   true,  BYTE,  op2(R, E),  0 },
/*19*/	{ "sbb",   true,  LONG,  op2(R, E),  0 },
/*1a*/	{ "sbb",   true,  BYTE,  op2(E, R),  0 },
/*1b*/	{ "sbb",   true,  LONG,  op2(E, R),  0 },
/*1c*/	{ "sbb",   false, BYTE,  op2(I, A),  0 },
/*1d*/	{ "sbb",   false, LONG,  op2(Is, A), 0 },
/*1e*/	{ "push",  false, NONE,  op1(Si),    0 },
/*1f*/	{ "pop",   false, NONE,  op1(Si),    0 },

/*20*/	{ "and",   true,  BYTE,  op2(R, E),  0 },
/*21*/	{ "and",   true,  LONG,  op2(R, E),  0 },
/*22*/	{ "and",   true,  BYTE,  op2(E, R),  0 },
/*23*/	{ "and",   true,  LONG,  op2(E, R),  0 },
/*24*/	{ "and",   false, BYTE,  op2(I, A),  0 },
/*25*/	{ "and",   false, LONG,  op2(I, A),  0 },
/*26*/	{ "",      false, NONE,  0,	     0 },
/*27*/	{ "daa",   false, NONE,  0,	     0 },

/*28*/	{ "sub",   true,  BYTE,  op2(R, E),  0 },
/*29*/	{ "sub",   true,  LONG,  op2(R, E),  0 },
/*2a*/	{ "sub",   true,  BYTE,  op2(E, R),  0 },
/*2b*/	{ "sub",   true,  LONG,  op2(E, R),  0 },
/*2c*/	{ "sub",   false, BYTE,  op2(I, A),  0 },
/*2d*/	{ "sub",   false, LONG,  op2(Is, A), 0 },
/*2e*/	{ "",      false, NONE,  0,	     0 },
/*2f*/	{ "das",   false, NONE,  0,	     0 },

/*30*/	{ "xor",   true,  BYTE,  op2(R, E),  0 },
/*31*/	{ "xor",   true,  LONG,  op2(R, E),  0 },
/*32*/	{ "xor",   true,  BYTE,  op2(E, R),  0 },
/*33*/	{ "xor",   true,  LONG,  op2(E, R),  0 },
/*34*/	{ "xor",   false, BYTE,  op2(I, A),  0 },
/*35*/	{ "xor",   false, LONG,  op2(I, A),  0 },
/*36*/	{ "",      false, NONE,  0,	     0 },
/*37*/	{ "aaa",   false, NONE,  0,	     0 },

/*38*/	{ "cmp",   true,  BYTE,  op2(R, E),  0 },
/*39*/	{ "cmp",   true,  LONG,  op2(R, E),  0 },
/*3a*/	{ "cmp",   true,  BYTE,  op2(E, R),  0 },
/*3b*/	{ "cmp",   true,  LONG,  op2(E, R),  0 },
/*3c*/	{ "cmp",   false, BYTE,  op2(I, A),  0 },
/*3d*/	{ "cmp",   false, LONG,  op2(Is, A), 0 },
/*3e*/	{ "",      false, NONE,  0,	     0 },
/*3f*/	{ "aas",   false, NONE,  0,	     0 },

/*40*/	{ "inc",   false, LONG,  op1(Ri),    0 },
/*41*/	{ "inc",   false, LONG,  op1(Ri),    0 },
/*42*/	{ "inc",   false, LONG,  op1(Ri),    0 },
/*43*/	{ "inc",   false, LONG,  op1(Ri),    0 },
/*44*/	{ "inc",   false, LONG,  op1(Ri),    0 },
/*45*/	{ "inc",   false, LONG,  op1(Ri),    0 },
/*46*/	{ "inc",   false, LONG,  op1(Ri),    0 },
/*47*/	{ "inc",   false, LONG,  op1(Ri),    0 },

/*48*/	{ "dec",   false, LONG,  op1(Ri),    0 },
/*49*/	{ "dec",   false, LONG,  op1(Ri),    0 },
/*4a*/	{ "dec",   false, LONG,  op1(Ri),    0 },
/*4b*/	{ "dec",   false, LONG,  op1(Ri),    0 },
/*4c*/	{ "dec",   false, LONG,  op1(Ri),    0 },
/*4d*/	{ "dec",   false, LONG,  op1(Ri),    0 },
/*4e*/	{ "dec",   false, LONG,  op1(Ri),    0 },
/*4f*/	{ "dec",   false, LONG,  op1(Ri),    0 },

/*50*/	{ "push",  false, LONG,  op1(Ri),    0 },
/*51*/	{ "push",  false, LONG,  op1(Ri),    0 },
/*52*/	{ "push",  false, LONG,  op1(Ri),    0 },
/*53*/	{ "push",  false, LONG,  op1(Ri),    0 },
/*54*/	{ "push",  false, LONG,  op1(Ri),    0 },
/*55*/	{ "push",  false, LONG,  op1(Ri),    0 },
/*56*/	{ "push",  false, LONG,  op1(Ri),    0 },
/*57*/	{ "push",  false, LONG,  op1(Ri),    0 },

/*58*/	{ "pop",   false, LONG,  op1(Ri),    0 },
/*59*/	{ "pop",   false, LONG,  op1(Ri),    0 },
/*5a*/	{ "pop",   false, LONG,  op1(Ri),    0 },
/*5b*/	{ "pop",   false, LONG,  op1(Ri),    0 },
/*5c*/	{ "pop",   false, LONG,  op1(Ri),    0 },
/*5d*/	{ "pop",   false, LONG,  op1(Ri),    0 },
/*5e*/	{ "pop",   false, LONG,  op1(Ri),    0 },
/*5f*/	{ "pop",   false, LONG,  op1(Ri),    0 },

/*60*/	{ "pusha", false, LONG,  0,	     0 },
/*61*/	{ "popa",  false, LONG,  0,	     0 },
/*62*/  { "bound", true,  LONG,  op2(E, R),  0 },
/*63*/	{ "arpl",  true,  NONE,  op2(Rw,Ew), 0 },

/*64*/	{ "",      false, NONE,  0,	     0 },
/*65*/	{ "",      false, NONE,  0,	     0 },
/*66*/	{ "",      false, NONE,  0,	     0 },
/*67*/	{ "",      false, NONE,  0,	     0 },

/*68*/	{ "push",  false, LONG,  op1(I),     0 },
/*69*/  { "imul",  true,  LONG,  op3(I,E,R), 0 },
/*6a*/	{ "push",  false, LONG,  op1(Ibs),   0 },
/*6b*/  { "imul",  true,  LONG,  op3(Ibs,E,R),0 },
/*6c*/	{ "ins",   false, BYTE,  op2(DX, DI), 0 },
/*6d*/	{ "ins",   false, LONG,  op2(DX, DI), 0 },
/*6e*/	{ "outs",  false, BYTE,  op2(SI, DX), 0 },
/*6f*/	{ "outs",  false, LONG,  op2(SI, DX), 0 },

/*70*/	{ "jo",    false, NONE,  op1(Db),     0 },
/*71*/	{ "jno",   false, NONE,  op1(Db),     0 },
/*72*/	{ "jb",    false, NONE,  op1(Db),     0 },
/*73*/	{ "jnb",   false, NONE,  op1(Db),     0 },
/*74*/	{ "jz",    false, NONE,  op1(Db),     0 },
/*75*/	{ "jnz",   false, NONE,  op1(Db),     0 },
/*76*/	{ "jbe",   false, NONE,  op1(Db),     0 },
/*77*/	{ "jnbe",  false, NONE,  op1(Db),     0 },

/*78*/	{ "js",    false, NONE,  op1(Db),     0 },
/*79*/	{ "jns",   false, NONE,  op1(Db),     0 },
/*7a*/	{ "jp",    false, NONE,  op1(Db),     0 },
/*7b*/	{ "jnp",   false, NONE,  op1(Db),     0 },
/*7c*/	{ "jl",    false, NONE,  op1(Db),     0 },
/*7d*/	{ "jnl",   false, NONE,  op1(Db),     0 },
/*7e*/	{ "jle",   false, NONE,  op1(Db),     0 },
/*7f*/	{ "jnle",  false, NONE,  op1(Db),     0 },

/*80*/  { "",	   true,  BYTE,  op2(I, E),   db_Grp1 },
/*81*/  { "",	   true,  LONG,  op2(I, E),   db_Grp1 },
/*82*/  { "",	   true,  BYTE,  op2(I, E),   db_Grp1 },
/*83*/  { "",	   true,  LONG,  op2(Ibs,E),  db_Grp1 },
/*84*/	{ "test",  true,  BYTE,  op2(R, E),   0 },
/*85*/	{ "test",  true,  LONG,  op2(R, E),   0 },
/*86*/	{ "xchg",  true,  BYTE,  op2(R, E),   0 },
/*87*/	{ "xchg",  true,  LONG,  op2(R, E),   0 },

/*88*/	{ "mov",   true,  BYTE,  op2(R, E),   0 },
/*89*/	{ "mov",   true,  LONG,  op2(R, E),   0 },
/*8a*/	{ "mov",   true,  BYTE,  op2(E, R),   0 },
/*8b*/	{ "mov",   true,  LONG,  op2(E, R),   0 },
/*8c*/  { "mov",   true,  NONE,  op2(S, Ew),  0 },
/*8d*/	{ "lea",   true,  LONG,  op2(E, R),   0 },
/*8e*/	{ "mov",   true,  NONE,  op2(Ew, S),  0 },
/*8f*/	{ "pop",   true,  LONG,  op1(E),      0 },

/*90*/	{ "nop",   false, NONE,  0,	      0 },
/*91*/	{ "xchg",  false, LONG,  op2(A, Ri),  0 },
/*92*/	{ "xchg",  false, LONG,  op2(A, Ri),  0 },
/*93*/	{ "xchg",  false, LONG,  op2(A, Ri),  0 },
/*94*/	{ "xchg",  false, LONG,  op2(A, Ri),  0 },
/*95*/	{ "xchg",  false, LONG,  op2(A, Ri),  0 },
/*96*/	{ "xchg",  false, LONG,  op2(A, Ri),  0 },
/*97*/	{ "xchg",  false, LONG,  op2(A, Ri),  0 },

/*98*/	{ "cbw",   false, SDEP,  0,	      "cwde" },	/* cbw/cwde */
/*99*/	{ "cwd",   false, SDEP,  0,	      "cdq"  },	/* cwd/cdq */
/*9a*/	{ "lcall", false, NONE,  op1(OS),     0 },
/*9b*/	{ "wait",  false, NONE,  0,	      0 },
/*9c*/	{ "pushf", false, LONG,  0,	      0 },
/*9d*/	{ "popf",  false, LONG,  0,	      0 },
/*9e*/	{ "sahf",  false, NONE,  0,	      0 },
/*9f*/	{ "lahf",  false, NONE,  0,	      0 },

/*a0*/	{ "mov",   false, BYTE,  op2(O, A),   0 },
/*a1*/	{ "mov",   false, LONG,  op2(O, A),   0 },
/*a2*/	{ "mov",   false, BYTE,  op2(A, O),   0 },
/*a3*/	{ "mov",   false, LONG,  op2(A, O),   0 },
/*a4*/	{ "movs",  false, BYTE,  op2(SI,DI),  0 },
/*a5*/	{ "movs",  false, LONG,  op2(SI,DI),  0 },
/*a6*/	{ "cmps",  false, BYTE,  op2(SI,DI),  0 },
/*a7*/	{ "cmps",  false, LONG,  op2(SI,DI),  0 },

/*a8*/	{ "test",  false, BYTE,  op2(I, A),   0 },
/*a9*/	{ "test",  false, LONG,  op2(I, A),   0 },
/*aa*/	{ "stos",  false, BYTE,  op1(DI),     0 },
/*ab*/	{ "stos",  false, LONG,  op1(DI),     0 },
/*ac*/	{ "lods",  false, BYTE,  op1(SI),     0 },
/*ad*/	{ "lods",  false, LONG,  op1(SI),     0 },
/*ae*/	{ "scas",  false, BYTE,  op1(SI),     0 },
/*af*/	{ "scas",  false, LONG,  op1(SI),     0 },

/*b0*/	{ "mov",   false, BYTE,  op2(I, Ri),  0 },
/*b1*/	{ "mov",   false, BYTE,  op2(I, Ri),  0 },
/*b2*/	{ "mov",   false, BYTE,  op2(I, Ri),  0 },
/*b3*/	{ "mov",   false, BYTE,  op2(I, Ri),  0 },
/*b4*/	{ "mov",   false, BYTE,  op2(I, Ri),  0 },
/*b5*/	{ "mov",   false, BYTE,  op2(I, Ri),  0 },
/*b6*/	{ "mov",   false, BYTE,  op2(I, Ri),  0 },
/*b7*/	{ "mov",   false, BYTE,  op2(I, Ri),  0 },

/*b8*/	{ "mov",   false, LONG,  op2(I, Ri),  0 },
/*b9*/	{ "mov",   false, LONG,  op2(I, Ri),  0 },
/*ba*/	{ "mov",   false, LONG,  op2(I, Ri),  0 },
/*bb*/	{ "mov",   false, LONG,  op2(I, Ri),  0 },
/*bc*/	{ "mov",   false, LONG,  op2(I, Ri),  0 },
/*bd*/	{ "mov",   false, LONG,  op2(I, Ri),  0 },
/*be*/	{ "mov",   false, LONG,  op2(I, Ri),  0 },
/*bf*/	{ "mov",   false, LONG,  op2(I, Ri),  0 },

/*c0*/	{ "",	   true,  BYTE,  op2(Ib, E),  db_Grp2 },
/*c1*/	{ "",	   true,  LONG,  op2(Ib, E),  db_Grp2 },
/*c2*/	{ "ret",   false, NONE,  op1(Iw),     0 },
/*c3*/	{ "ret",   false, NONE,  0,	      0 },
/*c4*/	{ "les",   true,  LONG,  op2(E, R),   0 },
/*c5*/	{ "lds",   true,  LONG,  op2(E, R),   0 },
/*c6*/	{ "mov",   true,  BYTE,  op2(I, E),   0 },
/*c7*/	{ "mov",   true,  LONG,  op2(I, E),   0 },

/*c8*/	{ "enter", false, NONE,  op2(Iw, Ib), 0 },
/*c9*/	{ "leave", false, NONE,  0,	      0 },
/*ca*/	{ "lret",  false, NONE,  op1(Iw),     0 },
/*cb*/	{ "lret",  false, NONE,  0,	      0 },
/*cc*/	{ "int",   false, NONE,  op1(o3),     0 },
/*cd*/	{ "int",   false, NONE,  op1(Ib),     0 },
/*ce*/	{ "into",  false, NONE,  0,	      0 },
/*cf*/	{ "iret",  false, NONE,  0,	      0 },

/*d0*/	{ "",	   true,  BYTE,  op2(o1, E),  db_Grp2 },
/*d1*/	{ "",	   true,  LONG,  op2(o1, E),  db_Grp2 },
/*d2*/	{ "",	   true,  BYTE,  op2(CL, E),  db_Grp2 },
/*d3*/	{ "",	   true,  LONG,  op2(CL, E),  db_Grp2 },
/*d4*/	{ "aam",   false, NONE,  op1(Iba),    0 },
/*d5*/	{ "aad",   false, NONE,  op1(Iba),    0 },
/*d6*/	{ ".byte\t0xd6", false, NONE, 0,      0 },
/*d7*/	{ "xlat",  false, BYTE,  op1(BX),     0 },

/*d8*/  { "",      true,  NONE,  0,	      db_Esc8 },
/*d9*/  { "",      true,  NONE,  0,	      db_Esc9 },
/*da*/  { "",      true,  NONE,  0,	      db_Esca },
/*db*/  { "",      true,  NONE,  0,	      db_Escb },
/*dc*/  { "",      true,  NONE,  0,	      db_Escc },
/*dd*/  { "",      true,  NONE,  0,	      db_Escd },
/*de*/  { "",      true,  NONE,  0,	      db_Esce },
/*df*/  { "",      true,  NONE,  0,	      db_Escf },

/*e0*/	{ "loopne",false, NONE,  op1(Db),     0 },
/*e1*/	{ "loope", false, NONE,  op1(Db),     0 },
/*e2*/	{ "loop",  false, NONE,  op1(Db),     0 },
/*e3*/	{ "jcxz",  false, SDEP,  op1(Db),     "jecxz" },
/*e4*/	{ "in",    false, BYTE,  op2(Ib, A),  0 },
/*e5*/	{ "in",    false, LONG,  op2(Ib, A) , 0 },
/*e6*/	{ "out",   false, BYTE,  op2(A, Ib),  0 },
/*e7*/	{ "out",   false, LONG,  op2(A, Ib) , 0 },

/*e8*/	{ "call",  false, NONE,  op1(Dl),     0 },
/*e9*/	{ "jmp",   false, NONE,  op1(Dl),     0 },
/*ea*/	{ "ljmp",  false, NONE,  op1(OS),     0 },
/*eb*/	{ "jmp",   false, NONE,  op1(Db),     0 },
/*ec*/	{ "in",    false, BYTE,  op2(DX, A),  0 },
/*ed*/	{ "in",    false, LONG,  op2(DX, A) , 0 },
/*ee*/	{ "out",   false, BYTE,  op2(A, DX),  0 },
/*ef*/	{ "out",   false, LONG,  op2(A, DX) , 0 },

/*f0*/	{ "",      false, NONE,  0,	     0 },
/*f1*/	{ ".byte\t0xf1", false, NONE, 0,     0 },
/*f2*/	{ "",      false, NONE,  0,	     0 },
/*f3*/	{ "",      false, NONE,  0,	     0 },
/*f4*/	{ "hlt",   false, NONE,  0,	     0 },
/*f5*/	{ "cmc",   false, NONE,  0,	     0 },
/*f6*/	{ "",      true,  BYTE,  0,	     db_Grp3 },
/*f7*/	{ "",	   true,  LONG,  0,	     db_Grp3 },

/*f8*/	{ "clc",   false, NONE,  0,	     0 },
/*f9*/	{ "stc",   false, NONE,  0,	     0 },
/*fa*/	{ "cli",   false, NONE,  0,	     0 },
/*fb*/	{ "sti",   false, NONE,  0,	     0 },
/*fc*/	{ "cld",   false, NONE,  0,	     0 },
/*fd*/	{ "std",   false, NONE,  0,	     0 },
/*fe*/	{ "",	   true,  NONE,  0,	     db_Grp4 },
/*ff*/	{ "",	   true,  NONE,  0,	     db_Grp5 },
};

static const struct inst db_bad_inst =
	{ "???",   false, NONE,  0,	      0 }
;

#define	f_mod(byte)	((byte)>>6)
#define	f_reg(byte)	(((byte)>>3)&0x7)
#define	f_rm(byte)	((byte)&0x7)

#define	sib_ss(byte)	((byte)>>6)
#define	sib_index(byte)	(((byte)>>3)&0x7)
#define	sib_base(byte)	((byte)&0x7)

struct i_addr {
	bool		is_reg;	/* if reg, reg number is in 'disp' */
	int		disp;
	const char *	base;
	const char *	index;
	int		ss;
	bool		defss;	/* set if %ss is the default segment */
};

static const char * const db_index_reg_16[8] = {
	"%bx,%si",
	"%bx,%di",
	"%bp,%si",
	"%bp,%di",
	"%si",
	"%di",
	"%bp",
	"%bx"
};

static const char * const db_reg[3][8] = {
	{ "%al",  "%cl",  "%dl",  "%bl",  "%ah",  "%ch",  "%dh",  "%bh" },
	{ "%ax",  "%cx",  "%dx",  "%bx",  "%sp",  "%bp",  "%si",  "%di" },
	{ "%eax", "%ecx", "%edx", "%ebx", "%esp", "%ebp", "%esi", "%edi" }
};

static const char * const db_seg_reg[8] = {
	"%es", "%cs", "%ss", "%ds", "%fs", "%gs", "", ""
};

/*
 * lengths for size attributes
 */
static const int db_lengths[] = {
	1,	/* BYTE */
	2,	/* WORD */
	4,	/* LONG */
	8,	/* QUAD */
	4,	/* SNGL */
	8,	/* DBLR */
	10,	/* EXTR */
};

#define	get_value_inc(result, loc, size, is_signed) \
	result = db_get_value((loc), (size), (is_signed)); \
	(loc) += (size);

static db_addr_t
		db_disasm_esc(db_addr_t loc, int inst, bool short_addr,
		    int size, const char *seg);
static void	db_print_address(const char *seg, int size,
		    struct i_addr *addrp);
static db_addr_t
		db_read_address(db_addr_t loc, bool short_addr, int regmodrm,
		    struct i_addr *addrp);

/*
 * Read address at location and return updated location.
 */
static db_addr_t
db_read_address(db_addr_t loc, bool short_addr, int regmodrm,
    struct i_addr *addrp)
{
	int		mod, rm, sib, index, disp;

	mod = f_mod(regmodrm);
	rm  = f_rm(regmodrm);

	if (mod == 3) {
	    addrp->is_reg = true;
	    addrp->disp = rm;
	    return (loc);
	}
	addrp->is_reg = false;
	addrp->index = NULL;
	addrp->ss = 0;
	addrp->defss = false;

	if (short_addr) {
	    if (rm == 2 || rm == 3 || (rm == 6 && mod != 0))
		addrp->defss = true;
	    switch (mod) {
		case 0:
		    if (rm == 6) {
			get_value_inc(disp, loc, 2, false);
			addrp->disp = disp;
			addrp->base = NULL;
		    }
		    else {
			addrp->disp = 0;
			addrp->base = db_index_reg_16[rm];
		    }
		    break;
		case 1:
		    get_value_inc(disp, loc, 1, true);
		    disp &= 0xFFFF;
		    addrp->disp = disp;
		    addrp->base = db_index_reg_16[rm];
		    break;
		case 2:
		    get_value_inc(disp, loc, 2, false);
		    addrp->disp = disp;
		    addrp->base = db_index_reg_16[rm];
		    break;
	    }
	}
	else {
	    if (rm == 4) {
		get_value_inc(sib, loc, 1, false);
		rm = sib_base(sib);
		index = sib_index(sib);
		if (index != 4)
		    addrp->index = db_reg[LONG][index];
		addrp->ss = sib_ss(sib);
	    }

	    switch (mod) {
		case 0:
		    if (rm == 5) {
			get_value_inc(addrp->disp, loc, 4, false);
			addrp->base = NULL;
		    }
		    else {
			addrp->disp = 0;
			addrp->base = db_reg[LONG][rm];
		    }
		    break;

		case 1:
		    get_value_inc(disp, loc, 1, true);
		    addrp->disp = disp;
		    addrp->base = db_reg[LONG][rm];
		    break;

		case 2:
		    get_value_inc(disp, loc, 4, false);
		    addrp->disp = disp;
		    addrp->base = db_reg[LONG][rm];
		    break;
	    }
	}
	return (loc);
}

static void
db_print_address(const char *seg, int size, struct i_addr *addrp)
{
	if (addrp->is_reg) {
	    db_printf("%s", db_reg[size][addrp->disp]);
	    return;
	}

	if (seg) {
	    db_printf("%s:", seg);
	}
	else if (addrp->defss) {
	    db_printf("%%ss:");
	}

	db_printsym((db_addr_t)addrp->disp, DB_STGY_ANY);
	if (addrp->base != NULL || addrp->index != NULL) {
	    db_printf("(");
	    if (addrp->base)
		db_printf("%s", addrp->base);
	    if (addrp->index)
		db_printf(",%s,%d", addrp->index, 1<<addrp->ss);
	    db_printf(")");
	}
}

/*
 * Disassemble floating-point ("escape") instruction
 * and return updated location.
 */
static db_addr_t
db_disasm_esc(db_addr_t loc, int inst, bool short_addr, int size,
    const char *seg)
{
	int		regmodrm;
	const struct finst *	fp;
	int		mod;
	struct i_addr	address;
	const char *	name;

	get_value_inc(regmodrm, loc, 1, false);
	fp = &db_Esc_inst[inst - 0xd8][f_reg(regmodrm)];
	mod = f_mod(regmodrm);
	if (mod != 3) {
	    if (*fp->f_name == '\0') {
		db_printf("<bad instruction>");
		return (loc);
	    }
	    /*
	     * Normal address modes.
	     */
	    loc = db_read_address(loc, short_addr, regmodrm, &address);
	    db_printf("%s", fp->f_name);
	    switch(fp->f_size) {
		case SNGL:
		    db_printf("s");
		    break;
		case DBLR:
		    db_printf("l");
		    break;
		case EXTR:
		    db_printf("t");
		    break;
		case WORD:
		    db_printf("s");
		    break;
		case LONG:
		    db_printf("l");
		    break;
		case QUAD:
		    db_printf("q");
		    break;
		default:
		    break;
	    }
	    db_printf("\t");
	    db_print_address(seg, BYTE, &address);
	}
	else {
	    /*
	     * 'reg-reg' - special formats
	     */
	    switch (fp->f_rrmode) {
		case op2(ST,STI):
		    name = (fp->f_rrname) ? fp->f_rrname : fp->f_name;
		    db_printf("%s\t%%st,%%st(%d)",name,f_rm(regmodrm));
		    break;
		case op2(STI,ST):
		    name = (fp->f_rrname) ? fp->f_rrname : fp->f_name;
		    db_printf("%s\t%%st(%d),%%st",name, f_rm(regmodrm));
		    break;
		case op1(STI):
		    name = (fp->f_rrname) ? fp->f_rrname : fp->f_name;
		    db_printf("%s\t%%st(%d)",name, f_rm(regmodrm));
		    break;
		case op1(X):
		    name = ((const char * const *)fp->f_rrname)[f_rm(regmodrm)];
		    if (*name == '\0')
			goto bad;
		    db_printf("%s", name);
		    break;
		case op1(XA):
		    name = ((const char * const *)fp->f_rrname)[f_rm(regmodrm)];
		    if (*name == '\0')
			goto bad;
		    db_printf("%s\t%%ax", name);
		    break;
		default:
		bad:
		    db_printf("<bad instruction>");
		    break;
	    }
	}

	return (loc);
}

/*
 * Disassemble instruction at 'loc'.  'altfmt' specifies an
 * (optional) alternate format.  Return address of start of
 * next instruction.
 */
db_addr_t
db_disasm(db_addr_t loc, bool altfmt)
{
	int	inst;
	int	size;
	bool	short_addr;
	const char *	seg;
	const struct inst *	ip;
	const char *	i_name;
	int	i_size;
	int	i_mode;
	int	regmodrm = 0;
	bool	first;
	int	displ;
	bool	prefix;
	bool	rep;
	int	imm;
	int	imm2;
	int	len;
	struct i_addr	address;

	if (db_segsize(kdb_frame) == 16)
	   altfmt = !altfmt;
	get_value_inc(inst, loc, 1, false);
	if (altfmt) {
	    short_addr = true;
	    size = WORD;
	}
	else {
	    short_addr = false;
	    size = LONG;
	}
	seg = NULL;

	/*
	 * Get prefixes
	 */
	rep = false;
	prefix = true;
	do {
	    switch (inst) {
		case 0x66:
		    size = (altfmt ? LONG : WORD);
		    break;
		case 0x67:
		    short_addr = !altfmt;
		    break;
		case 0x26:
		    seg = "%es";
		    break;
		case 0x36:
		    seg = "%ss";
		    break;
		case 0x2e:
		    seg = "%cs";
		    break;
		case 0x3e:
		    seg = "%ds";
		    break;
		case 0x64:
		    seg = "%fs";
		    break;
		case 0x65:
		    seg = "%gs";
		    break;
		case 0xf0:
		    db_printf("lock ");
		    break;
		case 0xf2:
		    db_printf("repne ");
		    break;
		case 0xf3:
		    rep = true;
		    break;
		default:
		    prefix = false;
		    break;
	    }
	    if (prefix) {
		get_value_inc(inst, loc, 1, false);
	    }
	    if (rep) {
		if (inst == 0x90) {
		    db_printf("pause\n");
		    return (loc);
		}
		db_printf("repe ");	/* XXX repe VS rep */
		rep = false;
	    }
	} while (prefix);

	if (inst >= 0xd8 && inst <= 0xdf) {
	    loc = db_disasm_esc(loc, inst, short_addr, size, seg);
	    db_printf("\n");
	    return (loc);
	}

	if (inst == 0x0f) {
	    get_value_inc(inst, loc, 1, false);
	    ip = db_inst_0f[inst>>4];
	    if (ip == NULL) {
		ip = &db_bad_inst;
	    }
	    else {
		ip = &ip[inst&0xf];
	    }
	}
	else
	    ip = &db_inst_table[inst];

	if (ip->i_has_modrm) {
	    get_value_inc(regmodrm, loc, 1, false);
	    loc = db_read_address(loc, short_addr, regmodrm, &address);
	}

	i_name = ip->i_name;
	i_size = ip->i_size;
	i_mode = ip->i_mode;

	if (ip->i_extra == db_Grp1 || ip->i_extra == db_Grp2 ||
	    ip->i_extra == db_Grp6 || ip->i_extra == db_Grp7 ||
	    ip->i_extra == db_Grp8 || ip->i_extra == db_Grp9 ||
	    ip->i_extra == db_Grp15) {
	    i_name = ((const char * const *)ip->i_extra)[f_reg(regmodrm)];
	}
	else if (ip->i_extra == db_Grp3) {
	    ip = ip->i_extra;
	    ip = &ip[f_reg(regmodrm)];
	    i_name = ip->i_name;
	    i_mode = ip->i_mode;
	}
	else if (ip->i_extra == db_Grp4 || ip->i_extra == db_Grp5) {
	    ip = ip->i_extra;
	    ip = &ip[f_reg(regmodrm)];
	    i_name = ip->i_name;
	    i_mode = ip->i_mode;
	    i_size = ip->i_size;
	}

	/* Special cases that don't fit well in the tables. */
	if (ip->i_extra == db_Grp7 && f_mod(regmodrm) == 3) {
		switch (regmodrm) {
		case 0xc8:
			i_name = "monitor";
			i_size = NONE;
			i_mode = 0;
			break;
		case 0xc9:
			i_name = "mwait";
			i_size = NONE;
			i_mode = 0;
			break;
		}
	}
	if (ip->i_extra == db_Grp15 && f_mod(regmodrm) == 3) {
		i_name = db_Grp15b[f_reg(regmodrm)];
		i_size = NONE;
		i_mode = 0;
	}

	if (i_size == SDEP) {
	    if (size == WORD)
		db_printf("%s", i_name);
	    else
		db_printf("%s", (const char *)ip->i_extra);
	}
	else {
	    db_printf("%s", i_name);
	    if (i_size != NONE) {
		if (i_size == BYTE) {
		    db_printf("b");
		    size = BYTE;
		}
		else if (i_size == WORD) {
		    db_printf("w");
		    size = WORD;
		}
		else if (size == WORD)
		    db_printf("w");
		else
		    db_printf("l");
	    }
	}
	db_printf("\t");
	for (first = true;
	     i_mode != 0;
	     i_mode >>= 8, first = false)
	{
	    if (!first)
		db_printf(",");

	    switch (i_mode & 0xFF) {
		case E:
		    db_print_address(seg, size, &address);
		    break;

		case Eind:
		    db_printf("*");
		    db_print_address(seg, size, &address);
		    break;

		case El:
		    db_print_address(seg, LONG, &address);
		    break;

		case Ew:
		    db_print_address(seg, WORD, &address);
		    break;

		case Eb:
		    db_print_address(seg, BYTE, &address);
		    break;

		case R:
		    db_printf("%s", db_reg[size][f_reg(regmodrm)]);
		    break;

		case Rw:
		    db_printf("%s", db_reg[WORD][f_reg(regmodrm)]);
		    break;

		case Ri:
		    db_printf("%s", db_reg[size][f_rm(inst)]);
		    break;

		case Ril:
		    db_printf("%s", db_reg[LONG][f_rm(inst)]);
		    break;

		case S:
		    db_printf("%s", db_seg_reg[f_reg(regmodrm)]);
		    break;

		case Si:
		    db_printf("%s", db_seg_reg[f_reg(inst)]);
		    break;

		case A:
		    db_printf("%s", db_reg[size][0]);	/* acc */
		    break;

		case BX:
		    if (seg)
			db_printf("%s:", seg);
		    db_printf("(%s)", short_addr ? "%bx" : "%ebx");
		    break;

		case CL:
		    db_printf("%%cl");
		    break;

		case DX:
		    db_printf("%%dx");
		    break;

		case SI:
		    if (seg)
			db_printf("%s:", seg);
		    db_printf("(%s)", short_addr ? "%si" : "%esi");
		    break;

		case DI:
		    db_printf("%%es:(%s)", short_addr ? "%di" : "%edi");
		    break;

		case CR:
		    db_printf("%%cr%d", f_reg(regmodrm));
		    break;

		case DR:
		    db_printf("%%dr%d", f_reg(regmodrm));
		    break;

		case TR:
		    db_printf("%%tr%d", f_reg(regmodrm));
		    break;

		case I:
		    len = db_lengths[size];
		    get_value_inc(imm, loc, len, false);
		    db_printf("$%#r", imm);
		    break;

		case Is:
		    len = db_lengths[size];
		    get_value_inc(imm, loc, len, false);
		    db_printf("$%+#r", imm);
		    break;

		case Ib:
		    get_value_inc(imm, loc, 1, false);
		    db_printf("$%#r", imm);
		    break;

		case Iba:
		    get_value_inc(imm, loc, 1, false);
		    if (imm != 0x0a)
			db_printf("$%#r", imm);
		    break;

		case Ibs:
		    get_value_inc(imm, loc, 1, true);
		    if (size == WORD)
			imm &= 0xFFFF;
		    db_printf("$%+#r", imm);
		    break;

		case Iw:
		    get_value_inc(imm, loc, 2, false);
		    db_printf("$%#r", imm);
		    break;

		case O:
		    len = (short_addr ? 2 : 4);
		    get_value_inc(displ, loc, len, false);
		    if (seg)
			db_printf("%s:%+#r",seg, displ);
		    else
			db_printsym((db_addr_t)displ, DB_STGY_ANY);
		    break;

		case Db:
		    get_value_inc(displ, loc, 1, true);
		    displ += loc;
		    if (size == WORD)
			displ &= 0xFFFF;
		    db_printsym((db_addr_t)displ, DB_STGY_XTRN);
		    break;

		case Dl:
		    len = db_lengths[size];
		    get_value_inc(displ, loc, len, false);
		    displ += loc;
		    if (size == WORD)
			displ &= 0xFFFF;
		    db_printsym((db_addr_t)displ, DB_STGY_XTRN);
		    break;

		case o1:
		    db_printf("$1");
		    break;

		case o3:
		    db_printf("$3");
		    break;

		case OS:
		    len = db_lengths[size];
		    get_value_inc(imm, loc, len, false);	/* offset */
		    get_value_inc(imm2, loc, 2, false);	/* segment */
		    db_printf("$%#r,%#r", imm2, imm);
		    break;
	    }
	}
	db_printf("\n");
	return (loc);
}
