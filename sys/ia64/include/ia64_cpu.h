/*-
 * Copyright (c) 2000 Doug Rabson
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
 *	$FreeBSD$
 */

#ifndef _MACHINE_IA64_CPU_H_
#define _MACHINE_IA64_CPU_H_

/*
 * Definition of PSR and IPSR bits.
 */
#define IA64_PSR_BE		0x0000000000000002
#define IA64_PSR_UP		0x0000000000000004
#define IA64_PSR_AC		0x0000000000000008
#define IA64_PSR_MFL		0x0000000000000010
#define IA64_PSR_MFH		0x0000000000000020
#define IA64_PSR_IC		0x0000000000002000
#define IA64_PSR_I		0x0000000000004000
#define IA64_PSR_PK		0x0000000000008000
#define IA64_PSR_DT		0x0000000000020000
#define IA64_PSR_DFL		0x0000000000040000
#define IA64_PSR_DFH		0x0000000000080000
#define IA64_PSR_SP		0x0000000000100000
#define IA64_PSR_PP		0x0000000000200000
#define IA64_PSR_DI		0x0000000000400000
#define IA64_PSR_SI		0x0000000000800000
#define IA64_PSR_DB		0x0000000001000000
#define IA64_PSR_LP		0x0000000002000000
#define IA64_PSR_TB		0x0000000004000000
#define IA64_PSR_RT		0x0000000008000000
#define IA64_PSR_CPL		0x0000000300000000
#define IA64_PSR_CPL_KERN	0x0000000000000000
#define IA64_PSR_CPL_1		0x0000000100000000
#define IA64_PSR_CPL_2		0x0000000200000000
#define IA64_PSR_CPL_USER	0x0000000300000000
#define IA64_PSR_IS		0x0000000400000000
#define IA64_PSR_MC		0x0000000800000000
#define IA64_PSR_IT		0x0000001000000000
#define IA64_PSR_ID		0x0000002000000000
#define IA64_PSR_DA		0x0000004000000000
#define IA64_PSR_DD		0x0000008000000000
#define IA64_PSR_SS		0x0000010000000000
#define IA64_PSR_RI		0x0000060000000000
#define IA64_PSR_RI_0		0x0000000000000000
#define IA64_PSR_RI_1		0x0000020000000000
#define IA64_PSR_RI_2		0x0000040000000000
#define IA64_PSR_ED		0x0000080000000000
#define IA64_PSR_BN		0x0000100000000000
#define IA64_PSR_IA		0x0000200000000000

/*
 * Definition of ISR bits.
 */
#define IA64_ISR_CODE		0x000000000000ffff
#define IA64_ISR_VECTOR		0x0000000000ff0000
#define IA64_ISR_X		0x0000000100000000
#define IA64_ISR_W		0x0000000200000000
#define IA64_ISR_R		0x0000000400000000
#define IA64_ISR_NA		0x0000000800000000
#define IA64_ISR_SP		0x0000001000000000
#define IA64_ISR_RS		0x0000002000000000
#define IA64_ISR_IR		0x0000004000000000
#define IA64_ISR_NI		0x0000008000000000
#define IA64_ISR_SO		0x0000010000000000
#define IA64_ISR_EI		0x0000060000000000
#define IA64_ISR_EI_0		0x0000000000000000
#define IA64_ISR_EI_1		0x0000020000000000
#define IA64_ISR_EI_2		0x0000040000000000
#define IA64_ISR_ED		0x0000080000000000

/*
 * Vector numbers for various ia64 interrupts.
 */
#define IA64_VEC_VHPT			0
#define IA64_VEC_ITLB			1
#define IA64_VEC_DTLB			2
#define IA64_VEC_ALT_ITLB		3
#define IA64_VEC_ALT_DTLB		4
#define IA64_VEC_NESTED_DTLB		5
#define IA64_VEC_IKEY_MISS		6
#define IA64_VEC_DKEY_MISS		7
#define IA64_VEC_DIRTY_BIT		8
#define IA64_VEC_INST_ACCESS		9
#define IA64_VEC_DATA_ACCESS		10
#define IA64_VEC_BREAK			11
#define IA64_VEC_EXT_INTR		12
#define IA64_VEC_PAGE_NOT_PRESENT	20
#define IA64_VEC_KEY_PERMISSION		21
#define IA64_VEC_INST_ACCESS_RIGHTS	22
#define IA64_VEC_DATA_ACCESS_RIGHTS	23
#define IA64_VEC_GENERAL_EXCEPTION	24
#define IA64_VEC_DISABLED_FP		25
#define IA64_VEC_NAT_CONSUMPTION	26
#define IA64_VEC_SPECULATION		27
#define IA64_VEC_DEBUG			29
#define IA64_VEC_UNALIGNED_REFERENCE	30
#define IA64_VEC_UNSUPP_DATA_REFERENCE	31
#define IA64_VEC_FLOATING_POINT_FAULT	32
#define IA64_VEC_FLOATING_POINT_TRAP	33
#define IA64_VEC_LOWER_PRIVILEGE_TRANSFER 34
#define IA64_VEC_TAKEN_BRANCH_TRAP	35
#define IA64_VEC_SINGLE_STEP_TRAP	36
#define IA64_VEC_IA32_EXCEPTION		45
#define IA64_VEC_IA32_INTERCEPT		46
#define IA64_VEC_IA32_INTERRUPT		47

/*
 * Manipulating region bits of an address.
 */
#define IA64_RR_BASE(n)		(((u_int64_t) (n)) << 61)
#define IA64_RR_MASK(x)		((x) & ((1L << 61) - 1))

#define IA64_PHYS_TO_RR6(x)	((x) | IA64_RR_BASE(6))
#define IA64_PHYS_TO_RR7(x)	((x) | IA64_RR_BASE(7))

#ifndef LOCORE

/*
 * Various special ia64 instructions.
 */

/*
 * Memory Fence.
 */
static __inline void
ia64_mf(void)
{
	__asm __volatile("mf");
}

static __inline void
ia64_mf_a(void)
{
	__asm __volatile("mf.a");
}

/*
 * Flush Cache.
 */
static __inline void
ia64_fc(u_int64_t va)
{
	__asm __volatile("fc %0" :: "r"(va));
}

/*
 * Sync instruction stream.
 */
static __inline void
ia64_sync_i(void)
{
	__asm __volatile("sync.i");
}

/*
 * Calculate address in VHPT for va.
 */
static __inline u_int64_t
ia64_thash(u_int64_t va)
{
	u_int64_t result;
	__asm __volatile("thash %0=%1" : "=r" (result) : "r" (va));
	return result;
}

/*
 * Calculate VHPT tag for va.
 */
static __inline u_int64_t
ia64_ttag(u_int64_t va)
{
	u_int64_t result;
	__asm __volatile("ttag %0=%1" : "=r" (result) : "r" (va));
	return result;
}

/*
 * Convert virtual address to physical.
 */
static __inline u_int64_t
ia64_tpa(u_int64_t va)
{
	u_int64_t result;
	__asm __volatile("tpa %0=%1" : "=r" (result) : "r" (va));
	return result;
}

/*
 * Generate a ptc.e instruction.
 */
static __inline void
ia64_ptc_e(u_int64_t v)
{
	__asm __volatile("ptc.e %0;; srlz.d;;" :: "r"(v));
}

/*
 * Generate a ptc.g instruction.
 */
static __inline void
ia64_ptc_g(u_int64_t va, u_int64_t log2size)
{
	__asm __volatile("ptc.g %0,%1;; srlz.d;;" :: "r"(va), "r"(log2size));
}

/*
 * Generate a ptc.ga instruction.
 */
static __inline void
ia64_ptc_ga(u_int64_t va, u_int64_t log2size)
{
	__asm __volatile("ptc.ga %0,%1;; srlz.d;;" :: "r"(va), "r"(log2size));
}

/*
 * Generate a ptc.l instruction.
 */
static __inline void
ia64_ptc_l(u_int64_t va, u_int64_t log2size)
{
	__asm __volatile("ptc.l %0,%1;; srlz.d;;" :: "r"(va), "r"(log2size));
}

/*
 * Read the value of psr.
 */
static __inline u_int64_t
ia64_get_psr(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=psr;;" : "=r" (result));
	return result;
}

/*
 * Set the value of ar.fpsr
 */
static __inline void
ia64_set_fpsr(u_int64_t v)
{
	__asm __volatile("mov ar.fpsr=%0" :: "r" (v));
}

/*
 * Read the value of ar.k0.
 */
static __inline u_int64_t
ia64_get_k0(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=ar.k0" : "=r" (result));
	return result;
}

/*
 * Read the value of ar.k1.
 */
static __inline u_int64_t
ia64_get_k1(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=ar.k1" : "=r" (result));
	return result;
}

/*
 * Read the value of ar.k2.
 */
static __inline u_int64_t
ia64_get_k2(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=ar.k2" : "=r" (result));
	return result;
}

/*
 * Read the value of ar.k3.
 */
static __inline u_int64_t
ia64_get_k3(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=ar.k3" : "=r" (result));
	return result;
}

/*
 * Read the value of ar.k4.
 */
static __inline u_int64_t
ia64_get_k4(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=ar.k4" : "=r" (result));
	return result;
}

/*
 * Read the value of ar.k5.
 */
static __inline u_int64_t
ia64_get_k5(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=ar.k5" : "=r" (result));
	return result;
}

/*
 * Read the value of ar.k6.
 */
static __inline u_int64_t
ia64_get_k6(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=ar.k6" : "=r" (result));
	return result;
}

/*
 * Read the value of ar.k7.
 */
static __inline u_int64_t
ia64_get_k7(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=ar.k7" : "=r" (result));
	return result;
}

/*
 * Write the value of ar.k0.
 */
static __inline void
ia64_set_k0(u_int64_t v)
{
	__asm __volatile("mov ar.k0=%0" :: "r" (v));
}

/*
 * Write the value of ar.k1.
 */
static __inline void
ia64_set_k1(u_int64_t v)
{
	__asm __volatile("mov ar.k1=%0" :: "r" (v));
}

/*
 * Write the value of ar.k2.
 */
static __inline void
ia64_set_k2(u_int64_t v)
{
	__asm __volatile("mov ar.k2=%0" :: "r" (v));
}

/*
 * Write the value of ar.k3.
 */
static __inline void
ia64_set_k3(u_int64_t v)
{
	__asm __volatile("mov ar.k3=%0" :: "r" (v));
}

/*
 * Write the value of ar.k4.
 */
static __inline void
ia64_set_k4(u_int64_t v)
{
	__asm __volatile("mov ar.k4=%0" :: "r" (v));
}

/*
 * Write the value of ar.k5.
 */
static __inline void
ia64_set_k5(u_int64_t v)
{
	__asm __volatile("mov ar.k5=%0" :: "r" (v));
}

/*
 * Write the value of ar.k6.
 */
static __inline void
ia64_set_k6(u_int64_t v)
{
	__asm __volatile("mov ar.k6=%0" :: "r" (v));
}

/*
 * Write the value of ar.k7.
 */
static __inline void
ia64_set_k7(u_int64_t v)
{
	__asm __volatile("mov ar.k7=%0" :: "r" (v));
}

/*
 * Read the value of ar.itc.
 */
static __inline u_int64_t
ia64_get_itc(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=ar.itc" : "=r" (result));
	return result;
}

/*
 * Read the value of cr.itm.
 */
static __inline u_int64_t
ia64_get_itm(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=cr.itm" : "=r" (result));
	return result;
}

/*
 * Write the value of cr.itm.
 */
static __inline void
ia64_set_itm(u_int64_t v)
{
	__asm __volatile("mov cr.itm=%0" :: "r" (v));
}

/*
 * Write the value of cr.itv.
 */
static __inline void
ia64_set_itv(u_int64_t v)
{
	__asm __volatile("mov cr.itv=%0" :: "r" (v));
}

/*
 * Read the value of cr.lid.
 */
static __inline u_int64_t
ia64_get_lid(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=cr.lid" : "=r" (result));
	return result;
}

/*
 * Read the value of cr.tpr.
 */
static __inline u_int64_t
ia64_get_tpr(void)
{
	u_int64_t result;
	__asm __volatile("mov %0=cr.tpr" : "=r" (result));
	return result;
}

/*
 * Write the value of cr.itv.
 */
static __inline void
ia64_set_tpr(u_int64_t v)
{
	__asm __volatile("mov cr.tpr=%0" :: "r" (v));
}

/*
 * Write a region register.
 */
static __inline void
ia64_set_rr(u_int64_t rrbase, u_int64_t v)
{
	__asm __volatile("mov rr[%0]=%1;; srlz.d;;"
			 :: "r"(rrbase), "r"(v) : "memory");
}

/*
 * Read a CPUID register.
 */
static __inline u_int64_t
ia64_get_cpuid(int i)
{
	u_int64_t result;
	__asm __volatile("mov %0=cpuid[%1]"
			 : "=r" (result) : "r"(i));
	return result;
}

#endif

#endif /* _MACHINE_IA64_CPU_H_ */

