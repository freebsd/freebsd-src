	.text
	.globl	foo
	.type	foo, @function
foo:
	push	%ebx
	movl	__GOTT_BASE__, %ebx
	movl	__GOTT_INDEX__(%ecx), %ebx
	movl	x@GOT(%ebx), %eax
	incl	(%eax)
	call	slocal@plt
	call	sglobal@plt
	call	sexternal@plt
	pop	%ebx
	ret
	.size	foo, .-foo

	.type	slocal, @function
slocal:
	ret
	.size	slocal, .-slocal

	.globl	sglobal
	.type	sglobal, @function
sglobal:
	ret
	.size	sglobal, .-sglobal

	.data
	.4byte	slocal

	.comm	x,4,4
