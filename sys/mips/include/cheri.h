/*-
 * Copyright (c) 2011-2017 Robert N. M. Watson
 * Copyright (c) 2015 SRI International
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
 */

#ifndef _MIPS_INCLUDE_CHERI_H_
#define	_MIPS_INCLUDE_CHERI_H_

#ifdef _KERNEL
#include <sys/sysctl.h>		/* SYSCTL_DECL() */
#include <sys/systm.h>		/* CTASSERT() */
#endif

#include <machine/cherireg.h>

#include <sys/types.h>

/*
 * In the past, struct cheri_frame was the in-kernel and kernel<->user
 * structure holding CHERI register state for context switching.  It is now a
 * public structure for kernel<->user interaction (e.g., signals), and struct
 * trapframe is used within the kernel.  Regardless, correct preservation of
 * state in this structure is critical to both correctness and security.
 */
struct cheri_frame {
	/* DDC has special properties for MIPS load/store instructions. */
#if !defined(_KERNEL) && __has_feature(capabilities)
	__capability void	*cf_ddc;
#else
	struct chericap	cf_ddc;
#endif

	/*
	 * General-purpose capabilities -- note, numbering is from v1.17 of
	 * the CHERI ISA spec (ISAv5 draft).
	 */
#if !defined(_KERNEL) && __has_feature(capabilities)
	__capability void *cf_c1, *cf_c2, *cf_c3, *cf_c4;
	__capability void *cf_c5, *cf_c6, *cf_c7;
	__capability void *cf_c8, *cf_c9, *cf_c10, *cf_stc, *cf_c12;
	__capability void *cf_c13, *cf_c14, *cf_c15, *cf_c16, *cf_c17;
	__capability void *cf_c18, *cf_c19, *cf_c20, *cf_c21, *cf_c22;
	__capability void *cf_c23, *cf_c24, *cf_c25, *cf_idc;
#else
	struct chericap	cf_c1, cf_c2, cf_c3, cf_c4;
	struct chericap	cf_c5, cf_c6, cf_c7;
	struct chericap	cf_c8, cf_c9, cf_c10, cf_stc, cf_c12;
	struct chericap	cf_c13, cf_c14, cf_c15, cf_c16, cf_c17;
	struct chericap	cf_c18, cf_c19, cf_c20, cf_c21, cf_c22;
	struct chericap cf_c23, cf_c24, cf_c25, cf_idc;
#endif

	/*
	 * Program counter capability -- extracted from exception frame EPCC.
	 */
#if !defined(_KERNEL) && __has_feature(capabilities)
	__capability void *cf_pcc;
#else
	struct chericap	cf_pcc;
#endif

	/*
	 * Padded out non-capability registers.
	 *
	 * XXXRW: The comment below on only updating for CP2 exceptions is
	 * incorrect, but should be made correct.
	 */
	register_t	cf_capcause;	/* Updated only on CP2 exceptions. */
	register_t	cf_capvalid;
#if (defined(CPU_CHERI) && !defined(CPU_CHERI128)) || (defined(_MIPS_SZCAP) && (_MIPS_SZCAP == 256))
	register_t	_cf_pad1[2];
#endif
};

#ifdef _KERNEL
/* 28 capability registers + capcause + padding. */
CTASSERT(sizeof(struct cheri_frame) == (29 * CHERICAP_SIZE));
#endif

#ifdef _KERNEL
/*
 * Data structure defining kernel per-thread caller-save state used in
 * voluntary context switches.  This is morally equivalent to pcb_context[].
 */
struct cheri_kframe {
	struct chericap	ckf_c17;
	struct chericap	ckf_c18;
	struct chericap ckf_c19;
	struct chericap ckf_c20;
	struct chericap ckf_c21;
	struct chericap ckf_c22;
	struct chericap ckf_c23;
	struct chericap ckf_c24;
};
#endif

/*
 * CHERI capability register manipulation macros.
 */
#define	CHERI_CGETBASE(v, cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cgetbase %0, $c%1\n"					\
	    ".set pop\n"						\
	    : "=r" (v) : "i" (cb));					\
} while (0)

#define	CHERI_CGETLEN(v, cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cgetlen %0, $c%1\n"					\
	    ".set pop\n"						\
	    : "=r" (v) :"i" (cb));					\
} while (0)

#define	CHERI_CGETOFFSET(v, cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cgetoffset %0, $c%1\n"					\
	    ".set pop\n"						\
	    : "=r" (v) : "i" (cb));					\
} while (0)

#define	CHERI_CGETTAG(v, cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cgettag %0, $c%1\n"					\
	    ".set pop\n"						\
	    : "=r" (v) : "i" (cb));					\
} while (0)

#define	CHERI_CGETSEALED(v, cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cgetsealed %0, $c%1\n"					\
	    ".set pop\n"						\
	    : "=r" (v) : "i" (cb));					\
} while (0)

#define	CHERI_CGETPERM(v, cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cgetperm %0, $c%1\n"					\
	    ".set pop\n"						\
	    : "=r" (v) : "i" (cb));					\
} while (0)

#define	CHERI_CGETTYPE(v, cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cgettype %0, $c%1\n"					\
	    ".set pop\n"						\
	    : "=r" (v) : "i" (cb));					\
} while (0)

#define	CHERI_CGETCAUSE(v) do {						\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cgetcause %0\n"						\
	    ".set pop\n"						\
	    : "=r" (v));						\
} while (0)

#define	CHERI_CTOPTR(v, cb, ct) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "ctoptr %0, $c%1, $c%2\n"					\
	    ".set pop\n"						\
	    : "=r" (v) : "i" (cb), "i" (ct));				\
} while (0)

/*
 * Implement a CToInt similar to CToPtr but without the tag check, which will
 * be useful to extract integer interpretations of untagged capabilities.  One
 * property of this conversion is that, since the capability might be
 * untagged, we can't assume that (base + offset) < (max capability address),
 * and so significant care should be taken -- ideally this variant would only
 * be used when we know that the capability is untagged and holds a value that
 * must be an integer (due to types or other compile-time information).
 *
 * This may someday be an instruction.  If so, it could directly return the
 * cursor, rather than extract (base, offset).
 */
#define	CHERI_CTOINT(v, cb) do {					\
	register_t _base, _offset;					\
									\
	CHERI_CGETBASE(_base, cb);					\
	CHERI_CGETOFFSET(_offset, cb);					\
	v = (__typeof__(v))(_base + _offset);				\
} while (0)

/*
 * Note that despite effectively being a CMove, CGetDefault doesn't require a
 * memory clobber: if it's writing to $ddc, it's a nop; otherwise, it's not
 * writing to $ddc so no clobber is needed.
 */
#define	CHERI_CGETDEFAULT(cd) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cgetdefault $c%0\n"					\
	    ".set pop\n"						\
	    : : "i" (cd));						\
} while (0)

/*
 * Instructions that check capability values and could throw exceptions; no
 * capability-register value changes, so no clobbers required.
 */
#define	CHERI_CCHECKPERM(cs, v) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "ccheckperm $c%0, %1\n" 					\
	    ".set pop\n"						\
	    : : "i" (cd), "r" (v));					\
} while (0)

#define	CHERI_CCHECKTYPE(cs, cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cchecktype $c%0, $c%1\n"					\
	    ".set pop\n"						\
	    : : "i" (cs), "i" (cb));					\
} while (0)

/*
 * Instructions relating to capability invocation, return, sealing, and
 * unsealing.  Memory clobbers are required for register manipulation when
 * targeting $ddc.  They are also required for both CCall and CReturn to
 * ensure that any memory write-back is done before invocation.
 *
 * XXXRW: Is the latter class of cases required?
 */
#define	CHERI_CSEAL(cd, cs, ct) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cseal $c%0, $c%1, $c%2\n"				\
		    ".set pop\n"					\
		    : : "i" (cd), "i" (cs), "i" (ct) : "memory");	\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cseal $c%0, $c%1, $c%2\n"				\
		    ".set pop\n"					\
		    : : "i" (cd), "i" (cs), "i" (ct));			\
} while (0)

#define CHERI_CUNSEAL(cd, cb, ct) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cunseal $c%0, $c%1, $c%2\n"			\
		    ".set pop\n"					\
		    : : "i" (cd), "i" (cb), "i" (ct) : "memory");	\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cunseal $c%0, $c%1, $c%2\n"			\
		    ".set pop\n"					\
		    : : "i" (cd), "i" (cb), "i" (ct));			\
} while (0)

#define	CHERI_CCALL(cs, cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "ccall $c%0, $c%1\n"					\
	    ".set pop\n"						\
	    : :	"i" (cs), "i" (cb) : "memory");				\
} while (0)

#define	CHERI_CRETURN() do {						\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "creturn\n"							\
	    ".set pop\n"						\
	    : : : "memory");						\
} while (0)

/*
 * Capability store; while this doesn't muck with $ddc, it does require a
 * memory clobber.
 */
#define	CHERI_CSC(cs, cb, regbase, offset) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "csc $c%0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : : "i" (cs), "r" (regbase), "i" (offset), "i" (cb) :	\
	    "memory");							\
} while (0)

/*
 * Data stores; while these don't muck with $ddc, they do require memory
 * clobbers.
 */
#define	CHERI_CSB(rs, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "csb %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : : "r" (rs), "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CSH(rs, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "csh %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : : "r" (rs), "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CSW(rs, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "csw %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : : "r" (rs), "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CSD(rs, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "csd %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : : "r" (rs), "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

/*
 * Data loads: while these don't much with $ddc, they do require memory
 * clobbers.
 */
#define	CHERI_CLB(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "clb %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	     : "=r" (rd) : "r" (rt), "i" (offset),"i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLH(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "clh %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLW(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "clw %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLD(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "cld %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLBU(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "clbu %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLHU(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "clhu %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLWU(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "clwu %0, %1, %2($c%3)\n"					\
	    ".set pop\n"						\
	    : "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

/*
 * Routines that modify or replace values in capability registers, and that if
 * if used on $ddc, require the compiler to write registers back to memory,
 * and reload afterwards, since we may effectively be changing the compiler-
 * visible address space.  This is also necessary for permissions changes as
 * well, to ensure that write-back occurs before a possible loss of store
 * permission.
 */
#define	CHERI_CGETPCC(v, cd) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cgetpcc %0, $c%1\n"				\
		    ".set pop\n"					\
		    : "=r" (v) : "i" (cd) : "memory");			\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cgetpcc %0, $c%1\n"				\
		    ".set pop\n"					\
		    : "=r" (v) : "i" (cd));				\
} while (0)

#define	CHERI_CINCBASE(cd, cb, v) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cincbase $c%0, $c%1, %2\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v) : "memory");	\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cincbase $c%0, $c%1, %2\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CINCOFFSET(cd, cb, v) do {				\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cincoffset $c%0, $c%1, %2\n"			\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v) : "memory");	\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cincoffset $c%0, $c%1, %2\n"			\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v));			\
} while (0)

#if (defined(CPU_CHERI) && !defined(CPU_CHERI128)) || (defined(_MIPS_SZCAP) && (_MIPS_SZCAP == 256))
#define	CHERI_CMOVE(cd, cb) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cmove $c%0, $c%1\n"				\
		    ".set pop\n"					\
		    : : "i" (cd), "i" (cb) : "memory");			\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cmove $c%0, $c%1\n"				\
		    ".set pop\n"					\
		    : : "i" (cd), "i" (cb));				\
} while (0)
#else /* 128-bit CHERI */
#define	CHERI_CMOVE(cd, cb)	CHERI_CINCOFFSET(cd, cb, 0)
#endif /* 128-bit CHERI */

#define	CHERI_CSETDEFAULT(cb) do {					\
	__asm__ __volatile__ (						\
	    ".set push\n"						\
	    ".set noreorder\n"						\
	    "csetdefault %c%0\n"					\
	    ".set pop\n"						\
	    : : "i" (cb) : "memory");					\
} while (0)

#define	CHERI_CSETLEN(cd, cb, v) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "csetlen $c%0, $c%1, %2\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v) : "memory");	\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "csetlen $c%0, $c%1, %2\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CSETOFFSET(cd, cb, v) do {				\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "csetoffset $c%0, $c%1, %2\n"			\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v) : "memory");	\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "csetoffset $c%0, $c%1, %2\n"			\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CCLEARTAG(cd, cb) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "ccleartag $c%0, $c%1\n"				\
		    ".set pop\n"					\
		    : : "i" (cd), "i" (cb) : "memory");			\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "ccleartag $c%0, $c%1\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb));				\
} while (0)

#define	CHERI_CANDPERM(cd, cb, v) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "candperm $c%0, $c%1, %2\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v) : "memory");	\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "candperm $c%0, $c%1, %2\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CSETBOUNDS(cd, cb, v) do {				\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "csetbounds $c%0, $c%1, %2\n"			\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v) : "memory");	\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "csetbounds $c%0, $c%1, %2\n"			\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CFROMPTR(cd, cb, v) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cfromptr $c%0, $c%1, %2\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v) : "memory");	\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "cfromptr $c%0, $c%1, %2\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CLC(cd, cb, regbase, offset) do {				\
	if ((cd) == 0)							\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "clc $c%0, %1, %2($c%3)\n"				\
		    ".set pop\n"					\
		    : :	"i" (cd), "r" (regbase), "i" (offset), "i" (cb)	\
		    : "memory");					\
	else								\
		__asm__ __volatile__ (					\
		    ".set push\n"					\
		    ".set noreorder\n"					\
		    "clc $c%0, %1, %2($c%3)\n"				\
		    ".set pop\n"					\
		    : : "i" (cd), "r" (regbase), "i" (offset),		\
		    "i" (cb));						\
} while (0)

/*
 * Utility functions for the kernel -- as they depend on $kdc.
 */
#ifdef _KERNEL
static inline void
cheri_capability_load(u_int crn_to, struct chericap *cp)
{

       CHERI_CLC(crn_to, CHERI_CR_KDC, cp, 0);
}

static inline void
cheri_capability_store(u_int crn_from, struct chericap *cp)
{

        CHERI_CSC(crn_from, CHERI_CR_KDC, cp, 0);
}

/*
 * Because contexts contain tagged capabilities, we can't just use memcpy()
 * on the data structure.  Once the C compiler knows about capabilities, then
 * direct structure assignment should be plausible.  In the mean time, an
 * explicit capability context copy routine is required.
 *
 * XXXRW: Compiler should know how to do copies of tagged capabilities.
 *
 * XXXRW: Compiler should be providing us with the temporary register.
 */
static inline void
cheri_capability_copy(struct chericap *cp_to, struct chericap *cp_from)
{

	cheri_capability_load(CHERI_CR_CTEMP0, cp_from);
	cheri_capability_store(CHERI_CR_CTEMP0, cp_to);
}

static inline void
cheri_capability_set_null(struct chericap *cp)
{

	CHERI_CFROMPTR(CHERI_CR_CTEMP0, CHERI_CR_KDC, 0);
	CHERI_CSC(CHERI_CR_CTEMP0, CHERI_CR_KDC, (register_t)cp, 0);
}

static inline void
cheri_capability_setoffset(struct chericap *cp, register_t offset)
{

	CHERI_CLC(CHERI_CR_CTEMP0, CHERI_CR_KDC, cp, 0);
	CHERI_CSETOFFSET(CHERI_CR_CTEMP0, CHERI_CR_CTEMP0, offset);
	CHERI_CSC(CHERI_CR_CTEMP0, CHERI_CR_KDC, (register_t)cp, 0);
}
#endif

/*
 * CHERI-MIPS-specific kernel utility functions.
 */
#ifdef _KERNEL
int	cheri_capcause_to_sicode(register_t capcause);
#endif

/*
 * Routines for measuring time -- depends on a later MIPS userspace cycle
 * counter.
 */
static __inline uint32_t
cheri_get_cyclecount(void)
{
	uint64_t _time;

	__asm__ __volatile__ (
	    ".set push\n"
	    ".set noreorder\n"
	    "rdhwr %0, $2\n"
	   ".set pop\n"
	    : "=r" (_time));
	return (_time & 0xffffffff);
}

/*
 * Special marker NOPs recognised by analyse_trace.py to start / stop region
 * of interest in trace.
 */
#define	CHERI_START_TRACE	do {					\
	__asm__ __volatile__("li $0, 0xbeef");				\
} while(0)
#define	CHERI_STOP_TRACE	do {					\
	__asm__ __volatile__("li $0, 0xdead");				\
} while(0)

#ifdef _KERNEL
/*
 * Special marker NOP to log messages in instruction traces.
 */
void cheri_trace_log(void *buf, size_t len, int format);

#define	CHERI_TRACE_STRING(s)						\
	cheri_trace_log((s), strlen((s)), 0);
#define CHERI_TRACE_MEM(buf, len)					\
	cheri_trace_log((buf), (len), 1);

#define	CHERI_CAP_PRINT(crn) do {					\
	uintmax_t c_perms, c_otype, c_base, c_length, c_offset;		\
	u_int ctag, c_sealed;						\
									\
	CHERI_CGETTAG(ctag, (crn));					\
	CHERI_CGETSEALED(c_sealed, (crn));				\
	CHERI_CGETPERM(c_perms, (crn));					\
	CHERI_CGETTYPE(c_otype, (crn));					\
	CHERI_CGETBASE(c_base, (crn));					\
	CHERI_CGETLEN(c_length, (crn));					\
	CHERI_CGETOFFSET(c_offset, (crn));				\
	printf("v:%u s:%u p:%08jx b:%016jx l:%016jx o:%jx t:%jx\n",	\
	    ctag, c_sealed, c_perms, c_base, c_length, c_offset,	\
	    c_otype);							\
} while (0)

#define	CHERI_REG_PRINT(crn, num) do {					\
	printf("$c%02u: ", num);					\
	CHERI_CAP_PRINT(crn);						\
} while (0)

#ifdef DDB
#define	DB_CHERI_CAP_PRINT(crn) do {					\
	uintmax_t c_perms, c_otype, c_base, c_length, c_offset;		\
	u_int ctag, c_sealed;						\
									\
	CHERI_CGETTAG(ctag, (crn));					\
	CHERI_CGETSEALED(c_sealed, (crn));				\
	CHERI_CGETPERM(c_perms, (crn));					\
	CHERI_CGETTYPE(c_otype, (crn));					\
	CHERI_CGETBASE(c_base, (crn));					\
	CHERI_CGETLEN(c_length, (crn));					\
	CHERI_CGETOFFSET(c_offset, (crn));				\
	db_printf("v:%u s:%u p:%08jx b:%016jx l:%016jx o:%jx t:%jx\n",	\
	    ctag, c_sealed, c_perms, c_base, c_length, c_offset,	\
	    c_otype);							\
} while (0)

#define	DB_CHERI_REG_PRINT(crn, num) do {				\
	db_printf("$c%02u: ", num);					\
	DB_CHERI_CAP_PRINT(crn);					\
} while (0)
#endif /* !DDB */
#endif /* !_KERNEL */

#endif /* _MIPS_INCLUDE_CHERI_H_ */
