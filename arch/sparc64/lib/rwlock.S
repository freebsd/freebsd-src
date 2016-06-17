/* $Id: rwlock.S,v 1.4 2000/09/09 00:00:34 davem Exp $
 * rwlocks.S: These things are too big to do inline.
 *
 * Copyright (C) 1999 David S. Miller (davem@redhat.com)
 */

	.text
	.align	64

	.globl	rwlock_impl_begin, rwlock_impl_end

	/* The non-contention read lock usage is 2 cache lines. */

	.globl	__read_lock, __read_unlock
rwlock_impl_begin:
__read_lock: /* %o0 = lock_ptr */
	ldsw		[%o0], %g5
	brlz,pn		%g5, __read_wait_for_writer
4:	 add		%g5, 1, %g7
	cas		[%o0], %g5, %g7
	cmp		%g5, %g7
	bne,pn		%icc, __read_lock
	 membar		#StoreLoad | #StoreStore
99:	retl
	 nop
__read_unlock: /* %o0 = lock_ptr */
	lduw		[%o0], %g5
	sub		%g5, 1, %g7
	cas		[%o0], %g5, %g7
	cmp		%g5, %g7
	be,pt		%xcc, 99b
	 membar		#StoreLoad | #StoreStore
	ba,a,pt		%xcc, __read_unlock

__read_wait_for_writer:
	ldsw		[%o0], %g5
	brlz,pt		%g5, __read_wait_for_writer
	 membar		#LoadLoad
	ba,a,pt		%xcc, 4b
__write_wait_for_any:
	lduw		[%o0], %g5
	brnz,pt		%g5, __write_wait_for_any
	 membar		#LoadLoad
	ba,a,pt		%xcc, 4f

	.align		64
	.globl		__write_unlock
__write_unlock: /* %o0 = lock_ptr */
	membar		#LoadStore | #StoreStore
	retl
	 stw		%g0, [%o0]

	.globl		__write_lock
__write_lock: /* %o0 = lock_ptr */
	sethi		%hi(0x80000000), %g2

1:	lduw		[%o0], %g5
	brnz,pn		%g5, __write_wait_for_any
4:	 or		%g5, %g2, %g7
	cas		[%o0], %g5, %g7

	cmp		%g5, %g7
	be,pt		%icc, 99b
	 membar		#StoreLoad | #StoreStore
	ba,a,pt		%xcc, 1b
rwlock_impl_end:

