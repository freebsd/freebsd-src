/*-
 * Copyright (c) 2001 Jake Burkholder.
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

#ifndef	_MACHINE_UTRAP_H_
#define	_MACHINE_UTRAP_H_

#define	UT_INSTRUCTION_EXCEPTION	1
#define	UT_INSTRUCTION_ERROR		2
#define	UT_INSTRUCTION_PROTECTION	3
#define	UT_ILLTRAP_INSTRUCTION		4
#define	UT_ILLEGAL_INSTRUCTION		5
#define	UT_PRIVILEGED_OPCODE		6
#define	UT_FP_DISABLED			7
#define	UT_FP_EXCEPTION_IEEE_754	8
#define	UT_FP_EXCEPTION_OTHER		9
#define	UT_TAG_OFERFLOW			10
#define	UT_DIVISION_BY_ZERO		11
#define	UT_DATA_EXCEPTION		12
#define	UT_DATA_ERROR			13
#define	UT_DATA_PROTECTION		14
#define	UT_MEM_ADDRESS_NOT_ALIGNED	15
#define	UT_PRIVILEGED_ACTION		16
#define	UT_ASYNC_DATA_ERROR		17
#define	UT_TRAP_INSTRUCTION_16		18
#define	UT_TRAP_INSTRUCTION_17		19
#define	UT_TRAP_INSTRUCTION_18		20
#define	UT_TRAP_INSTRUCTION_19		21
#define	UT_TRAP_INSTRUCTION_20		22
#define	UT_TRAP_INSTRUCTION_21		23
#define	UT_TRAP_INSTRUCTION_22		24
#define	UT_TRAP_INSTRUCTION_23		25
#define	UT_TRAP_INSTRUCTION_24		26
#define	UT_TRAP_INSTRUCTION_25		27
#define	UT_TRAP_INSTRUCTION_26		28
#define	UT_TRAP_INSTRUCTION_27		29
#define	UT_TRAP_INSTRUCTION_28		30
#define	UT_TRAP_INSTRUCTION_29		31
#define	UT_TRAP_INSTRUCTION_30		32
#define	UT_TRAP_INSTRUCTION_31		33
#define	UT_MAX				34

#define	ST_BREAKPOINT			1
#define	ST_DIVISION_BY_ZERO		2
#define	ST_CLEAN_WINDOW			4
#define	ST_RANGE_CHECK			5
#define	ST_FIX_ALIGNMENT		6
#define	ST_INTEGER_OVERFLOW		7
#define	ST_SYSCALL			9

#define	UTH_NOCHANGE			(-1)

#if 0

typedef	int utrap_entry_t;
typedef void *utrap_handler_t;

#endif

#endif
