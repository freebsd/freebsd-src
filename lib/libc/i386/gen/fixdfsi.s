	.file	"__fixdfsi.s"
.text
	.align 2
.globl ___fixdfsi
___fixdfsi:
	pushl %ebp
	movl %esp,%ebp
	subl	$12,%esp
	fstcw	-4(%ebp)  
	movw	-4(%ebp),%ax
	orw	$0x0c00,%ax  
	movw	%ax,-2(%ebp) 
	fldcw	-2(%ebp)     
	fldl	8(%ebp)
	fistpl	-12(%ebp)    
	fldcw	-4(%ebp)     
	movl	-12(%ebp),%eax
	leave
	ret
