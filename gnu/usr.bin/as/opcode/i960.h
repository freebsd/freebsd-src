/* Basic 80960 instruction formats.
 *
 * The 'COJ' instructions are actually COBR instructions with the 'b' in
 * the mnemonic replaced by a 'j';  they are ALWAYS "de-optimized" if necessary:
 * if the displacement will not fit in 13 bits, the assembler will replace them
 * with the corresponding compare and branch instructions.
 *
 * All of the 'MEMn' instructions are the same format; the 'n' in the name
 * indicates the default index scale factor (the size of the datum operated on).
 *
 * The FBRA formats are not actually an instruction format.  They are the
 * "convenience directives" for branching on floating-point comparisons,
 * each of which generates 2 instructions (a 'bno' and one other branch).
 *
 * The CALLJ format is not actually an instruction format.  It indicates that
 * the instruction generated (a CTRL-format 'call') should have its relocation
 * specially flagged for link-time replacement with a 'bal' or 'calls' if
 * appropriate.
 */

/* $FreeBSD: src/gnu/usr.bin/as/opcode/i960.h,v 1.5 1999/08/27 23:34:33 peter Exp $ */

#define CTRL	0
#define COBR	1
#define COJ	2
#define REG	3
#define MEM1	4
#define MEM2	5
#define MEM4	6
#define MEM8	7
#define MEM12	8
#define MEM16	9
#define FBRA	10
#define CALLJ	11

/* Masks for the mode bits in REG format instructions */
#define M1		0x0800
#define M2		0x1000
#define M3		0x2000

/* Generate the 12-bit opcode for a REG format instruction by placing the
 * high 8 bits in instruction bits 24-31, the low 4 bits in instruction bits
 * 7-10.
 */

#define REG_OPC(opc)	((opc & 0xff0) << 20) | ((opc & 0xf) << 7)

/* Generate a template for a REG format instruction:  place the opcode bits
 * in the appropriate fields and OR in mode bits for the operands that will not
 * be used.  I.e.,
 *		set m1=1, if src1 will not be used
 *		set m2=1, if src2 will not be used
 *		set m3=1, if dst  will not be used
 *
 * Setting the "unused" mode bits to 1 speeds up instruction execution(!).
 * The information is also useful to us because some 1-operand REG instructions
 * use the src1 field, others the dst field; and some 2-operand REG instructions
 * use src1/src2, others src1/dst.  The set mode bits enable us to distinguish.
 */
#define R_0(opc)	( REG_OPC(opc) | M1 | M2 | M3 )	/* No operands      */
#define R_1(opc)	( REG_OPC(opc) | M2 | M3 )	/* 1 operand: src1  */
#define R_1D(opc)	( REG_OPC(opc) | M1 | M2 )	/* 1 operand: dst   */
#define R_2(opc)	( REG_OPC(opc) | M3 )		/* 2 ops: src1/src2 */
#define R_2D(opc)	( REG_OPC(opc) | M2 )		/* 2 ops: src1/dst  */
#define R_3(opc)	( REG_OPC(opc) )		/* 3 operands       */

/* DESCRIPTOR BYTES FOR REGISTER OPERANDS
 *
 * Interpret names as follows:
 *	R:   global or local register only
 *	RS:  global, local, or (if target allows) special-function register only
 *	RL:  global or local register, or integer literal
 *	RSL: global, local, or (if target allows) special-function register;
 *		or integer literal
 *	F:   global, local, or floating-point register
 *	FL:  global, local, or floating-point register; or literal (including
 *		floating point)
 *
 * A number appended to a name indicates that registers must be aligned,
 * as follows:
 *	2: register number must be multiple of 2
 *	4: register number must be multiple of 4
 */

#define SFR	0x10		/* Mask for the "sfr-OK" bit */
#define LIT	0x08		/* Mask for the "literal-OK" bit */
#define FP	0x04		/* Mask for "floating-point-OK" bit */

/* This macro ors the bits together.  Note that 'align' is a mask
 * for the low 0, 1, or 2 bits of the register number, as appropriate.
 */
#define OP(align,lit,fp,sfr)	( align | lit | fp | sfr )

#define R	OP( 0, 0,   0,  0   )
#define RS	OP( 0, 0,   0,  SFR )
#define RL	OP( 0, LIT, 0,  0   )
#define RSL	OP( 0, LIT, 0,  SFR )
#define F	OP( 0, 0,   FP, 0   )
#define FL	OP( 0, LIT, FP, 0   )
#define R2	OP( 1, 0,   0,  0   )
#define RL2	OP( 1, LIT, 0,  0   )
#define F2	OP( 1, 0,   FP, 0   )
#define FL2	OP( 1, LIT, FP, 0   )
#define R4	OP( 3, 0,   0,  0   )
#define RL4	OP( 3, LIT, 0,  0   )
#define F4	OP( 3, 0,   FP, 0   )
#define FL4	OP( 3, LIT, FP, 0   )

#define M	0x7f	/* Memory operand (MEMA & MEMB format instructions) */

/* Macros to extract info from the register operand descriptor byte 'od'.
 */
#define SFR_OK(od)	(od & SFR)	/* TRUE if sfr operand allowed */
#define LIT_OK(od)	(od & LIT)	/* TRUE if literal operand allowed */
#define FP_OK(od)	(od & FP)	/* TRUE if floating-point op allowed */
#define REG_ALIGN(od,n)	((od & 0x3 & n) == 0)
					/* TRUE if reg #n is properly aligned */
#define MEMOP(od)	(od == M)	/* TRUE if operand is a memory operand*/

/* Description of a single i80960 instruction */
struct i960_opcode {
	long opcode;	/* 32 bits, constant fields filled in, rest zeroed */
	char *name;	/* Assembler mnemonic				   */
	short iclass;	/* Class: see #defines below			   */
	char format;	/* REG, COBR, CTRL, MEMn, COJ, FBRA, or CALLJ	   */
	char num_ops;	/* Number of operands				   */
	char operand[3];/* Operand descriptors; same order as assembler instr */
};

/* Classes of 960 intructions:
 *	- each instruction falls into one class.
 *	- each target architecture supports one or more classes.
 *
 * EACH CONSTANT MUST CONTAIN 1 AND ONLY 1 SET BIT!:  see targ_has_iclass().
 */
#define I_BASE	0x01	/* 80960 base instruction set	*/
#define I_CX	0x02	/* 80960Cx instruction		*/
#define I_DEC	0x04	/* Decimal instruction		*/
#define I_FP	0x08	/* Floating point instruction	*/
#define I_KX	0x10	/* 80960Kx instruction		*/
#define I_MIL	0x20	/* Military instruction		*/
#define I_CASIM	0x40	/* CA simulator instruction	*/

/******************************************************************************
 *
 *		TABLE OF i960 INSTRUCTION DESCRIPTIONS
 *
 ******************************************************************************/

const struct i960_opcode i960_opcodes[] = {

	/* if a CTRL instruction has an operand, it's always a displacement */

	{ 0x09000000,	"callj",	I_BASE,	CALLJ, 	1 },/*default=='call'*/
	{ 0x08000000,	"b",		I_BASE,	CTRL, 	1 },
	{ 0x09000000,	"call",		I_BASE,	CTRL, 	1 },
	{ 0x0a000000,	"ret",		I_BASE,	CTRL, 	0 },
	{ 0x0b000000,	"bal",		I_BASE,	CTRL, 	1 },
	{ 0x10000000,	"bno",		I_BASE,	CTRL, 	1 },
	{ 0x10000000,	"bf",		I_BASE,	CTRL, 	1 }, /* same as bno */
	{ 0x10000000,	"bru",		I_BASE,	CTRL, 	1 }, /* same as bno */
	{ 0x11000000,	"bg",		I_BASE,	CTRL, 	1 },
	{ 0x11000000,	"brg",		I_BASE,	CTRL, 	1 }, /* same as bg */
	{ 0x12000000,	"be",		I_BASE,	CTRL, 	1 },
	{ 0x12000000,	"bre",		I_BASE,	CTRL, 	1 }, /* same as be */
	{ 0x13000000,	"bge",		I_BASE,	CTRL, 	1 },
	{ 0x13000000,	"brge",		I_BASE,	CTRL, 	1 }, /* same as bge */
	{ 0x14000000,	"bl",		I_BASE,	CTRL, 	1 },
	{ 0x14000000,	"brl",		I_BASE,	CTRL, 	1 }, /* same as bl */
	{ 0x15000000,	"bne",		I_BASE,	CTRL, 	1 },
	{ 0x15000000,	"brlg",		I_BASE,	CTRL, 	1 }, /* same as bne */
	{ 0x16000000,	"ble",		I_BASE,	CTRL, 	1 },
	{ 0x16000000,	"brle",		I_BASE,	CTRL, 	1 }, /* same as ble */
	{ 0x17000000,	"bo",		I_BASE,	CTRL, 	1 },
	{ 0x17000000,	"bt",		I_BASE,	CTRL, 	1 }, /* same as bo */
	{ 0x17000000,	"bro",		I_BASE,	CTRL, 	1 }, /* same as bo */
	{ 0x18000000,	"faultno",	I_BASE,	CTRL, 	0 },
	{ 0x18000000,	"faultf",	I_BASE,	CTRL, 	0 }, /*same as faultno*/
	{ 0x19000000,	"faultg",	I_BASE,	CTRL, 	0 },
	{ 0x1a000000,	"faulte",	I_BASE,	CTRL, 	0 },
	{ 0x1b000000,	"faultge",	I_BASE,	CTRL, 	0 },
	{ 0x1c000000,	"faultl",	I_BASE,	CTRL, 	0 },
	{ 0x1d000000,	"faultne",	I_BASE,	CTRL, 	0 },
	{ 0x1e000000,	"faultle",	I_BASE,	CTRL, 	0 },
	{ 0x1f000000,	"faulto",	I_BASE,	CTRL, 	0 },
	{ 0x1f000000,	"faultt",	I_BASE,	CTRL, 	0 }, /* syn for faulto */

	{ 0x01000000,	"syscall",	I_CASIM,CTRL, 	0 },

	/* If a COBR (or COJ) has 3 operands, the last one is always a
	 * displacement and does not appear explicitly in the table.
	 */

	{ 0x20000000,	"testno",	I_BASE,	COBR,	1, R		},
	{ 0x21000000,	"testg",	I_BASE,	COBR,	1, R		},
	{ 0x22000000,	"teste",	I_BASE,	COBR,	1, R		},
	{ 0x23000000,	"testge",	I_BASE,	COBR,	1, R		},
	{ 0x24000000,	"testl",	I_BASE,	COBR,	1, R		},
	{ 0x25000000,	"testne",	I_BASE,	COBR,	1, R		},
	{ 0x26000000,	"testle",	I_BASE,	COBR,	1, R		},
	{ 0x27000000,	"testo",	I_BASE,	COBR,	1, R		},
	{ 0x30000000,	"bbc",		I_BASE,	COBR,	3, RL, RS	},
	{ 0x31000000,	"cmpobg",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x32000000,	"cmpobe",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x33000000,	"cmpobge",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x34000000,	"cmpobl",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x35000000,	"cmpobne",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x36000000,	"cmpoble",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x37000000,	"bbs",		I_BASE,	COBR,	3, RL, RS	},
	{ 0x38000000,	"cmpibno",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x39000000,	"cmpibg",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x3a000000,	"cmpibe",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x3b000000,	"cmpibge",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x3c000000,	"cmpibl",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x3d000000,	"cmpibne",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x3e000000,	"cmpible",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x3f000000,	"cmpibo",	I_BASE,	COBR,	3, RL, RS	},
	{ 0x31000000,	"cmpojg",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x32000000,	"cmpoje",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x33000000,	"cmpojge",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x34000000,	"cmpojl",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x35000000,	"cmpojne",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x36000000,	"cmpojle",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x38000000,	"cmpijno",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x39000000,	"cmpijg",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x3a000000,	"cmpije",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x3b000000,	"cmpijge",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x3c000000,	"cmpijl",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x3d000000,	"cmpijne",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x3e000000,	"cmpijle",	I_BASE,	COJ,	3, RL, RS	},
	{ 0x3f000000,	"cmpijo",	I_BASE,	COJ,	3, RL, RS	},

	{ 0x80000000,	"ldob",		I_BASE,	MEM1,	2, M,  R	},
	{ 0x82000000,	"stob",		I_BASE,	MEM1,	2, R , M	},
	{ 0x84000000,	"bx",		I_BASE,	MEM1,	1, M		},
	{ 0x85000000,	"balx",		I_BASE,	MEM1,	2, M,  R	},
	{ 0x86000000,	"callx",	I_BASE,	MEM1,	1, M		},
	{ 0x88000000,	"ldos",		I_BASE,	MEM2,	2, M,  R	},
	{ 0x8a000000,	"stos",		I_BASE,	MEM2,	2, R , M	},
	{ 0x8c000000,	"lda",		I_BASE,	MEM1,	2, M,  R	},
	{ 0x90000000,	"ld",		I_BASE,	MEM4,	2, M,  R	},
	{ 0x92000000,	"st",		I_BASE,	MEM4,	2, R , M	},
	{ 0x98000000,	"ldl",		I_BASE,	MEM8,	2, M,  R2	},
	{ 0x9a000000,	"stl",		I_BASE,	MEM8,	2, R2 ,M	},
	{ 0xa0000000,	"ldt",		I_BASE,	MEM12,	2, M,  R4	},
	{ 0xa2000000,	"stt",		I_BASE,	MEM12,	2, R4 ,M	},
	{ 0xb0000000,	"ldq",		I_BASE,	MEM16,	2, M,  R4	},
	{ 0xb2000000,	"stq",		I_BASE,	MEM16,	2, R4 ,M	},
	{ 0xc0000000,	"ldib",		I_BASE,	MEM1,	2, M,  R	},
	{ 0xc2000000,	"stib",		I_BASE,	MEM1,	2, R , M	},
	{ 0xc8000000,	"ldis",		I_BASE,	MEM2,	2, M,  R	},
	{ 0xca000000,	"stis",		I_BASE,	MEM2,	2, R , M	},

	{ R_3(0x580),	"notbit",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x581),	"and",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x582),	"andnot",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x583),	"setbit",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x584),	"notand",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x586),	"xor",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x587),	"or",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x588),	"nor",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x589),	"xnor",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_2D(0x58a),	"not",		I_BASE,	REG,	2, RSL,RS	},
	{ R_3(0x58b),	"ornot",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x58c),	"clrbit",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x58d),	"notor",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x58e),	"nand",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x58f),	"alterbit",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x590),	"addo",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x591),	"addi",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x592),	"subo",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x593),	"subi",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x598),	"shro",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x59a),	"shrdi",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x59b),	"shri",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x59c),	"shlo",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x59d),	"rotate",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x59e),	"shli",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_2(0x5a0),	"cmpo",		I_BASE,	REG,	2, RSL,RSL	},
	{ R_2(0x5a1),	"cmpi",		I_BASE,	REG,	2, RSL,RSL	},
	{ R_2(0x5a2),	"concmpo",	I_BASE,	REG,	2, RSL,RSL	},
	{ R_2(0x5a3),	"concmpi",	I_BASE,	REG,	2, RSL,RSL	},
	{ R_3(0x5a4),	"cmpinco",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x5a5),	"cmpinci",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x5a6),	"cmpdeco",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x5a7),	"cmpdeci",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_2(0x5ac),	"scanbyte",	I_BASE,	REG,	2, RSL,RSL	},
	{ R_2(0x5ae),	"chkbit",	I_BASE,	REG,	2, RSL,RSL	},
	{ R_3(0x5b0),	"addc",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x5b2),	"subc",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_2D(0x5cc),	"mov",		I_BASE,	REG,	2, RSL,RS	},
	{ R_2D(0x5dc),	"movl",		I_BASE,	REG,	2, RL2,R2	},
	{ R_2D(0x5ec),	"movt",		I_BASE,	REG,	2, RL4,R4	},
	{ R_2D(0x5fc),	"movq",		I_BASE,	REG,	2, RL4,R4	},
	{ R_3(0x610),	"atmod",	I_BASE,	REG,	3, RS, RSL,R	},
	{ R_3(0x612),	"atadd",	I_BASE,	REG,	3, RS, RSL,RS	},
	{ R_2D(0x640),	"spanbit",	I_BASE,	REG,	2, RSL,RS	},
	{ R_2D(0x641),	"scanbit",	I_BASE,	REG,	2, RSL,RS	},
	{ R_3(0x645),	"modac",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x650),	"modify",	I_BASE,	REG,	3, RSL,RSL,R	},
	{ R_3(0x651),	"extract",	I_BASE,	REG,	3, RSL,RSL,R	},
	{ R_3(0x654),	"modtc",	I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x655),	"modpc",	I_BASE,	REG,	3, RSL,RSL,R	},
	{ R_1(0x660),	"calls",	I_BASE,	REG,	1, RSL		},
	{ R_0(0x66b),	"mark",		I_BASE,	REG,	0,		},
	{ R_0(0x66c),	"fmark",	I_BASE,	REG,	0,		},
	{ R_0(0x66d),	"flushreg",	I_BASE,	REG,	0,		},
	{ R_0(0x66f),	"syncf",	I_BASE,	REG,	0,		},
	{ R_3(0x670),	"emul",		I_BASE,	REG,	3, RSL,RSL,R2	},
	{ R_3(0x671),	"ediv",		I_BASE,	REG,	3, RSL,RL2,RS	},
	{ R_2D(0x672),	"cvtadr",	I_CASIM,REG, 	2, RL, R2	},
	{ R_3(0x701),	"mulo",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x708),	"remo",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x70b),	"divo",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x741),	"muli",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x748),	"remi",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x749),	"modi",		I_BASE,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x74b),	"divi",		I_BASE,	REG,	3, RSL,RSL,RS	},

	/* Floating-point instructions */

	{ R_2D(0x674),	"cvtir",	I_FP,	REG,	2, RL, F	},
	{ R_2D(0x675),	"cvtilr",	I_FP,	REG,	2, RL, F	},
	{ R_3(0x676),	"scalerl",	I_FP,	REG,	3, RL, FL2,F2	},
	{ R_3(0x677),	"scaler",	I_FP,	REG,	3, RL, FL, F	},
	{ R_3(0x680),	"atanr",	I_FP,	REG,	3, FL, FL, F	},
	{ R_3(0x681),	"logepr",	I_FP,	REG,	3, FL, FL, F	},
	{ R_3(0x682),	"logr",		I_FP,	REG,	3, FL, FL, F	},
	{ R_3(0x683),	"remr",		I_FP,	REG,	3, FL, FL, F	},
	{ R_2(0x684),	"cmpor",	I_FP,	REG,	2, FL, FL	},
	{ R_2(0x685),	"cmpr",		I_FP,	REG,	2, FL, FL	},
	{ R_2D(0x688),	"sqrtr",	I_FP,	REG,	2, FL, F	},
	{ R_2D(0x689),	"expr",		I_FP,	REG,	2, FL, F	},
	{ R_2D(0x68a),	"logbnr",	I_FP,	REG,	2, FL, F	},
	{ R_2D(0x68b),	"roundr",	I_FP,	REG,	2, FL, F	},
	{ R_2D(0x68c),	"sinr",		I_FP,	REG,	2, FL, F	},
	{ R_2D(0x68d),	"cosr",		I_FP,	REG,	2, FL, F	},
	{ R_2D(0x68e),	"tanr",		I_FP,	REG,	2, FL, F	},
	{ R_1(0x68f),	"classr",	I_FP,	REG,	1, FL		},
	{ R_3(0x690),	"atanrl",	I_FP,	REG,	3, FL2,FL2,F2	},
	{ R_3(0x691),	"logeprl",	I_FP,	REG,	3, FL2,FL2,F2	},
	{ R_3(0x692),	"logrl",	I_FP,	REG,	3, FL2,FL2,F2	},
	{ R_3(0x693),	"remrl",	I_FP,	REG,	3, FL2,FL2,F2	},
	{ R_2(0x694),	"cmporl",	I_FP,	REG,	2, FL2,FL2	},
	{ R_2(0x695),	"cmprl",	I_FP,	REG,	2, FL2,FL2	},
	{ R_2D(0x698),	"sqrtrl",	I_FP,	REG,	2, FL2,F2	},
	{ R_2D(0x699),	"exprl",	I_FP,	REG,	2, FL2,F2	},
	{ R_2D(0x69a),	"logbnrl",	I_FP,	REG,	2, FL2,F2	},
	{ R_2D(0x69b),	"roundrl",	I_FP,	REG,	2, FL2,F2	},
	{ R_2D(0x69c),	"sinrl",	I_FP,	REG,	2, FL2,F2	},
	{ R_2D(0x69d),	"cosrl",	I_FP,	REG,	2, FL2,F2	},
	{ R_2D(0x69e),	"tanrl",	I_FP,	REG,	2, FL2,F2	},
	{ R_1(0x69f),	"classrl",	I_FP,	REG,	1, FL2		},
	{ R_2D(0x6c0),	"cvtri",	I_FP,	REG,	2, FL, R	},
	{ R_2D(0x6c1),	"cvtril",	I_FP,	REG,	2, FL, R2	},
	{ R_2D(0x6c2),	"cvtzri",	I_FP,	REG,	2, FL, R	},
	{ R_2D(0x6c3),	"cvtzril",	I_FP,	REG,	2, FL, R2	},
	{ R_2D(0x6c9),	"movr",		I_FP,	REG,	2, FL, F	},
	{ R_2D(0x6d9),	"movrl",	I_FP,	REG,	2, FL2,F2	},
	{ R_2D(0x6e1),	"movre",	I_FP,	REG,	2, FL4,F4	},
	{ R_3(0x6e2),	"cpysre",	I_FP,	REG,	3, FL4,FL4,F4	},
	{ R_3(0x6e3),	"cpyrsre",	I_FP,	REG,	3, FL4,FL4,F4	},
	{ R_3(0x78b),	"divr",		I_FP,	REG,	3, FL, FL, F	},
	{ R_3(0x78c),	"mulr",		I_FP,	REG,	3, FL, FL, F	},
	{ R_3(0x78d),	"subr",		I_FP,	REG,	3, FL, FL, F	},
	{ R_3(0x78f),	"addr",		I_FP,	REG,	3, FL, FL, F	},
	{ R_3(0x79b),	"divrl",	I_FP,	REG,	3, FL2,FL2,F2	},
	{ R_3(0x79c),	"mulrl",	I_FP,	REG,	3, FL2,FL2,F2	},
	{ R_3(0x79d),	"subrl",	I_FP,	REG,	3, FL2,FL2,F2	},
	{ R_3(0x79f),	"addrl",	I_FP,	REG,	3, FL2,FL2,F2	},

	/* These are the floating point branch instructions.  Each actually
	 * generates 2 branch instructions:  the first a CTRL instruction with
	 * the indicated opcode, and the second a 'bno'.
	 */

	{ 0x12000000,	"brue",		I_FP,	FBRA, 	1	},
	{ 0x11000000,	"brug",		I_FP,	FBRA, 	1	},
	{ 0x13000000,	"bruge",	I_FP,	FBRA, 	1	},
	{ 0x14000000,	"brul",		I_FP,	FBRA, 	1	},
	{ 0x16000000,	"brule",	I_FP,	FBRA, 	1	},
	{ 0x15000000,	"brulg",	I_FP,	FBRA, 	1	},


	/* Decimal instructions */

	{ R_3(0x642),	"daddc",	I_DEC,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x643),	"dsubc",	I_DEC,	REG,	3, RSL,RSL,RS	},
	{ R_2D(0x644),	"dmovt",	I_DEC,	REG,	2, RSL,RS	},


	/* KX extensions */

	{ R_2(0x600),	"synmov",	I_KX,	REG,	2, R,  R	},
	{ R_2(0x601),	"synmovl",	I_KX,	REG,	2, R,  R	},
	{ R_2(0x602),	"synmovq",	I_KX,	REG,	2, R,  R	},
	{ R_2D(0x615),	"synld",	I_KX,	REG,	2, R,  R	},


	/* MC extensions */

	{ R_3(0x603),	"cmpstr",	I_MIL,	REG,	3, R,  R,  RL	},
	{ R_3(0x604),	"movqstr",	I_MIL,	REG,	3, R,  R,  RL	},
	{ R_3(0x605),	"movstr",	I_MIL,	REG,	3, R,  R,  RL	},
	{ R_2D(0x613),	"inspacc",	I_MIL,	REG,	2, R,  R	},
	{ R_2D(0x614),	"ldphy",	I_MIL,	REG,	2, R,  R	},
	{ R_3(0x617),	"fill",		I_MIL,	REG,	3, R,  RL, RL	},
	{ R_2D(0x646),	"condrec",	I_MIL,	REG,	2, R,  R	},
	{ R_2D(0x656),	"receive",	I_MIL,	REG,	2, R,  R	},
	{ R_3(0x662),	"send",		I_MIL,	REG,	3, R,  RL, R	},
	{ R_1(0x663),	"sendserv",	I_MIL,	REG,	1, R		},
	{ R_1(0x664),	"resumprcs",	I_MIL,	REG,	1, R		},
	{ R_1(0x665),	"schedprcs",	I_MIL,	REG,	1, R		},
	{ R_0(0x666),	"saveprcs",	I_MIL,	REG,	0,		},
	{ R_1(0x668),	"condwait",	I_MIL,	REG,	1, R		},
	{ R_1(0x669),	"wait",		I_MIL,	REG,	1, R		},
	{ R_1(0x66a),	"signal",	I_MIL,	REG,	1, R		},
	{ R_1D(0x673),	"ldtime",	I_MIL,	REG,	1, R2		},


	/* CX extensions */

	{ R_3(0x5d8),	"eshro",	I_CX,	REG,	3, RSL,RSL,RS	},
	{ R_3(0x630),	"sdma",		I_CX,	REG,	3, RSL,RSL,RL	},
	{ R_3(0x631),	"udma",		I_CX,	REG,	0		},
	{ R_3(0x659),	"sysctl",	I_CX,	REG,	3, RSL,RSL,RL	},


	/* END OF TABLE */

	{ 0,		NULL,		0,	0 }
};

 /* end of i960-opcode.h */
