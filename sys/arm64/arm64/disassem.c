/*-
 * Copyright (c) 2016 Cavium
 * All rights reserved.
 *
 * This software was developed by Semihalf.
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/armreg.h>
#include <machine/disassem.h>

#include <ddb/ddb.h>

#define	ARM64_MAX_TOKEN_LEN	8
#define	ARM64_MAX_TOKEN_CNT	10

#define	ARM_INSN_SIZE_OFFSET	30
#define	ARM_INSN_SIZE_MASK	0x3

/* Special options for instruction printing */
#define	OP_SIGN_EXT	(1UL << 0)	/* Sign-extend immediate value */
#define	OP_LITERAL	(1UL << 1)	/* Use literal (memory offset) */
#define	OP_MULT_4	(1UL << 2)	/* Multiply immediate by 4 */
#define	OP_SF32		(1UL << 3)	/* Force 32-bit access */
#define	OP_SF_INV	(1UL << 6)	/* SF is inverted (1 means 32 bit access) */
#define	OP_RD_SP	(1UL << 7)	/* Use sp for RD otherwise xzr */
#define	OP_RT_SP	(1UL << 8)	/* Use sp for RT otherwise xzr */
#define	OP_RN_SP	(1UL << 9)	/* Use sp for RN otherwise xzr */
#define	OP_RM_SP	(1UL << 10)	/* Use sp for RM otherwise xzr */
#define	OP_SHIFT_ROR	(1UL << 11)	/* Use ror shift type */

static const char *w_reg[] = {
	"w0", "w1", "w2", "w3", "w4", "w5", "w6", "w7",
	"w8", "w9", "w10", "w11", "w12", "w13", "w14", "w15",
	"w16", "w17", "w18", "w19", "w20", "w21", "w22", "w23",
	"w24", "w25", "w26", "w27", "w28", "w29", "w30"
};

static const char *x_reg[] = {
	"x0", "x1", "x2", "x3", "x4", "x5", "x6", "x7",
	"x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15",
	"x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23",
	"x24", "x25", "x26", "x27", "x28", "x29", "lr"
};

static const char *shift_2[] = {
	"lsl", "lsr", "asr", "ror"
};

static const char *extend_types[] = {
	"uxtb", "uxth", "uxtw", "uxtx",
	"sxtb", "sxth", "sxtw", "sxtx",
};

/*
 * Structure representing single token (operand) inside instruction.
 * name   - name of operand
 * pos    - position within the instruction (in bits)
 * len    - operand length (in bits)
 */
struct arm64_insn_token {
	char name[ARM64_MAX_TOKEN_LEN];
	int pos;
	int len;
};

/*
 * Define generic types for instruction printing.
 */
enum arm64_format_type {
	/*
	 * OP <RD>, <RN>, <RM>{, <shift [LSL, LSR, ASR]> #imm} SF32/64
	 * OP <RD>, <RN>, #<imm>{, <shift [0, 12]>} SF32/64
	 * OP <RD>, <RM> {, <shift> #<imm> }
	 * OP <RN>, <RM> {, <shift> #<imm> }
	 */
	TYPE_01,

	/*
	 * OP <RT>, [<XN|SP>, #<simm>]!
	 * OP <RT>, [<XN|SP>], #<simm>
	 * OP <RT>, [<XN|SP> {, #<pimm> }]
	 * OP <RT>, [<XN|SP>, <RM> {, EXTEND AMOUNT }]
	 */
	TYPE_02,

	/* OP <RT>, #imm SF32/64 */
	TYPE_03,

	/*
	 * OP <RD>, <RN|SP>, <RM> {, <extend> { #<amount> } }
	 * OP <RN|SP>, <RM>, {, <extend> { #<amount> } }
	 */
	TYPE_04,
};

/*
 * Structure representing single parsed instruction format.
 * name   - opcode name
 * format - opcode format in a human-readable way
 * type   - syntax type for printing
 * special_ops  - special options passed to a printer (if any)
 * mask   - bitmask for instruction matching
 * pattern      - pattern to look for
 * tokens - array of tokens (operands) inside instruction
 */
struct arm64_insn {
	char *name;
	char *format;
	enum arm64_format_type type;
	uint64_t special_ops;
	uint32_t mask;
	uint32_t pattern;
	struct arm64_insn_token tokens[ARM64_MAX_TOKEN_CNT];
};

/*
 * Specify instruction opcode format in a human-readable way. Use notation
 * obtained from ARM Architecture Reference Manual for ARMv8-A.
 *
 * Format string description:
 *  Each group must be separated by "|". Group made of 0/1 is used to
 *  generate mask and pattern for instruction matching. Groups containing
 *  an operand token (in format NAME(length_bits)) are used to retrieve any
 *  operand data from the instruction. Names here must be meaningful
 *  and match the one described in the Manual.
 *
 * Token description:
 * SF     - "0" represents 32-bit access, "1" represents 64-bit access
 * SHIFT  - type of shift (instruction dependent)
 * IMM    - immediate value
 * Rx     - register number
 * OPTION - command specific options
 * SCALE  - scaling of immediate value
 */
static struct arm64_insn arm64_i[] = {
	{ "add", "SF(1)|0001011|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, 0 },			/* add shifted register */
	{ "mov", "SF(1)|001000100000000000000|RN(5)|RD(5)",
	    TYPE_01, OP_RD_SP | OP_RN_SP },	/* mov (to/from sp) */
	{ "add", "SF(1)|0010001|SHIFT(2)|IMM(12)|RN(5)|RD(5)",
	    TYPE_01, OP_RD_SP | OP_RN_SP },	/* add immediate */
	{ "cmn", "SF(1)|0101011|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|11111",
	    TYPE_01, 0 },			/* cmn shifted register */
	{ "adds", "SF(1)|0101011|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, 0 },			/* adds shifted register */
	{ "ldr", "1|SF(1)|111000010|IMM(9)|OPTION(2)|RN(5)|RT(5)",
	    TYPE_02, OP_SIGN_EXT },
	    /* ldr immediate post/pre index */
	{ "ldr", "1|SF(1)|11100101|IMM(12)|RN(5)|RT(5)",
	    TYPE_02, 0 },			/* ldr immediate unsigned */
	{ "ldr", "1|SF(1)|111000011|RM(5)|OPTION(3)|SCALE(1)|10|RN(5)|RT(5)",
	    TYPE_02, 0 },			/* ldr register */
	{ "ldr", "0|SF(1)|011000|IMM(19)|RT(5)",
	    TYPE_03, OP_SIGN_EXT | OP_LITERAL | OP_MULT_4 },	/* ldr literal */
	{ "ldrb", "00|111000010|IMM(9)|OPTION(2)|RN(5)|RT(5)",
	    TYPE_02, OP_SIGN_EXT | OP_SF32 },
	    /* ldrb immediate post/pre index */
	{ "ldrb", "00|11100101|IMM(12)|RN(5)|RT(5)",
	    TYPE_02, OP_SF32 },			/* ldrb immediate unsigned */
	{ "ldrb", "00|111000011|RM(5)|OPTION(3)|SCALE(1)|10|RN(5)|RT(5)",
	    TYPE_02, OP_SF32 },			/* ldrb register */
	{ "ldrh", "01|111000010|IMM(9)|OPTION(2)|RN(5)|RT(5)", TYPE_02,
	    OP_SIGN_EXT | OP_SF32 },
	    /* ldrh immediate post/pre index */
	{ "ldrh", "01|11100101|IMM(12)|RN(5)|RT(5)",
	    TYPE_02, OP_SF32 },			/* ldrh immediate unsigned */
	{ "ldrh", "01|111000011|RM(5)|OPTION(3)|SCALE(1)|10|RN(5)|RT(5)",
	    TYPE_02, OP_SF32 },			/* ldrh register */
	{ "ldrsb", "001110001|SF(1)|0|IMM(9)|OPTION(2)|RN(5)|RT(5)",
	    TYPE_02, OP_SIGN_EXT | OP_SF_INV },
	    /* ldrsb immediate post/pre index */
	{ "ldrsb", "001110011|SF(1)|IMM(12)|RN(5)|RT(5)",\
	    TYPE_02, OP_SF_INV },		/* ldrsb immediate unsigned */
	{ "ldrsb", "001110001|SF(1)|1|RM(5)|OPTION(3)|SCALE(1)|10|RN(5)|RT(5)",
	    TYPE_02,  OP_SF_INV },		/* ldrsb register */
	{ "ldrsh", "011110001|SF(1)|0|IMM(9)|OPTION(2)|RN(5)|RT(5)",
	    TYPE_02, OP_SIGN_EXT | OP_SF_INV },
	    /* ldrsh immediate post/pre index */
	{ "ldrsh", "011110011|SF(1)|IMM(12)|RN(5)|RT(5)",
	    TYPE_02, OP_SF_INV },		/* ldrsh immediate unsigned */
	{ "ldrsh", "011110001|SF(1)|1|RM(5)|OPTION(3)|SCALE(1)|10|RN(5)|RT(5)",
	    TYPE_02, OP_SF_INV },		/* ldrsh register */
	{ "ldrsw", "10111000100|IMM(9)|OPTION(2)|RN(5)|RT(5)",
	    TYPE_02, OP_SIGN_EXT },
	    /* ldrsw immediate post/pre index */
	{ "ldrsw", "1011100110|IMM(12)|RN(5)|RT(5)",
	    TYPE_02, 0 },			/* ldrsw immediate unsigned */
	{ "ldrsw", "10111000101|RM(5)|OPTION(3)|SCALE(1)|10|RN(5)|RT(5)",
	    TYPE_02, 0 },			/* ldrsw register */
	{ "ldrsw", "10011000|IMM(19)|RT(5)",
	    TYPE_03, OP_SIGN_EXT | OP_LITERAL | OP_MULT_4 },	/* ldrsw literal */
	{ "str", "1|SF(1)|111000000|IMM(9)|OPTION(2)|RN(5)|RT(5)",
	    TYPE_02, OP_SIGN_EXT },
	    /* str immediate post/pre index */
	{ "str", "1|SF(1)|11100100|IMM(12)|RN(5)|RT(5)",
	    TYPE_02, 0 },			/* str immediate unsigned */
	{ "str", "1|SF(1)|111000001|RM(5)|OPTION(3)|SCALE(1)|10|RN(5)|RT(5)",
	    TYPE_02, 0 },			/* str register */
	{ "strb", "00111000000|IMM(9)|OPTION(2)|RN(5)|RT(5)",
	    TYPE_02, OP_SIGN_EXT | OP_SF32 },
	    /* strb immediate post/pre index */
	{ "strb", "0011100100|IMM(12)|RN(5)|RT(5)",
	    TYPE_02, OP_SF32 },			/* strb immediate unsigned */
	{ "strb", "00111000001|RM(5)|OPTION(3)|SCALE(1)|10|RN(5)|RT(5)",
	    TYPE_02, OP_SF32 },			/* strb register */
	{ "strh", "01111000000|IMM(9)|OPTION(2)|RN(5)|RT(5)",
	    TYPE_02, OP_SF32 | OP_SIGN_EXT },
	    /* strh immediate post/pre index */
	{ "strh", "0111100100|IMM(12)|RN(5)|RT(5)",
	    TYPE_02, OP_SF32 },
	    /* strh immediate unsigned */
	{ "strh", "01111000001|RM(5)|OPTION(3)|SCALE(1)|10|RN(5)|RT(5)",
	    TYPE_02, OP_SF32 },
	    /* strh register */
	{ "neg", "SF(1)|1001011|SHIFT(2)|0|RM(5)|IMM(6)|11111|RD(5)",
	    TYPE_01, 0 },			/* neg shifted register */
	{ "sub", "SF(1)|1001011|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, 0 },			/* sub shifted register */
	{ "cmp", "SF(1)|1101011|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|11111",
	    TYPE_01, 0 },			/* cmp shifted register */
	{ "negs", "SF(1)|1101011|SHIFT(2)|0|RM(5)|IMM(6)|11111|RD(5)",
	    TYPE_01, 0 },			/* negs shifted register */
	{ "subs", "SF(1)|1101011|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, 0 },			/* subs shifted register */
	{ "mvn", "SF(1)|0101010|SHIFT(2)|1|RM(5)|IMM(6)|11111|RD(5)",
	    TYPE_01, OP_SHIFT_ROR },		/* mvn shifted register */
	{ "orn", "SF(1)|0101010|SHIFT(2)|1|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, OP_SHIFT_ROR },		/* orn shifted register */
	{ "mov", "SF(1)|0101010000|RM(5)|000000|11111|RD(5)",
	    TYPE_01, 0 },			/* mov register */
	{ "orr", "SF(1)|0101010|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, OP_SHIFT_ROR },		/* orr shifted register */
	{ "and", "SF(1)|0001010|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, OP_SHIFT_ROR },		/* and shifted register */
	{ "tst", "SF(1)|1101010|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|11111",
	    TYPE_01, OP_SHIFT_ROR },		/* tst shifted register */
	{ "ands", "SF(1)|1101010|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, OP_SHIFT_ROR },		/* ands shifted register */
	{ "bic", "SF(1)|0001010|SHIFT(2)|1|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, OP_SHIFT_ROR },		/* bic shifted register */
	{ "bics", "SF(1)|1101010|SHIFT(2)|1|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, OP_SHIFT_ROR },		/* bics shifted register */
	{ "eon", "SF(1)|1001010|SHIFT(2)|1|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, OP_SHIFT_ROR },		/* eon shifted register */
	{ "eor", "SF(1)|1001010|SHIFT(2)|0|RM(5)|IMM(6)|RN(5)|RD(5)",
	    TYPE_01, OP_SHIFT_ROR },		/* eor shifted register */
	{ "add", "SF(1)|0001011001|RM(5)|OPTION(3)|IMM(3)|RN(5)|RD(5)",
	    TYPE_04, OP_RD_SP },		/* add extended register */
	{ "cmn", "SF(1)|0101011001|RM(5)|OPTION(3)|IMM(3)|RN(5)|11111",
	    TYPE_04, 0 },			/* cmn extended register */
	{ "adds", "SF(1)|0101011001|RM(5)|OPTION(3)|IMM(3)|RN(5)|RD(5)",
	    TYPE_04, 0 },			/* adds extended register */
	{ "sub", "SF(1)|1001011001|RM(5)|OPTION(3)|IMM(3)|RN(5)|RD(5)",
	    TYPE_04, OP_RD_SP },		/* sub extended register */
	{ "cmp", "SF(1)|1101011001|RM(5)|OPTION(3)|IMM(3)|RN(5)|11111",
	    TYPE_04, 0 },			/* cmp extended register */
	{ "subs", "SF(1)|1101011001|RM(5)|OPTION(3)|IMM(3)|RN(5)|RD(5)",
	    TYPE_04, 0 },			/* subs extended register */
	{ NULL, NULL }
};

static void
arm64_disasm_generate_masks(struct arm64_insn *tab)
{
	uint32_t mask, val;
	int a, i;
	int len, ret;
	int token = 0;
	char *format;
	int error;

	while (tab->name != NULL) {
		mask = 0;
		val = 0;
		format = tab->format;
		token = 0;
		error = 0;

		/*
		 * For each entry analyze format strings from the
		 * left (i.e. from the MSB).
		 */
		a = (INSN_SIZE * NBBY) - 1;
		while (*format != '\0' && (a >= 0)) {
			switch (*format) {
			case '0':
				/* Bit is 0, add to mask and pattern */
				mask |= (1 << a);
				a--;
				format++;
				break;
			case '1':
				/* Bit is 1, add to mask and pattern */
				mask |= (1 << a);
				val |= (1 << a);
				a--;
				format++;
				break;
			case '|':
				/* skip */
				format++;
				break;
			default:
				/* Token found, copy the name */
				memset(tab->tokens[token].name, 0,
				    sizeof(tab->tokens[token].name));
				i = 0;
				while (*format != '(') {
					tab->tokens[token].name[i] = *format;
					i++;
					format++;
					if (i >= ARM64_MAX_TOKEN_LEN) {
						printf("ERROR: "
						    "token too long in op %s\n",
						    tab->name);
						error = 1;
						break;
					}
				}
				if (error != 0)
					break;

				/* Read the length value */
				ret = sscanf(format, "(%d)", &len);
				if (ret == 1) {
					if (token >= ARM64_MAX_TOKEN_CNT) {
						printf("ERROR: "
						    "too many tokens in op %s\n",
						    tab->name);
						error = 1;
						break;
					}

					a -= len;
					tab->tokens[token].pos = a + 1;
					tab->tokens[token].len = len;
					token++;
				}

				/* Skip to the end of the token */
				while (*format != 0 && *format != '|')
					format++;
			}
		}

		/* Write mask and pattern to the instruction array */
		tab->mask = mask;
		tab->pattern = val;

		/*
		 * If we got here, format string must be parsed and "a"
		 * should point to -1. If it's not, wrong number of bits
		 * in format string. Mark this as invalid and prevent
		 * from being matched.
		 */
		if (*format != 0 || (a != -1) || (error != 0)) {
			tab->mask = 0;
			tab->pattern = 0xffffffff;
			printf("ERROR: skipping instruction op %s\n",
			    tab->name);
		}

		tab++;
	}
}

static int
arm64_disasm_read_token(struct arm64_insn *insn, u_int opcode,
    const char *token, int *val)
{
	int i;

	for (i = 0; i < ARM64_MAX_TOKEN_CNT; i++) {
		if (strcmp(insn->tokens[i].name, token) == 0) {
			*val = (opcode >> insn->tokens[i].pos &
			    ((1 << insn->tokens[i].len) - 1));
			return (0);
		}
	}

	return (EINVAL);
}

static int
arm64_disasm_read_token_sign_ext(struct arm64_insn *insn, u_int opcode,
    const char *token, int *val)
{
	int i;
	int msk;

	for (i = 0; i < ARM64_MAX_TOKEN_CNT; i++) {
		if (strcmp(insn->tokens[i].name, token) == 0) {
			msk = (1 << insn->tokens[i].len) - 1;
			*val = ((opcode >> insn->tokens[i].pos) & msk);

			/* If last bit is 1, sign-extend the value */
			if (*val & (1 << (insn->tokens[i].len - 1)))
				*val |= ~msk;

			return (0);
		}
	}

	return (EINVAL);
}

static const char *
arm64_disasm_reg_extend(int sf, int option, int rd, int rn, int amount)
{
	bool is_sp, lsl_preferred_uxtw, lsl_preferred_uxtx, lsl_preferred;

	is_sp = rd == 31 || rn == 31;
	lsl_preferred_uxtw = sf == 0 && option == 2;
	lsl_preferred_uxtx = sf == 1 && option == 3;
	lsl_preferred = is_sp && (lsl_preferred_uxtw || lsl_preferred_uxtx);

	/*
	 * LSL may be omitted when <amount> is 0.
	 * In all other cases <extend> is required.
	 */
	if (lsl_preferred && amount == 0)
		return (NULL);
	if (lsl_preferred)
		return ("lsl");
	return (extend_types[option]);
}

static const char *
arm64_w_reg(int num, int wsp)
{
	if (num == 31)
		return (wsp != 0 ? "wsp" : "wzr");
	return (w_reg[num]);
}

static const char *
arm64_x_reg(int num, int sp)
{
	if (num == 31)
		return (sp != 0 ? "sp" : "xzr");
	return (x_reg[num]);
}

static const char *
arm64_reg(int b64, int num, int sp)
{
	if (b64 != 0)
		return (arm64_x_reg(num, sp));
	return (arm64_w_reg(num, sp));
}

/*
 * Decodes OPTION(3) to get <Xn|Wn> register or <WZR|XZR>
 * for extended register instruction.
 */
static const char *
arm64_disasm_reg_width(int option, int reg)
{
	if (option == 3 || option == 7)
		return (arm64_x_reg(reg, 0));
	return (arm64_w_reg(reg, 0));
}

vm_offset_t
disasm(const struct disasm_interface *di, vm_offset_t loc, int altfmt)
{
	struct arm64_insn *i_ptr = arm64_i;
	uint32_t insn;
	int matchp;
	int ret;
	int shift, rm, rt, rd, rn, imm, sf, idx, option, scale, amount;
	int sign_ext;
	bool rm_absent, rd_absent, rn_absent;
	/* Indicate if immediate should be outside or inside brackets */
	int inside;
	/* Print exclamation mark if pre-incremented */
	int pre;
	/* Indicate if x31 register should be printed as sp or xzr */
	int rm_sp, rt_sp, rd_sp, rn_sp;
	/* Indicate if shift type ror is supported */
	bool has_shift_ror;

	const char *extend;

	/* Initialize defaults, all are 0 except SF indicating 64bit access */
	shift = rd = rm = rn = imm = idx = option = amount = scale = 0;
	sign_ext = 0;
	sf = 1;
	extend = NULL;

	matchp = 0;
	insn = di->di_readword(loc);
	while (i_ptr->name) {
		/* If mask is 0 then the parser was not initialized yet */
		if ((i_ptr->mask != 0) &&
		    ((insn & i_ptr->mask) == i_ptr->pattern)) {
			matchp = 1;
			break;
		}
		i_ptr++;
	}
	if (matchp == 0)
		goto undefined;

	/* Global options */
	if (i_ptr->special_ops & OP_SF32)
		sf = 0;

	/* Global optional tokens */
	arm64_disasm_read_token(i_ptr, insn, "SF", &sf);
	if (i_ptr->special_ops & OP_SF_INV)
		sf = 1 - sf;
	if (arm64_disasm_read_token(i_ptr, insn, "SIGN", &sign_ext) == 0)
		sign_ext = 1 - sign_ext;
	if (i_ptr->special_ops & OP_SIGN_EXT)
		sign_ext = 1;
	if (sign_ext != 0)
		arm64_disasm_read_token_sign_ext(i_ptr, insn, "IMM", &imm);
	else
		arm64_disasm_read_token(i_ptr, insn, "IMM", &imm);
	if (i_ptr->special_ops & OP_MULT_4)
		imm <<= 2;

	rm_sp = i_ptr->special_ops & OP_RM_SP;
	rt_sp = i_ptr->special_ops & OP_RT_SP;
	rd_sp = i_ptr->special_ops & OP_RD_SP;
	rn_sp = i_ptr->special_ops & OP_RN_SP;

	has_shift_ror = i_ptr->special_ops & OP_SHIFT_ROR;

	/* Print opcode by type */
	switch (i_ptr->type) {
	case TYPE_01:
		/*
		 * OP <RD>, <RN>, <RM>{, <shift [LSL, LSR, ASR]> #<imm>} SF32/64
		 * OP <RD>, <RN>, #<imm>{, <shift [0, 12]>} SF32/64
		 * OP <RD>, <RM> {, <shift> #<imm> }
		 * OP <RN>, <RM> {, <shift> #<imm> }
		 */

		rd_absent = arm64_disasm_read_token(i_ptr, insn, "RD", &rd);
		rn_absent = arm64_disasm_read_token(i_ptr, insn, "RN", &rn);
		rm_absent = arm64_disasm_read_token(i_ptr, insn, "RM", &rm);
		arm64_disasm_read_token(i_ptr, insn, "SHIFT", &shift);

		/*
		 * if shift type is RESERVED for shifted register instruction,
		 * print undefined
		 */
		if (shift == 3 && !has_shift_ror)
			goto undefined;

		di->di_printf("%s\t", i_ptr->name);

		/*
		 * If RD and RN are present, we will display the following
		 * patterns:
		 * - OP <RD>, <RN>, <RM>{, <shift [LSL, LSR, ASR]> #<imm>} SF32/64
		 * - OP <RD>, <RN>, #<imm>{, <shift [0, 12]>} SF32/64
		 * Otherwise if only RD is present:
		 * - OP <RD>, <RM> {, <shift> #<imm> }
		 * Otherwise if only RN is present:
		 * - OP <RN>, <RM> {, <shift> #<imm> }
		 */
		if (!rd_absent && !rn_absent)
			di->di_printf("%s, %s", arm64_reg(sf, rd, rd_sp),
			    arm64_reg(sf, rn, rn_sp));
		else if (!rd_absent)
			di->di_printf("%s", arm64_reg(sf, rd, rd_sp));
		else
			di->di_printf("%s", arm64_reg(sf, rn, rn_sp));

		/* If RM is present use it, otherwise use immediate notation */
		if (!rm_absent) {
			di->di_printf(", %s", arm64_reg(sf, rm, rm_sp));
			if (imm != 0)
				di->di_printf(", %s #%d", shift_2[shift], imm);
		} else {
			if (imm != 0 || shift != 0)
				di->di_printf(", #0x%x", imm);
			if (shift != 0)
				di->di_printf(" lsl #12");
		}
		break;
	case TYPE_02:
		/*
		 * OP <RT>, [<XN|SP>, #<simm>]!
		 * OP <RT>, [<XN|SP>], #<simm>
		 * OP <RT>, [<XN|SP> {, #<pimm> }]
		 * OP <RT>, [<XN|SP>, <RM> {, EXTEND AMOUNT }]
		 */

		/* Mandatory tokens */
		ret = arm64_disasm_read_token(i_ptr, insn, "RT", &rt);
		ret |= arm64_disasm_read_token(i_ptr, insn, "RN", &rn);
		if (ret != 0) {
			printf("ERROR: "
			    "Missing mandatory token for op %s type %d\n",
			    i_ptr->name, i_ptr->type);
			goto undefined;
		}

		/* Optional tokens */
		arm64_disasm_read_token(i_ptr, insn, "OPTION", &option);
		arm64_disasm_read_token(i_ptr, insn, "SCALE", &scale);
		rm_absent = arm64_disasm_read_token(i_ptr, insn, "RM", &rm);

		if (rm_absent) {
			/*
			 * In unsigned operation, shift immediate value
			 * and reset options to default.
			 */
			if (sign_ext == 0) {
				imm = imm << ((insn >> ARM_INSN_SIZE_OFFSET) &
				    ARM_INSN_SIZE_MASK);
				option = 0;
			}
			switch (option) {
			case 0x0:
				pre = 0;
				inside = 1;
				break;
			case 0x1:
				pre = 0;
				inside = 0;
				break;
			case 0x2:
			default:
				pre = 1;
				inside = 1;
				break;
			}

			di->di_printf("%s\t%s, ", i_ptr->name,
			    arm64_reg(sf, rt, rt_sp));
			if (inside != 0) {
				di->di_printf("[%s", arm64_x_reg(rn, 1));
				if (imm != 0)
					di->di_printf(", #%d", imm);
				di->di_printf("]");
			} else {
				di->di_printf("[%s]", arm64_x_reg(rn, 1));
				if (imm != 0)
					di->di_printf(", #%d", imm);
			}
			if (pre != 0)
				di->di_printf("!");
		} else {
			/* Last bit of option field determines 32/64 bit offset */
			di->di_printf("%s\t%s, [%s, %s", i_ptr->name,
			    arm64_reg(sf, rt, rt_sp), arm64_x_reg(rn, 1),
			    arm64_reg(option & 1, rm, rm_sp));

			if (scale == 0)
				amount = 0;
			else {
				/* Calculate amount, it's op(31:30) */
				amount = (insn >> ARM_INSN_SIZE_OFFSET) &
			            ARM_INSN_SIZE_MASK;
			}

			switch (option) {
			case 0x2:
				di->di_printf(", uxtw #%d", amount);
				break;
			case 0x3:
				if (scale != 0)
					di->di_printf(", lsl #%d", amount);
				break;
			case 0x6:
				di->di_printf(", sxtw #%d", amount);
				break;
			case 0x7:
				di->di_printf(", sxtx #%d", amount);
				break;
			default:
				di->di_printf(", rsv");
				break;
			}
			di->di_printf("]");
		}

		break;

	case TYPE_03:
		/* OP <RT>, #imm SF32/64 */

		/* Mandatory tokens */
		ret = arm64_disasm_read_token(i_ptr, insn, "RT", &rt);
		if (ret != 0) {
			printf("ERROR: "
			    "Missing mandatory token for op %s type %d\n",
			    i_ptr->name, i_ptr->type);
			goto undefined;
		}

		di->di_printf("%s\t%s, ", i_ptr->name, arm64_reg(sf, rt, rt_sp));
		if (i_ptr->special_ops & OP_LITERAL)
			di->di_printf("0x%lx", loc + imm);
		else
			di->di_printf("#%d", imm);

		break;

	case TYPE_04:
		/*
		 * OP <RD>, <RN|SP>, <RM> {, <extend> { #<amount> } }
		 * OP <RN|SP>, <RM>, {, <extend> { #<amount> } }
		 */

		arm64_disasm_read_token(i_ptr, insn, "RN", &rn);
		arm64_disasm_read_token(i_ptr, insn, "RM", &rm);
		arm64_disasm_read_token(i_ptr, insn, "OPTION", &option);

		rd_absent = arm64_disasm_read_token(i_ptr, insn, "RD", &rd);
		extend = arm64_disasm_reg_extend(sf, option, rd, rn, imm);

		di->di_printf("%s\t", i_ptr->name);

		if (!rd_absent)
			di->di_printf("%s, ", arm64_reg(sf, rd, rd_sp));

		di->di_printf("%s, ", arm64_reg(sf, rn, 1));

		if (sf != 0)
			di->di_printf("%s",
			    arm64_disasm_reg_width(option, rm));
		else
			di->di_printf("%s", arm64_w_reg(rm, 0));

		if (extend != NULL)
			di->di_printf(", %s #%d", extend, imm);

		break;
	default:
		goto undefined;
	}

	di->di_printf("\n");
	return (loc + INSN_SIZE);

undefined:
	di->di_printf("undefined\t%08x\n", insn);
	return (loc + INSN_SIZE);
}

/* Parse format strings at the very beginning */
SYSINIT(arm64_disasm_generate_masks, SI_SUB_DDB_SERVICES, SI_ORDER_FIRST,
    arm64_disasm_generate_masks, arm64_i);
