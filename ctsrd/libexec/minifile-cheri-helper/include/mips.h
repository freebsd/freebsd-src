/*-
 * Copyright (c) 2011 Robert N. M. Watson
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

#ifndef _CHERIDEMO_MIPS_H_
#define	_CHERIDEMO_MIPS_H_

/*
 * Provide more convenient names for useful qualifiers from gcc/clang.
 */
#define	__aligned__(x)	__attribute__ ((aligned(x)))
#define	__packed__	__attribute__ ((packed))

/*
 * 64-bit MIPS types.
 */
typedef unsigned long	register_t;		/* 64-bit MIPS register */
typedef unsigned long	paddr_t;		/* Physical address */
typedef unsigned long	vaddr_t;		/* Virtual address */

typedef long		ssize_t;
typedef	unsigned long	size_t;

/*
 * Useful integer type names that we can't pick up from the compile-time
 * environment.
 */
typedef unsigned char	u_char;
typedef short		int16_t;
typedef unsigned short	u_short;
typedef unsigned short	uint16_t;
typedef int		int32_t;
typedef unsigned int	u_int;
typedef unsigned int	uint32_t;
typedef long		intmax_t;
typedef long		quad_t;
typedef long		ptrdiff_t;
typedef unsigned long	u_long;
typedef unsigned long	uint64_t;
typedef unsigned long	uintptr_t;
typedef	unsigned long	uintmax_t;
typedef unsigned long	u_quad_t;

#define	NBBY		8	/* Number of bits per byte. */
#define	NULL		((void *)0)

/*
 * Useful addresses on MIPS.
 */
#define	MIPS_BEV0_EXCEPTION_VECTOR	0xffffffff80000180
#define	MIPS_BEV0_EXCEPTION_VECTOR_PTR	((void *)MIPS_BEV0_EXCEPTION_VECTOR)

/*
 * Hard-coded MIPS interrupt numbers.
 */
#define	MIPS_CP0_INTERRUPT_TIMER	7	/* Compare register. */

/*
 * MIPS CP0 status register fields.
 */
#define	MIPS_CP0_STATUS_IE	0x00000001
#define	MIPS_CP0_STATUS_EXL	0x00000002	/* Exception level */
#define	MIPS_CP0_STATUS_ERL	0x00000004	/* Error level */
#define	MIPS_CP0_STATUS_KSU	0x00000018	/* Ring */
#define	MIPS_CP0_STATUS_UX	0x00000020	/* 64-bit userspace */
#define	MIPS_CP0_STATUS_SX	0x00000040	/* 64-bit supervisor */
#define	MIPS_CP0_STATUS_KX	0x00000080	/* 64-bit kernel */
#define	MIPS_CP0_STATUS_IM	0x0000ff00	/* Interrupt mask */
#define	MIPS_CP0_STATUS_DE	0x00010000	/* DS: Disable parity/ECC */
#define	MIPS_CP0_STATUS_CE	0x00020000	/* DS: Disable parity/ECC */
#define	MIPS_CP0_STATUS_CH	0x00040000	/* DS: Cache hit on cache op */
#define	MIPS_CP0_STATUS_RESERVE0	0x00080000	/* DS: Reserved */
#define	MIPS_CP0_STATUS_SR	0x00100000	/* DS: Reset signal */
#define	MIPS_CP0_STATUS_TS	0x00200000	/* DS: TLB shootdown occurred */
#define	MIPS_CP0_STATUS_BEV	0x00400000	/* DS: Boot-time exc. vectors */
#define	MIPS_CP0_STATUS_RESERVE1	0x01800000	/* DS: Reserved */
#define	MIPS_CP0_STATUS_RE	0x02000000	/* Reverse-endian bit */
#define	MIPS_CP0_STATUS_FR	0x04000000	/* Additional FP registers */
#define	MIPS_CP0_STATUS_RP	0x08000000	/* Reduced power */
#define	MIPS_CP0_STATUS_CU0	0x10000000	/* Coprocessor 0 usability */
#define	MIPS_CP0_STATUS_CU1	0x20000000	/* Coprocessor 1 usability */
#define	MIPS_CP0_STATUS_CU2	0x40000000	/* Coprocessor 2 usability */
#define	MIPS_CP0_STATUS_CU3	0x80000000	/* Coprocessor 3 usability */

/*
 * Shift values to extract multi-bit status register fields.
 */
#define	MIPS_CP0_STATUS_IM_SHIFT	8	/* Interrupt mask */

/*
 * Hard-coded MIPS interrupt bits for MIPS_CP0_STATUS_IM.
 */
#define	MIPS_CP0_STATUS_IM_TIMER	(1 << MIPS_CP0_INTERRUPT_TIMER)

/*
 * MIPS CP0 cause register fields.
 */
#define	MIPS_CP0_CAUSE_RESERVE0	0x00000003	/* Reserved bits */
#define	MIPS_CP0_CAUSE_EXCCODE	0x0000007c	/* Exception code */
#define	MIPS_CP0_CAUSE_RESERVE1	0x00000080	/* Reserved bit */
#define	MIPS_CP0_CAUSE_IP	0x0000ff00	/* Interrupt pending */
#define	MIPS_CP0_CAUSE_RESERVE2	0x0fff0000	/* Reserved bits */
#define	MIPS_CP0_CAUSE_CE	0x30000000	/* Coprocessor exception */
#define	MIPS_CP0_CAUSE_RESERVE3	0x40000000	/* Reserved bit */
#define	MIPS_CP0_CAUSE_BD	0x80000000	/* Branch-delay slot */

/*
 * Shift values to extract multi-bit cause register fields.
 */
#define	MIPS_CP0_CAUSE_EXCODE_SHIFT	2	/* Exception code */
#define	MIPS_CP0_CAUSE_IP_SHIFT		8	/* Interrupt pending */
#define	MIPS_CP0_CAUSE_CE_SHIFT		28	/* Coprocessor exception */

/*
 * MIPS exception cause codes.
 */
#define	MIPS_CP0_EXCODE_INT	0	/* Interrupt */
#define	MIPS_CP0_EXCODE_TLBMOD	1	/* TLB modification exception */
#define	MIPS_CP0_EXCODE_TLBL	2	/* TLB load/fetch exception */
#define	MIPS_CP0_EXCODE_TLBS	3	/* TLB store exception */
#define	MIPS_CP0_EXCODE_ADEL	4	/* Address load/fetch exception */
#define	MIPS_CP0_EXCODE_ADES	5	/* Address store exception */
#define	MIPS_CP0_EXCODE_IBE	6	/* Bus fetch exception */
#define	MIPS_CP0_EXCODE_DBE	7	/* Bus load/store exception */
#define	MIPS_CP0_EXCODE_SYSCALL	8	/* System call exception */
#define	MIPS_CP0_EXCODE_BREAK	9	/* Breakpoint exception */
#define	MIPS_CP0_EXCODE_RI	10	/* Reserved instruction exception */
#define	MIPS_CP0_EXCODE_CPU	11	/* Coprocessor unusable exception */
#define	MIPS_CP0_EXCODE_OV	12	/* Arithmetic overflow exception */
#define	MIPS_CP0_EXCODE_TRAP	13	/* Trap exception */
#define	MIPS_CP0_EXCODE_VCEI	14	/* Virtual coherency inst. exception */
#define	MIPS_CP0_EXCODE_FPE	15	/* Floating point exception */
#define	MIPS_CP0_EXCODE_RES0	16	/* Reserved */
#define	MIPS_CP0_EXCODE_RES1	17	/* Reserved */
#ifdef CP2
#define	MIPS_CP0_EXCODE_C2E	18	/* Capability coprocessor exception */
#else
#define	MIPS_CP0_EXCODE_RES2	18	/* Reserved */
#endif
#define	MIPS_CP0_EXCODE_RES3	19	/* Reserved */
#define	MIPS_CP0_EXCODE_RES4	20	/* Reserved */
#define	MIPS_CP0_EXCODE_RES5	21	/* Reserved */
#define	MIPS_CP0_EXCODE_RES6	22	/* Reserved */
#define	MIPS_CP0_EXCODE_WATCH	23	/* Watchpoint exception */
#define	MIPS_CP0_EXCODE_RES7	24	/* Reserved */
#define	MIPS_CP0_EXCODE_RES8	25	/* Reserved */
#define	MIPS_CP0_EXCODE_RES9	26	/* Reserved */
#define	MIPS_CP0_EXCODE_RES10	27	/* Reserved */
#define	MIPS_CP0_EXCODE_RES11	28	/* Reserved */
#define	MIPS_CP0_EXCODE_RES12	29	/* Reserved */
#define	MIPS_CP0_EXCODE_RES13	30	/* Reserved */
#define	MIPS_CP0_EXCODE_VCED	31	/* Virtual coherency data exception */

/*
 * Hard-coded MIPS interrupt bits from MIPS_CP0_CAUSE_IP.
 */
#define	MIPS_CP0_CAUSE_IP_TIMER		(1 << MIPS_CP0_INTERRUPT_TIMER)

/*
 * MIPS address space layout.
 */
#define	MIPS_XKPHYS_UNCACHED_BASE	0x9000000000000000
#define	MIPS_XKPHYS_CACHED_NC_BASE	0x9800000000000000

static inline vaddr_t
mips_phys_to_cached(paddr_t phys)
{

	return (phys | MIPS_XKPHYS_CACHED_NC_BASE);
}

static inline vaddr_t
mips_phys_to_uncached(paddr_t phys)
{

	return (phys | MIPS_XKPHYS_UNCACHED_BASE);
}

/*
 * Endian conversion routines for use in I/O -- most Altera devices are little
 * endian, but our processor is big endian.
 */
static inline uint16_t
byteswap16(uint16_t v)
{

	return ((v & 0xff00) >> 8 | (v & 0xff) << 8);
}

static inline uint32_t
byteswap32(uint32_t v)
{

	return ((v & 0xff000000) >> 24 | (v & 0x00ff0000) >> 8 |
	    (v & 0x0000ff00) << 8 | (v & 0x000000ff) << 24);
}

/*
 * MIPS simple I/O routines -- arguments are virtual addresses so that the
 * caller can determine required caching properties.
 */
static inline uint32_t
mips_ioread_uint32(vaddr_t vaddr)
{
	uint32_t v;

	__asm__ __volatile__ ("lw %0, 0(%1)" : "=r" (v) : "r" (vaddr));
	return (v);
}

static inline void
mips_iowrite_uint32(vaddr_t vaddr, uint32_t v)
{

	__asm__ __volatile__ ("sw %0, 0(%1)" : : "r" (v), "r" (vaddr));
}

/*
 * Little-endian versions of 32-bit I/O routines.
 */
static inline uint32_t
mips_ioread_uint32le(vaddr_t vaddr)
{

	return (byteswap32(mips_ioread_uint32(vaddr)));
}

static inline void
mips_iowrite_uint32le(vaddr_t vaddr, uint32_t v)
{

	mips_iowrite_uint32(vaddr, byteswap32(v));
}

/*
 * Data structure describing a MIPS register frame.  Assembler routines in
 * init.s know about this layout, so great care should be taken.
 */
struct mips_frame {
	/*
	 * General-purpose MIPS registers.
	 */
	/* No need to preserve $zero. */
	register_t	mf_at, mf_v0, mf_v1;
	register_t	mf_a0, mf_a1, mf_a2, mf_a3, mf_a4, mf_a5, mf_a6, mf_a7;
	register_t	mf_t0, mf_t1, mf_t2, mf_t3;
	register_t	mf_s0, mf_s1, mf_s2, mf_s3, mf_s4, mf_s5, mf_s6, mf_s7;
	register_t	mf_t8, mf_t9;
	/* No need to preserve $k0, $k1. */
	register_t	mf_gp, mf_sp, mf_fp, mf_ra;

	/* Multiply/divide result registers. */
	register_t	mf_hi, mf_lo;

	/* Program counter. */
	register_t	mf_pc;
};

#endif /* _CHERIDEMO_MIPS_H_ */
