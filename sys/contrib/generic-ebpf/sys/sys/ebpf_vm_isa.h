/*-
 * SPDX-License-Identifier: Apache License 2.0
 *
 * Copyright 2019 Yutaro Hayakawa
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once

struct ebpf_inst {
	uint8_t opcode;
	uint8_t dst : 4;
	uint8_t src : 4;
	int16_t offset;
	int32_t imm;
};

enum ebpf_registers {
	EBPF_R0,
	EBPF_R1,
	EBPF_R2,
	EBPF_R3,
	EBPF_R4,
	EBPF_R5,
	EBPF_R6,
	EBPF_R7,
	EBPF_R8,
	EBPF_R9,
	EBPF_R10,
	EBPF_REG_MAX
};

#define EBPF_PSEUDO_MAP_DESC 1

#define EBPF_CLS_LD 0x00
#define EBPF_CLS_LDX 0x01
#define EBPF_CLS_ST 0x02
#define EBPF_CLS_STX 0x03
#define EBPF_CLS_ALU 0x04
#define EBPF_CLS_JMP 0x05
#define EBPF_CLS_ALU64 0x07
#define EBPF_CLS(op) ((op) & 0x07)

#define EBPF_SRC_IMM 0x00
#define EBPF_SRC_REG 0x08
#define EBPF_SRC(op) ((op) & 0x08)

#define EBPF_TO_LE 0x00
#define EBPF_TO_BE 0x08

#define EBPF_SIZE_W 0x00 /* word: 32bit */
#define EBPF_SIZE_H 0x08 /* half word: 16bit */
#define EBPF_SIZE_B 0x10 /* byte: 8bit */
#define EBPF_SIZE_DW 0x18 /* double word: 64bit */
#define EBPF_SIZE(op) ((op) & 0x18)

/* Other memory modes are not yet supported */
#define EBPF_MODE_IMM 0x00
#define EBPF_MODE_MEM 0x60
#define EBPF_MODE(op) ((op) & 0xe0)

/* ALU operations */
#define EBPF_ADD 0x00
#define EBPF_SUB 0x10
#define EBPF_MUL 0x20
#define EBPF_DIV 0x30
#define EBPF_OR 0x40
#define EBPF_AND 0x50
#define EBPF_LSH 0x60
#define EBPF_RSH 0x70
#define EBPF_NEG 0x80
#define EBPF_MOD 0x90
#define EBPF_XOR 0xa0
#define EBPF_MOV 0xb0
#define EBPF_ARSH 0xc0 /* sign extending shift right */
#define EBPF_END 0xd0 /* convert endianness */
#define EBPF_ALU_OP(op) ((op) & 0xf0)

/* jump operations */
#define EBPF_JA 0x00 /* unconditional */
#define EBPF_JEQ 0x10 /* == */
#define EBPF_JGT 0x20 /* unsigned > */
#define EBPF_JGE 0x30 /* unsigned >= */
#define EBPF_JSET 0x40 /* & */
#define EBPF_JNE 0x50	/* != */
#define EBPF_JLT 0xa0	/* unsigned, < */
#define EBPF_JLE 0xb0	/* unsigned, <= */
#define EBPF_JSGT 0x60	/* signed > */
#define EBPF_JSGE 0x70	/* signed >= */
#define EBPF_JSLT 0xc0	/* signed, < */
#define EBPF_JSLE 0xd0	/* signed, <= */
#define EBPF_CALL 0x80	/* function call */
#define EBPF_EXIT 0x90	/* function return */
#define EBPF_JMP_OP(op) ((op) & 0xf0)

#define EBPF_ALU_IMM(op, dst, imm) \
	{ (EBPF_CLS_ALU | EBPF_SRC_IMM | op), dst, 0, 0, imm }
#define EBPF_ALU_REG(op, dst, src) \
	{ (EBPF_CLS_ALU | EBPF_SRC_IMM | op), dst, src, 0, 0 }
#define EBPF_ALU64_IMM(op, dst, imm) \
	{ (EBPF_CLS_ALU64 | EBPF_SRC_IMM | op), dst, 0, 0, imm }
#define EBPF_ALU64_REG(op, dst, src) \
	{ (EBPF_CLS_ALU64 | EBPF_SRC_IMM | op), dst, src, 0, 0 }
#define EBPF_LE(dst, size) \
	{ (EBPF_CLS_ALU | EBPF_TO_LE | EBPF_END), dst, 0, 0, size }
#define EBPF_BE(dst, size) \
	{ (EBPF_CLS_ALU | EBPF_TO_BE | EBPF_END), dst, 0, 0, size }
#define EBPF_LD(size, dst, src, ofs) \
	{ (EBPF_CLS_LD | EBPF_SRC_MEM | size), dst, src, ofs, 0 }
#define EBPF_LDX(size, dst, src, ofs) \
	{ (EBPF_CLS_LDX | EBPF_SRC_MEM | size), dst, src, ofs, 0 }
#define EBPF_ST(size, dst, src, ofs) \
	{ (EBPF_CLS_ST | EBPF_SRC_MEM | size), dst, src, ofs, 0 }
#define EBPF_STX(size, dst, src, ofs) \
	{ (EBPF_CLS_ST | EBPF_SRC_MEM | size), dst, src, ofs, 0 }
#define EBPF_LDDW(dst, imm) \
	{ (EBPF_CLS_LD | EBPF_SRC_IMM | EBPF_DW), dst, 0, 0, (uint32_t)imm }, \
	{ 0, 0, 0, 0, ((uint64_t)imm) >> 32 }
#define EBPF_PSEUDO_MAP_LD(dst, imm) \
	{ (EBPF_CLS_LD | EBPF_SRC_IMM | EBPF_DW), dst, \
		EBPF_PSEUDO_MAP_DESC, 0, (uint32_t)imm } \
	{ 0, 0, 0, 0, 0 }
#define EBPF_JMP_JA(ofs) \
	{ (EBPF_CLS_JMP | EBPF_JA ), 0, 0, imm, 0 }
#define EBPF_JMP_IMM(op, dst, ofs, imm) \
	{ (EBPF_CLS_JMP | EBPF_SRC_IMM | op), dst, 0, ofs, imm }
#define EBPF_JMP_REG(op, dst, src, ofs) \
	{ (EBPF_CLS_JMP | EBPF_SRC_REG | op), dst, src, ofs, 0 }
#define EBPF_JMP_CALL(id) \
	{ (EBPF_CLS_JMP | EBPF_CALL), 0, 0, 0, id }
#define EBPF_JMP_EXIT \
	{ (EBPF_CLS_JMP | EBPF_EXIT), 0, 0, 0, 0 }

#define EBPF_OP_ADD_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_ADD)
#define EBPF_OP_ADD_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_ADD)
#define EBPF_OP_SUB_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_SUB)
#define EBPF_OP_SUB_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_SUB)
#define EBPF_OP_MUL_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_MUL)
#define EBPF_OP_MUL_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_MUL)
#define EBPF_OP_DIV_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_DIV)
#define EBPF_OP_DIV_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_DIV)
#define EBPF_OP_OR_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_OR)
#define EBPF_OP_OR_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_OR)
#define EBPF_OP_AND_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_AND)
#define EBPF_OP_AND_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_AND)
#define EBPF_OP_LSH_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_LSH)
#define EBPF_OP_LSH_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_LSH)
#define EBPF_OP_RSH_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_RSH)
#define EBPF_OP_RSH_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_RSH)
#define EBPF_OP_NEG (EBPF_CLS_ALU | EBPF_NEG)
#define EBPF_OP_MOD_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_MOD)
#define EBPF_OP_MOD_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_MOD)
#define EBPF_OP_XOR_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_XOR)
#define EBPF_OP_XOR_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_XOR)
#define EBPF_OP_MOV_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_MOV)
#define EBPF_OP_MOV_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_MOV)
#define EBPF_OP_ARSH_IMM (EBPF_CLS_ALU | EBPF_SRC_IMM | EBPF_ARSH)
#define EBPF_OP_ARSH_REG (EBPF_CLS_ALU | EBPF_SRC_REG | EBPF_ARSH)
#define EBPF_OP_LE (EBPF_CLS_ALU | EBPF_TO_LE | EBPF_END)
#define EBPF_OP_BE (EBPF_CLS_ALU | EBPF_TO_BE | EBPF_END)

#define EBPF_OP_ADD64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_ADD)
#define EBPF_OP_ADD64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_ADD)
#define EBPF_OP_SUB64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_SUB)
#define EBPF_OP_SUB64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_SUB)
#define EBPF_OP_MUL64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_MUL)
#define EBPF_OP_MUL64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_MUL)
#define EBPF_OP_DIV64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_DIV)
#define EBPF_OP_DIV64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_DIV)
#define EBPF_OP_OR64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_OR)
#define EBPF_OP_OR64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_OR)
#define EBPF_OP_AND64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_AND)
#define EBPF_OP_AND64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_AND)
#define EBPF_OP_LSH64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_LSH)
#define EBPF_OP_LSH64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_LSH)
#define EBPF_OP_RSH64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_RSH)
#define EBPF_OP_RSH64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_RSH)
#define EBPF_OP_NEG64 (EBPF_CLS_ALU64 | EBPF_NEG)
#define EBPF_OP_MOD64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_MOD)
#define EBPF_OP_MOD64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_MOD)
#define EBPF_OP_XOR64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_XOR)
#define EBPF_OP_XOR64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_XOR)
#define EBPF_OP_MOV64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_MOV)
#define EBPF_OP_MOV64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_MOV)
#define EBPF_OP_ARSH64_IMM (EBPF_CLS_ALU64 | EBPF_SRC_IMM | EBPF_ARSH)
#define EBPF_OP_ARSH64_REG (EBPF_CLS_ALU64 | EBPF_SRC_REG | EBPF_ARSH)

#define EBPF_OP_LDXW (EBPF_CLS_LDX | EBPF_MODE_MEM | EBPF_SIZE_W)
#define EBPF_OP_LDXH (EBPF_CLS_LDX | EBPF_MODE_MEM | EBPF_SIZE_H)
#define EBPF_OP_LDXB (EBPF_CLS_LDX | EBPF_MODE_MEM | EBPF_SIZE_B)
#define EBPF_OP_LDXDW (EBPF_CLS_LDX | EBPF_MODE_MEM | EBPF_SIZE_DW)
#define EBPF_OP_STW (EBPF_CLS_ST | EBPF_MODE_MEM | EBPF_SIZE_W)
#define EBPF_OP_STH (EBPF_CLS_ST | EBPF_MODE_MEM | EBPF_SIZE_H)
#define EBPF_OP_STB (EBPF_CLS_ST | EBPF_MODE_MEM | EBPF_SIZE_B)
#define EBPF_OP_STDW (EBPF_CLS_ST | EBPF_MODE_MEM | EBPF_SIZE_DW)
#define EBPF_OP_STXW (EBPF_CLS_STX | EBPF_MODE_MEM | EBPF_SIZE_W)
#define EBPF_OP_STXH (EBPF_CLS_STX | EBPF_MODE_MEM | EBPF_SIZE_H)
#define EBPF_OP_STXB (EBPF_CLS_STX | EBPF_MODE_MEM | EBPF_SIZE_B)
#define EBPF_OP_STXDW (EBPF_CLS_STX | EBPF_MODE_MEM | EBPF_SIZE_DW)
#define EBPF_OP_LDDW (EBPF_CLS_LD | EBPF_MODE_IMM | EBPF_SIZE_DW)

#define EBPF_OP_JA (EBPF_CLS_JMP | EBPF_JA)
#define EBPF_OP_JEQ_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JEQ)
#define EBPF_OP_JEQ_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JEQ)
#define EBPF_OP_JGT_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JGT)
#define EBPF_OP_JGT_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JGT)
#define EBPF_OP_JGE_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JGE)
#define EBPF_OP_JGE_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JGE)
#define EBPF_OP_JSET_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JSET)
#define EBPF_OP_JSET_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JSET)
#define EBPF_OP_JNE_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JNE)
#define EBPF_OP_JNE_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JNE)
#define EBPF_OP_JSGT_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JSGT)
#define EBPF_OP_JSGT_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JSGT)
#define EBPF_OP_JSGE_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JSGE)
#define EBPF_OP_JSGE_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JSGE)
#define EBPF_OP_CALL (EBPF_CLS_JMP | EBPF_CALL)
#define EBPF_OP_EXIT (EBPF_CLS_JMP | EBPF_EXIT)

#define EBPF_OP_JLT_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JLT)
#define EBPF_OP_JLT_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JLT)
#define EBPF_OP_JLE_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JLE)
#define EBPF_OP_JLE_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JLE)
#define EBPF_OP_JSLT_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JSLT)
#define EBPF_OP_JSLT_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JSLT)
#define EBPF_OP_JSLE_IMM (EBPF_CLS_JMP | EBPF_SRC_IMM | EBPF_JSLE)
#define EBPF_OP_JSLE_REG (EBPF_CLS_JMP | EBPF_SRC_REG | EBPF_JSLE)
