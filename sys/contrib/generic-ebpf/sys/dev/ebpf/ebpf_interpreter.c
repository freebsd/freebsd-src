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

#include "ebpf_platform.h"
#include <sys/ebpf_vm_isa.h>
#include "ebpf_prog.h"

uint64_t
ebpf_prog_run(void *ctx, struct ebpf_prog *ep)
{
	uint32_t pc = 0;
	uint64_t reg[EBPF_REG_MAX];
	uint8_t stack[EBPF_STACK_SIZE];
	struct ebpf_inst *inst;
	const struct ebpf_helper_type *const *helpers =
		ep->eo.eo_ee->ec->helper_types;

	pc = 0;
	inst = ep->prog;
	reg[1] = (uint64_t)ctx;
	reg[10] = (uint64_t)(stack + EBPF_STACK_SIZE);

	while (true) {
		inst = inst + pc++;
		switch (inst->opcode) {
		case EBPF_OP_ADD_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] + (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_ADD_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] + (uint32_t)inst->imm;
			break;
		case EBPF_OP_SUB_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] - (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_SUB_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] - (uint32_t)inst->imm;
			break;
		case EBPF_OP_MUL_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] * (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_MUL_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] * (uint32_t)inst->imm;
			break;
		case EBPF_OP_DIV_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] / (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_DIV_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] / (uint32_t)inst->imm;
			break;
		case EBPF_OP_OR_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] | (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_OR_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] | (uint32_t)inst->imm;
			break;
		case EBPF_OP_AND_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] & (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_AND_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] & (uint32_t)inst->imm;
			break;
		case EBPF_OP_LSH_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] << (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_LSH_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] << (uint32_t)inst->imm;
			break;
		case EBPF_OP_RSH_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] >> (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_RSH_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] >> (uint32_t)inst->imm;
			break;
		case EBPF_OP_NEG:
			reg[inst->dst] = -(uint32_t)inst->imm;
			break;
		case EBPF_OP_MOD_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] % (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_MOD_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] % (uint32_t)inst->imm;
			break;
		case EBPF_OP_XOR_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] ^ (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_XOR_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] ^ (uint32_t)inst->imm;
			break;
		case EBPF_OP_MOV_REG:
			reg[inst->dst] = (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_MOV_IMM:
			reg[inst->dst] = (uint32_t)inst->imm;
			break;
		case EBPF_OP_ARSH_REG:
			reg[inst->dst] = (uint32_t)reg[inst->dst] >> (uint32_t)reg[inst->src];
			break;
		case EBPF_OP_ARSH_IMM:
			reg[inst->dst] = (uint32_t)reg[inst->dst] >> (uint32_t)inst->imm;
			break;
		case EBPF_OP_LE:
			if (inst->imm == 16) {
				reg[inst->dst] = htole16((uint16_t)reg[inst->dst]);
			} else if (inst->imm == 32) {
				reg[inst->dst] = htole32((uint32_t)reg[inst->dst]);
			} else if (inst->imm == 64) {
				reg[inst->dst] = htole64((uint64_t)reg[inst->dst]);
			}
			break;
		case EBPF_OP_BE:
			if (inst->imm == 16) {
				reg[inst->dst] = htobe16((uint16_t)reg[inst->dst]);
			} else if (inst->imm == 32) {
				reg[inst->dst] = htobe32((uint32_t)reg[inst->dst]);
			} else if (inst->imm == 64) {
				reg[inst->dst] = htobe64((uint64_t)reg[inst->dst]);
			}
			break;
		case EBPF_OP_ADD64_REG:
			reg[inst->dst] = reg[inst->dst] + reg[inst->src];
			break;
		case EBPF_OP_ADD64_IMM:
			reg[inst->dst] = reg[inst->dst] + inst->imm;
			break;
		case EBPF_OP_SUB64_REG:
			reg[inst->dst] = reg[inst->dst] - reg[inst->src];
			break;
		case EBPF_OP_SUB64_IMM:
			reg[inst->dst] = reg[inst->dst] - inst->imm;
			break;
		case EBPF_OP_MUL64_REG:
			reg[inst->dst] = reg[inst->dst] * reg[inst->src];
			break;
		case EBPF_OP_MUL64_IMM:
			reg[inst->dst] = reg[inst->dst] * inst->imm;
			break;
		case EBPF_OP_DIV64_REG:
			reg[inst->dst] = reg[inst->dst] / reg[inst->src];
			break;
		case EBPF_OP_DIV64_IMM:
			reg[inst->dst] = reg[inst->dst] / inst->imm;
			break;
		case EBPF_OP_OR64_REG:
			reg[inst->dst] = reg[inst->dst] | reg[inst->src];
			break;
		case EBPF_OP_OR64_IMM:
			reg[inst->dst] = reg[inst->dst] | inst->imm;
			break;
		case EBPF_OP_AND64_REG:
			reg[inst->dst] = reg[inst->dst] & reg[inst->src];
			break;
		case EBPF_OP_AND64_IMM:
			reg[inst->dst] = reg[inst->dst] & inst->imm;
			break;
		case EBPF_OP_LSH64_REG:
			reg[inst->dst] = reg[inst->dst] << reg[inst->src];
			break;
		case EBPF_OP_LSH64_IMM:
			reg[inst->dst] = reg[inst->dst] << inst->imm;
			break;
		case EBPF_OP_RSH64_REG:
			reg[inst->dst] = reg[inst->dst] >> reg[inst->src];
			break;
		case EBPF_OP_RSH64_IMM:
			reg[inst->dst] = reg[inst->dst] >> inst->imm;
			break;
		case EBPF_OP_NEG64:
			reg[inst->dst] = reg[inst->dst] - inst->imm;
			break;
		case EBPF_OP_MOD64_REG:
			reg[inst->dst] = reg[inst->dst] % reg[inst->src];
			break;
		case EBPF_OP_MOD64_IMM:
			reg[inst->dst] = reg[inst->dst] % inst->imm;
			break;
		case EBPF_OP_XOR64_REG:
			reg[inst->dst] = reg[inst->dst] ^ reg[inst->src];
			break;
		case EBPF_OP_XOR64_IMM:
			reg[inst->dst] = reg[inst->dst] ^ inst->imm;
			break;
		case EBPF_OP_MOV64_REG:
			reg[inst->dst] = reg[inst->dst] + reg[inst->src];
			break;
		case EBPF_OP_MOV64_IMM:
			reg[inst->dst] = reg[inst->dst] + inst->imm;
			break;
		case EBPF_OP_ARSH64_REG:
			reg[inst->dst] = reg[inst->dst] >> reg[inst->src];
			break;
		case EBPF_OP_ARSH64_IMM:
			reg[inst->dst] = reg[inst->dst] >> inst->imm;
			break;
		case EBPF_OP_JA:
			pc += inst->offset;
			break;
		case EBPF_OP_JEQ_REG:
			if ((uint64_t)reg[inst->dst] == (uint64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JEQ_IMM:
			if ((uint64_t)reg[inst->dst] == (uint64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JGT_REG:
			if ((uint64_t)reg[inst->dst] > (uint64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JGT_IMM:
			if ((uint64_t)reg[inst->dst] > (uint64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JGE_REG:
			if ((uint64_t)reg[inst->dst] >= (uint64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JGE_IMM:
			if ((uint64_t)reg[inst->dst] >= (uint64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSET_REG:
			if ((uint64_t)reg[inst->dst] & (uint64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSET_IMM:
			if ((uint64_t)reg[inst->dst] & (uint64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JNE_REG:
			if ((uint64_t)reg[inst->dst] != (uint64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JNE_IMM:
			if ((uint64_t)reg[inst->dst] != (uint64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSGT_REG:
			if ((int64_t)reg[inst->dst] > (int64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSGT_IMM:
			if ((int64_t)reg[inst->dst] > (int64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSGE_REG:
			if ((int64_t)reg[inst->dst] >= (int64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSGE_IMM:
			if ((int64_t)reg[inst->dst] >= (int64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_CALL:
			reg[0] = helpers[inst->imm]->fn(reg[1], reg[2], reg[3], reg[4], reg[5]);
			break;
		case EBPF_OP_EXIT:
			return reg[0];
		case EBPF_OP_JLT_REG:
			if ((uint64_t)reg[inst->dst] < (uint64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JLT_IMM:
			if ((uint64_t)reg[inst->dst] < (uint64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JLE_REG:
			if ((uint64_t)reg[inst->dst] <= (uint64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JLE_IMM:
			if ((uint64_t)reg[inst->dst] <= (uint64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSLT_REG:
			if ((int64_t)reg[inst->dst] < (int64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSLT_IMM:
			if ((int64_t)reg[inst->dst] < (int64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSLE_REG:
			if ((int64_t)reg[inst->dst] <= (int64_t)reg[inst->src]) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_JSLE_IMM:
			if ((int64_t)reg[inst->dst] <= (int64_t)inst->imm) {
				pc += inst->offset;
			}
			break;
		case EBPF_OP_LDXB:
			reg[inst->dst] = *(uint8_t *)(uintptr_t)(reg[inst->src] + inst->offset);
			break;
		case EBPF_OP_LDXDW:
			reg[inst->dst] = *(uint64_t *)(uintptr_t)(reg[inst->src] + inst->offset);
			break;
		case EBPF_OP_LDXH:
			reg[inst->dst] = *(uint16_t *)(uintptr_t)(reg[inst->src] + inst->offset);
			break;
		case EBPF_OP_LDXW:
			reg[inst->dst] = *(uint32_t *)(uintptr_t)(reg[inst->src] + inst->offset);
			break;
		case EBPF_OP_LDDW:
			reg[inst->dst] = (uint32_t)inst->imm | ((uint64_t)((inst + 1)->imm) << 32);
			pc++;
			break;
		case EBPF_OP_STXB:
			*(uint8_t *)(uintptr_t)(reg[inst->dst] + inst->offset) = reg[inst->src];
			break;
		case EBPF_OP_STXDW:
			*(uint64_t *)(uintptr_t)(reg[inst->dst] + inst->offset) = reg[inst->src];
			break;
		case EBPF_OP_STXH:
			*(uint16_t *)(uintptr_t)(reg[inst->dst] + inst->offset) = reg[inst->src];
			break;
		case EBPF_OP_STXW:
			*(uint32_t *)(uintptr_t)(reg[inst->dst] + inst->offset) = reg[inst->src];
			break;
		case EBPF_OP_STB:
			*(uint8_t *)(uintptr_t)(reg[inst->dst] + inst->offset) = (uint8_t)inst->imm;
			break;
		case EBPF_OP_STDW:
			*(uint64_t *)(uintptr_t)(reg[inst->dst] + inst->offset) = (uint64_t)inst->imm;
			break;
		case EBPF_OP_STH:
			*(uint16_t *)(uintptr_t)(reg[inst->dst] + inst->offset) = (uint16_t)inst->imm;
			break;
		case EBPF_OP_STW:
			*(uint32_t *)(uintptr_t)(reg[inst->dst] + inst->offset) = (uint32_t)inst->imm;
			break;
		default:
			ebpf_error("Invalid instruction at PC %u\n", pc);
			ebpf_assert(false);
		}
	}
}
