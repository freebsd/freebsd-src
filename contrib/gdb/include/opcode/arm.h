/* ARM opcode list.
   Copyright (C) 1989, Free Software Foundation, Inc.

This file is part of GDB and GAS.

GDB and GAS are free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 1, or (at your option)
any later version.

GDB and GAS are distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GDB or GAS; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */

/* types of instruction (encoded in bits 26 and 27 of the instruction) */

#define TYPE_ARITHMETIC		0
#define TYPE_LDR_STR		1
#define TYPE_BLOCK_BRANCH	2
#define TYPE_SWI		3

/* bit 25 decides whether an instruction is a block move or a branch */
#define SUBTYPE_BLOCK		0
#define SUBTYPE_BRANCH		1

/* codes to distinguish the arithmetic instructions */

#define OPCODE_AND	0
#define OPCODE_EOR	1
#define OPCODE_SUB	2
#define OPCODE_RSB	3
#define OPCODE_ADD	4
#define OPCODE_ADC	5
#define OPCODE_SBC	6
#define OPCODE_RSC	7
#define OPCODE_TST	8
#define OPCODE_TEQ	9
#define OPCODE_CMP	10
#define OPCODE_CMN	11
#define OPCODE_ORR	12
#define OPCODE_MOV	13
#define OPCODE_BIC	14
#define OPCODE_MVN	15

/* condition codes */

#define COND_EQ		0
#define COND_NE		1
#define COND_CS		2
#define COND_CC		3
#define COND_MI		4
#define COND_PL		5
#define COND_VS		6
#define COND_VC		7
#define COND_HI		8
#define COND_LS		9
#define COND_GE		10
#define COND_LT		11
#define COND_GT		12
#define COND_LE		13
#define COND_AL		14
#define COND_NV		15

/* Describes the format of an ARM machine instruction */

struct generic_fmt {
    unsigned rest	:25;	/* the rest of the instruction */
    unsigned subtype	:1;	/* used to decide between block and branch */
    unsigned type	:2;	/* one of TYPE_* */
    unsigned cond	:4;	/* one of COND_* defined above */
};

struct arith_fmt {
    unsigned operand2	:12;	/* #nn or rn or rn shift #m or rn shift rm */
    unsigned dest	:4;	/* place where the answer goes */
    unsigned operand1	:4;	/* first operand to instruction */
    unsigned set	:1;	/* == 1 means set processor flags */
    unsigned opcode	:4;	/* one of OPCODE_* defined above */
    unsigned immed	:1;	/* operand2 is an immediate value */
    unsigned type	:2;	/* == TYPE_ARITHMETIC */
    unsigned cond	:4;	/* one of COND_* defined above */
};

struct ldr_str_fmt {
    unsigned offset	:12;	/* #nn or rn or rn shift #m */
    unsigned reg	:4;	/* destination for LDR, source for STR */
    unsigned base	:4;	/* base register */
    unsigned is_load	:1;	/* == 1 for LDR */
    unsigned writeback	:1;	/* == 1 means write back (base+offset) into base */
    unsigned byte	:1;	/* == 1 means byte access else word */
    unsigned up		:1;	/* == 1 means add offset else subtract it */
    unsigned pre_index	:1;	/* == 1 means [a,b] form else [a],b form */
    unsigned immed	:1;	/* == 0 means immediate offset */
    unsigned type	:2;	/* == TYPE_LDR_STR */
    unsigned cond	:4;	/* one of COND_* defined above */
};

struct block_fmt {
    unsigned mask	:16;	/* register mask */
    unsigned base	:4;	/* register used as base of move */
    unsigned is_load	:1;	/* == 1 for LDM */
    unsigned writeback	:1;	/* == 1 means update base after move */
    unsigned set	:1;	/* == 1 means set flags in pc if included in mask */
    unsigned increment	:1;	/* == 1 means increment base register */
    unsigned before	:1;	/* == 1 means inc/dec before each move */
    unsigned is_block	:1;	/* == SUBTYPE_BLOCK */
    unsigned type	:2;	/* == TYPE_BLOCK_BRANCH */
    unsigned cond	:4;	/* one of COND_* defined above */
};

struct branch_fmt {
    unsigned dest	:24;	/* destination of the branch */
    unsigned link	:1;	/* branch with link (function call) */
    unsigned is_branch	:1;	/* == SUBTYPE_BRANCH */
    unsigned type	:2;	/* == TYPE_BLOCK_BRANCH */
    unsigned cond	:4;	/* one of COND_* defined above */
};

#define ROUND_N		0
#define ROUND_P		1
#define ROUND_M		2
#define ROUND_Z		3

#define FLOAT2_MVF	0
#define FLOAT2_MNF	1
#define FLOAT2_ABS	2
#define FLOAT2_RND	3
#define FLOAT2_SQT	4
#define FLOAT2_LOG	5
#define FLOAT2_LGN	6
#define FLOAT2_EXP	7
#define FLOAT2_SIN	8
#define FLOAT2_COS	9
#define FLOAT2_TAN	10
#define FLOAT2_ASN	11
#define FLOAT2_ACS	12
#define FLOAT2_ATN	13

#define FLOAT3_ADF	0
#define FLOAT3_MUF	1
#define FLOAT3_SUF	2
#define FLOAT3_RSF	3
#define FLOAT3_DVF	4
#define FLOAT3_RDF	5
#define FLOAT3_POW	6
#define FLOAT3_RPW	7
#define FLOAT3_RMF	8
#define FLOAT3_FML	9
#define FLOAT3_FDV	10
#define FLOAT3_FRD	11
#define FLOAT3_POL	12

struct float2_fmt {
    unsigned operand2	:3;	/* second operand */
    unsigned immed	:1;	/* == 1 if second operand is a constant */
    unsigned pad1	:1;	/* == 0 */
    unsigned rounding	:2;	/* ROUND_* */
    unsigned is_double	:1;	/* == 1 if precision is double (only if not extended) */
    unsigned pad2	:4;	/* == 1 */
    unsigned dest	:3;	/* destination */
    unsigned is_2_op	:1;	/* == 1 if 2 operand ins */
    unsigned operand1	:3;	/* first operand (only of is_2_op == 0) */
    unsigned is_extended :1;	/* == 1 if precision is extended */
    unsigned opcode	:4;	/* FLOAT2_* or FLOAT3_* depending on is_2_op */
    unsigned must_be_2	:2;	/* == 2 */
    unsigned type	:2;	/* == TYPE_SWI */
    unsigned cond	:4;	/* COND_* */
};

struct swi_fmt {
    unsigned argument	:24;	/* argument to SWI (syscall number) */
    unsigned must_be_3	:2;	/* == 3 */
    unsigned type	:2;	/* == TYPE_SWI */
    unsigned cond	:4;	/* one of COND_* defined above */
};

union insn_fmt {
    struct generic_fmt	generic;
    struct arith_fmt	arith;
    struct ldr_str_fmt	ldr_str;
    struct block_fmt	block;
    struct branch_fmt	branch;
    struct swi_fmt	swi;
    unsigned long	ins;
};

struct opcode {
    unsigned long value, mask;	/* recognise instruction if (op&mask)==value */
    char *assembler;		/* how to disassemble this instruction */
};

/* format of the assembler string :
   
   %%			%
   %<bitfield>d		print the bitfield in decimal
   %<bitfield>x		print the bitfield in hex
   %<bitfield>r		print as an ARM register
   %<bitfield>f		print a floating point constant if >7 else an fp register
   %c			print condition code (always bits 28-31)
   %P			print floating point precision in arithmetic insn
   %Q			print floating point precision in ldf/stf insn
   %R			print floating point rounding mode
   %<bitnum>'c		print specified char iff bit is one
   %<bitnum>`c		print specified char iff bit is zero
   %<bitnum>?ab		print a if bit is one else print b
   %p			print 'p' iff bits 12-15 are 15
   %o			print operand2 (immediate or register + shift)
   %a			print address for ldr/str instruction
   %b			print branch destination
   %A			print address for ldc/stc/ldf/stf instruction
   %m			print register mask for ldm/stm instruction
*/

static struct opcode opcodes[] = {
    /* ARM instructions */
    0x00000090, 0x0fe000f0, "mul%20's %12-15r, %16-19r, %0-3r",
    0x00200090, 0x0fe000f0, "mla%20's %12-15r, %16-19r, %0-3r, %8-11r",
    0x00000000, 0x0de00000, "and%c%20's %12-15r, %16-19r, %o",
    0x00200000, 0x0de00000, "eor%c%20's %12-15r, %16-19r, %o",
    0x00400000, 0x0de00000, "sub%c%20's %12-15r, %16-19r, %o",
    0x00600000, 0x0de00000, "rsb%c%20's %12-15r, %16-19r, %o",
    0x00800000, 0x0de00000, "add%c%20's %12-15r, %16-19r, %o",
    0x00a00000, 0x0de00000, "adc%c%20's %12-15r, %16-19r, %o",
    0x00c00000, 0x0de00000, "sbc%c%20's %12-15r, %16-19r, %o",
    0x00e00000, 0x0de00000, "rsc%c%20's %12-15r, %16-19r, %o",
    0x01000000, 0x0de00000, "tst%c%p %16-19r, %o",
    0x01200000, 0x0de00000, "teq%c%p %16-19r, %o",
    0x01400000, 0x0de00000, "cmp%c%p %16-19r, %o",
    0x01600000, 0x0de00000, "cmn%c%p %16-19r, %o",
    0x01800000, 0x0de00000, "orr%c%20's %12-15r, %16-19r, %o",
    0x01a00000, 0x0de00000, "mov%c%20's %12-15r, %o",
    0x01c00000, 0x0de00000, "bic%c%20's %12-15r, %16-19r, %o",
    0x01e00000, 0x0de00000, "mvn%c%20's %12-15r, %o",
    0x04000000, 0x0c100000, "str%c%22'b %12-15r, %a",
    0x04100000, 0x0c100000, "ldr%c%22'b %12-15r, %a",
    0x08000000, 0x0e100000, "stm%c%23?id%24?ba %16-19r%22`!, %m",
    0x08100000, 0x0e100000, "ldm%c%23?id%24?ba %16-19r%22`!, %m%22'^",
    0x0a000000, 0x0e000000, "b%c%24'l %b",
    0x0f000000, 0x0f000000, "swi%c %0-23x",
    /* Floating point coprocessor instructions */
    0x0e000100, 0x0ff08f10, "adf%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e100100, 0x0ff08f10, "muf%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e200100, 0x0ff08f10, "suf%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e300100, 0x0ff08f10, "rsf%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e400100, 0x0ff08f10, "dvf%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e500100, 0x0ff08f10, "rdf%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e600100, 0x0ff08f10, "pow%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e700100, 0x0ff08f10, "rpw%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e800100, 0x0ff08f10, "rmf%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e900100, 0x0ff08f10, "fml%c%P%R %12-14f, %16-18f, %0-3f",
    0x0ea00100, 0x0ff08f10, "fdv%c%P%R %12-14f, %16-18f, %0-3f",
    0x0eb00100, 0x0ff08f10, "frd%c%P%R %12-14f, %16-18f, %0-3f",
    0x0ec00100, 0x0ff08f10, "pol%c%P%R %12-14f, %16-18f, %0-3f",
    0x0e008100, 0x0ff08f10, "mvf%c%P%R %12-14f, %0-3f",
    0x0e108100, 0x0ff08f10, "mnf%c%P%R %12-14f, %0-3f",
    0x0e208100, 0x0ff08f10, "abs%c%P%R %12-14f, %0-3f",
    0x0e308100, 0x0ff08f10, "rnd%c%P%R %12-14f, %0-3f",
    0x0e408100, 0x0ff08f10, "sqt%c%P%R %12-14f, %0-3f",
    0x0e508100, 0x0ff08f10, "log%c%P%R %12-14f, %0-3f",
    0x0e608100, 0x0ff08f10, "lgn%c%P%R %12-14f, %0-3f",
    0x0e708100, 0x0ff08f10, "exp%c%P%R %12-14f, %0-3f",
    0x0e808100, 0x0ff08f10, "sin%c%P%R %12-14f, %0-3f",
    0x0e908100, 0x0ff08f10, "cos%c%P%R %12-14f, %0-3f",
    0x0ea08100, 0x0ff08f10, "tan%c%P%R %12-14f, %0-3f",
    0x0eb08100, 0x0ff08f10, "asn%c%P%R %12-14f, %0-3f",
    0x0ec08100, 0x0ff08f10, "acs%c%P%R %12-14f, %0-3f",
    0x0ed08100, 0x0ff08f10, "atn%c%P%R %12-14f, %0-3f",
    0x0e000110, 0x0ff00f1f, "flt%c%P%R %16-18f, %12-15r",
    0x0e100110, 0x0fff0f98, "fix%c%R %12-15r, %0-2f",
    0x0e200110, 0x0fff0fff, "wfs%c %12-15r",
    0x0e300110, 0x0fff0fff, "rfs%c %12-15r",
    0x0e400110, 0x0fff0fff, "wfc%c %12-15r",
    0x0e500110, 0x0fff0fff, "rfc%c %12-15r",
    0x0e90f110, 0x0ff8fff0, "cmf%c %16-18f, %0-3f",
    0x0eb0f110, 0x0ff8fff0, "cnf%c %16-18f, %0-3f",
    0x0ed0f110, 0x0ff8fff0, "cmfe%c %16-18f, %0-3f",
    0x0ef0f110, 0x0ff8fff0, "cnfe%c %16-18f, %0-3f",
    0x0c000100, 0x0e100f00, "stf%c%Q %12-14f, %A",
    0x0c100100, 0x0e100f00, "ldf%c%Q %12-14f, %A",
    /* Generic coprocessor instructions */
    0x0e000000, 0x0f000010, "cdp%c %8-11d, %20-23d, cr%12-15d, cr%16-19d, cr%0-3d, {%5-7d}",
    0x0e000010, 0x0f100010, "mrc%c %8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}",
    0x0e100010, 0x0f100010, "mcr%c %8-11d, %21-23d, %12-15r, cr%16-19d, cr%0-3d, {%5-7d}",
    0x0c000000, 0x0e100000, "stc%c%22`l %8-11d, cr%12-15d, %A",
    0x0c100000, 0x0e100000, "ldc%c%22`l %8-11d, cr%12-15d, %A",
    /* the rest */
    0x00000000, 0x00000000, "undefined instruction %0-31x",
};
#define N_OPCODES	(sizeof opcodes / sizeof opcodes[0])
