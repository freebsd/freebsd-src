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
 *	$Id: smptests.h,v 1.27 1997/08/31 03:02:19 smp Exp smp $
 */

#ifndef _MACHINE_SMPTESTS_H_
#define _MACHINE_SMPTESTS_H_


/*
 * Various 'tests in progress' and configuration parameters.
 */


/*
 * Debug version of simple_lock.  This will store the CPU id of the
 * holding CPU along with the lock.  When a CPU fails to get the lock
 * it compares its own id to the holder id.  If they are the same it
 * panic()s, as simple locks are binary, and this would cause a deadlock.
 *
 */
#define SL_DEBUG


/*
 * Put FAST_INTR() ISRs at an APIC priority above the regular INTs.
 * Allow the mp_lock() routines to handle FAST interrupts while spinning.
 */
#define FAST_HI


/*
 * There are places in the current kernel where it thinks it has exclusive
 * access to the world by bracketing things with disable_intr()/enable_intr().
 * Now that we started letting multiple CPUs into the kernel this is no
 * longer true.
 *
 * SIMPLE_MPINTRLOCK activates code that uses a simplelock to protect
 * all code suppossedly protected by disable_intr()/enable_intr().
 *
 * RECURSIVE_MPINTRLOCK is an attept to provide a recursive lock, doesn't work!
 *
 * Only define one of these (on neither, but FAST_HI is then problamatic):
#define SIMPLE_MPINTRLOCK
#define RECURSIVE_MPINTRLOCK
 */
#define SIMPLE_MPINTRLOCK


/*  */
#define USE_COMLOCK


/*
 * Regular INTerrupts without the giant lock, NOT READY YET!!!
 *
#define INTR_SIMPLELOCK
 */


/*
 * Ignore the ipending bits when exiting FAST_INTR() routines.
 *
 ***
 * according to Bruce:
 *
 * setsoft*() may set ipending.  setsofttty() is actually used in the
 * FAST_INTR handler in some serial drivers.  This is necessary to get
 * output completions and other urgent events handled as soon as possible.
 * The flag(s) could be set in a variable other than ipending, but they
 * needs to be checked against cpl to decide whether the software interrupt
 * handler can/should run.
 *
 *  (FAST_INTR used to just return
 * in all cases until rev.1.7 of vector.s.  This worked OK provided there
 * were no user-mode CPU hogs.  CPU hogs caused an average latency of 1/2
 * clock tick for output completions...)
 ***
 *
 * So I need to restore cpl handling someday, but AFTER
 *  I finish making spl/cpl MP-safe.
 */
#define FAST_WITHOUTCPL


/*
 * Use a simplelock to serialize FAST_INTR()s.
 * sio.c, and probably other FAST_INTR() drivers, never expected several CPUs
 * to be inside them at once.  Things such as global vars prevent more
 * than 1 thread of execution from existing at once, so we serialize
 * the access of FAST_INTR()s via a simple lock.
 * One optimization on this would be a simple lock per DRIVER, but I'm
 * not sure how to organize that yet...
 */
#define FAST_SIMPLELOCK


/*
 * Portions of the old TEST_LOPRIO code, back from the grave!
 */
#define GRAB_LOPRIO


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
 * Deal with broken smp_idleloop().
 */
#define IGNORE_IDLEPROCS


/*
 * Misc. counters.
 *
#define COUNT_XINVLTLB_HITS
 */


/**
 * Hack to "fake-out" kernel into thinking it is running on a 'default config'.
 *
 * value == default type
#define TEST_DEFAULT_CONFIG	6
 */


/*
 * Simple test code for IPI interaction, save for future...
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
 * These are all temps for debugging...
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
