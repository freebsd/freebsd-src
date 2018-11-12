/*-
 * Copyright (c) 2016 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * Portions of this software were developed by SRI International and the
 * University of Cambridge Computer Laboratory under DARPA/AFRL contract
 * FA8750-10-C-0237 ("CTSRD"), as part of the DARPA CRASH research programme.
 *
 * Portions of this software were developed by the University of Cambridge
 * Computer Laboratory as part of the CTSRD Project, with support from the
 * UK Higher Education Innovation Fund (HEIF).
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <ddb/ddb.h>
#include <ddb/db_access.h>
#include <ddb/db_sym.h>

#include <machine/riscvreg.h>
#include <machine/riscv_opcode.h>

struct riscv_op {
	char *name;
	char *type;
	char *fmt;
	int opcode;
	int funct3;
	int funct7; /* Or imm, depending on type. */
};

/*
 * Keep sorted by opcode, funct3, funct7 so some instructions
 * aliases will be supported (e.g. "mv" instruction alias)
 * Use same print format as binutils do.
 */
static struct riscv_op riscv_opcodes[] = {
	{ "lb",		"I",	"d,o(s)",	3,   0, -1 },
	{ "lh",		"I",	"d,o(s)",	3,   1, -1 },
	{ "lw",		"I",	"d,o(s)",	3,   2, -1 },
	{ "ld",		"I",	"d,o(s)",	3,   3, -1 },
	{ "lbu",	"I",	"d,o(s)",	3,   4, -1 },
	{ "lhu",	"I",	"d,o(s)",	3,   5, -1 },
	{ "lwu",	"I",	"d,o(s)",	3,   6, -1 },
	{ "ldu",	"I",	"d,o(s)",	3,   7, -1 },
	{ "fence",	"I",	"",		15,  0, -1 },
	{ "fence.i",	"I",	"",		15,  1, -1 },
	{ "mv",		"I",	"d,s",		19,  0,  0 },
	{ "addi",	"I",	"d,s,j",	19,  0, -1 },
	{ "slli",	"R2",	"d,s,>",	19,  1,  0 },
	{ "slti",	"I",	"d,s,j",	19,  2, -1 },
	{ "sltiu",	"I",	"d,s,j",	19,  3, -1 },
	{ "xori",	"I",	"d,s,j",	19,  4, -1 },
	{ "srli",	"R2",	"d,s,>",	19,  5,  0 },
	{ "srai",	"R2",	"d,s,>",	19,  5, 0b010000 },
	{ "ori",	"I",	"d,s,j",	19,  6, -1 },
	{ "andi",	"I",	"d,s,j",	19,  7, -1 },
	{ "auipc",	"U",	"d,u",		23, -1, -1 },
	{ "sext.w",	"I",	"d,s",		27,  0,  0 },
	{ "addiw",	"I",	"d,s,j",	27,  0, -1 },
	{ "slliw",	"R",	"d,s,<",	27,  1,  0 },
	{ "srliw",	"R",	"d,s,<",	27,  5,  0 },
	{ "sraiw",	"R",	"d,s,<",	27,  5, 0b0100000 },
	{ "sb",		"S",	"t,q(s)",	35,  0, -1 },
	{ "sh",		"S",	"t,q(s)",	35,  1, -1 },
	{ "sw",		"S",	"t,q(s)",	35,  2, -1 },
	{ "sd",		"S",	"t,q(s)",	35,  3, -1 },
	{ "sbu",	"S",	"t,q(s)",	35,  4, -1 },
	{ "shu",	"S",	"t,q(s)",	35,  5, -1 },
	{ "swu",	"S",	"t,q(s)",	35,  6, -1 },
	{ "sdu",	"S",	"t,q(s)",	35,  7, -1 },
	{ "lr.w",	"R",	"d,t,0(s)",	47,  2, 0b0001000 },
	{ "sc.w",	"R",	"d,t,0(s)",	47,  2, 0b0001100 },
	{ "amoswap.w",	"R",	"d,t,0(s)",	47,  2, 0b0000100 },
	{ "amoadd.w",	"R",	"d,t,0(s)",	47,  2, 0b0000000 },
	{ "amoxor.w",	"R",	"d,t,0(s)",	47,  2, 0b0010000 },
	{ "amoand.w",	"R",	"d,t,0(s)",	47,  2, 0b0110000 },
	{ "amoor.w",	"R",	"d,t,0(s)",	47,  2, 0b0100000 },
	{ "amomin.w",	"R",	"d,t,0(s)",	47,  2, 0b1000000 },
	{ "amomax.w",	"R",	"d,t,0(s)",	47,  2, 0b1010000 },
	{ "amominu.w",	"R",	"d,t,0(s)",	47,  2, 0b1100000 },
	{ "amomaxu.w",	"R",	"d,t,0(s)",	47,  2, 0b1110000 },
	{ "lr.w.aq",	"R",	"d,t,0(s)",	47,  2, 0b0001000 },
	{ "sc.w.aq",	"R",	"d,t,0(s)",	47,  2, 0b0001100 },
	{ "amoswap.w.aq","R",	"d,t,0(s)",	47,  2, 0b0000110 },
	{ "amoadd.w.aq","R",	"d,t,0(s)",	47,  2, 0b0000010 },
	{ "amoxor.w.aq","R",	"d,t,0(s)",	47,  2, 0b0010010 },
	{ "amoand.w.aq","R",	"d,t,0(s)",	47,  2, 0b0110010 },
	{ "amoor.w.aq",	"R",	"d,t,0(s)",	47,  2, 0b0100010 },
	{ "amomin.w.aq","R",	"d,t,0(s)",	47,  2, 0b1000010 },
	{ "amomax.w.aq","R",	"d,t,0(s)",	47,  2, 0b1010010 },
	{ "amominu.w.aq","R",	"d,t,0(s)",	47,  2, 0b1100010 },
	{ "amomaxu.w.aq","R",	"d,t,0(s)",	47,  2, 0b1110010 },
	{ "amoswap.w.rl","R",	"d,t,0(s)",	47,  2, 0b0000110 },
	{ "amoadd.w.rl","R",	"d,t,0(s)",	47,  2, 0b0000001 },
	{ "amoxor.w.rl","R",	"d,t,0(s)",	47,  2, 0b0010001 },
	{ "amoand.w.rl","R",	"d,t,0(s)",	47,  2, 0b0110001 },
	{ "amoor.w.rl",	"R",	"d,t,0(s)",	47,  2, 0b0100001 },
	{ "amomin.w.rl","R",	"d,t,0(s)",	47,  2, 0b1000001 },
	{ "amomax.w.rl","R",	"d,t,0(s)",	47,  2, 0b1010001 },
	{ "amominu.w.rl","R",	"d,t,0(s)",	47,  2, 0b1100001 },
	{ "amomaxu.w.rl","R",	"d,t,0(s)",	47,  2, 0b1110001 },
	{ "amoswap.d",	"R",	"d,t,0(s)",	47,  3, 0b0000100 },
	{ "amoadd.d",	"R",	"d,t,0(s)",	47,  3, 0b0000000 },
	{ "amoxor.d",	"R",	"d,t,0(s)",	47,  3, 0b0010000 },
	{ "amoand.d",	"R",	"d,t,0(s)",	47,  3, 0b0110000 },
	{ "amoor.d",	"R",	"d,t,0(s)",	47,  3, 0b0100000 },
	{ "amomin.d",	"R",	"d,t,0(s)",	47,  3, 0b1000000 },
	{ "amomax.d",	"R",	"d,t,0(s)",	47,  3, 0b1010000 },
	{ "amominu.d",	"R",	"d,t,0(s)",	47,  3, 0b1100000 },
	{ "amomaxu.d",	"R",	"d,t,0(s)",	47,  3, 0b1110000 },
	{ "lr.d.aq",	"R",	"d,t,0(s)",	47,  3, 0b0001000 },
	{ "sc.d.aq",	"R",	"d,t,0(s)",	47,  3, 0b0001100 },
	{ "amoswap.d.aq","R",	"d,t,0(s)",	47,  3, 0b0000110 },
	{ "amoadd.d.aq","R",	"d,t,0(s)",	47,  3, 0b0000010 },
	{ "amoxor.d.aq","R",	"d,t,0(s)",	47,  3, 0b0010010 },
	{ "amoand.d.aq","R",	"d,t,0(s)",	47,  3, 0b0110010 },
	{ "amoor.d.aq",	"R",	"d,t,0(s)",	47,  3, 0b0100010 },
	{ "amomin.d.aq","R",	"d,t,0(s)",	47,  3, 0b1000010 },
	{ "amomax.d.aq","R",	"d,t,0(s)",	47,  3, 0b1010010 },
	{ "amominu.d.aq","R",	"d,t,0(s)",	47,  3, 0b1100010 },
	{ "amomaxu.d.aq","R",	"d,t,0(s)",	47,  3, 0b1110010 },
	{ "amoswap.d.rl","R",	"d,t,0(s)",	47,  3, 0b0000110 },
	{ "amoadd.d.rl","R",	"d,t,0(s)",	47,  3, 0b0000001 },
	{ "amoxor.d.rl","R",	"d,t,0(s)",	47,  3, 0b0010001 },
	{ "amoand.d.rl","R",	"d,t,0(s)",	47,  3, 0b0110001 },
	{ "amoor.d.rl",	"R",	"d,t,0(s)",	47,  3, 0b0100001 },
	{ "amomin.d.rl","R",	"d,t,0(s)",	47,  3, 0b1000001 },
	{ "amomax.d.rl","R",	"d,t,0(s)",	47,  3, 0b1010001 },
	{ "amominu.d.rl","R",	"d,t,0(s)",	47,  3, 0b1100001 },
	{ "amomaxu.d.rl","R",	"d,t,0(s)",	47,  3, 0b1110001 },
	{ "add",	"R",	"d,s,t",	51,  0,  0 },
	{ "sub",	"R",	"d,s,t",	51,  0,  0b0100000 },
	{ "mul",	"R",	"d,s,t",	51,  0,  0b0000001 },
	{ "sll",	"R",	"d,s,t",	51,  1,  0 },
	{ "slt",	"R",	"d,s,t",	51,  2,  0 },
	{ "sltu",	"R",	"d,s,t",	51,  3,  0 },
	{ "xor",	"R",	"d,s,t",	51,  4,  0 },
	{ "srl",	"R",	"d,s,t",	51,  5,  0 },
	{ "sra",	"R",	"d,s,t",	51,  5,  0b0100000 },
	{ "or",		"R",	"d,s,t",	51,  6,  0 },
	{ "and",	"R",	"d,s,t",	51,  7,  0 },
	{ "lui",	"U",	"d,u",		55, -1, -1 },
	{ "addw",	"R",	"d,s,t",	59,  0,  0 },
	{ "subw",	"R",	"d,s,t",	59,  0,  0b0100000 },
	{ "mulw",	"R",	"d,s,t",	59,  0,  1 },
	{ "sllw",	"R",	"d,s,t",	59,  1,  0 },
	{ "srlw",	"R",	"d,s,t",	59,  5,  0 },
	{ "sraw",	"R",	"d,s,t",	59,  5,  0b0100000 },
	{ "beq",	"SB",	"s,t,p",	99,  0,  -1 },
	{ "bne",	"SB",	"s,t,p",	99,  1,  -1 },
	{ "blt",	"SB",	"s,t,p",	99,  4,  -1 },
	{ "bge",	"SB",	"s,t,p",	99,  5,  -1 },
	{ "bltu",	"SB",	"s,t,p",	99,  6,  -1 },
	{ "bgeu",	"SB",	"s,t,p",	99,  7,  -1 },
	{ "jalr",	"I",	"d,s,j",	103,  0, -1 },
	{ "jal",	"UJ",	"a",		111, -1, -1 },
	{ "eret",	"I",	"",		115,  0, 0b000100000000 },
	{ "sfence.vm",	"I",	"",		115,  0, 0b000100000001 },
	{ "wfi",	"I",	"",		115,  0, 0b000100000010 },
	{ "csrrw",	"I",	"d,E,s",	115,  1, -1},
	{ "csrrs",	"I",	"d,E,s",	115,  2, -1},
	{ "csrrc",	"I",	"d,E,s",	115,  3, -1},
	{ "csrrwi",	"I",	"d,E,Z",	115,  5, -1},
	{ "csrrsi",	"I",	"d,E,Z",	115,  6, -1},
	{ "csrrci",	"I",	"d,E,Z",	115,  7, -1},
	{ NULL, NULL, NULL, 0, 0, 0 }
};

struct csr_op {
	char *name;
	int imm;
};

static struct csr_op csr_name[] = {
	{ "fflags",		0x001 },
	{ "frm",		0x002 },
	{ "fcsr",		0x003 },
	{ "cycle",		0xc00 },
	{ "time",		0xc01 },
	{ "instret",		0xc02 },
	{ "stats",		0x0c0 },
	{ "uarch0",		0xcc0 },
	{ "uarch1",		0xcc1 },
	{ "uarch2",		0xcc2 },
	{ "uarch3",		0xcc3 },
	{ "uarch4",		0xcc4 },
	{ "uarch5",		0xcc5 },
	{ "uarch6",		0xcc6 },
	{ "uarch7",		0xcc7 },
	{ "uarch8",		0xcc8 },
	{ "uarch9",		0xcc9 },
	{ "uarch10",		0xcca },
	{ "uarch11",		0xccb },
	{ "uarch12",		0xccc },
	{ "uarch13",		0xccd },
	{ "uarch14",		0xcce },
	{ "uarch15",		0xccf },
	{ "sstatus",		0x100 },
	{ "stvec",		0x101 },
	{ "sie",		0x104 },
	{ "sscratch",		0x140 },
	{ "sepc",		0x141 },
	{ "sip",		0x144 },
	{ "sptbr",		0x180 },
	{ "sasid",		0x181 },
	{ "cyclew",		0x900 },
	{ "timew",		0x901 },
	{ "instretw",		0x902 },
	{ "stime",		0xd01 },
	{ "scause",		0xd42 },
	{ "sbadaddr",		0xd43 },
	{ "stimew",		0xa01 },
	{ "mstatus",		0x300 },
	{ "mtvec",		0x301 },
	{ "mtdeleg",		0x302 },
	{ "mie",		0x304 },
	{ "mtimecmp",		0x321 },
	{ "mscratch",		0x340 },
	{ "mepc",		0x341 },
	{ "mcause",		0x342 },
	{ "mbadaddr",		0x343 },
	{ "mip",		0x344 },
	{ "mtime",		0x701 },
	{ "mcpuid",		0xf00 },
	{ "mimpid",		0xf01 },
	{ "mhartid",		0xf10 },
	{ "mtohost",		0x780 },
	{ "mfromhost",		0x781 },
	{ "mreset",		0x782 },
	{ "send_ipi",		0x783 },
	{ "miobase",		0x784 },
	{ "cycleh",		0xc80 },
	{ "timeh",		0xc81 },
	{ "instreth",		0xc82 },
	{ "cyclehw",		0x980 },
	{ "timehw",		0x981 },
	{ "instrethw",		0x982 },
	{ "stimeh",		0xd81 },
	{ "stimehw",		0xa81 },
	{ "mtimecmph",		0x361 },
	{ "mtimeh",		0x741 },
	{ NULL,	0 }
};

static char *reg_name[32] = {
	"zero",	"ra",	"sp",	"gp",	"tp",	"t0",	"t1",	"t2",
	"s0",	"s1",	"a0",	"a1",	"a2",	"a3",	"a4",	"a5",
	"a6",	"a7",	"s2",	"s3",	"s4",	"s5",	"s6",	"s7",
	"s8",	"s9",	"s10",	"s11",	"t3",	"t4",	"t5",	"t6"
};

static int32_t
get_imm(InstFmt i, char *type, uint32_t *val)
{
	int imm;

	imm = 0;

	if (strcmp(type, "I") == 0) {
		imm = i.IType.imm;
		*val = imm;
		if (imm & (1 << 11))
			imm |= (0xfffff << 12);	/* sign extend */

	} else if (strcmp(type, "S") == 0) {
		imm = i.SType.imm0_4;
		imm |= (i.SType.imm5_11 << 5);
		*val = imm;
		if (imm & (1 << 11))
			imm |= (0xfffff << 12);	/* sign extend */

	} else if (strcmp(type, "U") == 0) {
		imm = i.UType.imm12_31;
		*val = imm;

	} else if (strcmp(type, "UJ") == 0) {
		imm = i.UJType.imm12_19 << 12;
		imm |= i.UJType.imm11 << 11;
		imm |= i.UJType.imm1_10 << 1;
		imm |= i.UJType.imm20 << 20;
		*val = imm;
		if (imm & (1 << 20))
			imm |= (0xfff << 21);	/* sign extend */

	} else if (strcmp(type, "SB") == 0) {
		imm = i.SBType.imm11 << 11;
		imm |= i.SBType.imm1_4 << 1;
		imm |= i.SBType.imm5_10 << 5;
		imm |= i.SBType.imm12 << 12;
		*val = imm;
		if (imm & (1 << 12))
			imm |= (0xfffff << 12);	/* sign extend */
	}

	return (imm);
}

static int
oprint(struct riscv_op *op, vm_offset_t loc, int rd,
    int rs1, int rs2, uint32_t val, vm_offset_t imm)
{
	char *p;
	int i;

	p = op->fmt;

	db_printf("%s\t", op->name);

	while (*p) {
		if (strncmp("d", p, 1) == 0)
			db_printf("%s", reg_name[rd]);

		else if (strncmp("s", p, 1) == 0)
			db_printf("%s", reg_name[rs1]);

		else if (strncmp("t", p, 1) == 0)
			db_printf("%s", reg_name[rs2]);

		else if (strncmp(">", p, 1) == 0)
			db_printf("0x%x", rs2);

		else if (strncmp("E", p, 1) == 0) {
			for (i = 0; csr_name[i].name != NULL; i++)
				if (csr_name[i].imm == val)
					db_printf("%s",
					    csr_name[i].name);
		} else if (strncmp("Z", p, 1) == 0)
			db_printf("%d", rs1);

		else if (strncmp("<", p, 1) == 0)
			db_printf("0x%x", rs2);

		else if (strncmp("j", p, 1) == 0)
			db_printf("%d", imm);

		else if (strncmp("u", p, 1) == 0)
			db_printf("0x%x", imm);

		else if (strncmp("a", p, 1) == 0)
			db_printf("0x%016lx", imm);

		else if (strncmp("p", p, 1) == 0)
			db_printf("0x%016lx", (loc + imm));

		else if (strlen(p) >= 4) {
			if (strncmp("o(s)", p, 4) == 0)
				db_printf("%d(%s)", imm, reg_name[rs1]);
			else if (strncmp("q(s)", p, 4) == 0)
				db_printf("%d(%s)", imm, reg_name[rs1]);
			else if (strncmp("0(s)", p, 4) == 0)
				db_printf("(%s)", reg_name[rs1]);
		}

		while (*p && strncmp(p, ",", 1) != 0)
			p++;

		if (*p) {
			db_printf(", ");
			p++;
		}
	}


	return (0);
}

static int
match_type(InstFmt i, struct riscv_op *op, vm_offset_t loc)
{
	uint32_t val;
	int found;
	int imm;

	val = 0;
	imm = get_imm(i, op->type, &val);

	if (strcmp(op->type, "U") == 0) {
		oprint(op, loc, i.UType.rd, 0, 0, val, imm);
		return (1);
	}
	if (strcmp(op->type, "UJ") == 0) {
		oprint(op, loc, 0, 0, 0, val, (loc + imm));
		return (1);
	}
	if ((strcmp(op->type, "I") == 0) && \
	    (op->funct3 == i.IType.funct3)) {
		found = 0;
		if (op->funct7 != -1) {
			if (op->funct7 == i.IType.imm)
				found = 1;
		} else
			found = 1;

		if (found) {
			oprint(op, loc, i.IType.rd,
			    i.IType.rs1, 0, val, imm);
			return (1);
		}
	}
	if ((strcmp(op->type, "S") == 0) && \
	    (op->funct3 == i.SType.funct3)) {
		oprint(op, loc, 0, i.SType.rs1, i.SType.rs2,
		    val, imm);
		return (1);
	}
	if ((strcmp(op->type, "SB") == 0) && \
	    (op->funct3 == i.SBType.funct3)) {
		oprint(op, loc, 0, i.SBType.rs1, i.SBType.rs2,
		    val, imm);
		return (1);
	}
	if ((strcmp(op->type, "R2") == 0) && \
	    (op->funct3 == i.R2Type.funct3) && \
	    (op->funct7 == i.R2Type.funct7)) {
		oprint(op, loc, i.R2Type.rd, i.R2Type.rs1,
		    i.R2Type.rs2, val, imm);
		return (1);
	}
	if ((strcmp(op->type, "R") == 0) && \
	    (op->funct3 == i.RType.funct3) && \
	    (op->funct7 == i.RType.funct7)) {
		oprint(op, loc, i.RType.rd, i.RType.rs1,
		    val, i.RType.rs2, imm);
		return (1);
	}

	return (0);
}

vm_offset_t
db_disasm(vm_offset_t loc, bool altfmt)
{
	struct riscv_op *op;
	InstFmt i;
	int j;

	i.word = db_get_value(loc, INSN_SIZE, 0);

	/* First match opcode */
	for (j = 0; riscv_opcodes[j].name != NULL; j++) {
		op = &riscv_opcodes[j];
		if (op->opcode == i.RType.opcode) {
			if (match_type(i, op, loc))
				break;
		}
	}

	db_printf("\n");
	return(loc + INSN_SIZE);
}
