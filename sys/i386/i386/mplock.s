/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: mplock.s,v 1.11 1997/07/18 21:27:53 fsmp Exp $
 *
 * Functions for locking between CPUs in a SMP system.
 *
 * This is an "exclusive counting semaphore".  This means that it can be
 * free (0xffffffff) or be owned by a CPU (0xXXYYYYYY where XX is CPU-id
 * and YYYYYY is the count).
 *
 * Contrary to most implementations around, this one is entirely atomic:
 * The attempt to seize/release the semaphore and the increment/decrement
 * is done in one atomic operation.  This way we are safe from all kinds
 * of weird reentrancy situations.
 * 
 */

#include <machine/asmacros.h>		/* miscellaneous asm macros */
#include <machine/smptests.h>		/** TEST_LOPRIO */
#include <machine/apic.h>

#include <i386/isa/intr_machdep.h>

/*
 * claim LOW PRIO, ie. accept ALL INTerrupts
 */
#ifdef TEST_LOPRIO

/* location of saved TPR on stack */
#define TPR_TARGET		20(%esp)

/* we assumme that the 'reserved bits' can be written with zeros */
#ifdef CHEAP_TPR

/* after 1st acquire of lock we attempt to grab all hardware INTs */
#define GRAB_HWI \
	movl	$ALLHWI_LEVEL, TPR_TARGET	/* task prio to 'all HWI' */

/* after last release of lock give up LOW PRIO (ie, arbitrate INTerrupts) */
#define ARB_HWI \
	movl	$LOPRIO_LEVEL, lapic_tpr	/* task prio to 'arbitrate' */

#else /** CHEAP_TPR */

#define GRAB_HWI \
	andl	$~APIC_TPR_PRIO, TPR_TARGET	/* task prio to 'all HWI' */

#define ARB_HWI								\
	movl	lapic_tpr, %eax ;		/* TPR */		\
	andl	$~APIC_TPR_PRIO, %eax ;		/* clear TPR field */	\
	orl	$LOPRIO_LEVEL, %eax ;		/* prio to arbitrate */	\
	movl	%eax, lapic_tpr ;		/* set it */		\
  	movl	(%edx), %eax			/* reload %eax with lock */

#endif /** CHEAP_TPR */

#else /** TEST_LOPRIO */

#define GRAB_HWI				/* nop */
#define ARB_HWI					/* nop */

#endif /** TEST_LOPRIO */


	.text
/***********************************************************************
 *  void MPgetlock(unsigned int *lock)
 *  ----------------------------------
 *  Destroys	%eax, %ecx and %edx.
 */

NON_GPROF_ENTRY(MPgetlock)
1:	movl	4(%esp), %edx		/* Get the address of the lock */
  	movl	(%edx), %eax		/* Try to see if we have it already */
	andl	$COUNT_FIELD, %eax	/* - get count */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	orl	%ecx, %eax		/* - combine them */
	movl	%eax, %ecx
	incl	%ecx			/* - new count is one more */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	2f			/* - miss */
	ret
2:	movl	$FREE_LOCK, %eax	/* Assume it's free */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	incl	%ecx			/* - new count is one */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	3f			/* ...do not collect $200 */
	GRAB_HWI			/* 1st acquire, grab hw INTs */
	ret
3:	cmpl	$FREE_LOCK, (%edx)	/* Wait for it to become free */
	jne	3b
	jmp	2b			/* XXX 1b ? */

/***********************************************************************
 *  int MPtrylock(unsigned int *lock)
 *  ---------------------------------
 *  Destroys	%eax, %ecx and %edx.
 *  Returns	1 if lock was successfull
 */

NON_GPROF_ENTRY(MPtrylock)
1:	movl	4(%esp), %edx		/* Get the address of the lock */
  	movl	(%edx), %eax		/* Try to see if we have it already */
	andl	$COUNT_FIELD, %eax	/* - get count */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	orl	%ecx, %eax		/* - combine them */
	movl	%eax, %ecx
	incl	%ecx			/* - new count is one more */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	2f			/* - miss */
	movl	$1, %eax
	ret
2:	movl	$FREE_LOCK, %eax	/* Assume it's free */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	incl	%ecx			/* - new count is one */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	3f			/* ...do not collect $200 */
	GRAB_HWI			/* 1st acquire, grab hw INTs */
	movl	$1, %eax
	ret
3:	movl	$0, %eax
	ret

/***********************************************************************
 *  void MPrellock(unsigned int *lock)
 *  ----------------------------------
 *  Destroys	%eax, %ecx and %edx.
 */

NON_GPROF_ENTRY(MPrellock)
1:	movl	4(%esp), %edx		/* Get the address of the lock */
  	movl	(%edx), %eax		/* - get the value */
	movl	%eax,%ecx
	decl	%ecx			/* - new count is one less */
	testl	$COUNT_FIELD, %ecx	/* - Unless it's zero... */
	jnz	2f
	ARB_HWI				/* last release, arbitrate hw INTs */
	movl	$FREE_LOCK, %ecx	/* - In which case we release it */
2:	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	1b			/* ...do not collect $200 */
	ret

/***********************************************************************
 *  void get_mplock()
 *  -----------------
 *  All registers preserved
 *
 *  Stack (after call to _MPgetlock):
 *	
 *	&mp_lock	 4(%esp)
 *	edx		 8(%esp)
 *	ecx		12(%esp)
 *	EFLAGS		16(%esp)
 *	local APIC TPR	20(%esp)
 *	eax		24(%esp)
 */

NON_GPROF_ENTRY(get_mplock)
	pushl	%eax

	/* block all HW INTs via Task Priority Register */
#ifdef CHEAP_TPR
	pushl	lapic_tpr		/* save current TPR */
	pushfl				/* save current EFLAGS */
	btl	$9, (%esp)		/* test EI bit */
	jc	1f			/* INTs currently enabled */
	movl	$TPR_BLOCK_HWI, lapic_tpr
#else
	movl	lapic_tpr, %eax		/* get current TPR */
	pushl	%eax			/* save current TPR */
	pushfl				/* save current EFLAGS */
	btl	$9, (%esp)		/* test EI bit */
	jc	1f			/* INTs currently enabled */
	andl	$~APIC_TPR_PRIO, %eax	/* clear task priority field */
	orl	$TPR_BLOCK_HWI, %eax	/* only allow IPIs */
	movl	%eax, lapic_tpr
#endif /** CHEAP_TPR */
	sti				/* allow (IPI and only IPI) INTs */
1:

	pushl	%ecx
	pushl	%edx
	pushl	$_mp_lock
	call	_MPgetlock
	add	$4, %esp
	popl	%edx
	popl	%ecx

	popfl				/* restore original EFLAGS */
	popl	lapic_tpr		/* restore TPR */
	popl	%eax			/* restore scratch */
	ret

/***********************************************************************
 *  void try_mplock()
 *  -----------------
 *  reg %eax == 1 if success
 */

NON_GPROF_ENTRY(try_mplock)
	pushl	%ecx
	pushl	%edx
	pushl	$_mp_lock
	call	_MPtrylock
	add	$4, %esp
	popl	%edx
	popl	%ecx
	ret

/***********************************************************************
 *  void rel_mplock()
 *  -----------------
 *  All registers preserved
 */

NON_GPROF_ENTRY(rel_mplock)
	pushl	%eax

	pushl	%ecx
	pushl	%edx
	pushl	$_mp_lock
	call	_MPrellock
	add	$4, %esp
	popl	%edx
	popl	%ecx

	popl	%eax
	ret


	.data
	.globl _mp_lock
	.align  4	/* mp_lock aligned on int boundary */
_mp_lock:	.long	0		
