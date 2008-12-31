/*-
 * Copyright (c) 2000-2006 Marcel Moolenaar
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/ia64/disasm/disasm_decode.c,v 1.5.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>

#include <ia64/disasm/disasm_int.h>
#include <ia64/disasm/disasm.h>

/*
 * Template names.
 */
static const char *asm_templname[] = {
	"MII", "MII;", "MI;I", "MI;I;", "MLX", "MLX;", 0, 0,
	"MMI", "MMI;", "M;MI", "M;MI;", "MFI", "MFI;", "MMF", "MMF;",
	"MIB", "MIB;", "MBB", "MBB;", 0, 0, "BBB", "BBB;",
	"MMB", "MMB;", 0, 0, "MFB", "MFB;", 0, 0
};

/*
 * Decode A-unit instructions.
 */
static int
asm_decodeA(uint64_t bits, struct asm_bundle *b, int slot)
{
	enum asm_fmt fmt;
	enum asm_op op;

	fmt = ASM_FMT_NONE, op = ASM_OP_NONE;
	switch((int)OPCODE(bits)) {
	case 0x8:
		switch (FIELD(bits, 34, 2)) { /* x2a */
		case 0x0:
			if (FIELD(bits, 33, 1) == 0) { /* ve */
				switch (FIELD(bits, 29, 4)) { /* x4 */
				case 0x0:
					if (FIELD(bits, 27, 2) <= 1) /* x2b */
						op = ASM_OP_ADD,
						    fmt = ASM_FMT_A1;
					break;
				case 0x1:
					if (FIELD(bits, 27, 2) <= 1) /* x2b */
						op = ASM_OP_SUB,
						    fmt = ASM_FMT_A1;
					break;
				case 0x2:
					if (FIELD(bits, 27, 2) == 0) /* x2b */
						op = ASM_OP_ADDP4,
						    fmt = ASM_FMT_A1;
					break;
				case 0x3:
					switch (FIELD(bits, 27, 2)) { /* x2b */
					case 0x0:
						op = ASM_OP_AND,
						    fmt = ASM_FMT_A1;
						break;
					case 0x1:
						op = ASM_OP_ANDCM,
						    fmt = ASM_FMT_A1;
						break;
					case 0x2:
						op = ASM_OP_OR,
						    fmt = ASM_FMT_A1;
						break;
					case 0x3:
						op = ASM_OP_XOR,
						    fmt = ASM_FMT_A1;
						break;
					}
					break;
				case 0xB:
					switch (FIELD(bits, 27, 2)) { /* x2b */
					case 0x0:
						op = ASM_OP_AND,
						    fmt = ASM_FMT_A3;
						break;
					case 0x1:
						op = ASM_OP_ANDCM,
						    fmt = ASM_FMT_A3;
						break;
					case 0x2:
						op = ASM_OP_OR,
						    fmt = ASM_FMT_A3;
						break;
					case 0x3:
						op = ASM_OP_XOR,
						    fmt = ASM_FMT_A3;
						break;
					}
					break;
				case 0x4:
					op = ASM_OP_SHLADD, fmt = ASM_FMT_A2;
					break;
				case 0x6:
					op = ASM_OP_SHLADDP4, fmt = ASM_FMT_A2;
					break;
				case 0x9:
					if (FIELD(bits, 27, 2) == 1) /* x2b */
						op = ASM_OP_SUB,
						    fmt = ASM_FMT_A3;
					break;
				}
			}
			break;
		case 0x1:
			switch (FIELD(bits, 29, 8)) { /* za + x2a + zb + x4 */
			case 0x20:
				switch (FIELD(bits, 27, 2)) { /* x2b */
				case 0x0:
					op = ASM_OP_PADD1_, fmt = ASM_FMT_A9;
					break;
				case 0x1:
					op = ASM_OP_PADD1_SSS,
					    fmt = ASM_FMT_A9;
					break;
				case 0x2:
					op = ASM_OP_PADD1_UUU,
					    fmt = ASM_FMT_A9;
					break;
				case 0x3:
					op = ASM_OP_PADD1_UUS,
					    fmt = ASM_FMT_A9;
					break;
				}
				break;
			case 0x21:
				switch (FIELD(bits, 27, 2)) { /* x2b */
				case 0x0:
					op = ASM_OP_PSUB1_, fmt = ASM_FMT_A9;
					break;
				case 0x1:
					op = ASM_OP_PSUB1_SSS,
					    fmt = ASM_FMT_A9;
					break;
				case 0x2:
					op = ASM_OP_PSUB1_UUU,
					    fmt = ASM_FMT_A9;
					break;
				case 0x3:
					op = ASM_OP_PSUB1_UUS,
					    fmt = ASM_FMT_A9;
					break;
				}
				break;
			case 0x22:
				switch (FIELD(bits, 27, 2)) { /* x2b */
				case 0x2:
					op = ASM_OP_PAVG1_, fmt = ASM_FMT_A9;
					break;
				case 0x3:
					op = ASM_OP_PAVG1_RAZ,
					    fmt = ASM_FMT_A9;
					break;
				}
				break;
			case 0x23:
				if (FIELD(bits, 27, 2) == 2) /* x2b */
					op = ASM_OP_PAVGSUB1, fmt = ASM_FMT_A9;
				break;
			case 0x29:
				switch (FIELD(bits, 27, 2)) { /* x2b */
				case 0x0:
					op = ASM_OP_PCMP1_EQ, fmt = ASM_FMT_A9;
					break;
				case 0x1:
					op = ASM_OP_PCMP1_GT, fmt = ASM_FMT_A9;
					break;
				}
				break;
			case 0x30:
				switch (FIELD(bits, 27, 2)) { /* x2b */
				case 0x0:
					op = ASM_OP_PADD2_, fmt = ASM_FMT_A9;
					break;
				case 0x1:
					op = ASM_OP_PADD2_SSS,
					    fmt = ASM_FMT_A9;
					break;
				case 0x2:
					op = ASM_OP_PADD2_UUU,
					    fmt = ASM_FMT_A9;
					break;
				case 0x3:
					op = ASM_OP_PADD2_UUS,
					    fmt = ASM_FMT_A9;
					break;
				}
				break;
			case 0x31:
				switch (FIELD(bits, 27, 2)) { /* x2b */
				case 0x0:
					op = ASM_OP_PSUB2_, fmt = ASM_FMT_A9;
					break;
				case 0x1:
					op = ASM_OP_PSUB2_SSS,
					    fmt = ASM_FMT_A9;
					break;
				case 0x2:
					op = ASM_OP_PSUB2_UUU,
					    fmt = ASM_FMT_A9;
					break;
				case 0x3:
					op = ASM_OP_PSUB2_UUS,
					    fmt = ASM_FMT_A9;
					break;
				}
				break;
			case 0x32:
				switch (FIELD(bits, 27, 2)) { /* x2b */
				case 0x2:
					op = ASM_OP_PAVG2_, fmt = ASM_FMT_A9;
					break;
				case 0x3:
					op = ASM_OP_PAVG2_RAZ,
					    fmt = ASM_FMT_A9;
					break;
				}
				break;
			case 0x33:
				if (FIELD(bits, 27, 2) == 2) /* x2b */
					op = ASM_OP_PAVGSUB2, fmt = ASM_FMT_A9;
				break;
			case 0x34:
				op = ASM_OP_PSHLADD2, fmt = ASM_FMT_A10;
				break;
			case 0x36:
				op = ASM_OP_PSHRADD2, fmt = ASM_FMT_A10;
				break;
			case 0x39:
				switch (FIELD(bits, 27, 2)) { /* x2b */
				case 0x0:
					op = ASM_OP_PCMP2_EQ, fmt = ASM_FMT_A9;
					break;
				case 0x1:
					op = ASM_OP_PCMP2_GT, fmt = ASM_FMT_A9;
					break;
				}
				break;
			case 0xA0:
				if (FIELD(bits, 27, 2) == 0) /* x2b */
					op = ASM_OP_PADD4, fmt = ASM_FMT_A9;
				break;
			case 0xA1:
				if (FIELD(bits, 27, 2) == 0) /* x2b */
					op = ASM_OP_PSUB4, fmt = ASM_FMT_A9;
				break;
			case 0xA9:
				switch (FIELD(bits, 27, 2)) { /* x2b */
				case 0x0:
					op = ASM_OP_PCMP4_EQ, fmt = ASM_FMT_A9;
					break;
				case 0x1:
					op = ASM_OP_PCMP4_GT, fmt = ASM_FMT_A9;
					break;
				}
				break;
			}
			break;
		case 0x2:
			if (FIELD(bits, 33, 1) == 0) /* ve */
				op = ASM_OP_ADDS, fmt = ASM_FMT_A4;
			break;
		case 0x3:
			if (FIELD(bits, 33, 1) == 0) /* ve */
				op = ASM_OP_ADDP4, fmt = ASM_FMT_A4;
			break;
		}
		break;
	case 0x9:
		op = ASM_OP_ADDL, fmt = ASM_FMT_A5;
		break;
	case 0xC: case 0xD: case 0xE:
		if (FIELD(bits, 12, 1) == 0) { /* c */
			switch (FIELD(bits, 33, 8)) { /* maj + tb + x2 + ta */
			case 0xC0:
				op = ASM_OP_CMP_LT, fmt = ASM_FMT_A6;
				break;
			case 0xC1:
				op = ASM_OP_CMP_EQ_AND, fmt = ASM_FMT_A6;
				break;
			case 0xC2:
				op = ASM_OP_CMP4_LT, fmt = ASM_FMT_A6;
				break;
			case 0xC3:
				op = ASM_OP_CMP4_EQ_AND, fmt = ASM_FMT_A6;
				break;
			case 0xC4: case 0xCC:
				op = ASM_OP_CMP_LT, fmt = ASM_FMT_A8;
				break;
			case 0xC5: case 0xCD:
				op = ASM_OP_CMP_EQ_AND, fmt = ASM_FMT_A8;
				break;
			case 0xC6: case 0xCE:
				op = ASM_OP_CMP4_LT, fmt = ASM_FMT_A8;
				break;
			case 0xC7: case 0xCF:
				op = ASM_OP_CMP4_EQ_AND, fmt = ASM_FMT_A8;
				break;
			case 0xC8:
				op = ASM_OP_CMP_GT_AND, fmt = ASM_FMT_A7;
				break;
			case 0xC9:
				op = ASM_OP_CMP_GE_AND, fmt = ASM_FMT_A7;
				break;
			case 0xCA:
				op = ASM_OP_CMP4_GT_AND, fmt = ASM_FMT_A7;
				break;
			case 0xCB:
				op = ASM_OP_CMP4_GE_AND, fmt = ASM_FMT_A7;
				break;
			case 0xD0:
				op = ASM_OP_CMP_LTU, fmt = ASM_FMT_A6;
				break;
			case 0xD1:
				op = ASM_OP_CMP_EQ_OR, fmt = ASM_FMT_A6;
				break;
			case 0xD2:
				op = ASM_OP_CMP4_LTU, fmt = ASM_FMT_A6;
				break;
			case 0xD3:
				op = ASM_OP_CMP4_EQ_OR, fmt = ASM_FMT_A6;
				break;
			case 0xD4: case 0xDC:
				op = ASM_OP_CMP_LTU, fmt = ASM_FMT_A8;
				break;
			case 0xD5: case 0xDD:
				op = ASM_OP_CMP_EQ_OR, fmt = ASM_FMT_A8;
				break;
			case 0xD6: case 0xDE:
				op = ASM_OP_CMP4_LTU, fmt = ASM_FMT_A8;
				break;
			case 0xD7: case 0xDF:
				op = ASM_OP_CMP4_EQ_OR, fmt = ASM_FMT_A8;
				break;
			case 0xD8:
				op = ASM_OP_CMP_GT_OR, fmt = ASM_FMT_A7;
				break;
			case 0xD9:
				op = ASM_OP_CMP_GE_OR, fmt = ASM_FMT_A7;
				break;
			case 0xDA:
				op = ASM_OP_CMP4_GT_OR, fmt = ASM_FMT_A7;
				break;
			case 0xDB:
				op = ASM_OP_CMP4_GE_OR, fmt = ASM_FMT_A7;
				break;
			case 0xE0:
				op = ASM_OP_CMP_EQ, fmt = ASM_FMT_A6;
				break;
			case 0xE1:
				op = ASM_OP_CMP_EQ_OR_ANDCM, fmt = ASM_FMT_A6;
				break;
			case 0xE2:
				op = ASM_OP_CMP4_EQ, fmt = ASM_FMT_A6;
				break;
			case 0xE3:
				op = ASM_OP_CMP4_EQ_OR_ANDCM, fmt = ASM_FMT_A6;
				break;
			case 0xE4: case 0xEC:
				op = ASM_OP_CMP_EQ, fmt = ASM_FMT_A8;
				break;
			case 0xE5: case 0xED:
				op = ASM_OP_CMP_EQ_OR_ANDCM, fmt = ASM_FMT_A8;
				break;
			case 0xE6: case 0xEE:
				op = ASM_OP_CMP4_EQ, fmt = ASM_FMT_A8;
				break;
			case 0xE7: case 0xEF:
				op = ASM_OP_CMP4_EQ_OR_ANDCM, fmt = ASM_FMT_A8;
				break;
			case 0xE8:
				op = ASM_OP_CMP_GT_OR_ANDCM, fmt = ASM_FMT_A7;
				break;
			case 0xE9:
				op = ASM_OP_CMP_GE_OR_ANDCM, fmt = ASM_FMT_A7;
				break;
			case 0xEA:
				op = ASM_OP_CMP4_GT_OR_ANDCM, fmt = ASM_FMT_A7;
				break;
			case 0xEB:
				op = ASM_OP_CMP4_GE_OR_ANDCM, fmt = ASM_FMT_A7;
				break;
			}
		} else {
			switch (FIELD(bits, 33, 8)) { /* maj + tb + x2 + ta */
			case 0xC0:
				op = ASM_OP_CMP_LT_UNC, fmt = ASM_FMT_A6;
				break;
			case 0xC1:
				op = ASM_OP_CMP_NE_AND, fmt = ASM_FMT_A6;
				break;
			case 0xC2:
				op = ASM_OP_CMP4_LT_UNC, fmt = ASM_FMT_A6;
				break;
			case 0xC3:
				op = ASM_OP_CMP4_NE_AND, fmt = ASM_FMT_A6;
				break;
			case 0xC4: case 0xCC:
				op = ASM_OP_CMP_LT_UNC, fmt = ASM_FMT_A8;
				break;
			case 0xC5: case 0xCD:
				op = ASM_OP_CMP_NE_AND, fmt = ASM_FMT_A8;
				break;
			case 0xC6: case 0xCE:
				op = ASM_OP_CMP4_LT_UNC, fmt = ASM_FMT_A8;
				break;
			case 0xC7: case 0xCF:
				op = ASM_OP_CMP4_NE_AND, fmt = ASM_FMT_A8;
				break;
			case 0xC8:
				op = ASM_OP_CMP_LE_AND, fmt = ASM_FMT_A7;
				break;
			case 0xC9:
				op = ASM_OP_CMP_LT_AND, fmt = ASM_FMT_A7;
				break;
			case 0xCA:
				op = ASM_OP_CMP4_LE_AND, fmt = ASM_FMT_A7;
				break;
			case 0xCB:
				op = ASM_OP_CMP4_LT_AND, fmt = ASM_FMT_A7;
				break;
			case 0xD0:
				op = ASM_OP_CMP_LTU_UNC, fmt = ASM_FMT_A6;
				break;
			case 0xD1:
				op = ASM_OP_CMP_NE_OR, fmt = ASM_FMT_A6;
				break;
			case 0xD2:
				op = ASM_OP_CMP4_LTU_UNC, fmt = ASM_FMT_A6;
				break;
			case 0xD3:
				op = ASM_OP_CMP4_NE_OR, fmt = ASM_FMT_A6;
				break;
			case 0xD4: case 0xDC:
				op = ASM_OP_CMP_LTU_UNC, fmt = ASM_FMT_A8;
				break;
			case 0xD5: case 0xDD:
				op = ASM_OP_CMP_NE_OR, fmt = ASM_FMT_A8;
				break;
			case 0xD6: case 0xDE:
				op = ASM_OP_CMP4_LTU_UNC, fmt = ASM_FMT_A8;
				break;
			case 0xD7: case 0xDF:
				op = ASM_OP_CMP4_NE_OR, fmt = ASM_FMT_A8;
				break;
			case 0xD8:
				op = ASM_OP_CMP_LE_OR, fmt = ASM_FMT_A7;
				break;
			case 0xD9:
				op = ASM_OP_CMP_LT_OR, fmt = ASM_FMT_A7;
				break;
			case 0xDA:
				op = ASM_OP_CMP4_LE_OR, fmt = ASM_FMT_A7;
				break;
			case 0xDB:
				op = ASM_OP_CMP4_LT_OR, fmt = ASM_FMT_A7;
				break;
			case 0xE0:
				op = ASM_OP_CMP_EQ_UNC, fmt = ASM_FMT_A6;
				break;
			case 0xE1:
				op = ASM_OP_CMP_NE_OR_ANDCM, fmt = ASM_FMT_A6;
				break;
			case 0xE2:
				op = ASM_OP_CMP4_EQ_UNC, fmt = ASM_FMT_A6;
				break;
			case 0xE3:
				op = ASM_OP_CMP4_NE_OR_ANDCM, fmt = ASM_FMT_A6;
				break;
			case 0xE4: case 0xEC:
				op = ASM_OP_CMP_EQ_UNC, fmt = ASM_FMT_A8;
				break;
			case 0xE5: case 0xED:
				op = ASM_OP_CMP_NE_OR_ANDCM, fmt = ASM_FMT_A8;
				break;
			case 0xE6: case 0xEE:
				op = ASM_OP_CMP4_EQ_UNC, fmt = ASM_FMT_A8;
				break;
			case 0xE7: case 0xEF:
				op = ASM_OP_CMP4_NE_OR_ANDCM, fmt = ASM_FMT_A8;
				break;
			case 0xE8:
				op = ASM_OP_CMP_LE_OR_ANDCM, fmt = ASM_FMT_A7;
				break;
			case 0xE9:
				op = ASM_OP_CMP_LT_OR_ANDCM, fmt = ASM_FMT_A7;
				break;
			case 0xEA:
				op = ASM_OP_CMP4_LE_OR_ANDCM, fmt = ASM_FMT_A7;
				break;
			case 0xEB:
				op = ASM_OP_CMP4_LT_OR_ANDCM, fmt = ASM_FMT_A7;
				break;
			}
		}
		break;
	}

	if (op != ASM_OP_NONE)
		return (asm_extract(op, fmt, bits, b, slot));
	return (0);
}

/*
 * Decode B-unit instructions.
 */
static int
asm_decodeB(uint64_t ip, struct asm_bundle *b, int slot)
{
	uint64_t bits;
	enum asm_fmt fmt;
	enum asm_op op;

	bits = SLOT(ip, slot);
	fmt = ASM_FMT_NONE, op = ASM_OP_NONE;

	switch((int)OPCODE(bits)) {
	case 0x0:
		switch (FIELD(bits, 27, 6)) { /* x6 */
		case 0x0:
			op = ASM_OP_BREAK_B, fmt = ASM_FMT_B9;
			break;
		case 0x2:
			op = ASM_OP_COVER, fmt = ASM_FMT_B8;
			break;
		case 0x4:
			op = ASM_OP_CLRRRB_, fmt = ASM_FMT_B8;
			break;
		case 0x5:
			op = ASM_OP_CLRRRB_PR, fmt = ASM_FMT_B8;
			break;
		case 0x8:
			op = ASM_OP_RFI, fmt = ASM_FMT_B8;
			break;
		case 0xC:
			op = ASM_OP_BSW_0, fmt = ASM_FMT_B8;
			break;
		case 0xD:
			op = ASM_OP_BSW_1, fmt = ASM_FMT_B8;
			break;
		case 0x10:
			op = ASM_OP_EPC, fmt = ASM_FMT_B8;
			break;
		case 0x18:
			op = ASM_OP_VMSW_0, fmt = ASM_FMT_B8;
			break;
		case 0x19:
			op = ASM_OP_VMSW_1, fmt = ASM_FMT_B8;
			break;
		case 0x20:
			switch (FIELD(bits, 6, 3)) { /* btype */
			case 0x0:
				op = ASM_OP_BR_COND, fmt = ASM_FMT_B4;
				break;
			case 0x1:
				op = ASM_OP_BR_IA, fmt = ASM_FMT_B4;
				break;
			}
			break;
		case 0x21:
			if (FIELD(bits, 6, 3) == 4) /* btype */
				op = ASM_OP_BR_RET, fmt = ASM_FMT_B4;
			break;
		}
		break;
	case 0x1:
		op = ASM_OP_BR_CALL, fmt = ASM_FMT_B5;
		break;
	case 0x2:
		switch (FIELD(bits, 27, 6)) { /* x6 */
		case 0x0:
			op = ASM_OP_NOP_B, fmt = ASM_FMT_B9;
			break;
		case 0x1:
			op = ASM_OP_HINT_B, fmt = ASM_FMT_B9;
			break;
		case 0x10:
			op = ASM_OP_BRP_, fmt = ASM_FMT_B7;
			break;
		case 0x11:
			op = ASM_OP_BRP_RET, fmt = ASM_FMT_B7;
			break;
		}
		break;
	case 0x4:
		switch (FIELD(bits, 6, 3)) { /* btype */
		case 0x0:
			op = ASM_OP_BR_COND, fmt = ASM_FMT_B1;
			break;
		case 0x2:
			op = ASM_OP_BR_WEXIT, fmt = ASM_FMT_B1;
			break;
		case 0x3:
			op = ASM_OP_BR_WTOP, fmt = ASM_FMT_B1;
			break;
		case 0x5:
			op = ASM_OP_BR_CLOOP, fmt = ASM_FMT_B2;
			break;
		case 0x6:
			op = ASM_OP_BR_CEXIT, fmt = ASM_FMT_B2;
			break;
		case 0x7:
			op = ASM_OP_BR_CTOP, fmt = ASM_FMT_B2;
			break;
		}
		break;
	case 0x5:
		op = ASM_OP_BR_CALL, fmt = ASM_FMT_B3;
		break;
	case 0x7:
		op = ASM_OP_BRP_, fmt = ASM_FMT_B6;
		break;
	}

	if (op != ASM_OP_NONE)
		return (asm_extract(op, fmt, bits, b, slot));
	return (0);
}

/*
 * Decode F-unit instructions.
 */
static int
asm_decodeF(uint64_t ip, struct asm_bundle *b, int slot)
{
	uint64_t bits;
	enum asm_fmt fmt;
	enum asm_op op;

	bits = SLOT(ip, slot);
	fmt = ASM_FMT_NONE, op = ASM_OP_NONE;

	switch((int)OPCODE(bits)) {
	case 0x0:
		if (FIELD(bits, 33, 1) == 0) { /* x */
			switch (FIELD(bits, 27, 6)) { /* x6 */
			case 0x0:
				op = ASM_OP_BREAK_F, fmt = ASM_FMT_F15;
				break;
			case 0x1:
				if (FIELD(bits, 26, 1) == 0) /* y */
					op = ASM_OP_NOP_F, fmt = ASM_FMT_F16;
				else  
					op = ASM_OP_HINT_F, fmt = ASM_FMT_F16;
				break;
			case 0x4:
				op = ASM_OP_FSETC, fmt = ASM_FMT_F12;
				break;
			case 0x5:
				op = ASM_OP_FCLRF, fmt = ASM_FMT_F13;
				break;
			case 0x8:
				op = ASM_OP_FCHKF, fmt = ASM_FMT_F14;
				break;
			case 0x10:
				op = ASM_OP_FMERGE_S, fmt = ASM_FMT_F9;
				break;
			case 0x11:
				op = ASM_OP_FMERGE_NS, fmt = ASM_FMT_F9;
				break;
			case 0x12:
				op = ASM_OP_FMERGE_SE, fmt = ASM_FMT_F9;
				break;
			case 0x14:
				op = ASM_OP_FMIN, fmt = ASM_FMT_F8;
				break;
			case 0x15:
				op = ASM_OP_FMAX, fmt = ASM_FMT_F8;
				break;
			case 0x16:
				op = ASM_OP_FAMIN, fmt = ASM_FMT_F8;
				break;
			case 0x17:
				op = ASM_OP_FAMAX, fmt = ASM_FMT_F8;
				break;
			case 0x18:
				op = ASM_OP_FCVT_FX, fmt = ASM_FMT_F10;
				break;
			case 0x19:
				op = ASM_OP_FCVT_FXU, fmt = ASM_FMT_F10;
				break;
			case 0x1A:
				op = ASM_OP_FCVT_FX_TRUNC, fmt = ASM_FMT_F10;
				break;
			case 0x1B:
				op = ASM_OP_FCVT_FXU_TRUNC, fmt = ASM_FMT_F10;
				break;
			case 0x1C:
				op = ASM_OP_FCVT_XF, fmt = ASM_FMT_F11;
				break;
			case 0x28:
				op = ASM_OP_FPACK, fmt = ASM_FMT_F9;
				break;
			case 0x2C:
				op = ASM_OP_FAND, fmt = ASM_FMT_F9;
				break;
			case 0x2D:
				op = ASM_OP_FANDCM, fmt = ASM_FMT_F9;
				break;
			case 0x2E:
				op = ASM_OP_FOR, fmt = ASM_FMT_F9;
				break;
			case 0x2F:
				op = ASM_OP_FXOR, fmt = ASM_FMT_F9;
				break;
			case 0x34:
				op = ASM_OP_FSWAP_, fmt = ASM_FMT_F9;
				break;
			case 0x35:
				op = ASM_OP_FSWAP_NL, fmt = ASM_FMT_F9;
				break;
			case 0x36:
				op = ASM_OP_FSWAP_NR, fmt = ASM_FMT_F9;
				break;
			case 0x39:
				op = ASM_OP_FMIX_LR, fmt = ASM_FMT_F9;
				break;
			case 0x3A:
				op = ASM_OP_FMIX_R, fmt = ASM_FMT_F9;
				break;
			case 0x3B:
				op = ASM_OP_FMIX_L, fmt = ASM_FMT_F9;
				break;
			case 0x3C:
				op = ASM_OP_FSXT_R, fmt = ASM_FMT_F9;
				break;
			case 0x3D:
				op = ASM_OP_FSXT_L, fmt = ASM_FMT_F9;
				break;
			}
		} else {
			if (FIELD(bits, 36, 1) == 0) /* q */
				op = ASM_OP_FRCPA, fmt = ASM_FMT_F6;
			else
				op = ASM_OP_FRSQRTA, fmt = ASM_FMT_F7;
		}
		break;
	case 0x1:
		if (FIELD(bits, 33, 1) == 0) { /* x */
			switch (FIELD(bits, 27, 6)) { /* x6 */
			case 0x10:
				op = ASM_OP_FPMERGE_S, fmt = ASM_FMT_F9;
				break;
			case 0x11:
				op = ASM_OP_FPMERGE_NS, fmt = ASM_FMT_F9;
				break;
			case 0x12:
				op = ASM_OP_FPMERGE_SE, fmt = ASM_FMT_F9;
				break;
			case 0x14:
				op = ASM_OP_FPMIN, fmt = ASM_FMT_F8;
				break;
			case 0x15:
				op = ASM_OP_FPMAX, fmt = ASM_FMT_F8;
				break;
			case 0x16:
				op = ASM_OP_FPAMIN, fmt = ASM_FMT_F8;
				break;
			case 0x17:
				op = ASM_OP_FPAMAX, fmt = ASM_FMT_F8;
				break;
			case 0x18:
				op = ASM_OP_FPCVT_FX, fmt = ASM_FMT_F10;
				break;
			case 0x19:
				op = ASM_OP_FPCVT_FXU, fmt = ASM_FMT_F10;
				break;
			case 0x1A:
				op = ASM_OP_FPCVT_FX_TRUNC, fmt = ASM_FMT_F10;
				break;
			case 0x1B:
				op = ASM_OP_FPCVT_FXU_TRUNC, fmt = ASM_FMT_F10;
				break;
			case 0x30:
				op = ASM_OP_FPCMP_EQ, fmt = ASM_FMT_F8;
				break;
			case 0x31:
				op = ASM_OP_FPCMP_LT, fmt = ASM_FMT_F8;
				break;
			case 0x32:
				op = ASM_OP_FPCMP_LE, fmt = ASM_FMT_F8;
				break;
			case 0x33:
				op = ASM_OP_FPCMP_UNORD, fmt = ASM_FMT_F8;
				break;
			case 0x34:
				op = ASM_OP_FPCMP_NEQ, fmt = ASM_FMT_F8;
				break;
			case 0x35:
				op = ASM_OP_FPCMP_NLT, fmt = ASM_FMT_F8;
				break;
			case 0x36:
				op = ASM_OP_FPCMP_NLE, fmt = ASM_FMT_F8;
				break;
			case 0x37:
				op = ASM_OP_FPCMP_ORD, fmt = ASM_FMT_F8;
				break;
			}
		} else {
			if (FIELD(bits, 36, 1) == 0) /* q */
				op = ASM_OP_FPRCPA, fmt = ASM_FMT_F6;
			else
				op = ASM_OP_FPRSQRTA, fmt = ASM_FMT_F7;
		}
		break;
	case 0x4:
		op = ASM_OP_FCMP, fmt = ASM_FMT_F4;
		break;
	case 0x5:
		op = ASM_OP_FCLASS_M, fmt = ASM_FMT_F5;
		break;
	case 0x8:
		if (FIELD(bits, 36, 1) == 0) /* x */
			op = ASM_OP_FMA_, fmt = ASM_FMT_F1;
		else
			op = ASM_OP_FMA_S, fmt = ASM_FMT_F1;
		break;
	case 0x9:
		if (FIELD(bits, 36, 1) == 0) /* x */
			op = ASM_OP_FMA_D, fmt = ASM_FMT_F1;
		else
			op = ASM_OP_FPMA, fmt = ASM_FMT_F1;
		break;
	case 0xA:
		if (FIELD(bits, 36, 1) == 0) /* x */
			op = ASM_OP_FMS_, fmt = ASM_FMT_F1;
		else
			op = ASM_OP_FMS_S, fmt = ASM_FMT_F1;
		break;
	case 0xB:
		if (FIELD(bits, 36, 1) == 0) /* x */
			op = ASM_OP_FMS_D, fmt = ASM_FMT_F1;
		else
			op = ASM_OP_FPMS, fmt = ASM_FMT_F1;
		break;
	case 0xC:
		if (FIELD(bits, 36, 1) == 0) /* x */
			op = ASM_OP_FNMA_, fmt = ASM_FMT_F1;
		else
			op = ASM_OP_FNMA_S, fmt = ASM_FMT_F1;
		break;
	case 0xD:
		if (FIELD(bits, 36, 1) == 0) /* x */
			op = ASM_OP_FNMA_D, fmt = ASM_FMT_F1;
		else
			op = ASM_OP_FPNMA, fmt = ASM_FMT_F1;
		break;
	case 0xE:
		if (FIELD(bits, 36, 1) == 1) { /* x */
			switch (FIELD(bits, 34, 2)) { /* x2 */
			case 0x0:
				op = ASM_OP_XMA_L, fmt = ASM_FMT_F2;
				break;
			case 0x2:
				op = ASM_OP_XMA_HU, fmt = ASM_FMT_F2;
				break;
			case 0x3:
				op = ASM_OP_XMA_H, fmt = ASM_FMT_F2;
				break;
			}
		} else
			op = ASM_OP_FSELECT, fmt = ASM_FMT_F3;
		break;
	}

	if (op != ASM_OP_NONE)
		return (asm_extract(op, fmt, bits, b, slot));
	return (0);
}

/*
 * Decode I-unit instructions.
 */
static int
asm_decodeI(uint64_t ip, struct asm_bundle *b, int slot)
{
	uint64_t bits;
	enum asm_fmt fmt;
	enum asm_op op;

	bits = SLOT(ip, slot);
	if ((int)OPCODE(bits) >= 8)
		return (asm_decodeA(bits, b, slot));
	fmt = ASM_FMT_NONE, op = ASM_OP_NONE;

	switch((int)OPCODE(bits)) {
	case 0x0:
		switch (FIELD(bits, 33, 3)) { /* x3 */
		case 0x0:
			switch (FIELD(bits, 27, 6)) { /* x6 */
			case 0x0:
				op = ASM_OP_BREAK_I, fmt = ASM_FMT_I19;
				break;
			case 0x1:
				if (FIELD(bits, 26, 1) == 0) /* y */
					op = ASM_OP_NOP_I, fmt = ASM_FMT_I18;
				else
					op = ASM_OP_HINT_I, fmt = ASM_FMT_I18;
				break;
			case 0xA:
				op = ASM_OP_MOV_I, fmt = ASM_FMT_I27;
				break;
			case 0x10:
				op = ASM_OP_ZXT1, fmt = ASM_FMT_I29;
				break;
			case 0x11:
				op = ASM_OP_ZXT2, fmt = ASM_FMT_I29;
				break;
			case 0x12:
				op = ASM_OP_ZXT4, fmt = ASM_FMT_I29;
				break;
			case 0x14:
				op = ASM_OP_SXT1, fmt = ASM_FMT_I29;
				break;
			case 0x15:
				op = ASM_OP_SXT2, fmt = ASM_FMT_I29;
				break;
			case 0x16:
				op = ASM_OP_SXT4, fmt = ASM_FMT_I29;
				break;
			case 0x18:
				op = ASM_OP_CZX1_L, fmt = ASM_FMT_I29;
				break;
			case 0x19:
				op = ASM_OP_CZX2_L, fmt = ASM_FMT_I29;
				break;
			case 0x1C:
				op = ASM_OP_CZX1_R, fmt = ASM_FMT_I29;
				break;
			case 0x1D:
				op = ASM_OP_CZX2_R, fmt = ASM_FMT_I29;
				break;
			case 0x2A:
				op = ASM_OP_MOV_I, fmt = ASM_FMT_I26;
				break;
			case 0x30:
				op = ASM_OP_MOV_IP, fmt = ASM_FMT_I25;
				break;
			case 0x31:
				op = ASM_OP_MOV_, fmt = ASM_FMT_I22;
				break;
			case 0x32:
				op = ASM_OP_MOV_I, fmt = ASM_FMT_I28;
				break;
			case 0x33:
				op = ASM_OP_MOV_PR, fmt = ASM_FMT_I25;
				break;
			}
			break;
		case 0x1:
			op = ASM_OP_CHK_S_I, fmt = ASM_FMT_I20;
			break;
		case 0x2:
			op = ASM_OP_MOV_, fmt = ASM_FMT_I24;
			break;
		case 0x3:
			op = ASM_OP_MOV_, fmt = ASM_FMT_I23;
			break;
		case 0x7:
			if (FIELD(bits, 22, 1) == 0) /* x */
				op = ASM_OP_MOV_, fmt = ASM_FMT_I21;
			else
				op = ASM_OP_MOV_RET, fmt = ASM_FMT_I21;
			break;
		}
		break;
	case 0x4:
		op = ASM_OP_DEP_, fmt = ASM_FMT_I15;
		break;
	case 0x5:
		switch (FIELD(bits, 33, 3)) { /* x + x2 */
		case 0x0:
			if (FIELD(bits, 36, 1) == 0) { /* tb */
				switch (FIELD(bits, 12, 2)) { /* c + y */
				case 0x0:
					op = ASM_OP_TBIT_Z, fmt = ASM_FMT_I16;
					break;
				case 0x1:
					op = ASM_OP_TBIT_Z_UNC,
					    fmt = ASM_FMT_I16;
					break;
				case 0x2:
					if (FIELD(bits, 19, 1) == 0) /* x */
						op = ASM_OP_TNAT_Z,
						    fmt = ASM_FMT_I17;
					else
						op = ASM_OP_TF_Z,
						    fmt = ASM_FMT_I30;
					break;
				case 0x3:
					if (FIELD(bits, 19, 1) == 0) /* x */
						op = ASM_OP_TNAT_Z_UNC,
						    fmt = ASM_FMT_I17;
					else
						op = ASM_OP_TF_Z_UNC,
						    fmt = ASM_FMT_I30;
					break;
				}
			} else {
				switch (FIELD(bits, 12, 2)) { /* c + y */
				case 0x0:
					op = ASM_OP_TBIT_Z_AND,
					    fmt = ASM_FMT_I16;
					break;
				case 0x1:
					op = ASM_OP_TBIT_NZ_AND,
					    fmt = ASM_FMT_I16;
					break;
				case 0x2:
					if (FIELD(bits, 19, 1) == 0) /* x */
						op = ASM_OP_TNAT_Z_AND,
						    fmt = ASM_FMT_I17;
					else
						op = ASM_OP_TF_Z_AND,
						    fmt = ASM_FMT_I30;
					break;
				case 0x3:
					if (FIELD(bits, 19, 1) == 0) /* x */
						op = ASM_OP_TNAT_NZ_AND,
						    fmt = ASM_FMT_I17;
					else
						op = ASM_OP_TF_NZ_AND,
						    fmt = ASM_FMT_I30;
					break;
				}
			}
			break;
		case 0x1:
			if (FIELD(bits, 36, 1) == 0) { /* tb */
				switch (FIELD(bits, 12, 2)) { /* c + y */
				case 0x0:
					op = ASM_OP_TBIT_Z_OR,
					    fmt = ASM_FMT_I16;
					break;
				case 0x1:
					op = ASM_OP_TBIT_NZ_OR,
					    fmt = ASM_FMT_I16;
					break;
				case 0x2:
					if (FIELD(bits, 19, 1) == 0) /* x */
						op = ASM_OP_TNAT_Z_OR,
						    fmt = ASM_FMT_I17;
					else
						op = ASM_OP_TF_Z_OR,
						    fmt = ASM_FMT_I30;
					break;
				case 0x3:
					if (FIELD(bits, 19, 1) == 0) /* x */
						op = ASM_OP_TNAT_NZ_OR,
						    fmt = ASM_FMT_I17;
					else
						op = ASM_OP_TF_NZ_OR,
						    fmt = ASM_FMT_I30;
					break;
				}
			} else {
				switch (FIELD(bits, 12, 2)) { /* c + y */
				case 0x0:
					op = ASM_OP_TBIT_Z_OR_ANDCM,
					    fmt = ASM_FMT_I16;
					break;
				case 0x1:
					op = ASM_OP_TBIT_NZ_OR_ANDCM,
					    fmt = ASM_FMT_I16;
					break;
				case 0x2:
					if (FIELD(bits, 19, 1) == 0) /* x */
						op = ASM_OP_TNAT_Z_OR_ANDCM,
						    fmt = ASM_FMT_I17;
					else
						op = ASM_OP_TF_Z_OR_ANDCM,
						    fmt = ASM_FMT_I30;
					break;
				case 0x3:
					if (FIELD(bits, 19, 1) == 0) /* x */
						op = ASM_OP_TNAT_NZ_OR_ANDCM,
						    fmt = ASM_FMT_I17;
					else
						op = ASM_OP_TF_NZ_OR_ANDCM,
						    fmt = ASM_FMT_I30;
					break;
				}
			}
			break;
		case 0x2:
			op = ASM_OP_EXTR, fmt = ASM_FMT_I11;
			break;
		case 0x3:
			if (FIELD(bits, 26, 1) == 0) /* y */
				op = ASM_OP_DEP_Z, fmt = ASM_FMT_I12;
			else
				op = ASM_OP_DEP_Z, fmt = ASM_FMT_I13;
			break;
		case 0x6:
			op = ASM_OP_SHRP, fmt = ASM_FMT_I10;
			break;
		case 0x7:
			op = ASM_OP_DEP_, fmt = ASM_FMT_I14;
			break;
		}
		break;
	case 0x7:
		switch (FIELD(bits, 32, 5)) { /* ve + zb + x2a + za */
		case 0x2:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x0:
				op = ASM_OP_PSHR2_U, fmt = ASM_FMT_I5;
				break;
			case 0x1: case 0x5: case 0x9: case 0xD:
				op = ASM_OP_PMPYSHR2_U, fmt = ASM_FMT_I1;
				break;
			case 0x2:
				op = ASM_OP_PSHR2_, fmt = ASM_FMT_I5;
				break;
			case 0x3: case 0x7: case 0xB: case 0xF:
				op = ASM_OP_PMPYSHR2_, fmt = ASM_FMT_I1;
				break;
			case 0x4:
				op = ASM_OP_PSHL2, fmt = ASM_FMT_I7;
				break;
			}
			break;
		case 0x6:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x1:
				op = ASM_OP_PSHR2_U, fmt = ASM_FMT_I6;
				break;
			case 0x3:
				op = ASM_OP_PSHR2_, fmt = ASM_FMT_I6;
				break;
			case 0x9:
				op = ASM_OP_POPCNT, fmt = ASM_FMT_I9;
				break;
			}
			break;
		case 0x8:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x1:
				op = ASM_OP_PMIN1_U, fmt = ASM_FMT_I2;
				break;
			case 0x4:
				op = ASM_OP_UNPACK1_H, fmt = ASM_FMT_I2;
				break;
			case 0x5:
				op = ASM_OP_PMAX1_U, fmt = ASM_FMT_I2;
				break;
			case 0x6:
				op = ASM_OP_UNPACK1_L, fmt = ASM_FMT_I2;
				break;
			case 0x8:
				op = ASM_OP_MIX1_R, fmt = ASM_FMT_I2;
				break;
			case 0xA:
				op = ASM_OP_MIX1_L, fmt = ASM_FMT_I2;
				break;
			case 0xB:
				op = ASM_OP_PSAD1, fmt = ASM_FMT_I2;
				break;
			}
			break;
		case 0xA:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x0:
				op = ASM_OP_PACK2_USS, fmt = ASM_FMT_I2;
				break;
			case 0x2:
				op = ASM_OP_PACK2_SSS, fmt = ASM_FMT_I2;
				break;
			case 0x3:
				op = ASM_OP_PMIN2, fmt = ASM_FMT_I2;
				break;
			case 0x4:
				op = ASM_OP_UNPACK2_H, fmt = ASM_FMT_I2;
				break;
			case 0x6:
				op = ASM_OP_UNPACK2_L, fmt = ASM_FMT_I2;
				break;
			case 0x7:
				op = ASM_OP_PMAX2, fmt = ASM_FMT_I2;
				break;
			case 0x8:
				op = ASM_OP_MIX2_R, fmt = ASM_FMT_I2;
				break;
			case 0xA:
				op = ASM_OP_MIX2_L, fmt = ASM_FMT_I2;
				break;
			case 0xD:
				op = ASM_OP_PMPY2_R, fmt = ASM_FMT_I2;
				break;
			case 0xF:
				op = ASM_OP_PMPY2_L, fmt = ASM_FMT_I2;
				break;
			}
			break;
		case 0xC:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0xA:
				op = ASM_OP_MUX1, fmt = ASM_FMT_I3;
				break;
			}
			break;
		case 0xE:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x5:
				op = ASM_OP_PSHL2, fmt = ASM_FMT_I8;
				break;
			case 0xA:
				op = ASM_OP_MUX2, fmt = ASM_FMT_I4;
				break;
			}
			break;
		case 0x10:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x0:
				op = ASM_OP_PSHR4_U, fmt = ASM_FMT_I5;
				break;
			case 0x2:
				op = ASM_OP_PSHR4_, fmt = ASM_FMT_I5;
				break;
			case 0x4:
				op = ASM_OP_PSHL4, fmt = ASM_FMT_I7;
				break;
			}
			break;
		case 0x12:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x0:
				op = ASM_OP_SHR_U, fmt = ASM_FMT_I5;
				break;
			case 0x2:
				op = ASM_OP_SHR_, fmt = ASM_FMT_I5;
				break;
			case 0x4:
				op = ASM_OP_SHL, fmt = ASM_FMT_I7;
				break;
			}
			break;
		case 0x14:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x1:
				op = ASM_OP_PSHR4_U, fmt = ASM_FMT_I6;
				break;
			case 0x3:
				op = ASM_OP_PSHR4_, fmt = ASM_FMT_I6;
				break;
			}
			break;
		case 0x18:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x2:
				op = ASM_OP_PACK4_SSS, fmt = ASM_FMT_I2;
				break;
			case 0x4:
				op = ASM_OP_UNPACK4_H, fmt = ASM_FMT_I2;
				break;
			case 0x6:
				op = ASM_OP_UNPACK4_L, fmt = ASM_FMT_I2;
				break;
			case 0x8:
				op = ASM_OP_MIX4_R, fmt = ASM_FMT_I2;
				break;
			case 0xA:
				op = ASM_OP_MIX4_L, fmt = ASM_FMT_I2;
				break;
			}
			break;
		case 0x1C:
			switch (FIELD(bits, 28, 4)) { /* x2b + x2c */
			case 0x5:
				op = ASM_OP_PSHL4, fmt = ASM_FMT_I8;
				break;
			}
			break;
		}
		break;
	}

	if (op != ASM_OP_NONE)
		return (asm_extract(op, fmt, bits, b, slot));
	return (0);
}

/*
 * Decode M-unit instructions.
 */
static int
asm_decodeM(uint64_t ip, struct asm_bundle *b, int slot)
{
	uint64_t bits;
	enum asm_fmt fmt;
	enum asm_op op;

	bits = SLOT(ip, slot);
	if ((int)OPCODE(bits) >= 8)
		return (asm_decodeA(bits, b, slot));
	fmt = ASM_FMT_NONE, op = ASM_OP_NONE;

	switch((int)OPCODE(bits)) {
	case 0x0:
		switch (FIELD(bits, 33, 3)) { /* x3 */
		case 0x0:
			switch (FIELD(bits, 27, 6)) { /* x6 (x4 + x2) */
			case 0x0:
				op = ASM_OP_BREAK_M, fmt = ASM_FMT_M37;
				break;
			case 0x1:
				if (FIELD(bits, 26, 1) == 0) /* y */
					op = ASM_OP_NOP_M, fmt = ASM_FMT_M48;
				else
					op = ASM_OP_HINT_M, fmt = ASM_FMT_M48;
				break;
			case 0x4: case 0x14: case 0x24: case 0x34:
				op = ASM_OP_SUM, fmt = ASM_FMT_M44;
				break;
			case 0x5: case 0x15: case 0x25: case 0x35:
				op = ASM_OP_RUM, fmt = ASM_FMT_M44;
				break;
			case 0x6: case 0x16: case 0x26: case 0x36:
				op = ASM_OP_SSM, fmt = ASM_FMT_M44;
				break;
			case 0x7: case 0x17: case 0x27: case 0x37:
				op = ASM_OP_RSM, fmt = ASM_FMT_M44;
				break;
			case 0xA:
				op = ASM_OP_LOADRS, fmt = ASM_FMT_M25;
				break;
			case 0xC:
				op = ASM_OP_FLUSHRS, fmt = ASM_FMT_M25;
				break;
			case 0x10:
				op = ASM_OP_INVALA_, fmt = ASM_FMT_M24;
				break;
			case 0x12:
				op = ASM_OP_INVALA_E, fmt = ASM_FMT_M26;
				break;
			case 0x13:
				op = ASM_OP_INVALA_E, fmt = ASM_FMT_M27;
				break;
			case 0x20:
				op = ASM_OP_FWB, fmt = ASM_FMT_M24;
				break;
			case 0x22:
				op = ASM_OP_MF_, fmt = ASM_FMT_M24;
				break;
			case 0x23:
				op = ASM_OP_MF_A, fmt = ASM_FMT_M24;
				break;
			case 0x28:
				op = ASM_OP_MOV_M, fmt = ASM_FMT_M30;
				break;
			case 0x30:
				op = ASM_OP_SRLZ_D, fmt = ASM_FMT_M24;
				break;
			case 0x31:
				op = ASM_OP_SRLZ_I, fmt = ASM_FMT_M24;
				break;
			case 0x33:
				op = ASM_OP_SYNC_I, fmt = ASM_FMT_M24;
				break;
			}
			break;
		case 0x4:
			op = ASM_OP_CHK_A_NC, fmt = ASM_FMT_M22;
			break;
		case 0x5:
			op = ASM_OP_CHK_A_CLR, fmt = ASM_FMT_M22;
			break;
		case 0x6:
			op = ASM_OP_CHK_A_NC, fmt = ASM_FMT_M23;
			break;
		case 0x7:
			op = ASM_OP_CHK_A_CLR, fmt = ASM_FMT_M23;
			break;
		}
		break;
	case 0x1:
		switch (FIELD(bits, 33, 3)) { /* x3 */
		case 0x0:
			switch (FIELD(bits, 27, 6)) { /* x6 (x4 + x2) */
			case 0x0:
				op = ASM_OP_MOV_RR, fmt = ASM_FMT_M42;
				break;
			case 0x1:
				op = ASM_OP_MOV_DBR, fmt = ASM_FMT_M42;
				break;
			case 0x2:
				op = ASM_OP_MOV_IBR, fmt = ASM_FMT_M42;
				break;
			case 0x3:
				op = ASM_OP_MOV_PKR, fmt = ASM_FMT_M42;
				break;
			case 0x4:
				op = ASM_OP_MOV_PMC, fmt = ASM_FMT_M42;
				break;
			case 0x5:
				op = ASM_OP_MOV_PMD, fmt = ASM_FMT_M42;
				break;
			case 0x6:
				op = ASM_OP_MOV_MSR, fmt = ASM_FMT_M42;
				break;
			case 0x9:
				op = ASM_OP_PTC_L, fmt = ASM_FMT_M45;
				break;
			case 0xA:
				op = ASM_OP_PTC_G, fmt = ASM_FMT_M45;
				break;
			case 0xB:
				op = ASM_OP_PTC_GA, fmt = ASM_FMT_M45;
				break;
			case 0xC:
				op = ASM_OP_PTR_D, fmt = ASM_FMT_M45;
				break;
			case 0xD:
				op = ASM_OP_PTR_I, fmt = ASM_FMT_M45;
				break;
			case 0xE:
				op = ASM_OP_ITR_D, fmt = ASM_FMT_M42;
				break;
			case 0xF:
				op = ASM_OP_ITR_I, fmt = ASM_FMT_M42;
				break;
			case 0x10:
				op = ASM_OP_MOV_RR, fmt = ASM_FMT_M43;
				break;
			case 0x11:
				op = ASM_OP_MOV_DBR, fmt = ASM_FMT_M43;
				break;
			case 0x12:
				op = ASM_OP_MOV_IBR, fmt = ASM_FMT_M43;
				break;
			case 0x13:
				op = ASM_OP_MOV_PKR, fmt = ASM_FMT_M43;
				break;
			case 0x14:
				op = ASM_OP_MOV_PMC, fmt = ASM_FMT_M43;
				break;
			case 0x15:
				op = ASM_OP_MOV_PMD, fmt = ASM_FMT_M43;
				break;
			case 0x16:
				op = ASM_OP_MOV_MSR, fmt = ASM_FMT_M43;
				break;
			case 0x17:
				op = ASM_OP_MOV_CPUID, fmt = ASM_FMT_M43;
				break;
			case 0x18:
				op = ASM_OP_PROBE_R, fmt = ASM_FMT_M39;
				break;
			case 0x19:
				op = ASM_OP_PROBE_W, fmt = ASM_FMT_M39;
				break;
			case 0x1A:
				op = ASM_OP_THASH, fmt = ASM_FMT_M46;
				break;
			case 0x1B:
				op = ASM_OP_TTAG, fmt = ASM_FMT_M46;
				break;
			case 0x1E:
				op = ASM_OP_TPA, fmt = ASM_FMT_M46;
				break;
			case 0x1F:
				op = ASM_OP_TAK, fmt = ASM_FMT_M46;
				break;
			case 0x21:
				op = ASM_OP_MOV_PSR_UM, fmt = ASM_FMT_M36;
				break;
			case 0x22:
				op = ASM_OP_MOV_M, fmt = ASM_FMT_M31;
				break;
			case 0x24:
				op = ASM_OP_MOV_, fmt = ASM_FMT_M33;
				break;
			case 0x25:
				op = ASM_OP_MOV_PSR, fmt = ASM_FMT_M36;
				break;
			case 0x29:
				op = ASM_OP_MOV_PSR_UM, fmt = ASM_FMT_M35;
				break;
			case 0x2A:
				op = ASM_OP_MOV_M, fmt = ASM_FMT_M29;
				break;
			case 0x2C:
				op = ASM_OP_MOV_, fmt = ASM_FMT_M32;
				break;
			case 0x2D:
				op = ASM_OP_MOV_PSR_L, fmt = ASM_FMT_M35;
				break;
			case 0x2E:
				op = ASM_OP_ITC_D, fmt = ASM_FMT_M41;
				break;
			case 0x2F:
				op = ASM_OP_ITC_I, fmt = ASM_FMT_M41;
				break;
			case 0x30:
				if (FIELD(bits, 36, 1) == 0) /* x */
					op = ASM_OP_FC_, fmt = ASM_FMT_M28;
				else
					op = ASM_OP_FC_I, fmt = ASM_FMT_M28;
				break;
			case 0x31:
				op = ASM_OP_PROBE_RW_FAULT, fmt = ASM_FMT_M40;
				break;
			case 0x32:
				op = ASM_OP_PROBE_R_FAULT, fmt = ASM_FMT_M40;
				break;
			case 0x33:
				op = ASM_OP_PROBE_W_FAULT, fmt = ASM_FMT_M40;
				break;
			case 0x34:
				op = ASM_OP_PTC_E, fmt = ASM_FMT_M47;
				break;
			case 0x38:
				op = ASM_OP_PROBE_R, fmt = ASM_FMT_M38;
				break;
			case 0x39:
				op = ASM_OP_PROBE_W, fmt = ASM_FMT_M38;
				break;
			}
			break;
		case 0x1:
			op = ASM_OP_CHK_S_M, fmt = ASM_FMT_M20;
			break;
		case 0x3:
			op = ASM_OP_CHK_S, fmt = ASM_FMT_M21;
			break;
		case 0x6:
			op = ASM_OP_ALLOC, fmt = ASM_FMT_M34;
			break;
		}
		break;
	case 0x4:
		if (FIELD(bits, 27, 1) == 0) { /* x */
			switch (FIELD(bits, 30, 7)) { /* x6 + m */
			case 0x0:
				op = ASM_OP_LD1_, fmt = ASM_FMT_M1;
				break;
			case 0x1:
				op = ASM_OP_LD2_, fmt = ASM_FMT_M1;
				break;
			case 0x2:
				op = ASM_OP_LD4_, fmt = ASM_FMT_M1;
				break;
			case 0x3:
				op = ASM_OP_LD8_, fmt = ASM_FMT_M1;
				break;
			case 0x4:
				op = ASM_OP_LD1_S, fmt = ASM_FMT_M1;
				break;
			case 0x5:
				op = ASM_OP_LD2_S, fmt = ASM_FMT_M1;
				break;
			case 0x6:
				op = ASM_OP_LD4_S, fmt = ASM_FMT_M1;
				break;
			case 0x7:
				op = ASM_OP_LD8_S, fmt = ASM_FMT_M1;
				break;
			case 0x8:
				op = ASM_OP_LD1_A, fmt = ASM_FMT_M1;
				break;
			case 0x9:
				op = ASM_OP_LD2_A, fmt = ASM_FMT_M1;
				break;
			case 0xA:
				op = ASM_OP_LD4_A, fmt = ASM_FMT_M1;
				break;
			case 0xB:
				op = ASM_OP_LD8_A, fmt = ASM_FMT_M1;
				break;
			case 0xC:
				op = ASM_OP_LD1_SA, fmt = ASM_FMT_M1;
				break;
			case 0xD:
				op = ASM_OP_LD2_SA, fmt = ASM_FMT_M1;
				break;
			case 0xE:
				op = ASM_OP_LD4_SA, fmt = ASM_FMT_M1;
				break;
			case 0xF:
				op = ASM_OP_LD8_SA, fmt = ASM_FMT_M1;
				break;
			case 0x10:
				op = ASM_OP_LD1_BIAS, fmt = ASM_FMT_M1;
				break;
			case 0x11:
				op = ASM_OP_LD2_BIAS, fmt = ASM_FMT_M1;
				break;
			case 0x12:
				op = ASM_OP_LD4_BIAS, fmt = ASM_FMT_M1;
				break;
			case 0x13:
				op = ASM_OP_LD8_BIAS, fmt = ASM_FMT_M1;
				break;
			case 0x14:
				op = ASM_OP_LD1_ACQ, fmt = ASM_FMT_M1;
				break;
			case 0x15:
				op = ASM_OP_LD2_ACQ, fmt = ASM_FMT_M1;
				break;
			case 0x16:
				op = ASM_OP_LD4_ACQ, fmt = ASM_FMT_M1;
				break;
			case 0x17:
				op = ASM_OP_LD8_ACQ, fmt = ASM_FMT_M1;
				break;
			case 0x1B:
				op = ASM_OP_LD8_FILL, fmt = ASM_FMT_M1;
				break;
			case 0x20:
				op = ASM_OP_LD1_C_CLR, fmt = ASM_FMT_M1;
				break;
			case 0x21:
				op = ASM_OP_LD2_C_CLR, fmt = ASM_FMT_M1;
				break;
			case 0x22:
				op = ASM_OP_LD4_C_CLR, fmt = ASM_FMT_M1;
				break;
			case 0x23:
				op = ASM_OP_LD8_C_CLR, fmt = ASM_FMT_M1;
				break;
			case 0x24:
				op = ASM_OP_LD1_C_NC, fmt = ASM_FMT_M1;
				break;
			case 0x25:
				op = ASM_OP_LD2_C_NC, fmt = ASM_FMT_M1;
				break;
			case 0x26:
				op = ASM_OP_LD4_C_NC, fmt = ASM_FMT_M1;
				break;
			case 0x27:
				op = ASM_OP_LD8_C_NC, fmt = ASM_FMT_M1;
				break;
			case 0x28:
				op = ASM_OP_LD1_C_CLR_ACQ, fmt = ASM_FMT_M1;
				break;
			case 0x29:
				op = ASM_OP_LD2_C_CLR_ACQ, fmt = ASM_FMT_M1;
				break;
			case 0x2A:
				op = ASM_OP_LD4_C_CLR_ACQ, fmt = ASM_FMT_M1;
				break;
			case 0x2B:
				op = ASM_OP_LD8_C_CLR_ACQ, fmt = ASM_FMT_M1;
				break;
			case 0x30:
				op = ASM_OP_ST1_, fmt = ASM_FMT_M4;
				break;
			case 0x31:
				op = ASM_OP_ST2_, fmt = ASM_FMT_M4;
				break;
			case 0x32:
				op = ASM_OP_ST4_, fmt = ASM_FMT_M4;
				break;
			case 0x33:
				op = ASM_OP_ST8_, fmt = ASM_FMT_M4;
				break;
			case 0x34:
				op = ASM_OP_ST1_REL, fmt = ASM_FMT_M4;
				break;
			case 0x35:
				op = ASM_OP_ST2_REL, fmt = ASM_FMT_M4;
				break;
			case 0x36:
				op = ASM_OP_ST4_REL, fmt = ASM_FMT_M4;
				break;
			case 0x37:
				op = ASM_OP_ST8_REL, fmt = ASM_FMT_M4;
				break;
			case 0x3B:
				op = ASM_OP_ST8_SPILL, fmt = ASM_FMT_M4;
				break;
			case 0x40:
				op = ASM_OP_LD1_, fmt = ASM_FMT_M2;
				break;
			case 0x41:
				op = ASM_OP_LD2_, fmt = ASM_FMT_M2;
				break;
			case 0x42:
				op = ASM_OP_LD4_, fmt = ASM_FMT_M2;
				break;
			case 0x43:
				op = ASM_OP_LD8_, fmt = ASM_FMT_M2;
				break;
			case 0x44:
				op = ASM_OP_LD1_S, fmt = ASM_FMT_M2;
				break;
			case 0x45:
				op = ASM_OP_LD2_S, fmt = ASM_FMT_M2;
				break;
			case 0x46:
				op = ASM_OP_LD4_S, fmt = ASM_FMT_M2;
				break;
			case 0x47:
				op = ASM_OP_LD8_S, fmt = ASM_FMT_M2;
				break;
			case 0x48:
				op = ASM_OP_LD1_A, fmt = ASM_FMT_M2;
				break;
			case 0x49:
				op = ASM_OP_LD2_A, fmt = ASM_FMT_M2;
				break;
			case 0x4A:
				op = ASM_OP_LD4_A, fmt = ASM_FMT_M2;
				break;
			case 0x4B:
				op = ASM_OP_LD8_A, fmt = ASM_FMT_M2;
				break;
			case 0x4C:
				op = ASM_OP_LD1_SA, fmt = ASM_FMT_M2;
				break;
			case 0x4D:
				op = ASM_OP_LD2_SA, fmt = ASM_FMT_M2;
				break;
			case 0x4E:
				op = ASM_OP_LD4_SA, fmt = ASM_FMT_M2;
				break;
			case 0x4F:
				op = ASM_OP_LD8_SA, fmt = ASM_FMT_M2;
				break;
			case 0x50:
				op = ASM_OP_LD1_BIAS, fmt = ASM_FMT_M2;
				break;
			case 0x51:
				op = ASM_OP_LD2_BIAS, fmt = ASM_FMT_M2;
				break;
			case 0x52:
				op = ASM_OP_LD4_BIAS, fmt = ASM_FMT_M2;
				break;
			case 0x53:
				op = ASM_OP_LD8_BIAS, fmt = ASM_FMT_M2;
				break;
			case 0x54:
				op = ASM_OP_LD1_ACQ, fmt = ASM_FMT_M2;
				break;
			case 0x55:
				op = ASM_OP_LD2_ACQ, fmt = ASM_FMT_M2;
				break;
			case 0x56:
				op = ASM_OP_LD4_ACQ, fmt = ASM_FMT_M2;
				break;
			case 0x57:
				op = ASM_OP_LD8_ACQ, fmt = ASM_FMT_M2;
				break;
			case 0x5B:
				op = ASM_OP_LD8_FILL, fmt = ASM_FMT_M2;
				break;
			case 0x60:
				op = ASM_OP_LD1_C_CLR, fmt = ASM_FMT_M2;
				break;
			case 0x61:
				op = ASM_OP_LD2_C_CLR, fmt = ASM_FMT_M2;
				break;
			case 0x62:
				op = ASM_OP_LD4_C_CLR, fmt = ASM_FMT_M2;
				break;
			case 0x63:
				op = ASM_OP_LD8_C_CLR, fmt = ASM_FMT_M2;
				break;
			case 0x64:
				op = ASM_OP_LD1_C_NC, fmt = ASM_FMT_M2;
				break;
			case 0x65:
				op = ASM_OP_LD2_C_NC, fmt = ASM_FMT_M2;
				break;
			case 0x66:
				op = ASM_OP_LD4_C_NC, fmt = ASM_FMT_M2;
				break;
			case 0x67:
				op = ASM_OP_LD8_C_NC, fmt = ASM_FMT_M2;
				break;
			case 0x68:
				op = ASM_OP_LD1_C_CLR_ACQ, fmt = ASM_FMT_M2;
				break;
			case 0x69:
				op = ASM_OP_LD2_C_CLR_ACQ, fmt = ASM_FMT_M2;
				break;
			case 0x6A:
				op = ASM_OP_LD4_C_CLR_ACQ, fmt = ASM_FMT_M2;
				break;
			case 0x6B:
				op = ASM_OP_LD8_C_CLR_ACQ, fmt = ASM_FMT_M2;
				break;
			}
		} else {
			switch (FIELD(bits, 30, 7)) { /* x6 + m */
			case 0x0:
				op = ASM_OP_CMPXCHG1_ACQ, fmt = ASM_FMT_M16;
				break;
			case 0x1:
				op = ASM_OP_CMPXCHG2_ACQ, fmt = ASM_FMT_M16;
				break;
			case 0x2:
				op = ASM_OP_CMPXCHG4_ACQ, fmt = ASM_FMT_M16;
				break;
			case 0x3:
				op = ASM_OP_CMPXCHG8_ACQ, fmt = ASM_FMT_M16;
				break;
			case 0x4:
				op = ASM_OP_CMPXCHG1_REL, fmt = ASM_FMT_M16;
				break;
			case 0x5:
				op = ASM_OP_CMPXCHG2_REL, fmt = ASM_FMT_M16;
				break;
			case 0x6:
				op = ASM_OP_CMPXCHG4_REL, fmt = ASM_FMT_M16;
				break;
			case 0x7:
				op = ASM_OP_CMPXCHG8_REL, fmt = ASM_FMT_M16;
				break;
			case 0x8:
				op = ASM_OP_XCHG1, fmt = ASM_FMT_M16;
				break;
			case 0x9:
				op = ASM_OP_XCHG2, fmt = ASM_FMT_M16;
				break;
			case 0xA:
				op = ASM_OP_XCHG4, fmt = ASM_FMT_M16;
				break;
			case 0xB:
				op = ASM_OP_XCHG8, fmt = ASM_FMT_M16;
				break;
			case 0x12:
				op = ASM_OP_FETCHADD4_ACQ, fmt = ASM_FMT_M17;
				break;
			case 0x13:
				op = ASM_OP_FETCHADD8_ACQ, fmt = ASM_FMT_M17;
				break;
			case 0x16:
				op = ASM_OP_FETCHADD4_REL, fmt = ASM_FMT_M17;
				break;
			case 0x17:
				op = ASM_OP_FETCHADD8_REL, fmt = ASM_FMT_M17;
				break;
			case 0x1C:
				op = ASM_OP_GETF_SIG, fmt = ASM_FMT_M19;
				break;
			case 0x1D:
				op = ASM_OP_GETF_EXP, fmt = ASM_FMT_M19;
				break;
			case 0x1E:
				op = ASM_OP_GETF_S, fmt = ASM_FMT_M19;
				break;
			case 0x1F:
				op = ASM_OP_GETF_D, fmt = ASM_FMT_M19;
				break;
			case 0x20:
				op = ASM_OP_CMP8XCHG16_ACQ, fmt = ASM_FMT_M16;
				break;
			case 0x24:
				op = ASM_OP_CMP8XCHG16_REL, fmt = ASM_FMT_M16;
				break;
			case 0x28:
				op = ASM_OP_LD16_, fmt = ASM_FMT_M1;
				break;
			case 0x2C:
				op = ASM_OP_LD16_ACQ, fmt = ASM_FMT_M1;
				break;
			case 0x30:
				op = ASM_OP_ST16_, fmt = ASM_FMT_M4;
				break;
			case 0x34:
				op = ASM_OP_ST16_REL, fmt = ASM_FMT_M4;
				break;
			}
		}
		break;
	case 0x5:
		switch (FIELD(bits, 30, 6)) { /* x6 */
		case 0x0:
			op = ASM_OP_LD1_, fmt = ASM_FMT_M3;
			break;
		case 0x1:
			op = ASM_OP_LD2_, fmt = ASM_FMT_M3;
			break;
		case 0x2:
			op = ASM_OP_LD4_, fmt = ASM_FMT_M3;
			break;
		case 0x3:
			op = ASM_OP_LD8_, fmt = ASM_FMT_M3;
			break;
		case 0x4:
			op = ASM_OP_LD1_S, fmt = ASM_FMT_M3;
			break;
		case 0x5:
			op = ASM_OP_LD2_S, fmt = ASM_FMT_M3;
			break;
		case 0x6:
			op = ASM_OP_LD4_S, fmt = ASM_FMT_M3;
			break;
		case 0x7:
			op = ASM_OP_LD8_S, fmt = ASM_FMT_M3;
			break;
		case 0x8:
			op = ASM_OP_LD1_A, fmt = ASM_FMT_M3;
			break;
		case 0x9:
			op = ASM_OP_LD2_A, fmt = ASM_FMT_M3;
			break;
		case 0xA:
			op = ASM_OP_LD4_A, fmt = ASM_FMT_M3;
			break;
		case 0xB:
			op = ASM_OP_LD8_A, fmt = ASM_FMT_M3;
			break;
		case 0xC:
			op = ASM_OP_LD1_SA, fmt = ASM_FMT_M3;
			break;
		case 0xD:
			op = ASM_OP_LD2_SA, fmt = ASM_FMT_M3;
			break;
		case 0xE:
			op = ASM_OP_LD4_SA, fmt = ASM_FMT_M3;
			break;
		case 0xF:
			op = ASM_OP_LD8_SA, fmt = ASM_FMT_M3;
			break;
		case 0x10:
			op = ASM_OP_LD1_BIAS, fmt = ASM_FMT_M3;
			break;
		case 0x11:
			op = ASM_OP_LD2_BIAS, fmt = ASM_FMT_M3;
			break;
		case 0x12:
			op = ASM_OP_LD4_BIAS, fmt = ASM_FMT_M3;
			break;
		case 0x13:
			op = ASM_OP_LD8_BIAS, fmt = ASM_FMT_M3;
			break;
		case 0x14:
			op = ASM_OP_LD1_ACQ, fmt = ASM_FMT_M3;
			break;
		case 0x15:
			op = ASM_OP_LD2_ACQ, fmt = ASM_FMT_M3;
			break;
		case 0x16:
			op = ASM_OP_LD4_ACQ, fmt = ASM_FMT_M3;
			break;
		case 0x17:
			op = ASM_OP_LD8_ACQ, fmt = ASM_FMT_M3;
			break;
		case 0x1B:
			op = ASM_OP_LD8_FILL, fmt = ASM_FMT_M3;
			break;
		case 0x20:
			op = ASM_OP_LD1_C_CLR, fmt = ASM_FMT_M3;
			break;
		case 0x21:
			op = ASM_OP_LD2_C_CLR, fmt = ASM_FMT_M3;
			break;
		case 0x22:
			op = ASM_OP_LD4_C_CLR, fmt = ASM_FMT_M3;
			break;
		case 0x23:
			op = ASM_OP_LD8_C_CLR, fmt = ASM_FMT_M3;
			break;
		case 0x24:
			op = ASM_OP_LD1_C_NC, fmt = ASM_FMT_M3;
			break;
		case 0x25:
			op = ASM_OP_LD2_C_NC, fmt = ASM_FMT_M3;
			break;
		case 0x26:
			op = ASM_OP_LD4_C_NC, fmt = ASM_FMT_M3;
			break;
		case 0x27:
			op = ASM_OP_LD8_C_NC, fmt = ASM_FMT_M3;
			break;
		case 0x28:
			op = ASM_OP_LD1_C_CLR_ACQ, fmt = ASM_FMT_M3;
			break;
		case 0x29:
			op = ASM_OP_LD2_C_CLR_ACQ, fmt = ASM_FMT_M3;
			break;
		case 0x2A:
			op = ASM_OP_LD4_C_CLR_ACQ, fmt = ASM_FMT_M3;
			break;
		case 0x2B:
			op = ASM_OP_LD8_C_CLR_ACQ, fmt = ASM_FMT_M3;
			break;
		case 0x30:
			op = ASM_OP_ST1_, fmt = ASM_FMT_M5;
			break;
		case 0x31:
			op = ASM_OP_ST2_, fmt = ASM_FMT_M5;
			break;
		case 0x32:
			op = ASM_OP_ST4_, fmt = ASM_FMT_M5;
			break;
		case 0x33:
			op = ASM_OP_ST8_, fmt = ASM_FMT_M5;
			break;
		case 0x34:
			op = ASM_OP_ST1_REL, fmt = ASM_FMT_M5;
			break;
		case 0x35:
			op = ASM_OP_ST2_REL, fmt = ASM_FMT_M5;
			break;
		case 0x36:
			op = ASM_OP_ST4_REL, fmt = ASM_FMT_M5;
			break;
		case 0x37:
			op = ASM_OP_ST8_REL, fmt = ASM_FMT_M5;
			break;
		case 0x3B:
			op = ASM_OP_ST8_SPILL, fmt = ASM_FMT_M5;
			break;
		}
		break;
	case 0x6:
		if (FIELD(bits, 27, 1) == 0) { /* x */
			switch (FIELD(bits, 30, 7)) { /* x6 + m */
			case 0x0:
				op = ASM_OP_LDFE_, fmt = ASM_FMT_M6;
				break;
			case 0x1:
				op = ASM_OP_LDF8_, fmt = ASM_FMT_M6;
				break;
			case 0x2:
				op = ASM_OP_LDFS_, fmt = ASM_FMT_M6;
				break;
			case 0x3:
				op = ASM_OP_LDFD_, fmt = ASM_FMT_M6;
				break;
			case 0x4:
				op = ASM_OP_LDFE_S, fmt = ASM_FMT_M6;
				break;
			case 0x5:
				op = ASM_OP_LDF8_S, fmt = ASM_FMT_M6;
				break;
			case 0x6:
				op = ASM_OP_LDFS_S, fmt = ASM_FMT_M6;
				break;
			case 0x7:
				op = ASM_OP_LDFD_S, fmt = ASM_FMT_M6;
				break;
			case 0x8:
				op = ASM_OP_LDFE_A, fmt = ASM_FMT_M6;
				break;
			case 0x9:
				op = ASM_OP_LDF8_A, fmt = ASM_FMT_M6;
				break;
			case 0xA:
				op = ASM_OP_LDFS_A, fmt = ASM_FMT_M6;
				break;
			case 0xB:
				op = ASM_OP_LDFD_A, fmt = ASM_FMT_M6;
				break;
			case 0xC:
				op = ASM_OP_LDFE_SA, fmt = ASM_FMT_M6;
				break;
			case 0xD:
				op = ASM_OP_LDF8_SA, fmt = ASM_FMT_M6;
				break;
			case 0xE:
				op = ASM_OP_LDFS_SA, fmt = ASM_FMT_M6;
				break;
			case 0xF:
				op = ASM_OP_LDFD_SA, fmt = ASM_FMT_M6;
				break;
			case 0x1B:
				op = ASM_OP_LDF_FILL, fmt = ASM_FMT_M6;
				break;
			case 0x20:
				op = ASM_OP_LDFE_C_CLR, fmt = ASM_FMT_M6;
				break;
			case 0x21:
				op = ASM_OP_LDF8_C_CLR, fmt = ASM_FMT_M6;
				break;
			case 0x22:
				op = ASM_OP_LDFS_C_CLR, fmt = ASM_FMT_M6;
				break;
			case 0x23:
				op = ASM_OP_LDFD_C_CLR, fmt = ASM_FMT_M6;
				break;
			case 0x24:
				op = ASM_OP_LDFE_C_NC, fmt = ASM_FMT_M6;
				break;
			case 0x25:
				op = ASM_OP_LDF8_C_NC, fmt = ASM_FMT_M6;
				break;
			case 0x26:
				op = ASM_OP_LDFS_C_NC, fmt = ASM_FMT_M6;
				break;
			case 0x27:
				op = ASM_OP_LDFD_C_NC, fmt = ASM_FMT_M6;
				break;
			case 0x2C:
				op = ASM_OP_LFETCH_, fmt = ASM_FMT_M13;
				break;
			case 0x2D:
				op = ASM_OP_LFETCH_EXCL, fmt = ASM_FMT_M13;
				break;
			case 0x2E:
				op = ASM_OP_LFETCH_FAULT, fmt = ASM_FMT_M13;
				break;
			case 0x2F:
				op = ASM_OP_LFETCH_FAULT_EXCL,
				    fmt = ASM_FMT_M13;
				break;
			case 0x30:
				op = ASM_OP_STFE, fmt = ASM_FMT_M9;
				break;
			case 0x31:
				op = ASM_OP_STF8, fmt = ASM_FMT_M9;
				break;
			case 0x32:
				op = ASM_OP_STFS, fmt = ASM_FMT_M9;
				break;
			case 0x33:
				op = ASM_OP_STFD, fmt = ASM_FMT_M9;
				break;
			case 0x3B:
				op = ASM_OP_STF_SPILL, fmt = ASM_FMT_M9;
				break;
			case 0x40:
				op = ASM_OP_LDFE_, fmt = ASM_FMT_M7;
				break;
			case 0x41:
				op = ASM_OP_LDF8_, fmt = ASM_FMT_M7;
				break;
			case 0x42:
				op = ASM_OP_LDFS_, fmt = ASM_FMT_M7;
				break;
			case 0x43:
				op = ASM_OP_LDFD_, fmt = ASM_FMT_M7;
				break;
			case 0x44:
				op = ASM_OP_LDFE_S, fmt = ASM_FMT_M7;
				break;
			case 0x45:
				op = ASM_OP_LDF8_S, fmt = ASM_FMT_M7;
				break;
			case 0x46:
				op = ASM_OP_LDFS_S, fmt = ASM_FMT_M7;
				break;
			case 0x47:
				op = ASM_OP_LDFD_S, fmt = ASM_FMT_M7;
				break;
			case 0x48:
				op = ASM_OP_LDFE_A, fmt = ASM_FMT_M7;
				break;
			case 0x49:
				op = ASM_OP_LDF8_A, fmt = ASM_FMT_M7;
				break;
			case 0x4A:
				op = ASM_OP_LDFS_A, fmt = ASM_FMT_M7;
				break;
			case 0x4B:
				op = ASM_OP_LDFD_A, fmt = ASM_FMT_M7;
				break;
			case 0x4C:
				op = ASM_OP_LDFE_SA, fmt = ASM_FMT_M7;
				break;
			case 0x4D:
				op = ASM_OP_LDF8_SA, fmt = ASM_FMT_M7;
				break;
			case 0x4E:
				op = ASM_OP_LDFS_SA, fmt = ASM_FMT_M7;
				break;
			case 0x4F:
				op = ASM_OP_LDFD_SA, fmt = ASM_FMT_M7;
				break;
			case 0x5B:
				op = ASM_OP_LDF_FILL, fmt = ASM_FMT_M7;
				break;
			case 0x60:
				op = ASM_OP_LDFE_C_CLR, fmt = ASM_FMT_M7;
				break;
			case 0x61:
				op = ASM_OP_LDF8_C_CLR, fmt = ASM_FMT_M7;
				break;
			case 0x62:
				op = ASM_OP_LDFS_C_CLR, fmt = ASM_FMT_M7;
				break;
			case 0x63:
				op = ASM_OP_LDFD_C_CLR, fmt = ASM_FMT_M7;
				break;
			case 0x64:
				op = ASM_OP_LDFE_C_NC, fmt = ASM_FMT_M7;
				break;
			case 0x65:
				op = ASM_OP_LDF8_C_NC, fmt = ASM_FMT_M7;
				break;
			case 0x66:
				op = ASM_OP_LDFS_C_NC, fmt = ASM_FMT_M7;
				break;
			case 0x67:
				op = ASM_OP_LDFD_C_NC, fmt = ASM_FMT_M7;
				break;
			case 0x6C:
				op = ASM_OP_LFETCH_, fmt = ASM_FMT_M14;
				break;
			case 0x6D:
				op = ASM_OP_LFETCH_EXCL, fmt = ASM_FMT_M14;
				break;
			case 0x6E:
				op = ASM_OP_LFETCH_FAULT, fmt = ASM_FMT_M14;
				break;
			case 0x6F:
				op = ASM_OP_LFETCH_FAULT_EXCL,
				    fmt = ASM_FMT_M14;
				break;
			}
		} else {
			switch (FIELD(bits, 30, 7)) { /* x6 + m */
			case 0x1:
				op = ASM_OP_LDFP8_, fmt = ASM_FMT_M11;
				break;
			case 0x2:
				op = ASM_OP_LDFPS_, fmt = ASM_FMT_M11;
				break;
			case 0x3:
				op = ASM_OP_LDFPD_, fmt = ASM_FMT_M11;
				break;
			case 0x5:
				op = ASM_OP_LDFP8_S, fmt = ASM_FMT_M11;
				break;
			case 0x6:
				op = ASM_OP_LDFPS_S, fmt = ASM_FMT_M11;
				break;
			case 0x7:
				op = ASM_OP_LDFPD_S, fmt = ASM_FMT_M11;
				break;
			case 0x9:
				op = ASM_OP_LDFP8_A, fmt = ASM_FMT_M11;
				break;
			case 0xA:
				op = ASM_OP_LDFPS_A, fmt = ASM_FMT_M11;
				break;
			case 0xB:
				op = ASM_OP_LDFPD_A, fmt = ASM_FMT_M11;
				break;
			case 0xD:
				op = ASM_OP_LDFP8_SA, fmt = ASM_FMT_M11;
				break;
			case 0xE:
				op = ASM_OP_LDFPS_SA, fmt = ASM_FMT_M11;
				break;
			case 0xF:
				op = ASM_OP_LDFPD_SA, fmt = ASM_FMT_M11;
				break;
			case 0x1C:
				op = ASM_OP_SETF_SIG, fmt = ASM_FMT_M18;
				break;
			case 0x1D:
				op = ASM_OP_SETF_EXP, fmt = ASM_FMT_M18;
				break;
			case 0x1E:
				op = ASM_OP_SETF_S, fmt = ASM_FMT_M18;
				break;
			case 0x1F:
				op = ASM_OP_SETF_D, fmt = ASM_FMT_M18;
				break;
			case 0x21:
				op = ASM_OP_LDFP8_C_CLR, fmt = ASM_FMT_M11;
				break;
			case 0x22:
				op = ASM_OP_LDFPS_C_CLR, fmt = ASM_FMT_M11;
				break;
			case 0x23:
				op = ASM_OP_LDFPD_C_CLR, fmt = ASM_FMT_M11;
				break;
			case 0x25:
				op = ASM_OP_LDFP8_C_NC, fmt = ASM_FMT_M11;
				break;
			case 0x26:
				op = ASM_OP_LDFPS_C_NC, fmt = ASM_FMT_M11;
				break;
			case 0x27:
				op = ASM_OP_LDFPD_C_NC, fmt = ASM_FMT_M11;
				break;
			case 0x41:
				op = ASM_OP_LDFP8_, fmt = ASM_FMT_M12;
				break;
			case 0x42:
				op = ASM_OP_LDFPS_, fmt = ASM_FMT_M12;
				break;
			case 0x43:
				op = ASM_OP_LDFPD_, fmt = ASM_FMT_M12;
				break;
			case 0x45:
				op = ASM_OP_LDFP8_S, fmt = ASM_FMT_M12;
				break;
			case 0x46:
				op = ASM_OP_LDFPS_S, fmt = ASM_FMT_M12;
				break;
			case 0x47:
				op = ASM_OP_LDFPD_S, fmt = ASM_FMT_M12;
				break;
			case 0x49:
				op = ASM_OP_LDFP8_A, fmt = ASM_FMT_M12;
				break;
			case 0x4A:
				op = ASM_OP_LDFPS_A, fmt = ASM_FMT_M12;
				break;
			case 0x4B:
				op = ASM_OP_LDFPD_A, fmt = ASM_FMT_M12;
				break;
			case 0x4D:
				op = ASM_OP_LDFP8_SA, fmt = ASM_FMT_M12;
				break;
			case 0x4E:
				op = ASM_OP_LDFPS_SA, fmt = ASM_FMT_M12;
				break;
			case 0x4F:
				op = ASM_OP_LDFPD_SA, fmt = ASM_FMT_M12;
				break;
			case 0x61:
				op = ASM_OP_LDFP8_C_CLR, fmt = ASM_FMT_M12;
				break;
			case 0x62:
				op = ASM_OP_LDFPS_C_CLR, fmt = ASM_FMT_M12;
				break;
			case 0x63:
				op = ASM_OP_LDFPD_C_CLR, fmt = ASM_FMT_M12;
				break;
			case 0x65:
				op = ASM_OP_LDFP8_C_NC, fmt = ASM_FMT_M12;
				break;
			case 0x66:
				op = ASM_OP_LDFPS_C_NC, fmt = ASM_FMT_M12;
				break;
			case 0x67:
				op = ASM_OP_LDFPD_C_NC, fmt = ASM_FMT_M12;
				break;
			}
		}
		break;
	case 0x7:
		switch (FIELD(bits, 30, 6)) { /* x6 */
		case 0x0:
			op = ASM_OP_LDFE_, fmt = ASM_FMT_M8;
			break;
		case 0x1:
			op = ASM_OP_LDF8_, fmt = ASM_FMT_M8;
			break;
		case 0x2:
			op = ASM_OP_LDFS_, fmt = ASM_FMT_M8;
			break;
		case 0x3:
			op = ASM_OP_LDFD_, fmt = ASM_FMT_M8;
			break;
		case 0x4:
			op = ASM_OP_LDFE_S, fmt = ASM_FMT_M8;
			break;
		case 0x5:
			op = ASM_OP_LDF8_S, fmt = ASM_FMT_M8;
			break;
		case 0x6:
			op = ASM_OP_LDFS_S, fmt = ASM_FMT_M8;
			break;
		case 0x7:
			op = ASM_OP_LDFD_S, fmt = ASM_FMT_M8;
			break;
		case 0x8:
			op = ASM_OP_LDFE_A, fmt = ASM_FMT_M8;
			break;
		case 0x9:
			op = ASM_OP_LDF8_A, fmt = ASM_FMT_M8;
			break;
		case 0xA:
			op = ASM_OP_LDFS_A, fmt = ASM_FMT_M8;
			break;
		case 0xB:
			op = ASM_OP_LDFD_A, fmt = ASM_FMT_M8;
			break;
		case 0xC:
			op = ASM_OP_LDFE_SA, fmt = ASM_FMT_M8;
			break;
		case 0xD:
			op = ASM_OP_LDF8_SA, fmt = ASM_FMT_M8;
			break;
		case 0xE:
			op = ASM_OP_LDFS_SA, fmt = ASM_FMT_M8;
			break;
		case 0xF:
			op = ASM_OP_LDFD_SA, fmt = ASM_FMT_M8;
			break;
		case 0x1B:
			op = ASM_OP_LDF_FILL, fmt = ASM_FMT_M8;
			break;
		case 0x20:
			op = ASM_OP_LDFE_C_CLR, fmt = ASM_FMT_M8;
			break;
		case 0x21:
			op = ASM_OP_LDF8_C_CLR, fmt = ASM_FMT_M8;
			break;
		case 0x22:
			op = ASM_OP_LDFS_C_CLR, fmt = ASM_FMT_M8;
			break;
		case 0x23:
			op = ASM_OP_LDFD_C_CLR, fmt = ASM_FMT_M8;
			break;
		case 0x24:
			op = ASM_OP_LDFE_C_NC, fmt = ASM_FMT_M8;
			break;
		case 0x25:
			op = ASM_OP_LDF8_C_NC, fmt = ASM_FMT_M8;
			break;
		case 0x26:
			op = ASM_OP_LDFS_C_NC, fmt = ASM_FMT_M8;
			break;
		case 0x27:
			op = ASM_OP_LDFD_C_NC, fmt = ASM_FMT_M8;
			break;
		case 0x2C:
			op = ASM_OP_LFETCH_, fmt = ASM_FMT_M15;
			break;
		case 0x2D:
			op = ASM_OP_LFETCH_EXCL, fmt = ASM_FMT_M15;
			break;
		case 0x2E:
			op = ASM_OP_LFETCH_FAULT, fmt = ASM_FMT_M15;
			break;
		case 0x2F:
			op = ASM_OP_LFETCH_FAULT_EXCL, fmt = ASM_FMT_M15;
			break;
		case 0x30:
			op = ASM_OP_STFE, fmt = ASM_FMT_M10;
			break;
		case 0x31:
			op = ASM_OP_STF8, fmt = ASM_FMT_M10;
			break;
		case 0x32:
			op = ASM_OP_STFS, fmt = ASM_FMT_M10;
			break;
		case 0x33:
			op = ASM_OP_STFD, fmt = ASM_FMT_M10;
			break;
		case 0x3B:
			op = ASM_OP_STF_SPILL, fmt = ASM_FMT_M10;
			break;
		}
		break;
	}

	if (op != ASM_OP_NONE)
		return (asm_extract(op, fmt, bits, b, slot));
	return (0);
}

/*
 * Decode X-unit instructions.
 */
static int
asm_decodeX(uint64_t ip, struct asm_bundle *b, int slot)
{
	uint64_t bits;
	enum asm_fmt fmt;
	enum asm_op op;

	KASSERT(slot == 2, ("foo"));
	bits = SLOT(ip, slot);
	fmt = ASM_FMT_NONE, op = ASM_OP_NONE;
	/* Initialize slot 1 (slot - 1) */
	b->b_inst[slot - 1].i_format = ASM_FMT_NONE;
	b->b_inst[slot - 1].i_bits = SLOT(ip, slot - 1);

	switch((int)OPCODE(bits)) {
	case 0x0:
		if (FIELD(bits, 33, 3) == 0) { /* x3 */
			switch (FIELD(bits, 27, 6)) { /* x6 */
			case 0x0:
				op = ASM_OP_BREAK_X, fmt = ASM_FMT_X1;
				break;
			case 0x1:
				if (FIELD(bits, 26, 1) == 0) /* y */
					op = ASM_OP_NOP_X, fmt = ASM_FMT_X5;
				else
					op = ASM_OP_HINT_X, fmt = ASM_FMT_X5;
				break;
			}
		}
		break;
	case 0x6:
		if (FIELD(bits, 20, 1) == 0)
			op = ASM_OP_MOVL, fmt = ASM_FMT_X2;
		break;
	case 0xC:
		if (FIELD(bits, 6, 3) == 0) /* btype */
			op = ASM_OP_BRL_COND, fmt = ASM_FMT_X3;
		break;
	case 0xD:
		op = ASM_OP_BRL_CALL, fmt = ASM_FMT_X4;
		break;
	}

	if (op != ASM_OP_NONE)
		return (asm_extract(op, fmt, bits, b, slot));
	return (0);
}

int
asm_decode(uint64_t ip, struct asm_bundle *b)
{
	const char *tp;
	unsigned int slot;
	int ok;

	memset(b, 0, sizeof(*b));

	b->b_templ = asm_templname[TMPL(ip)];
	if (b->b_templ == 0)
		return (0);

	slot = 0;
	tp = b->b_templ;

	ok = 1;
	while (ok && *tp != 0) {
		switch (*tp++) {
		case 'B':
			ok = asm_decodeB(ip, b, slot++);
			break;
		case 'F':
			ok = asm_decodeF(ip, b, slot++);
			break;
		case 'I':
			ok = asm_decodeI(ip, b, slot++);
			break;
		case 'L':
			ok = (slot++ == 1) ? 1 : 0;
			break;
		case 'M':
			ok = asm_decodeM(ip, b, slot++);
			break;
		case 'X':
			ok = asm_decodeX(ip, b, slot++);
			break;
		case ';':
			ok = 1;
			break;
		default:
			ok = 0;
			break;
		}
	}
	return (ok);
}
