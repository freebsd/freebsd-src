#include <machine/asm.h>

ENTRY(ldexp)
	pushl	%ebp
	movl	%esp, %ebp
	fldl	8(%ebp)
	fildl	16(%ebp)
	fxch	%st(1)
	popl	%ebp
	fscale 
	fstp	%st(1)
	ret
END(ldexp)
