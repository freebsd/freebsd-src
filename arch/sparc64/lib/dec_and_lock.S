/* $Id: dec_and_lock.S,v 1.5 2001/11/18 00:12:56 davem Exp $
 * dec_and_lock.S: Sparc64 version of "atomic_dec_and_lock()"
 *                 using cas and ldstub instructions.
 *
 * Copyright (C) 2000 David S. Miller (davem@redhat.com)
 */
#include <linux/config.h>

#ifndef CONFIG_DEBUG_SPINLOCK
	.text
	.align	64

	/* CAS basically works like this:
	 *
	 * void CAS(MEM, REG1, REG2)
	 * {
	 *   START_ATOMIC();
	 *   if (*(MEM) == REG1) {
	 *     TMP = *(MEM);
	 *     *(MEM) = REG2;
	 *     REG2 = TMP;
	 *   } else
	 *     REG2 = *(MEM);
	 *   END_ATOMIC();
	 * }
	 */

	.globl	atomic_dec_and_lock
atomic_dec_and_lock:	/* %o0 = counter, %o1 = lock */
loop1:	lduw	[%o0], %g5
	subcc	%g5, 1, %g7
	be,pn	%icc, to_zero
	 nop
nzero:	cas	[%o0], %g5, %g7
	cmp	%g5, %g7
	bne,pn	%icc, loop1
	 mov	0, %g1

out:
	membar	#StoreLoad | #StoreStore
	retl
	 mov	%g1, %o0
to_zero:
	ldstub	[%o1], %g3
	brnz,pn	%g3, spin_on_lock
	 membar	#StoreLoad | #StoreStore
loop2:	cas	[%o0], %g5, %g7		/* ASSERT(g7 == 0) */
	cmp	%g5, %g7

	be,pt	%icc, out
	 mov	1, %g1
	lduw	[%o0], %g5
	subcc	%g5, 1, %g7
	be,pn	%icc, loop2
	 nop
	membar	#StoreStore | #LoadStore
	stb	%g0, [%o1]

	b,pt	%xcc, nzero
	 nop
spin_on_lock:
	ldub	[%o1], %g3
	brnz,pt	%g3, spin_on_lock
	 membar	#LoadLoad
	ba,pt	%xcc, to_zero
	 nop
	nop

#endif /* !(CONFIG_DEBUG_SPINLOCK) */
