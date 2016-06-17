/* $Id: trampoline.S,v 1.13 1999/08/04 03:19:15 davem Exp $
 * trampoline.S: SMP cpu boot-up trampoline code.
 *
 * Copyright (C) 1995 David S. Miller (davem@caip.rutgers.edu)
 * Copyright (C) 1998 Jakub Jelinek (jj@sunsite.mff.cuni.cz)
 */

#include <linux/init.h>
#include <asm/cprefix.h>
#include <asm/head.h>
#include <asm/psr.h>
#include <asm/page.h>
#include <asm/asi.h>
#include <asm/ptrace.h>
#include <asm/vaddrs.h>
#include <asm/contregs.h>

	.globl C_LABEL(sun4m_cpu_startup), C_LABEL(__smp4m_processor_id)
	.globl C_LABEL(sun4d_cpu_startup), C_LABEL(__smp4d_processor_id)

	__INIT
	.align 4

/* When we start up a cpu for the first time it enters this routine.
 * This initializes the chip from whatever state the prom left it
 * in and sets PIL in %psr to 15, no irqs.
 */

C_LABEL(sun4m_cpu_startup):
cpu1_startup:
	sethi	%hi(C_LABEL(trapbase_cpu1)), %g3
	b	1f
	 or	%g3, %lo(C_LABEL(trapbase_cpu1)), %g3

cpu2_startup:
	sethi	%hi(C_LABEL(trapbase_cpu2)), %g3
	b	1f
	 or	%g3, %lo(C_LABEL(trapbase_cpu2)), %g3

cpu3_startup:
	sethi	%hi(C_LABEL(trapbase_cpu3)), %g3
	b	1f
	 or	%g3, %lo(C_LABEL(trapbase_cpu3)), %g3

1:
	/* Set up a sane %psr -- PIL<0xf> S<0x1> PS<0x1> CWP<0x0> */
	set	(PSR_PIL | PSR_S | PSR_PS), %g1
	wr	%g1, 0x0, %psr		! traps off though
	WRITE_PAUSE

	/* Our %wim is one behind CWP */
	mov	2, %g1
	wr	%g1, 0x0, %wim
	WRITE_PAUSE

	/* This identifies "this cpu". */
	wr	%g3, 0x0, %tbr
	WRITE_PAUSE

	/* Give ourselves a stack and curptr. */
	set	C_LABEL(current_set), %g5
	srl	%g3, 10, %g4
	and	%g4, 0xc, %g4
	ld	[%g5 + %g4], %g6

	sethi	%hi(TASK_UNION_SIZE - STACKFRAME_SZ), %sp
	or	%sp, %lo(TASK_UNION_SIZE - STACKFRAME_SZ), %sp
	add	%g6, %sp, %sp

	/* Turn on traps (PSR_ET). */
	rd	%psr, %g1
	wr	%g1, PSR_ET, %psr	! traps on
	WRITE_PAUSE

	/* Init our caches, etc. */
	set	C_LABEL(poke_srmmu), %g5
	ld	[%g5], %g5
	call	%g5
	 nop

	/* Start this processor. */
	call	C_LABEL(smp4m_callin)
	 nop

	b,a	smp_do_cpu_idle

	.text
	.align	4

smp_do_cpu_idle:
	call	C_LABEL(cpu_idle)
	 mov	0, %o0

	call	C_LABEL(cpu_panic)
	 nop

C_LABEL(__smp4m_processor_id):
	rd	%tbr, %g2
	srl	%g2, 12, %g2
	and	%g2, 3, %g2
	retl
	 mov	%g1, %o7

C_LABEL(__smp4d_processor_id):
	lda	[%g0] ASI_M_VIKING_TMP1, %g2
	retl
	 mov	%g1, %o7

/* CPUID in bootbus can be found at PA 0xff0140000 */
#define SUN4D_BOOTBUS_CPUID	0xf0140000

	__INIT
	.align	4

C_LABEL(sun4d_cpu_startup):
	/* Set up a sane %psr -- PIL<0xf> S<0x1> PS<0x1> CWP<0x0> */
	set	(PSR_PIL | PSR_S | PSR_PS), %g1
	wr	%g1, 0x0, %psr		! traps off though
	WRITE_PAUSE

	/* Our %wim is one behind CWP */
	mov	2, %g1
	wr	%g1, 0x0, %wim
	WRITE_PAUSE

	/* Set tbr - we use just one trap table. */
	set	C_LABEL(trapbase), %g1
	wr	%g1, 0x0, %tbr
	WRITE_PAUSE

	/* Get our CPU id out of bootbus */
	set	SUN4D_BOOTBUS_CPUID, %g3
	lduba	[%g3] ASI_M_CTL, %g3
	and	%g3, 0xf8, %g3
	srl	%g3, 3, %g1
	sta	%g1, [%g0] ASI_M_VIKING_TMP1

	/* Give ourselves a stack and curptr. */
	set	C_LABEL(current_set), %g5
	srl	%g3, 1, %g4
	ld	[%g5 + %g4], %g6

	sethi	%hi(TASK_UNION_SIZE - STACKFRAME_SZ), %sp
	or	%sp, %lo(TASK_UNION_SIZE - STACKFRAME_SZ), %sp
	add	%g6, %sp, %sp

	/* Turn on traps (PSR_ET). */
	rd	%psr, %g1
	wr	%g1, PSR_ET, %psr	! traps on
	WRITE_PAUSE

	/* Init our caches, etc. */
	set	C_LABEL(poke_srmmu), %g5
	ld	[%g5], %g5
	call	%g5
	 nop

	/* Start this processor. */
	call	C_LABEL(smp4d_callin)
	 nop

	b,a	smp_do_cpu_idle
