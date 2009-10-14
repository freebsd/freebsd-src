	.file	"ldexp.c"
#APP
	.ident	"$FreeBSD$"
#NO_APP
	.text
	.p2align 4,,15
.globl ldexp
	.type	ldexp, @function
ldexp:
	pushl	%ebp
	movl	%esp, %ebp
	fldl	8(%ebp)
	fildl	16(%ebp)
	fxch	%st(1)
	popl	%ebp
#APP
	fscale 
#NO_APP
	fstp	%st(1)
	ret
	.size	ldexp, .-ldexp
	.ident	"GCC: (GNU) 4.2.1 20070719  [FreeBSD]"
