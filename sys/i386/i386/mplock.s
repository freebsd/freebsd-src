/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $Id: mplock.s,v 1.20 1997/08/25 21:31:38 bde Exp $
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

#include <machine/asmacros.h>
#include <machine/smptests.h>		/** GRAB_LOPRIO */
#include <machine/apic.h>

#include <i386/isa/intr_machdep.h>


#define GLPROFILE

#ifdef CHEAP_TPR

/* we assumme that the 'reserved bits' can be written with zeros */

#else /* CHEAP_TPR */

#error HEADS UP: this code needs work
/*
 * The APIC doc says that reserved bits must be written with whatever
 * value they currently contain, ie you should:	read, modify, write,
 * instead of just writing new values to the TPR register.  Current
 * silicon seems happy with just writing.  If the behaviour of the
 * silicon changes, all code that access the lapic_tpr must be modified.
 * The last version to contain such code was:
 *   Id: mplock.s,v 1.17 1997/08/10 20:59:07 fsmp Exp
 */

#endif /* CHEAP_TPR */

#ifdef GRAB_LOPRIO
/*
 * Claim LOWest PRIOrity, ie. attempt to grab ALL INTerrupts.
 */

/* location of saved TPR on stack */
#define TPR_TARGET	12(%esp)

/* after 1st acquire of lock we attempt to grab all hardware INTs */
#define GRAB_HWI	movl	$ALLHWI_LEVEL, TPR_TARGET
#define GRAB_HWI_2	movl	$ALLHWI_LEVEL, lapic_tpr /* CHEAP_TPR */

/* after last release of lock give up LOW PRIO (ie, arbitrate INTerrupts) */
#define ARB_HWI		movl	$LOPRIO_LEVEL, lapic_tpr /* CHEAP_TPR */

#else /* GRAB_LOPRIO */

#define GRAB_HWI	/* nop */
#define GRAB_HWI_2	/* nop */
#define ARB_HWI		/* nop */

#endif /* GRAB_LOPRIO */


	.text
/***********************************************************************
 *  void MPgetlock(unsigned int *lock)
 *  ----------------------------------
 *  Destroys	%eax, %ecx and %edx.
 */

NON_GPROF_ENTRY(MPgetlock)
	movl	4(%esp), %edx		/* Get the address of the lock */
1:
	movl	$FREE_LOCK, %eax	/* Assume it's free */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	incl	%ecx			/* - new count is one */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	2f			/* ...do not collect $200 */
#ifdef GLPROFILE
	incl	_gethits2
#endif /* GLPROFILE */
	GRAB_HWI			/* 1st acquire, grab hw INTs */
	ret
2:
  	movl	(%edx), %eax		/* Try to see if we have it already */
	andl	$COUNT_FIELD, %eax	/* - get count */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	orl	%ecx, %eax		/* - combine them */
	movl	%eax, %ecx
	incl	%ecx			/* - new count is one more */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
#ifdef GLPROFILE
	jne	4f			/* - miss */
	incl	_gethits
#else
	jne	3f			/* - miss */
#endif /* GLPROFILE */
	ret
#ifdef GLPROFILE
4:
	incl	_gethits3
#endif /* GLPROFILE */
3:
	cmpl	$FREE_LOCK, (%edx)	/* Wait for it to become free */
	jne	3b
	jmp	1b


/***********************************************************************
 *  int MPtrylock(unsigned int *lock)
 *  ---------------------------------
 *  Destroys	%eax, %ecx and %edx.
 *  Returns	1 if lock was successfull
 */

NON_GPROF_ENTRY(MPtrylock)
	movl	4(%esp), %edx		/* Get the address of the lock */

	movl	$FREE_LOCK, %eax	/* Assume it's free */
	movl	_cpu_lockid, %ecx	/* - get pre-shifted logical cpu id */
	incl	%ecx			/* - new count is one */
	lock
	cmpxchg	%ecx, (%edx)		/* - try it atomically */
	jne	1f			/* ...do not collect $200 */
#ifdef GLPROFILE
	incl	_tryhits2
#endif /* GLPROFILE */
	GRAB_HWI_2			/* 1st acquire, grab hw INTs */
	movl	$1, %eax
	ret
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
#ifdef GLPROFILE
	incl	_tryhits
#endif /* GLPROFILE */
	movl	$1, %eax
	ret
2:
#ifdef GLPROFILE
	incl	_tryhits3
#endif /* GLPROFILE */
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
	ARB_HWI				/* last release, arbitrate hw INTs */
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
 *  Stack (after call to _MPgetlock):
 *	
 *	&mp_lock	 4(%esp)
 *	EFLAGS		 8(%esp)
 *	local APIC TPR	12(%esp)
 *	edx		16(%esp)
 *	ecx		20(%esp)
 *	eax		24(%esp)
 */

NON_GPROF_ENTRY(get_mplock)
	pushl	%eax
	pushl	%ecx
	pushl	%edx

	/* block all HW INTs via Task Priority Register */
	pushl	lapic_tpr		/* save current TPR */
	pushfl				/* save current EFLAGS */
	testl	$(1<<9), (%esp)		/* test EI bit */
	jnz	1f			/* INTs currently enabled */
	movl	$TPR_BLOCK_HWI, lapic_tpr /* CHEAP_TPR */
	sti				/* allow IPI (and only IPI) INTs */
1:
	pushl	$_mp_lock
	call	_MPgetlock
	add	$4, %esp

	popfl				/* restore original EFLAGS */
	popl	lapic_tpr		/* restore TPR */
	popl	%edx
	popl	%ecx
	popl	%eax
	ret

/*
 * Special version of get_mplock that is used during bootstrap when we can't
 * yet enable interrupts of any sort since the APIC isn't online yet.
 *
 * XXX FIXME.. - APIC should be online from the start to simplify IPI's.
 */
NON_GPROF_ENTRY(boot_get_mplock)
	pushl	%eax
	pushl	%ecx
	pushl	%edx

	pushl	$_mp_lock
	call	_MPgetlock
	add	$4, %esp

	popl	%edx
	popl	%ecx
	popl	%eax
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

/***********************************************************************
 *  void get_isrlock()
 *  -----------------
 *  no registers preserved, assummed the calling ISR does!
 *
 *  Stack (after call to _MPgetlock):
 *	
 *	&mp_lock	 4(%esp)
 *	EFLAGS		 8(%esp)
 *	local APIC TPR	12(%esp)
 */

NON_GPROF_ENTRY(get_isrlock)

	/* block all HW INTs via Task Priority Register */
	pushl	lapic_tpr		/* save current TPR */
	pushfl				/* save current EFLAGS */
	movl	$TPR_BLOCK_HWI, lapic_tpr /* CHEAP_TPR */
	sti				/* allow IPI (and only IPI) INTs */

	pushl	$_mp_lock
	call	_MPgetlock
	add	$4, %esp

	popfl				/* restore original EFLAGS */
	popl	lapic_tpr		/* restore TPR */
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
 * FPU locks
 */

NON_GPROF_ENTRY(get_fpu_lock)
	pushl	lapic_tpr
	pushfl
	movl	$TPR_BLOCK_HWI, lapic_tpr /* CHEAP_TPR */
	sti
	pushl	$_mp_lock
	call	_MPgetlock
	add	$4, %esp
	popfl
	popl	lapic_tpr
	ret

#ifdef notneeded
NON_GPROF_ENTRY(try_fpu_lock)
	pushl	$_mp_lock
	call	_MPtrylock
	add	$4, %esp
	ret

NON_GPROF_ENTRY(rel_fpu_lock)
	pushl	$_mp_lock
	call	_MPrellock
	add	$4, %esp
	ret
#endif /* notneeded */


/***********************************************************************
 * align locks
 */

NON_GPROF_ENTRY(get_align_lock)
	pushl	lapic_tpr
	pushfl
	movl	$TPR_BLOCK_HWI, lapic_tpr /* CHEAP_TPR */
	sti
	pushl	$_mp_lock
	call	_MPgetlock
	add	$4, %esp
	popfl
	popl	lapic_tpr
	ret

#ifdef notneeded
NON_GPROF_ENTRY(try_align_lock)
	pushl	$_mp_lock
	call	_MPtrylock
	add	$4, %esp
	ret

NON_GPROF_ENTRY(rel_align_lock)
	pushl	$_mp_lock
	call	_MPrellock
	add	$4, %esp
	ret
#endif /* notneeded */


/***********************************************************************
 * syscall locks
 */

NON_GPROF_ENTRY(get_syscall_lock)
	pushl	lapic_tpr
	pushfl
	movl	$TPR_BLOCK_HWI, lapic_tpr /* CHEAP_TPR */
	sti
	pushl	$_mp_lock
	call	_MPgetlock
	add	$4, %esp
	popfl
	popl	lapic_tpr
	ret

#ifdef notneeded
NON_GPROF_ENTRY(try_syscall_lock)
	pushl	$_mp_lock
	call	_MPtrylock
	add	$4, %esp
	ret

NON_GPROF_ENTRY(rel_syscall_lock)
	pushl	$_mp_lock
	call	_MPrellock
	add	$4, %esp
	ret
#endif /* notneeded */


/***********************************************************************
 * altsyscall locks
 */

NON_GPROF_ENTRY(get_altsyscall_lock)
	pushl	lapic_tpr
	pushfl
	movl	$TPR_BLOCK_HWI, lapic_tpr /* CHEAP_TPR */
	sti
	pushl	$_mp_lock
	call	_MPgetlock
	add	$4, %esp
	popfl
	popl	lapic_tpr
	ret

#ifdef notneeded
NON_GPROF_ENTRY(try_altsyscall_lock)
	pushl	$_mp_lock
	call	_MPtrylock
	add	$4, %esp
	ret

NON_GPROF_ENTRY(rel_altsyscall_lock)
	pushl	$_mp_lock
	call	_MPrellock
	add	$4, %esp
	ret
#endif /* notneeded */


/***********************************************************************
 * 
 */
	.data
	.p2align 2			/* xx_lock aligned on int boundary */

	.globl _mp_lock
_mp_lock:	.long	0		

	.globl _isr_lock
_isr_lock:	.long	0		


#ifdef GLPROFILE
	.globl	_gethits
_gethits:
	.long	0
_gethits2:
	.long	0
_gethits3:
	.long	0

	.globl	_tryhits
_tryhits:
	.long	0
_tryhits2:
	.long	0
_tryhits3:
	.long	0

msg:
	.asciz	"lock hits: 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x, 0x%08x\n"
#endif /* GLPROFILE */
