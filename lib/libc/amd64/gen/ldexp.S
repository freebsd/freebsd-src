#include <machine/asm.h>

ENTRY(ldexp)
	cvtsi2sd	%edi, %xmm1
	movsd	%xmm0, -16(%rsp)
	movsd	%xmm1, -8(%rsp)
	fldl	-8(%rsp)
	fldl	-16(%rsp)
	fscale 
	fstp	%st(1)
	fstpl	-16(%rsp)
	movsd	-16(%rsp), %xmm0
	ret
END(ldexp)
