/* -*- Fundamental -*- keep Emacs from f***ing up the formatting */
/*
 * Copyright (c) 1993 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 *	from: Steve McCanne's microtime code
 *	$Id: microtime.s,v 1.29 1997/08/24 00:05:35 fsmp Exp $
 */

#include "opt_cpu.h"

#include <machine/asmacros.h>
#include <machine/param.h>

#include <i386/isa/icu.h>
#include <i386/isa/isa.h>
#include <i386/isa/timerreg.h>

#ifdef SMP
#include <machine/smptests.h>			/** SIMPLE_MPINTRLOCK */
#endif

ENTRY(microtime)

#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
	movl	_i586_ctr_freq, %ecx
	testl	%ecx, %ecx
	jne	pentium_microtime
#else
	xorl	%ecx, %ecx	/* clear ecx */
#endif

	movb	$TIMER_SEL0|TIMER_LATCH, %al	/* prepare to latch */

	pushfl
	cli			/* disable interrupts */
#ifdef SIMPLE_MPINTRLOCK
	pushl	%eax			/* s_lock destroys %eax, %ecx */
	pushl	%ecx
	pushl	$_clock_lock
 	call	_s_lock
	addl	$4, %esp
	popl	%ecx
	popl	%eax
#endif /* SIMPLE_MPINTRLOCK */
	outb	%al, $TIMER_MODE	/* latch timer 0's counter */
	inb	$TIMER_CNTR0, %al	/* read counter value, LSB first */
	movb	%al, %cl
	inb	$TIMER_CNTR0, %al
	movb	%al, %ch

	/*
	 * Now check for counter overflow.  This is tricky because the
	 * timer chip doesn't let us atomically read the current counter
	 * value and the output state (i.e., overflow state).  We have
	 * to read the ICU interrupt request register (IRR) to see if the
	 * overflow has occured.  Because we lack atomicity, we use
	 * the (very accurate) heuristic that we only check for
	 * overflow if the value read is close to the interrupt period.
	 * E.g., if we just checked the IRR, we might read a non-overflowing
	 * value close to 0, experience overflow, then read this overflow
	 * from the IRR, and mistakenly add a correction to the "close
	 * to zero" value.
	 *
	 * We compare the counter value to the prepared overflow threshold.
	 * If the counter value is less than this, we assume the counter
	 * didn't overflow between disabling timer interrupts and latching
	 * the counter value above.  For example, we assume that interrupts
	 * are enabled when we are called (or were disabled just a few
	 * cycles before we are called and that the instructions before the
	 * "cli" are fast) and that the "cli" and "outb" instructions take
	 * less than 10 timer cycles to execute.  The last assumption is
	 * very safe.
	 *
	 * Otherwise, the counter might have overflowed.  We check for this
	 * condition by reading the interrupt request register out of the ICU.
	 * If it overflowed, we add in one clock period.
	 *
	 * The heuristic is "very accurate" because it works 100% if we're
	 * called with interrupts enabled.  Otherwise, it might not work.
	 * Currently, only siointrts() calls us with interrupts disabled, so
	 * the problem can be avoided at some cost to the general case.  The
	 * costs are complications in callers to disable interrupts in
	 * IO_ICU1 and extra reads of the IRR forced by a conservative
	 * overflow threshold.
	 *
	 * In 2.0, we are called at splhigh() from mi_switch(), so we have
	 * to allow for the overflow bit being in ipending instead of in
	 * the IRR.  Our caller may have executed many instructions since
	 * ipending was set, so the heuristic for the IRR is inappropriate
	 * for ipending.  However, we don't need another heuristic, since
	 * the "cli" suffices to lock ipending.
	 */

	movl	_timer0_max_count, %edx	/* prepare for 2 uses */

#ifdef APIC_IO
#if defined(REAL_MCPL)			/* XXX do we need this??? */
	pushl	%ecx			/* s_lock destroys %eax, %ecx */
	CPL_LOCK			/* MP-safe, INTs disabled above */
	popl	%ecx			/* restore %ecx */
	movl	_ipending, %eax
	movl	$0, _cpl_lock		/* s_unlock would destroy %eax */
	testl	%eax, _mask8254		/* is soft timer interrupt pending? */
#else /* REAL_MCPL */
	/** XXX FIXME: take our chances with a race, is this OK? */
	movl	_ipending, %eax
	testl	%eax, _mask8254		/* is soft timer interrupt pending? */
#endif /* REAL_MCPL */
#else
	testb	$IRQ0, _ipending	/* is soft timer interrupt pending? */
#endif /* APIC_IO */
	jne	overflow

	/* Do we have a possible overflow condition? */
	cmpl	_timer0_overflow_threshold, %ecx
	jbe	1f

#ifdef APIC_IO
	movl	lapic_irr1, %eax	/** XXX assumption: IRQ0-24 */
	testl	%eax, _mask8254		/* is hard timer interrupt pending? */
#else
	inb	$IO_ICU1, %al		/* read IRR in ICU */
	testb	$IRQ0, %al		/* is hard timer interrupt pending? */
#endif /* APIC_IO */
	je	1f
overflow:
	subl	%edx, %ecx	/* some intr pending, count timer down through 0 */
1:

	/*
	 * Subtract counter value from max count since it is a count-down value.
	 */
	subl	%ecx, %edx

	/* Adjust for partial ticks. */
	addl	_timer0_prescaler_count, %edx

	/*
	 * To divide by 1.193200, we multiply by 27465 and shift right by 15.
	 *
	 * The multiplier was originally calculated to be
	 *
	 *	2^18 * 1000000 / 1193200 = 219698.
	 *
	 * The frequency is 1193200 to be compatible with rounding errors in
	 * the calculation of the usual maximum count.  2^18 is the largest
	 * power of 2 such that multiplying `i' by it doesn't overflow for i
	 * in the range of interest ([0, 11932 + 5)).  We adjusted the
	 * multiplier a little to minimise the average of
	 *
	 *	fabs(i / 1.1193200 - ((multiplier * i) >> 18))
	 *
	 * for i in the range and then removed powers of 2 to speed up the
	 * multiplication and to avoid overflow for i outside the range
	 * (i may be as high as 2^17 if the timer is programmed to its
	 * maximum maximum count).  The absolute error is less than 1 for
	 * all i in the range.
	 */

#if 0
	imul	$27465, %edx				/* 25 cycles on a 486 */
#else
	leal	(%edx,%edx,2), %eax	/* a = 3	2 cycles on a 486   */
	leal	(%edx,%eax,4), %eax	/* a = 13	2		    */
	movl	%eax, %ecx		/* c = 13	1		    */
	shl	$5, %eax		/* a = 416	2		    */
	addl	%ecx, %eax		/* a = 429	1		    */
	leal	(%edx,%eax,8), %eax	/* a = 3433	2		    */
	leal	(%edx,%eax,8), %eax	/* a = 27465	2 (total 12 cycles) */
#endif /* 0 */
	shr	$15, %eax

common_microtime:
	addl	_time+4, %eax	/* usec += time.tv_sec */
	movl	_time, %edx	/* sec = time.tv_sec */

#ifdef SIMPLE_MPINTRLOCK
	pushl	%eax		/* s_lock destroys %eax, %ecx */
	pushl	$_clock_lock
 	call	_s_unlock
	addl	$4, %esp
	popl	%eax
#endif /* SIMPLE_MPINTRLOCK */
	popfl			/* restore interrupt mask */

	cmpl	$1000000, %eax	/* usec valid? */
	jb	1f
	subl	$1000000, %eax	/* adjust usec */
	incl	%edx		/* bump sec */
1:
	movl	4(%esp), %ecx	/* load timeval pointer arg */
	movl	%edx, (%ecx)	/* tvp->tv_sec = sec */
	movl	%eax, 4(%ecx)	/* tvp->tv_usec = usec */

	ret

#if (defined(I586_CPU) || defined(I686_CPU)) && !defined(SMP)
	ALIGN_TEXT
pentium_microtime:
	pushfl
	cli
	.byte	0x0f, 0x31	/* RDTSC */
	subl	_i586_ctr_bias, %eax
	mull	_i586_ctr_multiplier
	movl	%edx, %eax
	jmp	common_microtime
#endif
