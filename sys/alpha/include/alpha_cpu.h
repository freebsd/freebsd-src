/* $FreeBSD$ */
/* From: NetBSD: alpha_cpu.h,v 1.15 1997/09/20 19:02:34 mjacob Exp */

/*
 * Copyright (c) 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 *
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef __ALPHA_ALPHA_CPU_H__
#define	__ALPHA_ALPHA_CPU_H__

/*
 * Alpha CPU + OSF/1 PALcode definitions for use by the kernel.
 *
 * Definitions for:
 *
 *	Process Control Block
 *	Interrupt/Exception/Syscall Stack Frame
 *	Processor Status Register
 *	Machine Check Error Summary Register
 *	Machine Check Logout Area
 *	Virtual Memory Management
 *	Kernel Entry Vectors
 *	MMCSR Fault Type Codes
 *	Translation Buffer Invalidation
 *
 * and miscellaneous PALcode operations.
 */


/*
 * Process Control Block definitions [OSF/1 PALcode Specific]
 */

struct alpha_pcb {
	unsigned long	apcb_ksp;	/* kernel stack ptr */
	unsigned long	apcb_usp;	/* user stack ptr */
	unsigned long	apcb_ptbr;	/* page table base reg */
	unsigned int	apcb_cpc;	/* charged process cycles */
	unsigned int	apcb_asn;	/* address space number */
	unsigned long	apcb_unique;	/* process unique value */
	unsigned long	apcb_flags;	/* flags; see below */
	unsigned long	apcb_decrsv0;	/* DEC reserved */
	unsigned long	apcb_decrsv1;	/* DEC reserved */
};

#define	ALPHA_PCB_FLAGS_FEN	0x0000000000000001
#define	ALPHA_PCB_FLAGS_PME	0x4000000000000000

/*
 * Interrupt/Exception/Syscall "Hardware" (really PALcode)
 * Stack Frame definitions
 *
 * These are quadword offsets from the sp on kernel entry, i.e.
 * to get to the value in question you access (sp + (offset * 8)).
 *
 * On syscall entry, A0-A2 aren't written to memory but space
 * _is_ reserved for them.
 */

#define	ALPHA_HWFRAME_PS	0	/* processor status register */
#define	ALPHA_HWFRAME_PC	1	/* program counter */
#define	ALPHA_HWFRAME_GP	2	/* global pointer */
#define	ALPHA_HWFRAME_A0	3	/* a0 */
#define	ALPHA_HWFRAME_A1	4	/* a1 */
#define	ALPHA_HWFRAME_A2	5	/* a2 */

#define	ALPHA_HWFRAME_SIZE	6	/* 6 8-byte words */

/*
 * Processor Status Register [OSF/1 PALcode Specific]
 *
 * Includes user/kernel mode bit, interrupt priority levels, etc.
 */

#define	ALPHA_PSL_USERMODE	0x0008		/* set -> user mode */
#define	ALPHA_PSL_IPL_MASK	0x0007		/* interrupt level mask */

#define	ALPHA_PSL_IPL_0		0x0000		/* all interrupts enabled */
#define	ALPHA_PSL_IPL_SOFT	0x0001		/* software ints disabled */
#define	ALPHA_PSL_IPL_IO	0x0004		/* I/O dev ints disabled */
#define	ALPHA_PSL_IPL_CLOCK	0x0005		/* clock ints disabled */
#define	ALPHA_PSL_IPL_HIGH	0x0006		/* all but mchecks disabled */
#define	ALPHA_PSL_IPL_MCES	0x0007		/* all interrupts disabled */

#define	ALPHA_PSL_MUST_BE_ZERO	0xfffffffffffffff0

/* Convenience constants: what must be set/clear in user mode */
#define	ALPHA_PSL_USERSET	ALPHA_PSL_USERMODE
#define	ALPHA_PSL_USERCLR	(ALPHA_PSL_MUST_BE_ZERO | ALPHA_PSL_IPL_MASK)

/*
 * Interrupt Type Code Definitions [OSF/1 PALcode Specific]
 */
 
#define	ALPHA_INTR_XPROC	0	/* interprocessor interrupt */
#define	ALPHA_INTR_CLOCK	1	/* clock interrupt */
#define	ALPHA_INTR_ERROR	2	/* correctable error or mcheck */
#define	ALPHA_INTR_DEVICE	3	/* device interrupt */
#define	ALPHA_INTR_PERF		4	/* performance counter */
#define	ALPHA_INTR_PASSIVE	5	/* passive release */

/*
 * Machine Check Error Summary Register definitions [OSF/1 PALcode Specific]
 *
 * The following bits are values as read.  On write, _PCE, _SCE, and
 * _MIP are "write 1 to clear."
 */

#define	ALPHA_MCES_IMP							\
    0xffffffff00000000	/* impl. dependent */
#define	ALPHA_MCES_RSVD							\
    0x00000000ffffffe0	/* reserved */
#define	ALPHA_MCES_DSC							\
    0x0000000000000010	/* disable system correctable error reporting */
#define	ALPHA_MCES_DPC							\
    0x0000000000000008	/* disable processor correctable error reporting */
#define	ALPHA_MCES_PCE							\
    0x0000000000000004	/* processor correctable error in progress */
#define	ALPHA_MCES_SCE							\
    0x0000000000000002	/* system correctable error in progress */
#define	ALPHA_MCES_MIP							\
    0x0000000000000001	/* machine check in progress */

/*
 * Machine Check Error Summary Register definitions [OSF/1 PALcode Specific]
 */

struct alpha_logout_area {
	unsigned int	la_frame_size;		/* frame size */
	unsigned int	la_flags;		/* flags; see below */
	unsigned int	la_cpu_offset;		/* offset to cpu area */
	unsigned int	la_system_offset;	/* offset to system area */
};

#define	ALPHA_LOGOUT_FLAGS_RETRY	0x80000000	/* OK to continue */
#define	ALPHA_LOGOUT_FLAGS_SE		0x40000000	/* second error */
#define	ALPHA_LOGOUT_FLAGS_SBZ		0x3fffffff	/* should be zero */

#define	ALPHA_LOGOUT_NOT_BUILT						\
    (struct alpha_logout_area *)0xffffffffffffffff)

#define	ALPHA_LOGOUT_PAL_AREA(lap)					\
    (unsigned long *)((unsigned char *)(lap) + 16)
#define	ALPHA_LOGOUT_PAL_SIZE(lap)					\
    ((lap)->la_cpu_offset - 16)
#define	ALPHA_LOGOUT_CPU_AREA(lap)					\
    (unsigned long *)((unsigned char *)(lap) + (lap)->la_cpu_offset)
#define	ALPHA_LOGOUT_CPU_SIZE(lap)					\
    ((lap)->la_system_offset - (lap)->la_cpu_offset)
#define	ALPHA_LOGOUT_SYSTEM_AREA(lap)					\
    (unsigned long *)((unsigned char *)(lap) + (lap)->la_system_offset)
#define	ALPHA_LOGOUT_SYSTEM_SIZE(lap)					\
    ((lap)->la_frame_size - (lap)->la_system_offset)
	
/*
 * Virtual Memory Management definitions [OSF/1 PALcode Specific]
 *
 * Includes user and kernel space addresses and information,
 * page table entry definitions, etc.
 *
 * NOTE THAT THESE DEFINITIONS MAY CHANGE IN FUTURE ALPHA CPUS!
 */

#define	ALPHA_PGSHIFT		13 /* bits that index within page */
#define ALPHA_PTSHIFT		10 /* bits that index within page tables */
#define	ALPHA_PGBYTES		(1 << ALPHA_PGSHIFT)
#define ALPHA_L3SHIFT		ALPHA_PGSHIFT
#define ALPHA_L2SHIFT		(ALPHA_L3SHIFT+ALPHA_PTSHIFT)
#define ALPHA_L1SHIFT		(ALPHA_L2SHIFT+ALPHA_PTSHIFT)

#define	ALPHA_USEG_BASE		0			/* virtual */
#define	ALPHA_USEG_END		0x000003ffffffffffLL

#define	ALPHA_K0SEG_BASE	0xfffffc0000000000LL	/* direct-mapped */
#define	ALPHA_K0SEG_END		0xfffffdffffffffffLL
#define	ALPHA_K1SEG_BASE	0xfffffe0000000000LL	/* virtual */
#define	ALPHA_K1SEG_END		0xffffffffffffffffLL

#define ALPHA_K0SEG_TO_PHYS(x)	((x) & ~ALPHA_K0SEG_BASE)
#define ALPHA_PHYS_TO_K0SEG(x)	((x) | ALPHA_K0SEG_BASE)

#define	ALPHA_PTE_VALID			0x0001

#define	ALPHA_PTE_FAULT_ON_READ		0x0002
#define	ALPHA_PTE_FAULT_ON_WRITE	0x0004
#define	ALPHA_PTE_FAULT_ON_EXECUTE	0x0008

#define	ALPHA_PTE_ASM			0x0010		/* addr. space match */
#define	ALPHA_PTE_GRANULARITY		0x0060		/* granularity hint */

#define	ALPHA_PTE_PROT			0xff00
#define	ALPHA_PTE_KR			0x0100
#define	ALPHA_PTE_UR			0x0200
#define	ALPHA_PTE_KW			0x1000
#define	ALPHA_PTE_UW			0x2000

#define	ALPHA_PTE_WRITE			(ALPHA_PTE_KW | ALPHA_PTE_UW)

#define	ALPHA_PTE_SOFTWARE		0xffff0000

#define	ALPHA_PTE_PFN			0xffffffff00000000

#define	ALPHA_PTE_TO_PFN(pte)		((u_long)(pte) >> 32)
#define	ALPHA_PTE_FROM_PFN(pfn)		((u_long)(pfn) << 32)

typedef unsigned long alpha_pt_entry_t;

/*
 * Kernel Entry Vectors.  [OSF/1 PALcode Specific]
 */

#define	ALPHA_KENTRY_INT	0
#define	ALPHA_KENTRY_ARITH	1
#define	ALPHA_KENTRY_MM		2
#define	ALPHA_KENTRY_IF		3
#define	ALPHA_KENTRY_UNA	4
#define	ALPHA_KENTRY_SYS	5

/*
 * MMCSR Fault Type Codes.  [OSF/1 PALcode Specific]
 */

#define	ALPHA_MMCSR_INVALTRANS	0
#define	ALPHA_MMCSR_ACCESS	1
#define	ALPHA_MMCSR_FOR		2
#define	ALPHA_MMCSR_FOE		3
#define	ALPHA_MMCSR_FOW		4

/*
 * Instruction Fault Type Codes.  [OSF/1 PALcode Specific]
 */

#define	ALPHA_IF_CODE_BPT	0
#define	ALPHA_IF_CODE_BUGCHK	1
#define	ALPHA_IF_CODE_GENTRAP	2
#define	ALPHA_IF_CODE_FEN	3
#define	ALPHA_IF_CODE_OPDEC	4

/*
 * Translation Buffer Invalidation definitions [OSF/1 PALcode Specific]
 */

#define	ALPHA_TBIA()	alpha_pal_tbi(-2, 0)		/* all TB entries */
#define	ALPHA_TBIAP()	alpha_pal_tbi(-1, 0)		/* all per-process */
#define	ALPHA_TBISI(va)	alpha_pal_tbi(1, (va))		/* ITB entry for va */
#define	ALPHA_TBISD(va)	alpha_pal_tbi(2, (va))		/* DTB entry for va */
#define	ALPHA_TBIS(va)	alpha_pal_tbi(3, (va))		/* all for va */

/*
 * Bits used in the amask instruction [EV56 and later]
 */

#define	ALPHA_AMASK_BWX		0x0001		/* byte/word extension */
#define	ALPHA_AMASK_FIX		0x0002		/* sqrt and f <-> i conversion extension */
#define	ALPHA_AMASK_CIX		0x0004		/* count extension */
#define	ALPHA_AMASK_MVI		0x0100		/* multimedia extension */
#define	ALPHA_AMASK_PRECISE	0x0200		/* Precise arithmetic traps */

/*
 * Chip family IDs returned by implver instruction
 */

#define	ALPHA_IMPLVER_EV4	0		/* LCA/EV4/EV45 */
#define	ALPHA_IMPLVER_EV5	1		/* EV5/EV56/PCA56 */
#define	ALPHA_IMPLVER_EV6	2		/* EV6 */


/*
 * Inlines for Alpha instructions normally inaccessible from C.
 */

static __inline u_int64_t
alpha_amask(u_int64_t mask)
{
	u_int64_t result;
	__asm__ __volatile__ (
		"amask %1,%0"
		: "=r" (result)
		: "r" (mask));
	return result;
}

static __inline unsigned long
alpha_implver(void)
{
	u_int64_t result;
	__asm__ __volatile__ (
		"implver %0"
		: "=r" (result));
	return result;
}

static __inline unsigned long
alpha_rpcc(void)
{
	u_int64_t result;
	__asm__ __volatile__ (
		"rpcc %0"
		: "=r" (result));
	return result;
}

static __inline void
alpha_mb(void)
{
	__asm__ __volatile__ ("mb");
}

static __inline void
alpha_wmb(void)
{
	/*
	 * XXX dfr: NetBSD originally had mb instead of wmb for
	 * alpha_wmb(). I'm not sure why so I'm leaving it alone. I
	 * think it should be safe to use wmb though.
	 */
	__asm__ __volatile__ ("mb");
}

/*
 * Inlines for OSF/1 PALcode operations.
 */

static __inline void
alpha_pal_halt(void)
{
	__asm__ __volatile__ ("call_pal 0x0 #PAL_halt");
}

static __inline void
alpha_pal_cflush(u_int64_t pfn)
{
	register u_int64_t a0 __asm__("$16") = pfn;
	__asm__ __volatile__ (
		"call_pal 0x1 #PAL_cflush"
		:
		: "r" (a0));
}

static __inline void
alpha_pal_draina(void)
{
	__asm__ __volatile__ ("call_pal 0x2 #PAL_draina" : : : "memory");
}

static __inline void
alpha_pal_wripir(u_int64_t ipir)
{
	register u_int64_t a0 __asm__("$16") = ipir;
	__asm__ __volatile__ (
		"call_pal 0xd #PAL_ipir"
		: "=r" (a0)
		: "0" (a0)
		: "$1", "$22", "$23", "$24", "$25");
}

static __inline u_int64_t
alpha_pal_rdmces(void)
{
	register u_int64_t v0 __asm__("$0");
	__asm__ __volatile__ (
		"call_pal 0x10 #PAL_OSF1_rdmces"
		: "=r" (v0)
		:
		: "$1", "$22", "$23", "$24", "$25");
	return v0;
}

static __inline void
alpha_pal_wrmces(u_int64_t mces)
{
	register u_int64_t a0 __asm__("$16") = mces;
	__asm__ __volatile__ (
		"call_pal 0x11 #PAL_wrmces"
		: "=r" (a0)
		: "0" (a0)
		: "$1", "$22", "$23", "$24", "$25");
}

static __inline void
alpha_pal_wrfen(u_int64_t fen)
{
	register u_int64_t a0 __asm__("$16") = fen;
	__asm__ __volatile__ (
		"call_pal 0x2b #PAL_wrfen"
		: "=r" (a0)
		: "0" (a0)
		: "$1", "$22", "$23", "$24", "$25");
}

static __inline void
alpha_pal_wrvptptr(u_int64_t vptptr)
{
	register u_int64_t a0 __asm__("$16") = vptptr;
	__asm__ __volatile__ (
		"call_pal 0x2d #PAL_wrvptptr"
		: "=r" (a0)
		: "0" (a0)
		: "$1", "$22", "$23", "$24", "$25");
}

static __inline u_int64_t
alpha_pal_swpctx(u_int64_t pcb)
{
	register u_int64_t a0 __asm__("$16") = pcb;
	register u_int64_t v0 __asm__("$0");
	__asm__ __volatile__ (
		"call_pal 0x30 #PAL_OSF1_swpctx"
		: "=r" (v0), "=r" (a0)
		: "1" (a0)
		: "$1", "$22", "$23", "$24", "$25", "memory");
	return v0;
}

static __inline void
alpha_pal_wrval(u_int64_t sysvalue)
{
	register u_int64_t a0 __asm__("$16") = sysvalue;
	__asm__ __volatile__ (
		"call_pal 0x31 #PAL_wrval"
		: "=r" (a0)
		: "0" (a0)
		: "$1", "$22", "$23", "$24", "$25");
}

static __inline u_int64_t
alpha_pal_rdval(void)
{
	register u_int64_t v0 __asm__("$0");
	__asm__ __volatile__ (
		"call_pal 0x32 #PAL_OSF1_rdval"
		: "=r" (v0)
		:
		: "$1", "$22", "$23", "$24", "$25");
	return v0;
}

static __inline void
alpha_pal_tbi(u_int64_t op, u_int64_t va)
{
	register u_int64_t a0 __asm__("$16") = op;
	register u_int64_t a1 __asm__("$17") = va;
	__asm__ __volatile__ (
		"call_pal 0x33 #PAL_OSF1_tbi"
		: "=r" (a0), "=r" (a1)
		: "0" (a0), "1" (a1)
		: "$1", "$22", "$23", "$24", "$25");
}

static __inline void
alpha_pal_wrent(void *ent, u_int64_t which)
{
	register u_int64_t a0 __asm__("$16") = (u_int64_t) ent;
	register u_int64_t a1 __asm__("$17") = which;
	__asm__ __volatile__ (
		"call_pal 0x34 #PAL_OSF1_wrent"
		: "=r" (a0), "=r" (a1)
		: "0" (a0), "1" (a1)
		: "$1", "$22", "$23", "$24", "$25");
}

static __inline u_int64_t
alpha_pal_swpipl(u_int64_t newipl)
{
	register u_int64_t a0 __asm__("$16") = newipl;
	register u_int64_t v0 __asm__("$0");
	__asm__ __volatile__ (
		"call_pal 0x35 #PAL_OSF1_swpipl"
		: "=r" (v0), "=r" (a0)
		: "1" (a0)
		: "$1", "$22", "$23", "$24", "$25");
	return v0;
}

static __inline u_int64_t
alpha_pal_rdps(void)
{
	register u_int64_t v0 __asm__("$0");
	__asm__ __volatile__ (
		"call_pal 0x36 #PAL_OSF1_rdps"
		: "=r" (v0)
		:
		: "$1", "$22", "$23", "$24", "$25");
	return v0;
}

static __inline void
alpha_pal_wrusp(u_int64_t usp)
{
	register u_int64_t a0 __asm__("$16") = usp;
	__asm__ __volatile__ (
		"call_pal 0x38 #PAL_wrusp"
		: "=r" (a0)
		: "0" (a0)
		: "$1", "$22", "$23", "$24", "$25");
}

static __inline u_int64_t
alpha_pal_wrperfmon(u_int64_t arg0, u_int64_t arg1)
{
	register u_int64_t v0 __asm__("$0");
	register u_int64_t a0 __asm__("$16") = arg0;
	register u_int64_t a1 __asm__("$17") = arg1;
	__asm__ __volatile__ (
		"call_pal 0x39 #PAL_OSF1_wrperfmon"
		: "=r" (a0), "=r" (a1), "=r" (v0)
		: "0" (a0), "1" (a1)
		: "$1", "$22", "$23", "$24", "$25");
	return v0;
}

static __inline u_int64_t
alpha_pal_rdusp(void)
{
	register u_int64_t v0 __asm__("$0");
	__asm__ __volatile__ (
		"call_pal 0x3a #PAL_OSF1_rdusp"
		: "=r" (v0)
		:
		: "$1", "$22", "$23", "$24", "$25");
	return v0;
}

static __inline u_int64_t
alpha_pal_whami(void)
{
	register u_int64_t v0 __asm__("$0");
	__asm__ __volatile__ (
		"call_pal 0x3c #PAL_OSF1_whami"
		: "=r" (v0)
		:
		: "$1", "$22", "$23", "$24", "$25");
	return v0;
}

static __inline void
alpha_pal_imb(void)
{
	__asm__ __volatile__ ("call_pal 0x86 #PAL_imb");
}

#endif /* __ALPHA_ALPHA_CPU_H__ */
