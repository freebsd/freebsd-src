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

#define	T_RESERVED			0
#define	T_INSTRUCTION_EXCEPTION		1
#define	T_INSTRUCTION_ERROR		2
#define	T_INSTRUCTION_PROTECTION	3
#define	T_ILLTRAP_INSTRUCTION		4
#define	T_ILLEGAL_INSTRUCTION		5
#define	T_PRIVILEGED_OPCODE		6
#define	T_FP_DISABLED			7
#define	T_FP_EXCEPTION_IEEE_754		8
#define	T_FP_EXCEPTION_OTHER		9
#define	T_TAG_OVERFLOW			10
#define	T_DIVISION_BY_ZERO		11
#define	T_DATA_EXCEPTION		12
#define	T_DATA_ERROR			13
#define	T_DATA_PROTECTION		14
#define	T_MEM_ADDRESS_NOT_ALIGNED	15
#define	T_ALIGNMENT	                15
#define	T_PRIVILEGED_ACTION		16
#define	T_ASYNC_DATA_ERROR		17
#define	T_TRAP_INSTRUCTION_16		18
#define	T_TRAP_INSTRUCTION_17		19
#define	T_TRAP_INSTRUCTION_18		20
#define	T_TRAP_INSTRUCTION_19		21
#define	T_TRAP_INSTRUCTION_20		22
#define	T_TRAP_INSTRUCTION_21		23
#define	T_TRAP_INSTRUCTION_22		24
#define	T_TRAP_INSTRUCTION_23		25
#define	T_TRAP_INSTRUCTION_24		26
#define	T_TRAP_INSTRUCTION_25		27
#define	T_TRAP_INSTRUCTION_26		28
#define	T_TRAP_INSTRUCTION_27		29
#define	T_TRAP_INSTRUCTION_28		30
#define	T_TRAP_INSTRUCTION_29		31
#define	T_TRAP_INSTRUCTION_30		32
#define	T_TRAP_INSTRUCTION_31		33
#define	T_INSTRUCTION_MISS		34
#define	T_DATA_MISS			35

#define	T_INTERRUPT			36
#define	T_PA_WATCHPOINT			37
#define	T_VA_WATCHPOINT			38
#define	T_CORRECTED_ECC_ERROR		39
#define	T_SPILL				40
#define	T_FILL				41
#define	T_FILL_RET			42
#define	T_BREAKPOINT			43
#define	T_CLEAN_WINDOW			44
#define	T_RANGE_CHECK			45
#define	T_FIX_ALIGNMENT			46
#define	T_INTEGER_OVERFLOW		47
#define	T_SYSCALL			48
#define	T_RSTRWP_PHYS			49
#define	T_RSTRWP_VIRT			50
#define	T_KSTACK_FAULT			51
#define T_RESUMABLE_ERROR               52
#define T_NONRESUMABLE_ERROR            53

#define	T_MAX				(T_NONRESUMABLE_ERROR + 1)

#define	T_KERNEL			64

#define TRAP_MASK                       ((1<<6)-1)
#define TRAP_CTX_SHIFT                  8
/*
 * These defines are used by the TL1 tlb miss handlers to calculate
 * the pc to jump to in the case the entry was not found in the TSB.
 */

#define WTRAP_ALIGN     0x7f    /* window handlers are 128 byte align */
#define WTRAP_FAULTOFF  124     /* last instruction in handler */
 
/* use the following defines to determine if trap was a fill or a spill */
#define WTRAP_TTMASK    0x180
#define WTRAP_TYPE      0x080

#define TT_INSTRUCTION_EXCEPTION        0x8
#define TT_INSTRUCTION_MISS             0x9
#define TT_ILLEGAL_INSTRUCTION          0x10
#define TT_PRIVILEGED_OPCODE            0x11
#define TT_FP_EXCEPTION_IEEE_754        0x21
#define TT_FP_EXCEPTION_OTHER           0x22
#define TT_TAG_OVERFLOW                 0x23
#define TT_DIVISION_BY_ZERO             0x28
#define TT_DATA_EXCEPTION               0x30
#define TT_DATA_MISS                    0x31
#define TT_ALIGNNMENT                   0x34
#define TT_DATA_PROTECTION              0x6c
#define TT_ALIGNMENT                    0x6c
#define TT_BREAKPOINT                   0x76

#define PTL1_BAD_DEBUG          0
#define PTL1_BAD_WTRAP          1
#define PTL1_BAD_KMISS          2
#define PTL1_BAD_KPROT_FAULT    3
#define PTL1_BAD_ISM            4
#define PTL1_BAD_MMUTRAP        5
#define PTL1_BAD_TRAP           6
#define PTL1_BAD_NOT_WTRAP      7
#if 0
#define PTL1_BAD_FPTRAP         7
#endif
#define PTL1_BAD_INTR_REQ       8
#define PTL1_BAD_TRACE_PTR      9

#define PTL1_BAD_STACK          10
#define PTL1_BAD_DTRACE_FLAGS   11
#define PTL1_BAD_CTX_STEAL      12
#define PTL1_BAD_ECC            13
#define PTL1_BAD_HCALL          14
#define PTL1_BAD_GL             15


#ifndef LOCORE
extern const char *trap_msg[];
extern void set_mmfsa_traptable(void *, uint64_t);
extern void trap_init(void);
#endif

#endif

#endif /* !_MACHINE_TRAP_H_ */
