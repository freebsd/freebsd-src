#include <machine/asm.h>

	.text

/*
 * uint64_t atomic_cas_64(volatile uint64_t *p, uint64_t cmp, uint64_t v)
 */
ENTRY(atomic_cas_64, 3)
	mov             ar.ccv = r33
	;;
	cmpxchg8.acq    r8 = [r32], r34, ar.ccv
	;;
	br.ret.sptk     rp
END(atomic_cas_64)

/*
 * uint64_t atomic_add_64_nv(volatile uint64_t *p, uint64_t v)
 */
ENTRY(atomic_add_64_nv, 2)
1:
	ld8		r16 = [r32]
	;;
	mov		ar.ccv = r16
	add		r8 = r16, r33
	;;
	cmpxchg8.acq	r17 = [r32], r8, ar.ccv
	;;
	cmp.eq		p6, p7 = r16, r17
(p6)	br.ret.sptk	rp
(p7)	br.cond.spnt	1b
END(atomic_add_64_nv)

/*
 * uint8_t atomic_or_8_nv(volatile uint8_t *p, uint8_t v)
 */
ENTRY(atomic_or_8_nv, 2)
1:
	ld8		r16 = [r32]
	;;
	mov		ar.ccv = r16
	or		r8 = r16, r33
	;;
	cmpxchg1.acq	r17 = [r32], r8, ar.ccv
	;;
	cmp.eq		p6, p7 = r16, r17
(p6)	br.ret.sptk	rp
(p7)	br.cond.spnt	1b
END(atomic_or_8_nv)

ENTRY(membar_producer, 0)
	mf.a
	;;
	br.ret.sptk	rp
END(membar_producer)
