/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <phk@FreeBSD.org> wrote this file.  As long as you retain this notice you
 * can do whatever you want with this stuff. If we meet some day, and you think
 * this stuff is worth it, you can buy me a beer in return.   Poul-Henning Kamp
 * ----------------------------------------------------------------------------
 *
 * $FreeBSD: src/sys/i386/i386/mplock.s,v 1.29.2.2 2000/05/16 06:58:06 dillon Exp $
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
 */

#include <machine/asmacros.h>
#include <machine/smptests.h>		/** GRAB_LOPRIO */
#include <machine/apic.h>

#define GLPROFILE_NOT

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

/* after 1st acquire of lock we grab all hardware INTs */
#define GRAB_HWI	movl	$ALLHWI_LEVEL, lapic_tpr

/* after last release of lock give up LOW PRIO (ie, arbitrate INTerrupts) */
#define ARB_HWI		movl	$LOPRIO_LEVEL, lapic_tpr /* CHEAP_TPR */

#else /* GRAB_LOPRIO */

#define GRAB_HWI	/* nop */
#define ARB_HWI		/* nop */

#endif /* GRAB_LOPRIO */


	.text

#ifdef SMP 

/***********************************************************************
 *  void MPgetlock_edx(unsigned int *lock : %edx)
 *  ----------------------------------
 *  Destroys	%eax, %ecx.  %edx must hold lock argument.
 *
 *  Grabs hardware interrupts on first aquire.
 *
 *  NOTE: Serialization is not required if we already hold the lock, since
 *  we already hold the lock, nor do we need a locked instruction if we 
 *  already hold the lock.
 */

NON_GPROF_ENTRY(MPgetlock_edx)
1:
	movl	(%edx), %eax		/* Get current contents of lock */
	movl	%eax, %ecx
	andl	$CPU_FIELD,%ecx
	cmpl	_cpu_lockid, %ecx	/* Do we already own the lock? */
	jne	2f
	incl	%eax			/* yes, just bump the count */
	movl	%eax, (%edx)		/* serialization not required */
	ret
2:
	movl	$FREE_LOCK, %eax	/* lock must be free */
	movl	_cpu_lockid, %ecx
	incl	%ecx
	lock
	cmpxchg	%ecx, (%edx)		/* attempt to replace %eax<->%ecx */
#ifdef GLPROFILE
	jne	3f
	incl	_gethits2
#else
	jne	1b
#endif /* GLPROFILE */
	GRAB_HWI			/* 1st acquire, grab hw INTs */
	ret
#ifdef GLPROFILE
3:
	incl	_gethits3
	jmp	1b
#endif

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
	GRAB_HWI			/* 1st acquire, grab hw INTs */
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
 *  void MPrellock_edx(unsigned int *lock : %edx)
 *  ----------------------------------
 *  Destroys	%ecx, argument must be in %edx
 *
 *  SERIALIZATION NOTE!
 *
 *  After a lot of arguing, it turns out that there is no problem with
 *  not having a synchronizing instruction in the MP unlock code.  There
 *  are two things to keep in mind:  First, Intel guarentees that writes
 *  are ordered amoungst themselves.  Second, the P6 is allowed to reorder
 *  reads around writes.  Third, the P6 maintains cache consistency (snoops
 *  the bus).  The second is not an issue since the one read we do is the 
 *  basis for the conditional which determines whether the write will be 
 *  made or not.
 *
 *  Therefore, no synchronizing instruction is required on unlock.  There are
 *  three performance cases:  First, if a single cpu is getting and releasing
 *  the lock the removal of the synchronizing instruction saves approx
 *  200 nS (testing w/ duel cpu PIII 450).  Second, if one cpu is contending
 *  for the lock while the other holds it, the removal of the synchronizing
 *  instruction results in a 700nS LOSS in performance.  Third, if two cpu's
 *  are switching off ownership of the MP lock but not contending for it (the
 *  most common case), this results in a 400nS IMPROVEMENT in performance.
 *
 *  Since our goal is to reduce lock contention in the first place, we have
 *  decided to remove the synchronizing instruction from the unlock code.
 */

NON_GPROF_ENTRY(MPrellock_edx)
  	movl	(%edx), %ecx		/* - get the value */
	decl	%ecx			/* - new count is one less */
	testl	$COUNT_FIELD, %ecx	/* - Unless it's zero... */
	jnz	2f
	ARB_HWI				/* last release, arbitrate hw INTs */
	movl	$FREE_LOCK, %ecx	/* - In which case we release it */
#if 0
	lock
	addl	$0,0(%esp)		/* see note above */
#endif
2:
	movl	%ecx, (%edx)
	ret

/***********************************************************************
 *  void get_mplock()
 *  -----------------
 *  All registers preserved
 *
 *  Stack (after call to _MPgetlock):
 *	
 *	edx		 4(%esp)
 *	ecx		 8(%esp)
 *	eax		12(%esp)
 *
 * Requirements:  Interrupts should be enabled on call so we can take
 *		  IPI's and FAST INTs while we are waiting for the lock
 *		  (else the system may not be able to halt).
 *
 *		  XXX there are still places where get_mplock() is called
 *		  with interrupts disabled, so we have to temporarily reenable
 *		  interrupts.
 *
 * Side effects:  The current cpu will be given ownership of the
 *		  hardware interrupts when it first aquires the lock.
 *
 * Costs:	  Initial aquisition requires the use of a costly locked
 *		  instruction, but recursive aquisition is cheap.  Release
 *		  is very cheap.
 */

NON_GPROF_ENTRY(get_mplock)
	pushl	%eax
	pushl	%ecx
	pushl	%edx
	movl	$_mp_lock, %edx
	pushfl	
	testl   $(1<<9), (%esp)
	jz     2f           
	call	_MPgetlock_edx
	addl	$4,%esp
1:
	popl	%edx
	popl	%ecx
	popl	%eax
	ret
2:
	sti
	call	_MPgetlock_edx
	popfl
	jmp	1b

/*
 * Special version of get_mplock that is used during bootstrap when we can't
 * yet enable interrupts of any sort since the APIC isn't online yet.  We
 * do an endrun around MPgetlock_edx to avoid enabling interrupts.
 *
 * XXX FIXME.. - APIC should be online from the start to simplify IPI's.
 */
NON_GPROF_ENTRY(boot_get_mplock)
	pushl	%eax
	pushl	%ecx
	pushl	%edx
#ifdef GRAB_LOPRIO	
	pushfl
	pushl	lapic_tpr
	cli
#endif
	
	movl	$_mp_lock, %edx
	call	_MPgetlock_edx

#ifdef GRAB_LOPRIO	
	popl	lapic_tpr
	popfl
#endif
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
	pushl	%ecx
	pushl	%edx
	movl	$_mp_lock,%edx
	call	_MPrellock_edx
	popl	%edx
	popl	%ecx
	ret

#endif

/***********************************************************************
 * 
 */
	.data
	.p2align 2			/* xx_lock aligned on int boundary */

#ifdef SMP

	.globl _mp_lock
_mp_lock:	.long	0		

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
#endif /* SMP */
