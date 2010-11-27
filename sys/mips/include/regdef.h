/*-
 * Copyright (c) 2001, Juniper Networks, Inc.
 * All rights reserved.
 * Truman Joe, February 2001.
 *
 * regdef.h -- MIPS register definitions.
 *
 *	JNPR: regdef.h,v 1.3 2006/08/07 05:38:57 katta
 * $FreeBSD$
 */

#ifndef _MACHINE_REGDEF_H_
#define	_MACHINE_REGDEF_H_

#include <machine/cdefs.h>		/* For API selection */

#if defined(__ASSEMBLER__)
/* General purpose CPU register names */
#define	zero	$0	/* wired zero */
#define	AT	$at	/* assembler temp */
#define	v0	$2	/* return value */
#define	v1	$3
#define	a0	$4	/* argument registers */
#define	a1	$5
#define	a2	$6
#define	a3	$7
#if defined(__mips_n32) || defined(__mips_n64)
#define	a4	$8
#define	a5	$9
#define	a6	$10
#define	a7	$11
#define	t0	$12	/* Temp regs, not saved accross subroutine calls */
#define	t1	$13
#define	t2	$14
#define	t3	$15
#else
#define	t0	$8	/* caller saved */
#define	t1	$9
#define	t2	$10
#define	t3	$11
#define	t4	$12	/* caller saved - 32 bit env arg reg 64 bit */
#define	t5	$13
#define	t6	$14
#define	t7	$15
#endif
#define	s0	$16	/* callee saved */
#define	s1	$17
#define	s2	$18
#define	s3	$19
#define	s4	$20
#define	s5	$21
#define	s6	$22
#define	s7	$23
#define	t8	$24	/* code generator */
#define	t9	$25
#define	k0	$26	/* kernel temporary */
#define	k1	$27
#define	gp	$28	/* global pointer */
#define	sp	$29	/* stack pointer */
#define	fp	$30	/* frame pointer */
#define	s8	$30	/* callee saved */
#define	ra	$31	/* return address */

/*
 * These are temp registers whose names can be used in either the old
 * or new ABI, although they map to different physical registers.  In
 * the old ABI, they map to t4-t7, and in the new ABI, they map to a4-a7.
 *
 * Because they overlap with the last 4 arg regs in the new ABI, ta0-ta3
 * should be used only when we need more than t0-t3.
 */
#if defined(__mips_n32) || defined(__mips_n64)
#define	ta0	$8
#define	ta1	$9
#define	ta2	$10
#define	ta3	$11
#else
#define	ta0	$12
#define	ta1	$13
#define	ta2	$14
#define	ta3	$15
#endif /* __mips_n32 || __mips_n64 */

#endif /* __ASSEMBLER__ */

#endif /* !_MACHINE_REGDEF_H_ */
