/*-
 * Copyright (c) 2012 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _VMM_INSTRUCTION_EMUL_H_
#define _VMM_INSTRUCTION_EMUL_H_

enum vie_op_size {
	VIE_OP_SIZE_32BIT,		/* default */
	VIE_OP_SIZE_64BIT,
	VIE_OP_SIZE_8BIT
};

#define	VIE_INST_SIZE	15
struct vie {
	uint8_t		inst[VIE_INST_SIZE];

	uint8_t		rex_w:1,
			rex_r:1,
			rex_x:1,
			rex_b:1;

	uint8_t		mod:2,
			reg:4,
			rm:4;


	uint8_t		opcode_byte;
	uint16_t	opcode_flags;
	uint8_t		disp_bytes;
	uint8_t		imm_bytes;

	int		num_valid;
	int		num_processed;

	enum vm_reg_name base_register;
	enum vm_reg_name index_register;
	enum vm_reg_name operand_register;

	int		op_size;
	int64_t		displacement;
	int64_t		immediate;
};

#define	VIE_F_HAS_MODRM	(1 << 0)
#define	VIE_F_FROM_RM	(1 << 1)
#define	VIE_F_FROM_REG	(1 << 2)
#define	VIE_F_TO_RM	(1 << 3)
#define	VIE_F_TO_REG	(1 << 4)
#define	VIE_F_FROM_IMM	(1 << 5)

#define	VIE_MOD_INDIRECT		0
#define	VIE_MOD_INDIRECT_DISP8		1
#define	VIE_MOD_INDIRECT_DISP32		2
#define	VIE_MOD_DIRECT			3

#define	VIE_RM_SIB			4
#define	VIE_RM_DISP32			5

struct vm;

void	vmm_fetch_instruction(struct vm *vm, uint64_t rip, uint64_t cr3,
			      struct vie *vie);

int	vmm_decode_instruction(struct vie *vie);

#endif
