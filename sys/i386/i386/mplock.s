/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: mplock.s,v 1.12 1997/07/30 22:51:11 smp Exp smp $
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

#define	MAYBE_PUSHL_EAX	pushl	%eax
#define	MAYBE_POPL_EAX	popl	%eax


	.text
/***********************************************************************
 *  void MPgetlock(unsigned int *lock)
 *  ----------------------------------
 *  Destroys	%eax, %ecx and %edx.
 */

NON_GPROF_ENTRY(MPgetlock)
	movl	4(%esp), %edx		/* Get the address of the lock */
1:
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
2:
	movl	$FREE_LOCK, %eax	/* Assume it's free */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	incl	%ecx			/* - new count is one */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	je	4f			/* ...do not collect $200 */
3:
	cmpl	$FREE_LOCK, (%edx)	/* Wait for it to become free */
	jne	3b
	jmp	2b			/* XXX 1b ? */
4:
	ret

/***********************************************************************
 *  int MPtrylock(unsigned int *lock)
 *  ---------------------------------
 *  Destroys	%eax, %ecx and %edx.
 *  Returns	1 if lock was successfull
 */

NON_GPROF_ENTRY(MPtrylock)
	movl	4(%esp), %edx		/* Get the address of the lock */
  	movl	(%edx), %eax		/* Try to see if we have it already */
	andl	$COUNT_FIELD, %eax	/* - get count */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	orl	%ecx, %eax		/* - combine them */
	movl	%eax, %ecx
	incl	%ecx			/* - new count is one more */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	1f			/* - miss */
	movl	$1, %eax
	ret
1:
	movl	$FREE_LOCK, %eax	/* Assume it's free */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	incl	%ecx			/* - new count is one */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	2f			/* ...do not collect $200 */
	movl	$1, %eax
	ret
2:
	movl	$0, %eax
	ret

/***********************************************************************
 *  void MPrellock(unsigned int *lock)
 *  ----------------------------------
 *  Destroys	%eax, %ecx and %edx.
 */

NON_GPROF_ENTRY(MPrellock)
	movl	4(%esp), %edx		/* Get the address of the lock */
1:
  	movl	(%edx), %eax		/* - get the value */
	movl	%eax, %ecx
	decl	%ecx			/* - new count is one less */
	testl	$COUNT_FIELD, %ecx	/* - Unless it's zero... */
	jnz	2f
	movl	$FREE_LOCK, %ecx	/* - In which case we release it */
2:
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	1b			/* ...do not collect $200 */
	ret


/***********************************************************************
 *  void get_mplock()
 *  -----------------
 *  All registers preserved
 *
 */

NON_GPROF_ENTRY(get_mplock)
	MAYBE_PUSHL_EAX

	/* block all HW INTs via Task Priority Register */
#ifdef CHEAP_TPR
	pushl	lapic_tpr		/* save current TPR */
	pushfl				/* save current EFLAGS */
	testl	$(1<<9), (%esp)		/* test EI bit */
	jnz	1f			/* INTs currently enabled */
	movl	$TPR_BLOCK_HWI, lapic_tpr
#else
	movl	lapic_tpr, %eax		/* get current TPR */
	pushl	%eax			/* save current TPR */
	pushfl				/* save current EFLAGS */
	testl	$(1<<9), (%esp)		/* test EI bit */
	jnz	1f			/* INTs currently enabled */
	andl	$~APIC_TPR_PRIO, %eax	/* clear task priority field */
	orl	$TPR_BLOCK_HWI, %eax	/* only allow IPIs */
	movl	%eax, lapic_tpr
#endif /** CHEAP_TPR */
	sti				/* allow IPI (and only IPI) INTs */
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
	MAYBE_POPL_EAX
	ret

/***********************************************************************
 *  void get_isrlock()
 *  -----------------
 *  no registers preserved, assummed the calling ISR does!
 *
 */

NON_GPROF_ENTRY(get_isrlock)

	/* block all HW INTs via Task Priority Register */
#ifdef CHEAP_TPR
	pushl	lapic_tpr		/* save current TPR */
	pushfl				/* save current EFLAGS */
	movl	$TPR_BLOCK_HWI, lapic_tpr
#else
	movl	lapic_tpr, %eax		/* get current TPR */
	pushl	%eax			/* save current TPR */
	pushfl				/* save current EFLAGS */
	andl	$~APIC_TPR_PRIO, %eax	/* clear task priority field */
	orl	$TPR_BLOCK_HWI, %eax	/* only allow IPIs */
	movl	%eax, lapic_tpr
#endif /** CHEAP_TPR */
	sti				/* allow IPI (and only IPI) INTs */
1:
	pushl	$_mp_lock
	call	_MPgetlock
	add	$4, %esp

	popfl				/* restore original EFLAGS */
	popl	lapic_tpr		/* restore TPR */
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
 *  void try_isrlock()
 *  -----------------
 *  no registers preserved, assummed the calling ISR does!
 *  reg %eax == 1 if success
 */

NON_GPROF_ENTRY(try_isrlock)
	pushl	$_mp_lock
	call	_MPtrylock
	add	$4, %esp
	ret


/***********************************************************************
 *  void rel_mplock()
 *  -----------------
 *  All registers preserved
 */

NON_GPROF_ENTRY(rel_mplock)
	MAYBE_PUSHL_EAX
	pushl	%ecx
	pushl	%edx
	pushl	$_mp_lock
	call	_MPrellock
	add	$4, %esp
	popl	%edx
	popl	%ecx
	MAYBE_POPL_EAX
	ret

/***********************************************************************
 *  void rel_isrlock()
 *  -----------------
 *  no registers preserved, assummed the calling ISR does!
 */

NON_GPROF_ENTRY(rel_isrlock)
	pushl	$_mp_lock
	call	_MPrellock
	add	$4, %esp
	ret


/***********************************************************************
 * 
 */

	.data
	.globl _mp_lock
	.align  4	/* mp_lock aligned on int boundary */
_mp_lock:	.long	0		
