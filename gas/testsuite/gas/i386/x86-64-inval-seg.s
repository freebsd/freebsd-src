	.text
# All the following should be illegal
	movq	%ds,(%rax)
	movl	%ds,(%rax)
	movq	(%rax),%ds
	movl	(%rax),%ds
