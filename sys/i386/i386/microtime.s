/*-
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
 *	$Id: microtime.s,v 1.2 1993/10/16 14:15:08 rgrimes Exp $
 */

#include "asm.h"
#include "../isa/isa.h"
#include "../isa/timerreg.h"

/*
 * Use a higher resolution version of microtime if HZ is not
 * overridden (i.e. it is 100Hz).
 */
#ifndef HZ
ENTRY(microtime)
	pushl %edi
	pushl %esi
	pushl %ebx

	movl $_time,%ebx

	cli				# disable interrupts

	movl	(%ebx),%edi		# sec = time.tv_sec
	movl	4(%ebx),%esi		# usec = time.tv_usec

	movl	$(TIMER_SEL0|TIMER_LATCH),%eax
	outb	%al,$TIMER_MODE		# latch timer 0's counter

	#	
	# Read counter value into ebx, LSB first
	#
	inb	$TIMER_CNTR0,%al
	movzbl	%al,%ebx
	inb	$TIMER_CNTR0,%al
	movzbl	%al,%eax
	sall	$8,%eax
	orl	%eax,%ebx

	#
	# Now check for counter overflow.  This is tricky because the
	# timer chip doesn't let us atomically read the current counter
	# value and the output state (i.e., overflow state).  We have
	# to read the ICU interrupt request register (IRR) to see if the
	# overflow has occured.  Because we lack atomicity, we use
	# the (very accurate) heuristic that we only check for
	# overflow if the value read is close to the interrupt period.
	# E.g., if we just checked the IRR, we might read a non-overflowing
	# value close to 0, experience overflow, then read this overflow
	# from the IRR, and mistakenly add a correction to the "close
	# to zero" value.
	#
	# We compare the counter value to heuristic constant 11890.
	# If the counter value is less than this, we assume the counter
	# didn't overflow between disabling interrupts above and latching
	# the counter value.  For example, we assume that the above 10 or so
	# instructions take less than 11932 - 11890 = 42 microseconds to
	# execute.
	#
	# Otherwise, the counter might have overflowed.  We check for this
	# condition by reading the interrupt request register out of the ICU.
	# If it overflowed, we add in one clock period.
	#
	# The heuristic is "very accurate" because it works 100% if 
	# we're called from an ipl less than the clock.  Otherwise,
	# it might not work.  Currently, only gettimeofday and bpf
	# call microtime so it's not a problem.
	#
	cmpl	$11890,%ebx
	jle	2f
	movl	$0x0a,%eax	# tell ICU we want IRR
	outb	%al,$IO_ICU1

	inb	$IO_ICU1,%al	# read IRR in ICU
	testb	$1,%al		# is a timer interrupt pending?
	je	1f
	addl	$-11932,%ebx	# yes, subtract one clock period
1:
	movl	$0x0b,%eax	# tell ICU we want ISR 
	outb	%al,$IO_ICU1	#   (rest of kernel expects this)
2:
	sti			# enable interrupts

	movl	$11932,%eax	# subtract counter value from 11932 since
	subl	%ebx,%eax	#   it is a count-down value
	imull	$1000,%eax,%eax
	movl	$0,%edx		# zero extend eax for div
	movl	$1193,%ecx
	idivl	%ecx		# convert to usecs: mult by 1000/1193

	addl	%eax,%esi	# add counter usecs to time.tv_usec
	cmpl	$1000000,%esi	# carry in timeval?
	jl	3f
	subl	$1000000,%esi	# adjust usec
	incl	%edi		# bump sec
3:
	movl	16(%esp),%ecx	# load timeval pointer arg
	movl	%edi,(%ecx)	# tvp->tv_sec = sec
	movl	%esi,4(%ecx)	# tvp->tv_usec = usec

	popl	%ebx		# restore regs
	popl	%esi
	popl	%edi
	ret
#endif
