/*	$NetBSD: cpu.h,v 1.70 2003/01/17 23:36:08 thorpej Exp $	*/

/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell and Rick Macklem.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 *	@(#)cpu.h	8.4 (Berkeley) 1/4/94
 */

#ifndef _CPUINFO_H_
#define _CPUINFO_H_

/*
 * Exported definitions unique to NetBSD/mips cpu support.
 */

#ifdef _KERNEL
#ifndef LOCORE

struct mips_cpuinfo {
	u_int8_t	cpu_vendor;
	u_int8_t	cpu_rev;
	u_int8_t	cpu_impl;
	u_int8_t	tlb_type;
	u_int16_t	tlb_nentries;
	u_int8_t	icache_virtual;
	struct {
		u_int32_t	ic_size;
		u_int8_t	ic_linesize;
		u_int8_t	ic_nways;
		u_int16_t	ic_nsets;
		u_int32_t	dc_size;
		u_int8_t	dc_linesize;
		u_int8_t	dc_nways;
		u_int16_t	dc_nsets;
	} l1;
};

/* TODO: Merge above structure with NetBSD's below. */

struct cpu_info {
#ifdef notyet
	struct schedstate_percpu ci_schedstate; /* scheduler state */
#endif
	u_long ci_cpu_freq;		/* CPU frequency */
	u_long ci_cycles_per_hz;	/* CPU freq / hz */
	u_long ci_divisor_delay;	/* for delay/DELAY */
	u_long ci_divisor_recip;	/* scaled reciprocal of previous;
					   see below */
#if defined(DIAGNOSTIC) || defined(LOCKDEBUG)
	u_long ci_spin_locks;		/* # of spin locks held */
	u_long ci_simple_locks;		/* # of simple locks held */
#endif
};

/*
 * To implement a more accurate microtime using the CP0 COUNT register
 * we need to divide that register by the number of cycles per MHz.
 * But...
 *
 * DIV and DIVU are expensive on MIPS (eg 75 clocks on the R4000).  MULT
 * and MULTU are only 12 clocks on the same CPU.
 *
 * The strategy we use is to calculate the reciprical of cycles per MHz,
 * scaled by 1<<32.  Then we can simply issue a MULTU and pluck of the
 * HI register and have the results of the division.
 */
#define	MIPS_SET_CI_RECIPRICAL(cpu)					\
do {									\
	KASSERT((cpu)->ci_divisor_delay != 0, ("divisor delay"));		\
	(cpu)->ci_divisor_recip = 0x100000000ULL / (cpu)->ci_divisor_delay; \
} while (0)

#define	MIPS_COUNT_TO_MHZ(cpu, count, res)				\
	__asm __volatile ("multu %1,%2 ; mfhi %0"			\
	    : "=r"((res)) : "r"((count)), "r"((cpu)->ci_divisor_recip))


extern struct cpu_info cpu_info_store;

#if 0
#define	curcpu()	(&cpu_info_store)
#define	cpu_number()	(0)
#endif

#endif /* !LOCORE */
#endif /* _KERNEL */
#endif /* _CPUINFO_H_ */
