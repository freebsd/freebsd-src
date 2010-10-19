	.text
# All the following should be illegal
	movl	%ds,(%eax)
	movl	(%eax),%ds
