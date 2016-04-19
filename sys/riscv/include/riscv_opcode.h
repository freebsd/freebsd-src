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
 *
 * $FreeBSD$
 */

#ifndef _MACHINE_RISCV_OPCODE_H_
#define	_MACHINE_RISCV_OPCODE_H_

/*
 * Define the instruction formats and opcode values for the
 * RISC-V instruction set.
 */
#include <machine/endian.h>

/*
 * Define the instruction formats.
 */
typedef union {
	unsigned word;

	struct {
		unsigned opcode: 7;
		unsigned rd: 5;
		unsigned funct3: 3;
		unsigned rs1: 5;
		unsigned rs2: 5;
		unsigned funct7: 7;
	} RType;

	struct {
		unsigned opcode: 7;
		unsigned rd: 5;
		unsigned funct3: 3;
		unsigned rs1: 5;
		unsigned rs2: 6;
		unsigned funct7: 6;
	} R2Type;

	struct {
		unsigned opcode: 7;
		unsigned rd: 5;
		unsigned funct3: 3;
		unsigned rs1: 5;
		unsigned imm: 12;
	} IType;

	struct {
		unsigned opcode: 7;
		unsigned imm0_4: 5;
		unsigned funct3: 3;
		unsigned rs1: 5;
		unsigned rs2: 5;
		unsigned imm5_11: 7;
	} SType;

	struct {
		unsigned opcode: 7;
		unsigned imm11: 1;
		unsigned imm1_4: 4;
		unsigned funct3: 3;
		unsigned rs1: 5;
		unsigned rs2: 5;
		unsigned imm5_10: 6;
		unsigned imm12: 1;
	} SBType;

	struct {
		unsigned opcode: 7;
		unsigned rd: 5;
		unsigned imm12_31: 20;
	} UType;

	struct {
		unsigned opcode: 7;
		unsigned rd: 5;
		unsigned imm12_19: 8;
		unsigned imm11: 1;
		unsigned imm1_10: 10;
		unsigned imm20: 1;
	} UJType;
} InstFmt;

#define	RISCV_OPCODE(r)		(r & 0x7f)

#endif /* !_MACHINE_RISCV_OPCODE_H_ */
