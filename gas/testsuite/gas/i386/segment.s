.psize 0
.text
# test segment reg insns with memory operand
	movw	%ds,(%eax)
	mov	%ds,(%eax)
	movw	(%eax),%ds
	mov	(%eax),%ds
	# Force a good alignment.
	.p2align	4,0
