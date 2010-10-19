.psize 0
.text
# test segment reg insns with memory operand
	movw	%ds,(%rax)
	mov	%ds,(%rax)
	movw	(%rax),%ds
	mov	(%rax),%ds
# test segment reg insns with REX
	movq	%ds,%rax
	movq	%rax,%ds
	# Force a good alignment.
	.p2align	4,0
