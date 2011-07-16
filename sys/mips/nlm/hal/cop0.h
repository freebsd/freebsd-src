/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __NLM_COP0_H__
#define __NLM_COP0_H__

#define NLM_C0_INDEX            0
#define NLM_C0_RANDOM           1
#define NLM_C0_ENTRYLO0         2
#define NLM_C0_ENTRYLO1         3
#define NLM_C0_CONTEXT          4
#define NLM_C0_USERLOCAL        4
#define NLM_C0_PAGEMASK         5
#define NLM_C0_WIRED            6
#define NLM_C0_BADVADDR         8
#define NLM_C0_COUNT            9
#define NLM_C0_EIRR             9
#define NLM_C0_EIMR             9
#define NLM_C0_ENTRYHI          10
#define NLM_C0_COMPARE          11
#define NLM_C0_STATUS           12
#define NLM_C0_INTCTL           12
#define NLM_C0_SRSCTL           12
#define NLM_C0_CAUSE            13
#define NLM_C0_EPC              14
#define NLM_C0_PRID             15
#define NLM_C0_EBASE            15
#define NLM_C0_CONFIG           16
#define NLM_C0_CONFIG0          16
#define NLM_C0_CONFIG1          16
#define NLM_C0_CONFIG2          16
#define NLM_C0_CONFIG3          16
#define NLM_C0_CONFIG4          16
#define NLM_C0_CONFIG5          16
#define NLM_C0_CONFIG6          16
#define NLM_C0_CONFIG7          16
#define NLM_C0_WATCHLO          18
#define NLM_C0_WATCHHI          19
#define NLM_C0_XCONTEXT         20
#define NLM_C0_SCRATCH          22
#define NLM_C0_SCRATCH0         22
#define NLM_C0_SCRATCH1         22
#define NLM_C0_SCRATCH2         22
#define NLM_C0_SCRATCH3         22
#define NLM_C0_SCRATCH4         22
#define NLM_C0_SCRATCH5         22
#define NLM_C0_SCRATCH6         22
#define NLM_C0_SCRATCH7         22
#define NLM_C0_DEBUG            23
#define NLM_C0_DEPC             24
#define NLM_C0_PERFCNT          25
#define NLM_C0_PERFCNT0         25
#define NLM_C0_PERFCNT1         25
#define NLM_C0_TAGLO            28
#define NLM_C0_DATALO           28
#define NLM_C0_TAGHI            29
#define NLM_C0_DATAHI           29
#define NLM_C0_ERROREPC         30
#define NLM_C0_DESAVE           31

/* cop0 status bits */
#define NLM_STATUS_CP0_EN	(1<<28)
#define NLM_STATUS_CP1_EN	(1<<29)
#define NLM_STATUS_CP2_EN	(1<<30)
#define NLM_STATUS_KX_EN	(1<<7)
#define NLM_STATUS_UX_EN	(1<<5)

#ifndef LOCORE

#define nlm_memory_barrier()			\
	__asm__ __volatile__(			\
		".set	push\n\t"		\
		".set	noreorder\n\t"		\
		" sync\n\t"			\
		".set	pop"			\
		::: "memory")

#define NLM_DEFINE_ACCESSORS32(name, reg, sel) 			\
static __inline__ uint32_t nlm_read_c0_##name(void)			\
{								\
	uint32_t __rv; 						\
        __asm__ __volatile__ (                                  \
        ".set	push\n"                                         \
        ".set	noreorder\n"                                    \
        ".set	mips64\n"                                       \
        "mfc0	%0, $%1, %2\n"                                  \
        ".set	pop\n"                                          \
        : "=r" (__rv)						\
	: "i" (reg), "i" (sel)					\
	); 		                			\
        return __rv;						\
}								\
								\
static __inline__ void nlm_write_c0_##name(uint32_t val)	\
{								\
        __asm__ __volatile__(                                   \
        ".set	push\n"                                         \
        ".set	noreorder\n"                                    \
        ".set	mips64\n"                                       \
        "mtc0	%0, $%1, %2\n"                                  \
        ".set	pop\n"                                          \
        :: "r" (val), "i" (reg), "i" (sel) 			\
	);							\
} struct __hack

/* struct __hack above swallows a semicolon - otherwise the macro
 * usage below cannot have the terminating semicolon */
#if (__mips == 64)
#define NLM_DEFINE_ACCESSORS64(name, reg, sel)			\
static __inline__ uint64_t nlm_read_c0_##name(void) 	\
{								\
	uint64_t __rv;                                \
        __asm__ __volatile__ (                                  \
        ".set	push\n"                                         \
        ".set	noreorder\n"                                    \
        ".set	mips64\n"                                       \
        "dmfc0	%0,$%1,%2\n"                                    \
        ".set	pop\n"                                          \
        : "=r" (__rv)						\
	: "i" (reg), "i" (sel) );                 		\
        return __rv;						\
}								\
								\
static __inline__ void nlm_write_c0_##name(uint64_t val)	\
{								\
        __asm__ __volatile__ (                                  \
        ".set	push\n"                                         \
        ".set	noreorder\n"                                    \
        ".set	mips64\n"                                       \
        "dmtc0	%0,$%1,%2\n"                                    \
        ".set	pop\n"                                          \
        :: "r" (val), "i" (reg), "i" (sel) );			\
} struct __hack

#else

#define NLM_DEFINE_ACCESSORS64(name, reg, sel) 			\
static __inline__ uint64_t nlm_read_c0_##name(void)	\
{								\
	uint32_t __high, __low;                             \
        __asm__ __volatile__ (                                  \
        ".set	push\n"                                         \
        ".set	noreorder\n"                                    \
        ".set	mips64\n"                                       \
        "dmfc0	$8, $%2, %3\n"                                  \
	"dsra32	%0, $8, 0\n"					\
	"sll	%1, $8, 0\n"					\
        ".set	pop\n"                                          \
        : "=r"(__high), "=r"(__low)				\
	: "i"(reg), "i"(sel)					\
	: "$8" );						\
								\
        return (((uint64_t)__high << 32) | __low);	\
}								\
								\
static __inline__ void nlm_write_c0_##name(uint64_t val)	\
{								\
       uint32_t __high = val >> 32;                           \
       uint32_t __low = val & 0xffffffff;                   \
        __asm__ __volatile__ (                                  \
        ".set	push\n"                                         \
        ".set	noreorder\n"                                    \
        ".set	mips64\n"                                       \
        "dsll32	$8, %1, 0\n"                                    \
        "dsll32	$9, %0, 0\n"                                    \
	"dsrl32	$8, $8, 0\n"					\
        "or	$8, $8, $9\n"                                   \
        "dmtc0	$8, $%2, %3\n"                                  \
        ".set	pop\n"                                          \
        :: "r"(__high), "r"(__low), "i"(reg), "i"(sel)		\
	: "$8", "$9");						\
} struct __hack

#endif

NLM_DEFINE_ACCESSORS32(index, 0, 0);
NLM_DEFINE_ACCESSORS32(random, 1, 0);
NLM_DEFINE_ACCESSORS64(entrylo0, 2, 0);
NLM_DEFINE_ACCESSORS64(entrylo1, 3, 0);
NLM_DEFINE_ACCESSORS64(context, 4, 0);
NLM_DEFINE_ACCESSORS64(userlocal, 4, 0);
NLM_DEFINE_ACCESSORS32(pagemask, 5, 0);
NLM_DEFINE_ACCESSORS32(wired, 6, 0);
NLM_DEFINE_ACCESSORS64(badvaddr, 8, 0);
NLM_DEFINE_ACCESSORS32(count, 9, 0);
NLM_DEFINE_ACCESSORS64(eirr, 9, 6);
NLM_DEFINE_ACCESSORS64(eimr, 9, 7);
NLM_DEFINE_ACCESSORS64(entryhi, 10, 0);
NLM_DEFINE_ACCESSORS32(compare, 11, 0);
NLM_DEFINE_ACCESSORS32(status, 12, 0);
NLM_DEFINE_ACCESSORS32(intctl, 12, 1);
NLM_DEFINE_ACCESSORS32(srsctl, 12, 2);
NLM_DEFINE_ACCESSORS32(cause, 13, 0);
NLM_DEFINE_ACCESSORS64(epc, 14, 0);
NLM_DEFINE_ACCESSORS32(prid, 15, 0);
NLM_DEFINE_ACCESSORS32(ebase, 15, 1);
NLM_DEFINE_ACCESSORS32(config0, 16, 0);
NLM_DEFINE_ACCESSORS32(config1, 16, 1);
NLM_DEFINE_ACCESSORS32(config2, 16, 2);
NLM_DEFINE_ACCESSORS32(config3, 16, 3);
NLM_DEFINE_ACCESSORS32(config6, 16, 6);
NLM_DEFINE_ACCESSORS32(config7, 16, 7);
NLM_DEFINE_ACCESSORS64(watchlo0, 18, 0);
NLM_DEFINE_ACCESSORS32(watchhi0, 19, 0);
NLM_DEFINE_ACCESSORS64(xcontext, 20, 0);
NLM_DEFINE_ACCESSORS64(scratch0, 22, 0);
NLM_DEFINE_ACCESSORS64(scratch1, 22, 1);
NLM_DEFINE_ACCESSORS64(scratch2, 22, 2);
NLM_DEFINE_ACCESSORS64(scratch3, 22, 3);
NLM_DEFINE_ACCESSORS64(scratch4, 22, 4);
NLM_DEFINE_ACCESSORS64(scratch5, 22, 5);
NLM_DEFINE_ACCESSORS64(scratch6, 22, 6);
NLM_DEFINE_ACCESSORS64(scratch7, 22, 7);
NLM_DEFINE_ACCESSORS32(debug, 23, 0);
NLM_DEFINE_ACCESSORS32(depc, 24, 0);
NLM_DEFINE_ACCESSORS32(perfctrl0, 25, 0);
NLM_DEFINE_ACCESSORS64(perfcntr0, 25, 1);
NLM_DEFINE_ACCESSORS32(perfctrl1, 25, 2);
NLM_DEFINE_ACCESSORS64(perfcntr1, 25, 3);
NLM_DEFINE_ACCESSORS32(perfctrl2, 25, 4);
NLM_DEFINE_ACCESSORS64(perfcntr2, 25, 5);
NLM_DEFINE_ACCESSORS32(perfctrl3, 25, 6);
NLM_DEFINE_ACCESSORS64(perfcntr3, 25, 7);
NLM_DEFINE_ACCESSORS64(taglo0, 28, 0);
NLM_DEFINE_ACCESSORS64(taglo2, 28, 2);
NLM_DEFINE_ACCESSORS64(taghi0, 29, 0);
NLM_DEFINE_ACCESSORS64(taghi2, 29, 2);
NLM_DEFINE_ACCESSORS64(errorepc, 30, 0);
NLM_DEFINE_ACCESSORS64(desave, 31, 0);

static __inline__ int nlm_nodeid(void)
{
	return (nlm_read_c0_ebase() >> 5) & 0x3;
}

static __inline__ int nlm_cpuid(void)
{
	return nlm_read_c0_ebase() & 0x1f;
}

static __inline__ int nlm_threadid(void)
{
	return nlm_read_c0_ebase() & 0x3;
}

static __inline__ int nlm_coreid(void)
{
	return (nlm_read_c0_ebase() >> 2) & 0x7;
}

#endif

#endif
