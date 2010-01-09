/*-
 * Copyright (c) 2003-2009 RMI Corporation
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
 * 3. Neither the name of RMI Corporation, nor the names of its contributors,
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 * RMI_BSD */

#ifdef XLR_PERFMON

#ifndef XLRCONFIG_PERFMON_H
#define XLRCONFIG_PERFMON_H

#include <mips/rmi/perfmon_utils.h>	/* for DPRINT */

#define NCPUS    32
#define NCORES   8
#define NTHREADS 4
#define PERF_SAMPLE_BUFSZ 32
/*select timeout is 512*1024 microsecs */
#define DEFAULT_SYS_SAMPLING_INTERVAL (512*1024)
/* default timer value programmed to PIC is 10*1024*1024 */
#define DEFAULT_CPU_SAMPLING_INTERVAL (10*1024)
#define DEFAULT_CC_SAMPLE_RATE 16
#define DEFAULT_CP0_FLAGS 0x0A
#define NUM_L2_BANKS 8
#define NUM_DRAM_BANKS 4

/* CP0 register for timestamp */
#define CP0_COUNT              9
#define CP0_EIRR_REG           9
#define CP0_EIRR_SEL           6
#define CP0_EIMR_REG           9
#define CP0_EIMR_SEL           7

/* CP0 register for perf counters */
#define CP0_PERF_COUNTER       25
/* selector values */
#define PERFCNTRCTL0       0
#define PERFCNTR0          1
#define PERFCNTRCTL1       2
#define PERFCNTR1          3

#define XLR_IO_PIC_OFFSET   0x08000
#define PIC_SYS_TIMER_0_BASE 0x120
#define PIC_SYS_TIMER_NUM_6 6

/* CP2 registers for reading credit counters */
#define CC_REG0  16

#define read_c0_register(reg, sel)                              \
({ unsigned int __rv;                                           \
        __asm__ __volatile__(                                   \
	".set\tpush\n\t"					\
        ".set mips32\n\t"                                       \
        "mfc0\t%0,$%1,%2\n\t"                                   \
	".set\tpop"						\
        : "=r" (__rv) : "i" (reg), "i" (sel) );                 \
        __rv;})

#define write_c0_register(reg,  sel, value)                     \
        __asm__ __volatile__(                                   \
	".set\tpush\n\t"					\
        ".set mips32\n\t"                                       \
        "mtc0\t%0,$%1,%2\n\t"                                   \
	".set\tpop"						\
        : : "r" (value), "i" (reg), "i" (sel) );

#define read_c2_register(reg, sel)                              \
({ unsigned int __rv;                                           \
        __asm__ __volatile__(                                   \
	".set\tpush\n\t"					\
        ".set mips32\n\t"                                       \
        "mfc0\t%0,$%1,%2\n\t"                                   \
	".set\tpop"					 	\
        : "=r" (__rv) : "i"(reg), "i" (sel) );                  \
        __rv;})

/*
 * We have 128 registers in C2 credit counters, reading them one at
 * a time using bitmap will take a lot of code, so we have two functions
 * to read registers sel0-3 and sel 4-7 into one 32 bit word.
 */

#define read_cc_registers_0123(reg)                            \
({     	      						       \
	unsigned int __rv;				       \
							       \
        __asm__ __volatile__(				       \
		".set	push\n\t"			       \
		".set	mips32\n\t"			       \
		".set	noreorder\n\t"			       \
		"mfc2	%0, $%1, 0\n\t"	                       \
		"mfc2	$8, $%1, 1\n\t"	                       \
		"sll    %0, %0, 8\n\t"			       \
		"or     %0, %0, $8\n\t"			       \
		"mfc2	$8, $%1, 2\n\t"	                       \
		"sll    %0, %0, 8\n\t"			       \
		"or     %0, %0, $8\n\t"			       \
		"mfc2	$8, $%1, 3\n\t"	                       \
		"sll    %0, %0, 8\n\t"			       \
		"or     %0, %0, $8\n\t"			       \
		".set	pop"				       \
		: "=r" (__rv) : "i"(reg) : "$8");	       \
							       \
       __rv;						       \
})

#define read_cc_registers_4567(reg)                            \
({     	      						       \
	unsigned int __rv;				       \
							       \
        __asm__ __volatile__(				       \
		".set	push\n\t"			       \
		".set	mips32\n\t"			       \
		".set	noreorder\n\t"			       \
		"mfc2	%0, $%1, 4\n\t"	                       \
		"mfc2	$8, $%1, 5\n\t"	                       \
		"sll    %0, %0, 8\n\t"			       \
		"or     %0, %0, $8\n\t"			       \
		"mfc2	$8, $%1, 6\n\t"	                       \
		"sll    %0, %0, 8\n\t"			       \
		"or     %0, %0, $8\n\t"			       \
		"mfc2	$8, $%1, 7\n\t"	                       \
		"sll    %0, %0, 8\n\t"			       \
		"or     %0, %0, $8\n\t"			       \
		".set	pop"				       \
		: "=r" (__rv) :"i"(reg) : "$8");	       \
							       \
       __rv;						       \
})

#endif
#endif
