/*
 * Copyright (c) 1996, by Steve Passe
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the developer may NOT be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *	$Id: smptests.h,v 1.19 1997/08/04 17:31:28 fsmp Exp $
 */

#ifndef _MACHINE_SMPTESTS_H_
#define _MACHINE_SMPTESTS_H_


/*
 * Various 'tests in progress' and configuration parameters.
 */


/*
 * Ignore the ipending bits when exiting FAST_INTR() routines.
 */
#define FAST_WITHOUTCPL


/*
 * Use a simplelock to serialize FAST_INTR()s.
 */
#define FAST_SIMPLELOCK


/*
 * Use the new INT passoff algorithm:
 *
 *	int_is_already_active = iactive & (1 << INT_NUMBER);
 *	iactive |= (1 << INT_NUMBER);
 *	if ( int_is_already_active || (try_mplock() == FAIL ) {
 *		mask_apic_int( INT_NUMBER );
 *		ipending |= (1 << INT_NUMBER);
 *		do_eoi();
 *		cleanup_and_iret();
 *	}
 */
#define PEND_INTS


/*
 * Portions of the old TEST_LOPRIO code, back from the grave!
 */
#define GRAB_LOPRIO


/*
 * 1st attempt to use the 'ExtInt' connected 8259 to attach 8254 timer.
 * failing that, attempt to attach 8254 timer via direct APIC pin.
 * failing that, panic!
 *
 */
#define NEW_STRATEGY


/*
 * For emergency fallback, define ONLY if 'NEW_STRATEGY' fails to work.
 * Formerly needed by Tyan Tomcat II and SuperMicro P6DNxxx motherboards.
 *
#define SMP_TIMER_NC
 */


/*
 * Send CPUSTOP IPI for stop/restart of other CPUs on DDB break.
 *
 */
#define CPUSTOP_ON_DDBBREAK
#define VERBOSE_CPUSTOP_ON_DDBBREAK


/*
 * Bracket code/comments relevant to the current 'giant lock' model.
 * Everything is now the 'giant lock' model, but we will use this as
 * we start to "push down" the lock.
 */
#define GIANT_LOCK


/*
 * deal with broken smp_idleloop()
 */
#define IGNORE_IDLEPROCS


/*
 * misc. counters
 *
#define COUNT_XINVLTLB_HITS
#define COUNT_SPURIOUS_INTS
#define COUNT_CSHITS
 */


/**
 * hack to "fake-out" kernel into thinking it is running on a 'default config'
 *
 * value == default type
#define TEST_DEFAULT_CONFIG	6
 */


/*
 * simple test code for IPI interaction, save for future...
 *
#define TEST_TEST1
#define IPI_TARGET_TEST1	1
 */


/*
 * Address of POST hardware port.
 * Defining this enables POSTCODE macros.
 *
#define POST_ADDR		0x80
 */


/*
 * POST hardware macros.
 */
#ifdef POST_ADDR
#define ASMPOSTCODE_INC				\
	pushl	%eax ;				\
	movl	_current_postcode, %eax ;	\
	incl	%eax ;				\
	andl	$0xff, %eax ;			\
	movl	%eax, _current_postcode ;	\
	outb	%al, $POST_ADDR ;		\
	popl	%eax

/*
 * Overwrite the current_postcode value.
 */
#define ASMPOSTCODE(X)				\
	pushl	%eax ;				\
	movl	$X, %eax ;			\
	movl	%eax, _current_postcode ;	\
	outb	%al, $POST_ADDR ;		\
	popl	%eax

/*
 * Overwrite the current_postcode low nibble.
 */
#define ASMPOSTCODE_LO(X)			\
	pushl	%eax ;				\
	movl	_current_postcode, %eax ;	\
	andl	$0xf0, %eax ;			\
	orl	$X, %eax ;			\
	movl	%eax, _current_postcode ;	\
	outb	%al, $POST_ADDR ;		\
	popl	%eax

/*
 * Overwrite the current_postcode high nibble.
 */
#define ASMPOSTCODE_HI(X)			\
	pushl	%eax ;				\
	movl	_current_postcode, %eax ;	\
	andl	$0x0f, %eax ;			\
	orl	$(X<<4), %eax ;			\
	movl	%eax, _current_postcode ;	\
	outb	%al, $POST_ADDR ;		\
	popl	%eax
#else
#define ASMPOSTCODE_INC
#define ASMPOSTCODE(X)
#define ASMPOSTCODE_LO(X)
#define ASMPOSTCODE_HI(X)
#endif /* POST_ADDR */


/*
 * these are all temps for debugging...
 *
#define GUARD_INTS
 */

/*
 * This macro traps unexpected INTs to a specific CPU, eg. GUARD_CPU.
 */
#ifdef GUARD_INTS
#define GUARD_CPU	1
#define MAYBE_PANIC(irq_num)		\
	cmpl	$GUARD_CPU, _cpuid ;	\
	jne	9f ;			\
	cmpl	$1, _ok_test1 ;		\
	jne	9f ;			\
	pushl	lapic_isr3 ;		\
	pushl	lapic_isr2 ;		\
	pushl	lapic_isr1 ;		\
	pushl	lapic_isr0 ;		\
	pushl	lapic_irr3 ;		\
	pushl	lapic_irr2 ;		\
	pushl	lapic_irr1 ;		\
	pushl	lapic_irr0 ;		\
	pushl	$irq_num ;		\
	pushl	_cpuid ;		\
	pushl	$panic_msg ;		\
	call	_printf ;		\
	addl	$44, %esp ;		\
9:
#else
#define MAYBE_PANIC(irq_num)
#endif /* GUARD_INTS */

#endif /* _MACHINE_SMPTESTS_H_ */
