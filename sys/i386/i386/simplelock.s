/*-
 * Copyright (c) 1997, by Steve Passe
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
 *	$Id: simplelock.s,v 1.3 1997/08/24 00:05:37 fsmp Exp $
 */

/*
 * credit to Bruce Evans <bde@zeta.org.au> for help with asm optimization.
 */

#include <machine/asmacros.h>			/* miscellaneous macros */
#include <i386/isa/intr_machdep.h>
	

/*
 * The following impliments the primitives described in i386/i386/param.h
 * necessary for the Lite2 lock manager system.
 * The major difference is that the "volatility" of the lock datum has been
 * pushed down from the various functions to lock_data itself.
 */

/*
 * The simple-lock routines are the primitives out of which the lock
 * package is built. The machine-dependent code must implement an
 * atomic test_and_set operation that indivisibly sets the simple lock
 * to non-zero and returns its old value. It also assumes that the
 * setting of the lock to zero below is indivisible. Simple locks may
 * only be used for exclusive locks.
 * 
 * struct simplelock {
 * 	volatile int	lock_data;
 * };
 */

/*
 * void
 * s_lock_init(struct simplelock *lkp)
 * {
 * 	lkp->lock_data = 0;
 * }
 */
ENTRY(s_lock_init)
	movl	4(%esp), %eax		/* get the address of the lock */
	movl	$0, (%eax)
	ret


/*
 * void
 * s_lock(struct simplelock *lkp)
 * {
 * 	while (test_and_set(&lkp->lock_data))
 * 		continue;
 * }
 *
 * Note:
 *	If the acquire fails we do a loop of reads waiting for the lock to
 *	become free instead of continually beating on the lock with xchgl.
 *	The theory here is that the CPU will stay within its cache until
 *	a write by the other CPU updates it, instead of continually updating
 *	the local cache (and thus causing external bus writes) with repeated
 *	writes to the lock.
 */
ENTRY(s_lock)
	movl	4(%esp), %eax		/* get the address of the lock */
	movl	$1, %ecx
setlock:
	xchgl	%ecx, (%eax)
	testl	%ecx, %ecx
	jz	gotit			/* it was clear, return */
wait:
	cmpl	$0, (%eax)		/* wait to empty */
	jne	wait			/* still set... */
	jmp	setlock			/* empty again, try once more */
gotit:
	ret


/*
 * int
 * s_lock_try(struct simplelock *lkp)
 * {
 * 	return (!test_and_set(&lkp->lock_data));
 * }
 */
ENTRY(s_lock_try)
	movl	4(%esp), %eax		/* get the address of the lock */
	movl	$1, %ecx

	xchgl	%ecx, (%eax)
	testl	%ecx, %ecx
	setz	%al			/* 1 if previous value was 0 */
	movzbl	%al, %eax		/* convert to an int */

	ret


/*
 * void
 * s_unlock(struct simplelock *lkp)
 * {
 * 	lkp->lock_data = 0;
 * }
 */
ENTRY(s_unlock)
	movl	4(%esp), %eax		/* get the address of the lock */
	movl	$0, (%eax)
	ret


#ifdef needed

/*
 * int test_and_set(struct simplelock *lkp);
 */
ENTRY(test_and_set)
	movl	4(%esp), %eax		/* get the address of the lock */
	movl	$1, %ecx

	xchgl	%ecx, (%eax)
	testl	%ecx, %ecx
	setz	%al			/* 1 if previous value was 0 */
	movzbl	%al, %eax		/* convert to an int */

	ret

#endif /* needed */


/*
 * These versions of simple_lock block hardware INTS,
 * making it suitable for regions accessed by both top and bottom levels.
 * This is done by saving the current value of the TPR in a per-cpu global,
 * then taking the lock.  On the way out the lock is released, then the
 * original value of the TPR is restored.
 * Because of this, it must ONLY be used for SHORT, deterministic paths!
 *
 * Note:
 * It would appear to be "bad behaviour" to blindly store a value in
 * ss_tpr, as this could destroy the previous contents.  But since ss_tpr
 * is a per-cpu variable, and its fatal to attempt to acquire a simplelock
 * that you already hold, we get away with it.  This needs to be cleaned
 * up someday...
 */

/*
 * void ss_lock(struct simplelock *lkp)
 */
ENTRY(ss_lock)
	movl	4(%esp), %eax		/* get the address of the lock */
	movl	$1, %ecx		/* value for a held lock */
ssetlock:
	pushl	lapic_tpr		/* save current task priority */
	movl	$TPR_BLOCK_HWI, lapic_tpr	/* block hw INTs */
	xchgl	%ecx, (%eax)		/* compete */
	testl	%ecx, %ecx
	jz	sgotit			/* it was clear, return */
	popl	lapic_tpr		/* previous value while waiting */
swait:
	cmpl	$0, (%eax)		/* wait to empty */
	jne	swait			/* still set... */
	jmp	ssetlock		/* empty again, try once more */
sgotit:
	popl	_ss_tpr			/* save the old task priority */
	ret

/*
 * void ss_unlock(struct simplelock *lkp)
 */
ENTRY(ss_unlock)
	movl	4(%esp), %eax		/* get the address of the lock */
	movl	$0, (%eax)		/* clear the simple lock */
	movl	_ss_tpr, %eax
	movl	%eax, lapic_tpr		/* restore the old task priority */
	ret
