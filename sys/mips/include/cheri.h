/*-
 * Copyright (c) 2011-2014 Robert N. M. Watson
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
#else
#include <assert.h>		/* assert() */
#endif

#include <machine/cherireg.h>
#include <sys/types.h>

/*
 * Canonical C-language representation of a capability for compilers that
 * don't support capabilities directly.  We don't define internal fields by
 * default as instructions, rather than memory access, should be preferred.
 */
#define	CHERICAP_SIZE	32
struct chericap {
#ifdef CHERICAP_INTERNAL
	uint32_t	c_reserved;
#if BYTE_ORDER == BIG_ENDIAN
	/* XXXRW: This definitely needs some testing. */
	uint32_t	c_sealed:1;
	uint32_t	c_perms:31;
#else
#error	"BYTE_ORDER != BIG_ENDIAN not yet supported"
#endif
	uint64_t	c_otype;
	uint64_t	c_base;
	uint64_t	c_length;
#else
	uint8_t		c_data[CHERICAP_SIZE];
#endif
} __packed __aligned(CHERICAP_SIZE);
#ifdef _KERNEL
CTASSERT(sizeof(struct chericap) == CHERICAP_SIZE);
#endif

/*
 * Canonical C-language representation of a CHERI object capability -- code
 * and data capabilities in registers or memory.
 */
struct cheri_object {
#if !defined(_KERNEL) && __has_feature(capabilities)
	__capability void	*co_codecap;
	__capability void	*co_datacap;
#else
	struct chericap		 co_codecap;
	struct chericap		 co_datacap;
#endif
};

/*
 * Register frame to be preserved on context switching -- very similar to
 * struct mips_frame.  As with mips_frame, the order of save/restore is very
 * important for both reasons of correctness and security.
 *
 * Must match the register offset definitions (CHERIFRAME_OFF_*) in
 * cherireg.h.
 */
struct cheri_frame {
	/* c0 has special properties for MIPS load/store instructions. */
	struct chericap	cf_c0;

	/*
	 * General-purpose capabilities -- note, numbering is from v1.7 of the
	 * CHERI ISA spec (ISAv2).
	 */
	struct chericap	cf_c1, cf_c2, cf_c3, cf_c4;
	struct chericap	cf_c5, cf_c6, cf_c7;
	struct chericap	cf_c8, cf_c9, cf_c10, cf_c11, cf_c12;
	struct chericap	cf_c13, cf_c14, cf_c15, cf_c16, cf_c17;
	struct chericap	cf_c18, cf_c19, cf_c20, cf_c21, cf_c22;
	struct chericap cf_c23, cf_rcc, cf_c25, cf_idc;

	/*
	 * Program counter capability -- extracted from exception frame EPCC.
	 */
	struct chericap	cf_pcc;

	/*
	 * Padded out non-capability registers.
	 *
	 * XXXRW: The comment below on only updating for CP2 exceptions is
	 * incorrect, but should be made correct.
	 */
	register_t	cf_capcause;	/* Updated only on CP2 exceptions. */
	register_t	_cf_pad[3];
};

#ifdef _KERNEL
/* 28 capability registers + capcause + padding. */
CTASSERT(sizeof(struct cheri_frame) == (29 * CHERICAP_SIZE));
#endif

#ifdef _KERNEL
/*
 * Data structure defining kernel per-thread caller-save state used in
 * voluntary context switches.  This is morally equivalent to pcb_context[].
 *
 * XXXRW: For now, we define a 'micro-ABI' for the kernel, preserving and
 * restoring only a few capability registers used by overt inline assembly.
 * Once we use a CHERI-aware compiler for the kernel, this will need to be
 * expanded to include a full set of caller-save registers.
 */
struct cheri_kframe {
	struct chericap	ckf_c11;
	struct chericap	ckf_c12;
};
#endif

/*
 * Data structure describing CHERI's sigaltstack-like extensions to signal
 * delivery.  In the event that a thread takes a signal when $pcc doesn't hold
 * CHERI_PERM_SYSCALL, we will need to install new $pcc, $c0, $c11, and $idc
 * state, and move execution to the per-thread alternative stack, whose
 * pointer should (presumably) be relative to the c0/c11 defined here.
 */
struct cheri_signal {
#if !defined(_KERNEL) && __has_feature(capabilities)
	__capability void	*csig_pcc;
	__capability void	*csig_c0;
	__capability void	*csig_c11;
	__capability void	*csig_idc;
#else
	struct chericap		 csig_pcc;
	struct chericap		 csig_c0;
	struct chericap		 csig_c11;
	struct chericap		 csig_idc;
#endif
};

/*
 * Per-thread CHERI CCall/CReturn stack, which preserves the calling PC/PCC/
 * IDC across CCall so that CReturn can restore them.
 *
 * XXXRW: This is a very early experiment -- it's not clear if this idea will
 * persist in its current form, or at all.  For more complex userspace
 * language, there's a reasonable expectation that it, rather than the kernel,
 * will want to manage the idea of a "trusted stack".
 *
 * XXXRW: This is currently part of the kernel-user ABI due to the
 * CHERI_GET_STACK and CHERI_SET_STACK sysarch() calls.  In due course we need
 * to revise those APIs and differentiate the kernel-internal representation
 * from the public one.
 */
struct cheri_stack_frame {
	register_t	_csf_pad0;	/* Used to be MIPS program counter. */
	register_t	_csf_pad1;
	register_t	_csf_pad2;
	register_t	_csf_pad3;
#if !defined(_KERNEL) && __has_feature(capabilities)
	__capability void	*csf_pcc;
	__capability void	*csf_idc;
#else
	struct chericap	csf_pcc;	/* XXXRW: Store $pc in here? */
	struct chericap	csf_idc;
#endif
};

#define	CHERI_STACK_DEPTH	4	/* XXXRW: 4 is a nice round number. */
struct cheri_stack {
	register_t	cs_tsp;		/* Byte offset, not frame index. */
	register_t	cs_tsize;	/* Stack size, in bytes. */
	register_t	_cs_pad0;
	register_t	_cs_pad1;
	struct cheri_stack_frame	cs_frames[CHERI_STACK_DEPTH];
} __aligned(CHERICAP_SIZE);

#define	CHERI_FRAME_SIZE	sizeof(struct cheri_stack_frame)
#define	CHERI_STACK_SIZE	(CHERI_STACK_DEPTH * CHERI_FRAME_SIZE)

/*
 * CHERI capability register manipulation macros.
 */
#define	CHERI_CGETBASE(v, cb) do {					\
	__asm__ __volatile__ ("cgetbase %0, $c%1" : "=r" (v) :		\
	    "i" (cb));							\
} while (0)

#define	CHERI_CGETLEN(v, cb) do {					\
	__asm__ __volatile__ ("cgetlen %0, $c%1" : "=r" (v) :		\
	    "i" (cb));							\
} while (0)

#define	CHERI_CGETOFFSET(v, cb) do {					\
	__asm__ __volatile__ ("cgetoffset %0, $c%1" : "=r" (v) :	\
	    "i" (cb));							\
} while (0)

#define	CHERI_CGETTAG(v, cb) do {					\
	__asm__ __volatile__ ("cgettag %0, $c%1" : "=r" (v) :		\
	    "i" (cb));							\
} while (0)

#define	CHERI_CGETSEALED(v, cb) do {					\
	__asm__ __volatile__ ("cgetsealed %0, $c%1" :	"=r" (v) :	\
	    "i" (cb));							\
} while (0)

#define	CHERI_CGETPERM(v, cb) do {					\
	__asm__ __volatile__ ("cgetperm %0, $c%1" : "=r" (v) :		\
	    "i" (cb));							\
} while (0)

#define	CHERI_CGETTYPE(v, cb) do {					\
	__asm__ __volatile__ ("cgettype %0, $c%1" : "=r" (v) :		\
	    "i" (cb));							\
} while (0)

#define	CHERI_CGETCAUSE(v) do {						\
	__asm__ __volatile__ ("cgetcause %0" : "=r" (v));		\
} while (0)

#define	CHERI_CTOPTR(v, cb, ct) do {					\
	__asm__ __volatile__ ("ctoptr %0, $c%1, $c%2" : "=r" (v) :	\
	    "i" (cb), "i" (ct));					\
} while (0)

/*
 * Note that despite effectively being a CMove, CGetDefault doesn't require a
 * memory clobber: if it's writing to $c0, it's a nop; otherwise, it's not
 * writing to $c0 so no clobber is needed.
 */
#define	CHERI_CGETDEFAULT(cd) do {					\
	__asm__ __volatile__ ("cgetdefault $c%0" : : "i" (cd));		\
} while (0)

/*
 * Instructions that check capability values and could throw exceptions; no
 * capability-register value changes, so no clobbers required.
 */
#define	CHERI_CCHECKPERM(cs, v) do {					\
	__asm__ __volatile__ ("ccheckperm $c%0, %1" : :			\
	    "i" (cd), "r" (v));						\
} while (0)

#define	CHERI_CCHECKTYPE(cs, cb) do {					\
	__asm__ __volatile__ ("cchecktype $c%0, $c%1" : :		\
	    "i" (cs), "i" (cb));					\
} while (0)

/*
 * Routines that modify or replace values in capability registers that don't
 * affect memory access via the register.  These do not require memory
 * clobbers.
 *
 * XXXRW: Are there now none of these?
 */

/*
 * Instructions relating to capability invocation, return, sealing, and
 * unsealing.  Memory clobbers are required for register manipulation when
 * targeting $c0.  They are also required for both CCall and CReturn to ensure
 * that any memory write-back is done before invocation.
 *
 * XXXRW: Is the latter class of cases required?
 */
#define	CHERI_CSEAL(cd, cs, ct) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("cseal $c%0, $c%1, $c%2" : :	\
		    "i" (cd), "i" (cs), "i" (ct) : "memory");		\
	else								\
		__asm__ __volatile__ ("cseal $c%0, $c%1, $c%2" : :	\
		    "i" (cd), "i" (cs), "i" (ct));			\
} while (0)

#define CHERI_CUNSEAL(cd, cb, ct) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("cunseal $c%0, $c%1, $c%2" : :	\
		    "i" (cd), "i" (cb), "i" (ct) : "memory");		\
	else								\
		__asm__ __volatile__ ("cunseal $c%0, $c%1, $c%2" : :	\
		    "i" (cd), "i" (cb), "i" (ct));			\
} while (0)

#define	CHERI_CCALL(cs, cb) do {					\
	__asm__ __volatile__ ("ccall $c%0, $c%1" : :			\
	    "i" (cs), "i" (cb) : "memory");				\
} while (0)

#define	CHERI_CRETURN() do {						\
	__asm__ __volatile__ ("creturn" : : : "memory");		\
} while (0)

/*
 * Capability store; while this doesn't muck with c0, it does require a memory
 * clobber.
 */
#define	CHERI_CSC(cs, cb, regbase, offset) do {				\
	__asm__ __volatile__ ("csc $c%0, %1, %2($c%3)" : :		\
	    "i" (cs), "r" (regbase), "i" (offset), "i" (cb) :		\
	    "memory");							\
} while (0)

/*
 * Data stores; while these don't muck with c0, they do require memory
 * clobbers.
 */
#define	CHERI_CSB(rs, rt, offset, cb) do {				\
	__asm__ __volatile__ ("csb %0, %1, %2($c%3)" : :		\
	    "r" (rs), "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CSH(rs, rt, offset, cb) do {				\
	__asm__ __volatile__ ("csh %0, %1, %2($c%3)" : :		\
	    "r" (rs), "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CSW(rs, rt, offset, cb) do {				\
	__asm__ __volatile__ ("csw %0, %1, %2($c%3)" : :		\
	    "r" (rs), "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CSD(rs, rt, offset, cb) do {				\
	__asm__ __volatile__ ("csd %0, %1, %2($c%3)" : :		\
	    "r" (rs), "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

/*
 * Data loads: while these don't much with c0, they do require memory
 * clobbers.
 */
#define	CHERI_CLB(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ ("clb %0, %1, %2($c%3)" :			\
	    "=r" (rd) : "r" (rt), "i" (offset),"i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLH(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ ("clh %0, %1, %2($c%3)" :			\
	    "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLW(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ ("clw %0, %1, %2($c%3)" :			\
	    "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLD(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ ("cld %0, %1, %2($c%3)" :			\
	    "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLBU(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ ("clbu %0, %1, %2($c%3)" :			\
	    "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLHU(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ ("clhu %0, %1, %2($c%3)" :			\
	    "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

#define	CHERI_CLWU(rd, rt, offset, cb) do {				\
	__asm__ __volatile__ ("clwu %0, %1, %2($c%3)" :			\
	    "=r" (rd) : "r" (rt), "i" (offset), "i" (cb) : "memory");	\
} while (0)

/*
 * Routines that modify or replace values in capability registers, and that if
 * if used on C0, require the compiler to write registers back to memory, and
 * reload afterwards, since we may effectively be changing the compiler-
 * visible address space.  This is also necessary for permissions changes as
 * well, to ensure that write-back occurs before a possible loss of store
 * permission.
 */
#define	CHERI_CGETPCC(v, cd) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("cgetpcc %0, $c%1" : "=r" (v) :	\
		    "i" (cd) : "memory");				\
	else								\
		__asm__ __volatile__ ("cgetpcc %0, $c%1" : "=r" (v) :	\
		    "i" (cd));						\
} while (0)

#define	CHERI_CINCBASE(cd, cb, v) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("cincbase $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v) : "memory");		\
	else								\
		__asm__ __volatile__ ("cincbase $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CMOVE(cd, cb) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("cmove $c%0, $c%1" : :		\
		    "i" (cd), "i" (cb) : "memory");			\
	else								\
		__asm__ __volatile__ ("cmove $c%0, $c%1" : :		\
		    "i" (cd), "i" (cb));				\
} while (0)

#define	CHERI_CSETDEFAULT(cb) do {					\
	__asm__ __volatile__ ("csetdefault %c%0" : : "i" (cb) :		\
	    "memory");							\
} while (0)

#define	CHERI_CSETLEN(cd, cb, v) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("csetlen $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v) : "memory");		\
	else								\
		__asm__ __volatile__ ("csetlen $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CSETOFFSET(cd, cb, v) do {				\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("csetoffset $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v) : "memory");		\
	else								\
		__asm__ __volatile__ ("csetoffset $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CCLEARTAG(cd, cb) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("ccleartag $c%0, $c%1" : :	\
		    "i" (cd), "i" (cb) : "memory");			\
	else								\
		__asm__ __volatile__ ("ccleartag $c%0, $c%1" : :	\
		    "i" (cd), "i" (cb));				\
} while (0)

#define	CHERI_CANDPERM(cd, cb, v) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("candperm $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v) : "memory");		\
	else								\
		__asm__ __volatile__ ("candperm $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CFROMPTR(cd, cb, v) do {					\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("cfromptr $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v) : "memory");		\
	else								\
		__asm__ __volatile__ ("cfromptr $c%0, $c%1, %2" : :	\
		    "i" (cd), "i" (cb), "r" (v));			\
} while (0)

#define	CHERI_CLC(cd, cb, regbase, offset) do {				\
	if ((cd) == 0)							\
		__asm__ __volatile__ ("clc $c%0, %1, %2($c%3)" : :	\
		    "i" (cd), "r" (regbase), "i" (offset), "i" (cb) :	\
		    "memory");						\
	else								\
		__asm__ __volatile__ ("clc $c%0, %1, %2($c%3)" : :	\
		    "i" (cd), "r" (regbase), "i" (offset), "i" (cb));	\
} while (0)

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
 * Extract a flattened but useful memory representation of a complete
 * capability register.
 */
#define	CHERI_GETCAPREG(crn, c) do {					\
	CHERI_CGETPERM((c).c_perms, (crn));				\
	CHERI_CGETSEALED((c).c_sealed, (crn));				\
	CHERI_CGETTYPE((c).c_otype, (crn));				\
	CHERI_CGETBASE((c).c_base, (crn));				\
	CHERI_CGETLEN((c).c_length, (crn));				\
} while (0)


/*
 * Routines for measuring time -- depends on a later MIPS userspace cycle
 * counter.
 */
static __inline uint64_t
cheri_get_cyclecount(void)
{
	uint64_t _time;
	__asm __volatile("rdhwr %0, $2" : "=r" (_time));
	return (_time);
}

#ifdef _KERNEL
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
	printf("C%02u: ", num);						\
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
	db_printf("C%02u ", num);					\
	DB_CHERI_CAP_PRINT(crn);					\
} while (0)
#endif /* !_KERNEL */

/*
 * APIs that act on C language representations of capabilities -- but not
 * capabilities themselves.
 */
void	cheri_capability_copy(struct chericap *cp_to,
	    struct chericap *cp_from);
void	cheri_capability_set(struct chericap *cp, uint32_t uperms,
	    void *otype, void *basep, size_t length, off_t off);
void	cheri_capability_set_null(struct chericap *cp);

/*
 * CHERI capability utility functions.
 */
void	 cheri_bcopy(void *src, void *dst, size_t len);
void	*cheri_memcpy(void *dst, void *src, size_t len);

/*
 * CHERI context management functions.
 */
void	cheri_exec_setregs(struct thread *td);
void	cheri_log_exception(struct trapframe *frame, int trap_type);
int	cheri_syscall_authorize(struct thread *td, u_int code,
	    int nargs, register_t *args);
int	cheri_signal_sandboxed(struct thread *td);
void	cheri_sendsig(struct thread *td);

/*
 * Functions to set up and manipulate CHERI contexts and stacks.
 */
struct pcb;
struct sysarch_args;
void	cheri_context_copy(struct pcb *dst, struct pcb *src);
void	cheri_signal_copy(struct pcb *dst, struct pcb *src);
void	cheri_stack_copy(struct pcb *dst, struct pcb *src);
void	cheri_stack_init(struct pcb *pcb);
int	cheri_stack_unwind(struct thread *td, struct trapframe *tf,
	    int signum);
int	cheri_sysarch_getstack(struct thread *td, struct sysarch_args *uap);
int	cheri_sysarch_setstack(struct thread *td, struct sysarch_args *uap);

/*
 * Global sysctl definitions.
 */
SYSCTL_DECL(_security_cheri);
SYSCTL_DECL(_security_cheri_stats);
extern u_int	security_cheri_debugger_on_sandbox_signal;
extern u_int	security_cheri_debugger_on_sandbox_syscall;
extern u_int	security_cheri_debugger_on_sandbox_unwind;
extern u_int	security_cheri_debugger_on_sigprot;
extern u_int	security_cheri_sandboxed_signals;
extern u_int	security_cheri_syscall_violations;
#endif /* !_KERNEL */

#endif /* _MIPS_INCLUDE_CHERI_H_ */
