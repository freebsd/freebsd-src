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

#ifndef	_MACHINE_TRAP_H_
#define	_MACHINE_TRAP_H_

#ifdef _KERNEL


#define T_DATA_MISS                     0x31
#define T_ALIGNMENT                     0x34
#define	T_DATA_PROTECTION		0x6c
#define T_MEM_ADDRESS_NOT_ALIGNED       T_ALIGNMENT

#define	T_RESERVED			0
#define	T_INSTRUCTION_EXCEPTION		1
#define	T_INSTRUCTION_ERROR		2
#define	T_INSTRUCTION_PROTECTION	3
#define	T_ILLTRAP_INSTRUCTION		4
#define	T_ILLEGAL_INSTRUCTION		5
#define	T_PRIVILEGED_OPCODE		6
#define	T_FP_DISABLED			7
#define	T_FP_EXCEPTION_IEEE_754		8

#define	T_INSTRUCTION_MISS		0x09
#define	T_TAG_OVERFLOW			0x0a
#define	T_DIVISION_BY_ZERO		0x0b
#define	T_DATA_EXCEPTION		0x0c
#define	T_DATA_ERROR			0x0d


#define	T_PRIVILEGED_ACTION		0x10
#define	T_ASYNC_DATA_ERROR		0x11
#define	T_TRAP_INSTRUCTION_16		0x12
#define	T_TRAP_INSTRUCTION_17		0x13
#define	T_TRAP_INSTRUCTION_18		0x14
#define	T_TRAP_INSTRUCTION_19		0x15
#define	T_TRAP_INSTRUCTION_20		0x16
#define	T_TRAP_INSTRUCTION_21		0x17
#define	T_TRAP_INSTRUCTION_22		0x18
#define	T_TRAP_INSTRUCTION_23		0x19
#define	T_TRAP_INSTRUCTION_24		0x1a
#define	T_TRAP_INSTRUCTION_25		0x1b
#define	T_TRAP_INSTRUCTION_26		0x1c
#define	T_TRAP_INSTRUCTION_27		0x1d
#define	T_TRAP_INSTRUCTION_28		0x1e
#define	T_TRAP_INSTRUCTION_29		0x1f
#define	T_TRAP_INSTRUCTION_30		0x20
#define	T_TRAP_INSTRUCTION_31		0x21
#define	T_FP_EXCEPTION_OTHER		0x22



#define	T_INTERRUPT			0x24
#define	T_PA_WATCHPOINT			0x25
#define	T_VA_WATCHPOINT			0x26
#define	T_CORRECTED_ECC_ERROR		0x27
#define	T_SPILL				0x28
#define	T_FILL				0x29
#define	T_FILL_RET			0x2a
#define	T_BREAKPOINT			0x2b
#define	T_CLEAN_WINDOW			0x2c
#define	T_RANGE_CHECK			0x2d
#define T_FIX_ALIGNMENT                 0x2e
#define	T_INTEGER_OVERFLOW		0x2f
#define	T_SYSCALL			0x30
#define	T_RSTRWP_PHYS			
#define	T_RSTRWP_VIRT			
#define	T_KSTACK_FAULT			51
#define T_RESUMABLE_ERROR               52
#define T_NONRESUMABLE_ERROR            53

#define	T_MAX				(T_NONRESUMABLE_ERROR + 1)

#define	T_KERNEL			0x100
#define TRAP_MASK                       ((1<<8)-1)
#define TRAP_CTX_SHIFT                  10

#define	PTL1_BAD_DEBUG		0
#define	PTL1_BAD_WTRAP		1
#define	PTL1_BAD_KMISS		2
#define	PTL1_BAD_KPROT_FAULT	3
#define	PTL1_BAD_ISM		4
#define	PTL1_BAD_MMUTRAP	5
#define	PTL1_BAD_TRAP		6
#define	PTL1_BAD_FPTRAP		7
#define	PTL1_BAD_INTR_REQ	8
#define	PTL1_BAD_TRACE_PTR	9
#define	PTL1_BAD_STACK		10
#define	PTL1_BAD_DTRACE_FLAGS	11
#define	PTL1_BAD_CTX_STEAL	12
#define	PTL1_BAD_ECC		13
#define	PTL1_BAD_HCALL		14
#define	PTL1_BAD_GL		15


#define TL_CPU_MONDO    0x1
#define TL_DEV_MONDO    0x2
#define TL_TSB_MISS     0x3
#define TL_TL0_TRAP     0x4
#define TL_SET_ACKMASK  0x5

/*
 * These defines are used by the TL1 tlb miss handlers to calculate
 * the pc to jump to in the case the entry was not found in the TSB.
 */
#define	WTRAP_ALIGN	0x7f	/* window handlers are 128 byte align */
#define	WTRAP_FAULTOFF	124	/* last instruction in handler */

/* use the following defines to determine if trap was a fill or a spill */
#define	WTRAP_TTMASK	0x180
#define	WTRAP_TYPE	0x080

#ifndef LOCORE
extern const char *trap_msg[];
void trap_init(void);
#endif

#endif

#endif /* !_MACHINE_TRAP_H_ */
