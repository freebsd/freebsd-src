	.file	"__fixdfsi.s"
.text
	.align 2
.globl ___fixunsdfsi
___fixunsdfsi:
	pushl	%ebp
	movl	%esp,%ebp
	subl	$12,%esp
	fstcw	-4(%ebp)  
	movw	-4(%ebp),%ax
	orw	$0x0c00,%ax  
	movw	%ax,-2(%ebp) 
	fldcw	-2(%ebp)     
	fldl	8(%ebp)

	fcoml	fbiggestsigned	/* bigger than biggest signed? */
	fstsw	%ax
	sahf
	jnb	1f
	
	fistpl	-12(%ebp)    
	movl	-12(%ebp),%eax
	jmp	2f

1:	fsubl	fbiggestsigned	/* reduce for proper conversion */
	fistpl	-12(%ebp)		/* convert */
	movl	-12(%ebp),%eax
	orl	$0x80000000,%eax	/* restore bias */

2:	fldcw	-4(%ebp)     
	leave
	ret

	fcoml	fbiggestsigned	/* bigger than biggest signed? */
	fstsw	%ax
	sahf
	jnb	1f
	
	fistpl	4(%esp)
	movl	4(%esp),%eax
	ret

1:	fsubl	fbiggestsigned	/* reduce for proper conversion */
	fistpl	4(%esp)		/* convert */
	movl	4(%esp),%eax
	orl	$0x80000000,%eax	/* restore bias */
	ret

	.data
fbiggestsigned:	.double	0r2147483648.0
