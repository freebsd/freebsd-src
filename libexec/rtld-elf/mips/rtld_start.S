/*	$NetBSD: rtld_start.S,v 1.10 2009/12/14 00:41:19 matt Exp $	*/

/*
 * Copyright 1997 Michael L. Hitch <mhitch@montana.edu>
 * Portions copyright 2002 Charles M. Hannum <root@ihack.net>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <machine/asm.h>

.globl _C_LABEL(_rtld_relocate_nonplt_self)
.globl _C_LABEL(_rtld)

#define	PTR_SIZE	(1<<PTR_SCALESHIFT)

/*
 *      a0      stack pointer
 *      a1      rtld cleanup (filled in by dynamic loader)
 *      a2      rtld object (filled in by dynamic loader)
 *      a3      ps_strings
 */
LEAF(rtld_start)
	.frame	sp, 4*PTR_SIZE, ra
	.mask	0x10090000,-PTR_SIZE
	.set	noreorder
	SETUP_GP
	PTR_SUBU sp, 4*PTR_SIZE		/* adjust stack pointer */
	SETUP_GP64(s4, rtld_start)
	SAVE_GP(0)
					/* -> 1*PTR_SIZE(sp) for atexit */
					/* -> 2*PTR_SIZE(sp) for obj_main */
	move	s0, a0			/* save stack pointer from a0 */
	move	s3, a3			/* save ps_strings pointer */

	PTR_LA	a1, 1f
	bal	1f
	 PTR_LA	t0, _C_LABEL(_rtld_relocate_nonplt_self)
1:	PTR_SUBU a1, ra, a1		/* relocbase */
	PTR_LA	a0, _DYNAMIC
	PTR_ADDU t9, a1, t0
	jalr	t9			/* _rtld_relocate_nonplt_self(dynp, relocabase) */
	 PTR_ADDU a0, a1, a0		/* &_DYNAMIC */

	move	a0, s0			/* sp */
	PTR_ADDU a1, sp, 2*PTR_SIZE	/* &our atexit function */
	PTR_ADDU a2, sp, 3*PTR_SIZE	/* obj_main entry */
	PTR_LA	t9, _C_LABEL(_rtld)
	jalr	t9			/* v0 = _rtld(sp, cleanup, objp) */
	 nop

	PTR_L	a1, 2*PTR_SIZE(sp)	/* our atexit function */
	PTR_L	a2, 3*PTR_SIZE(sp)	/* obj_main entry */
	PTR_ADDU sp, 4*PTR_SIZE		/* readjust stack */
	move	a0, s0			/* stack pointer */
	move	t9, v0
	jr	t9			/* _start(sp, cleanup, obj); */
	 move	a3, s3			/* restore ps_strings */

END(rtld_start)

#define	XCALLFRAME_SIZ		(12*SZREG)
#define	XCALLFRAME_RA		(10*SZREG)
#define	XCALLFRAME_GP		(9*SZREG)
#define	XCALLFRAME_S0		(8*SZREG)
#define	XCALLFRAME_A3		(7*SZREG)
#define	XCALLFRAME_A2		(6*SZREG)
#define	XCALLFRAME_A1		(5*SZREG)
#define	XCALLFRAME_A0		(4*SZREG)
#if defined(__mips_n32) || defined(__mips_n64)
#define	XCALLFRAME_A7		(3*SZREG)
#define	XCALLFRAME_A6		(2*SZREG)
#define	XCALLFRAME_A5		(1*SZREG)
#define	XCALLFRAME_A4		(0*SZREG)
#endif

	.globl	_rtld_bind_start
	.ent	_rtld_bind_start
_rtld_bind_start:
	.frame	sp, XCALLFRAME_SIZ, $15
	move	v1, gp			/* save old GP */
#if defined(__mips_o32) || defined(__mips_o64)
	PTR_ADDU t9, 8			/* modify T9 to point at .cpload */
#endif
	SETUP_GP
	PTR_SUBU sp, XCALLFRAME_SIZ	/* save arguments and sp value in stack */
	SETUP_GP64(XCALLFRAME_GP, _rtld_bind_start)
	SAVE_GP(XCALLFRAME_GP)
#if defined(__mips_n32) || defined(__mips_n64)
	REG_S	a4,  XCALLFRAME_A4(sp)
	REG_S	a5,  XCALLFRAME_A5(sp)
	REG_S	a6,  XCALLFRAME_A6(sp)
	REG_S	a7,  XCALLFRAME_A7(sp)
#endif
	REG_S	a0,  XCALLFRAME_A0(sp)
	REG_S	a1,  XCALLFRAME_A1(sp)
	REG_S	a2,  XCALLFRAME_A2(sp)
	REG_S	a3,  XCALLFRAME_A3(sp)
	REG_S	$15,  XCALLFRAME_RA(sp)	/* ra is in t7/t3 */
	REG_S	s0,  XCALLFRAME_S0(sp)
	move	s0, sp

	move	a0, v1			/* old GP */
	PTR_SUBU	a0, a0, 0x7ff0		/* The offset of $gp from the	*/
       					/* beginning of the .got section: */
					/* $gp = .got + 0x7ff0, so	*/
					/* .got = $gp - 0x7ff0		*/
					/* Simple math as you can see.	*/
#if defined(__mips_n64)
	ld	a0, 8(a0)		/* object = pltgot[1] & 0x7fffffff */
#else
	lw	a0, 4(a0)		/* object = pltgot[1] & 0x7fffffff */
#endif
	and	a0, a0, 0x7fffffff
	move	a1, t8			/* symbol index */

	PTR_LA	t9, _C_LABEL(_mips_rtld_bind)
	jalr	t9
	 nop

	move	sp, s0
	REG_L	ra, XCALLFRAME_RA(sp)
	REG_L	s0, XCALLFRAME_S0(sp)
	REG_L	a0, XCALLFRAME_A0(sp)
	REG_L	a1, XCALLFRAME_A1(sp)
	REG_L	a2, XCALLFRAME_A2(sp)
	REG_L	a3, XCALLFRAME_A3(sp)
#if defined(__mips_n32) || defined(__mips_n64)
	REG_L	a4, XCALLFRAME_A4(sp)
	REG_L	a5, XCALLFRAME_A5(sp)
	REG_L	a6, XCALLFRAME_A6(sp)
	REG_L	a7, XCALLFRAME_A7(sp)
#endif
	RESTORE_GP64
	PTR_ADDU sp, XCALLFRAME_SIZ
	move	t9, v0
	jr	t9
	 nop
END(_rtld_bind_start)
