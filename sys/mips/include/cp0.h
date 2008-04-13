/*-
 * Copyright (c) 2001, 2005, Juniper Networks, Inc.
 * All rights reserved.
 *
 * Truman Joe, March 2001.
 *
 * cp0.h -- MIPS coprocessor 0 defines.
 *
 *	JNPR: cp0.h,v 1.4 2006/12/02 09:53:40 katta
 * $FreeBSD$
 */

/*
 * This header file is updated from:
 * pfe/include/mips/cp0.h
 */

/*
 * Note: Registers and bit descriptions that do NOT adhere to
 * the MIPS64 descriptions as defined in the "MIPS64
 * Architecture for Programmers, Volume III: The MIPS64
 * Privileged Resource Architecture" document (doc # MD00091)
 * are considered to be processor specific and must have the
 * processor type included in the constant name.
 */

#ifndef _MACHINE_CP0_H_
#define	_MACHINE_CP0_H_

#ifndef ASMINCLUDE

/* Coprocessor 0 set 0 */

#define	C0_INDEX		0
#define	C0_RANDOM		1
#define	C0_ENTRYLO0		2
#define	C0_ENTRYLO1		3
#define	C0_CONTEXT		4
#define	C0_PAGEMASK		5
#define	C0_WIRED		6
#define	R7K_C0_INFO		7
#define	R9K_C0_INFO		7
#define	C0_BADVADDR		8
#define	C0_COUNT		9
#define	C0_ENTRYHI		10
#define	C0_COMPARE		11
#define	C0_STATUS		12
#define	C0_CAUSE		13
#define	C0_EPC			14
#define	C0_PRID			15
#define	C0_CONFIG		16
#define	C0_LLADDR		17
#define	C0_WATCH1		18
#define	C0_WATCH2		19
#define	C0_XCONTEXT		20
#define	R7K_C0_PERFCTL		22
#define	C0_DEBUG		23
#define	R9K_C0_JTAG_DEBUG	23
#define	R7K_C0_WATCHMASK	24
#define	R9K_C0_JTAG_DEPC	24
#define	C0_PERFCOUNT		25
#define	C0_ECC			26
#define	C0_CACHEERR		27
#define	C0_TAGLO		28
#define	C0_TAGHI		29
#define	C0_ERROREPC		30
#define	R9K_C0_JTAG_DESAV	31

/* Coprocessor 0 Set 1 */

#define	R7K_C0_1_IPLLO		18
#define	R7K_C0_1_IPLHI		19
#define	R7K_C0_1_INTCTL		20
#define	R9K_C0_1_TBCTL		22
#define	R9K_C0_1_TBIDX		24
#define	R9K_C0_1_TBOUT		25
#define	R7K_C0_1_DERRADDR0	26
#define	R7K_C0_1_DERRADDR1	27

#else /* ASMINCLUDE */

/* Coprocessor 0 set 0 */

#define	C0_INDEX		$0
#define	C0_RANDOM		$1
#define	C0_ENTRYLO0		$2
#define	C0_ENTRYLO1		$3
#define	C0_CONTEXT		$4
#define	C0_PAGEMASK		$5
#define	C0_WIRED		$6
#define	C0_INFO			$7
#define	C0_BADVADDR		$8
#define	C0_COUNT		$9
#define	C0_ENTRYHI		$10
#define	C0_COMPARE		$11
#define	C0_STATUS		$12
#define	C0_CAUSE		$13
#define	C0_EPC			$14
#define	C0_PRID			$15
#define	C0_CONFIG		$16
#define	C0_LLADDR		$17
#define	C0_WATCH1		$18
#define	C0_WATCH2		$19
#define	C0_XCONTEXT		$20
#define	R7K_C0_PERFCTL		$22
#define	C0_DEBUG		$23
#define	R9K_C0_JTAG_DEBUG	$23
#define	R7K_C0_WATCHMASK	$24
#define	R9K_C0_JTAG_DEPC	$24
#define	C0_PERFCOUNT		$25
#define	C0_ECC			$26
#define	C0_CACHEERR		$27
#define	C0_TAGLO		$28
#define	C0_TAGHI		$29
#define	C0_ERROREPC		$30
#define	R9K_C0_JTAG_DESAV	$31

/* Coprocessor 0 Set 1 */

#define	R7K_C0_1_IPLLO		$18
#define	R7K_C0_1_IPLHI		$19
#define	R7K_C0_1_INTCTL		$20
#define	R7K_C0_1_DERRADDR0	$26
#define	R7K_C0_1_DERRADDR1	$27

#endif /* ASMINCLUDE */

/* CACHE INSTR OPERATIONS */

#define	CACHE_I			0
#define	CACHE_D			1
#define	CACHE_T			2
#define	CACHE_S			3

#define	INDEX_INVL_I		((0 << 2) | CACHE_I)
#define	INDEX_WB_INVL_D		((0 << 2) | CACHE_D)
#define	FLASH_INVL_T		((0 << 2) | CACHE_T)
#define	INDEX_WB_INVL_S		((0 << 2) | CACHE_S)
#define	INDEX_LD_TAG_I		((1 << 2) | CACHE_I)
#define	INDEX_LD_TAG_D		((1 << 2) | CACHE_D)
#define	INDEX_LD_TAG_T		((1 << 2) | CACHE_T)
#define	INDEX_LD_TAG_S		((1 << 2) | CACHE_S)
#define	INDEX_ST_TAG_I		((2 << 2) | CACHE_I)
#define	INDEX_ST_TAG_D		((2 << 2) | CACHE_D)
#define	INDEX_ST_TAG_T		((2 << 2) | CACHE_T)
#define	INDEX_ST_TAG_S		((2 << 2) | CACHE_S)
#define	CREATE_DRTY_EXCL_D	((3 << 2) | CACHE_D)
#define	HIT_INVL_I		((4 << 2) | CACHE_I)
#define	HIT_INVL_D		((4 << 2) | CACHE_D)
#define	HIT_INVL_S		((4 << 2) | CACHE_S)
#define	HIT_WB_INVL_D		((5 << 2) | CACHE_D)
#define	FILL_I			((5 << 2) | CACHE_I)
#define	HIT_WB_INVL_S		((5 << 2) | CACHE_S)
#define	PAGE_INVL_T		((5 << 2) | CACHE_T)
#define	HIT_WB_D		((6 << 2) | CACHE_D)
#define	HIT_WB_I		((6 << 2) | CACHE_I)
#define	HIT_WB_S		((6 << 2) | CACHE_S)

/* CO_CONFIG bit definitions */
#define	R7K_CFG_TE	(0x1 << 12)	/* diff from MIPS64 standard */
#define	R7K_CFG_SE	(0x1 << 3)	/* diff from MIPS64 standard */
#define	R9K_CFG_SE	(0x1 << 3)	/* diff from MIPS64 standard */
#define	R9K_CFG_SC	(0x1 << 31)	/* diff from MIPS64 standard */
#define	CFG_K0_MASK	(0x7 << 0)
#define	CFG_K0_UNC	(0x2 << 0)
#define	CFG_K0_WB	(0x3 << 0)

#define	R9K_CFG_K0_WT	 0x0	/* Write thru */
#define	R9K_CFG_K0_WTWA	 0x1	/* Write thru with write alloc */
#define	R9K_CFG_K0_UNCB  0x2	/* Uncached, blocking */
#define	R9K_CFG_K0_WB	 0x3	/* Write Back */
#define	R9K_CFG_K0_CWBEA 0x4	/* Coherent WB wih exclusive alloc */
#define	R9K_CFG_K0_CWB	 0x5	/* Coherent WB */
#define	R9K_CFG_K0_UNCNB 0x6	/* Uncached, nonblocking */
#define	R9K_CFG_K0_FPC   0x7	/* Fast Packet Cache (bypass 2nd cache) */

/* Special C0_INFO bit descriptions for the R9K processor */
#define	R9K_INFO_AE	(1 << 0)	/* atomic SR_IE for R9K */
#define	R9K_INFO_64_TLB	(1 << 29)/* R9K C0_INFO bit - chip has 64 TLB entries */

/* CO_PAGEMASK bit definitions */

/*
 * These look wierd because the 'size' used is twice what you
 * think it is, but remember that the MIPs TLB maps even odd
 * pages so that you need to acount for the 2x page size
 * R9K supports 256M pages (it has a 16 bit Mask field in the
 * PageMask register).
 */
#define	PAGEMASK_256M	((0x20000000 - 1) & ~0x1fff)	/* R9K only */
#define	PAGEMASK_64M	((0x08000000 - 1) & ~0x1fff)	/* R9K only */
#define	PAGEMASK_16M	((0x02000000 - 1) & ~0x1fff)
#define	PAGEMASK_4M	((0x00800000 - 1) & ~0x1fff)
#define	PAGEMASK_1M	((0x00200000 - 1) & ~0x1fff)
#define	PAGEMASK_256K	((0x00080000 - 1) & ~0x1fff)
#define	PAGEMASK_64K	((0x00020000 - 1) & ~0x1fff)
#define	PAGEMASK_16K	((0x00008000 - 1) & ~0x1fff)
#define	PAGEMASK_4K	((0x00002000 - 1) & ~0x1fff)

#define	R9K_PAGEMASK	0xffff		/* R9K has a 16 bit of PageMask reg */
#define	PAGEMASK_SHIFT	13

/*
 * Cache Coherency Attributes
 * These are different for R7K and R9K
 */
#define	R7K_TLB_COHERENCY_WTNA		0x0
#define	R7K_TLB_COHERENCY_WTWA		0x1
#define	R7K_TLB_COHERENCY_UNCBLK	0x2
#define	R7K_TLB_COHERENCY_WB		0x3
#define	R7K_TLB_COHERENCY_UNCNBLK	0x6
#define	R7K_TLB_COHERENCY_BYPASS	0x7

#define	ENTRYHI_ASID_MASK	0xff
#define	R9K_ENTRYHI_ASID_MASK	0xfff
#define	R7K_ENTRYHI_VPNMASK	0x7ffffff
#define	ENTRYHI_VPNSHIFT	13
#define	ENTRYHI_R_SHIFT		62
#define	R7K_ENTRYLO_PFNMASK	0xffffff
#define	ENTRYLO_PFNSHIFT	6
#define	ENTRYLO_C_SHIFT		3

#define	R9K_ENTRYHI_VPNMASK	0x7ffffff  /* same as r7k */
#define	R9K_ENTRYLO_PFNMASK	0xffffff   /* same as r7k */

#define	R9K_ENTRYLO_C_WTNWA	(0x0 << 3) /* Cache NonCoher WriteThru No Alloc */
#define	R9K_ENTRYLO_C_WTWA	(0x1 << 3) /* Cache NonCoher WriteThru Wr Alloc */
#define	R9K_ENTRYLO_C_UNCACHED	(0x2 << 3) /* Uncached, blocking */
#define	R9K_ENTRYLO_C_CNONC_WB	(0x3 << 3) /* Cacheable NonCoherent WriteBack */
#define	R9K_ENTRYLO_C_CCEXCLU	(0x4 << 3) /* Cacheable Coherent Exclusive */
#define	R9K_ENTRYLO_C_CC_WB	(0x5 << 3) /* Cacheable Coherent Write Back */
#define	R9K_ENTRYLO_C_UNCNBLK	(0x6 << 3) /* Uncached, Nonblocking */
#define	R9K_ENTRYLO_C_FPC	(0x7 << 3) /* Fast Packet Cache */

#define	R7K_ENTRYLO_C_WB	(R7K_TLB_COHERENCY_WB << 3)
#define	R7K_ENTRYLO_C_UNCBLK	(R7K_TLB_COHERENCY_UNCBLK << 3)
#define	R7K_ENTRYLO_C_UNCNBLK	(R7K_TLB_COHERENCY_UNCNBLK << 3)
#define	R7K_ENTRYLO_C_BYPASS	(R7K_TLB_COHERENCY_BYPASS << 3)
#define	ENTRYLO_D		(0x1 << 2)
#define	ENTRYLO_V		(0x1 << 1)
#define	ENTRYLO_G		(0x1 << 0)

/* C0_CAUSE bit definitions */

#define	CAUSE_BD		(0x1 << 31)
#define	CAUSE_CE_SHIFT		28
#define	CAUSE_CE_MASK		3
#define	R7K_CAUSE_IV		(0x1 << 24) /* different from MIPS64 standard */
#define	R9K_CAUSE_IV		(0x1 << 24) /* different from MIPS64 standard */
#define	R9K_CAUSE_W1		(0x1 << 25) /* different from MIPS64 standard */
#define	R9K_CAUSE_W2		(0x1 << 26) /* different from MIPS64 standard */
#define	CAUSE_IV		(0x1 << 23)
#define	CAUSE_WP		(0x1 << 22)
#define	CAUSE_EXCCODE_MASK	0x1f
#define	CAUSE_EXCCODE_SHIFT	2
#define	CAUSE_IP_MASK		0xff
#define	R7K_CAUSE_IP_MASK	0xffff	/* different from MIPS64 standard */
#define	R9K_CAUSE_IP_MASK	0xffff	/* different from MIPS64 standard */
#define	CAUSE_IP_SHIFT		8
#define	CAUSE_IP(num)		(0x1 << ((num) + CAUSE_IP_SHIFT))

#define	CAUSE_EXCCODE_INT	(0 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_MOD	(1 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_TLBL	(2 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_TLBS	(3 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_ADEL	(4 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_ADES	(5 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_IBE	(6 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_DBE	(7 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_SYS	(8 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_BP	(9 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_RI	(10 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_CPU	(11 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_OV	(12 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_TR	(13 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_FPE	(15 << CAUSE_EXCCODE_SHIFT)
#define	R7K_CAUSE_EXCCODE_IWE	(16 << CAUSE_EXCCODE_SHIFT) /* r7k implementation */
#define	CAUSE_EXCCODE_C2E	(18 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_MDMX	(22 << CAUSE_EXCCODE_SHIFT)
#define	R7K_CAUSE_EXCCODE_DWE	(23 << CAUSE_EXCCODE_SHIFT) /* diff from standard */
#define	CAUSE_EXCCODE_WATCH	(23 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_MACH_CHK	(24 << CAUSE_EXCCODE_SHIFT)
#define	CAUSE_EXCCODE_CACHE_ERR	(30 << CAUSE_EXCCODE_SHIFT)

/* C0_PRID bit definitions */
#define	PRID_GET_REV(val)	((val) & 0xff)
#define	PRID_GET_RPID(val)	(((val) >> 8) & 0xff)
#define	R9K_PRID_GET_IMP(val)	(((val) >> 8) & 0xff)
#define	PRID_GET_CID(val)	(((val) >> 16) & 0xff)
#define	PRID_GET_OPT(val)	(((val) >> 24) & 0xff)

/* C0_PRID bit definitions for R9K multiprocessor */
#define	R9K_PRID_GET_PNUM(val)	(((val) >> 24) & 0x07)	/* only 0 & 1 are valid */

/* C0_1_INTCTL bit definitions for R7K and R9K */
#define	R7K_INTCTL_VS_MASK	0x1f
#define	R7K_INTCTL_VS_SHIFT	0
#define	R7K_INTCTL_IMASK	0xff00

/* C0_Watch bit definitions */
#define	WATCHLO_STORE		0x00000001	/* watch stores */
#define	WATCHLO_LOAD		0x00000002	/* watch loads */
#define	WATCHLO_FETCH		0x00000003	/* watch loads */
#define	WATCHLO_PADDR0_MASK	0xfffffff8	/* bits 31:3 of the paddr */

#define	WATCHHI_GLOBAL_BIT	(1 << 30)

#endif /* __MACHINE_CP0_H__ */

/* end of file */
