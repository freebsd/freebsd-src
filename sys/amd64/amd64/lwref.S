#include <machine/asmacros.h>

/*
 * void *lwref_acquire(lwref_t lwr, counter_u64_t *cp)
 * {
 *	void *ptr;
 *
 *	ptr = lwr->ptr;
 *	cp = &lwr->refs;
 *
 *	counter_u64_add(*cp, 1);
 *
 *	return (ptr);
 * }
 */

ENTRY(lwref_acquire)
	mov	(%rdi), %rax
	mov	0x8(%rdi), %rcx
	mov	%rcx, (%rsi)
	mov	$__pcpu, %rdx
	sub	%rdx, %rcx
	mov	$100000, %r10
cycle:
	sub	$1, %r10
	cmpq	$0, %r10
	jne	cycle
	addq	$1, %gs:(%rcx)
.globl	lwref_acquire_ponr
lwref_acquire_ponr:
	ret
END(lwref_acquire)
