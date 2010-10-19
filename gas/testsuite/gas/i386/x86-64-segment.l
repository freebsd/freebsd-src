   1              	.psize 0
   2              	.text
   3              	# test segment reg insns with memory operand
   4 0000 8C18     		movw	%ds,\(%rax\)
   5 0002 8C18     		mov	%ds,\(%rax\)
   6 0004 8E18     		movw	\(%rax\),%ds
   7 0006 8E18     		mov	\(%rax\),%ds
   8              	# test segment reg insns with REX
   9 0008 488CD8   		movq	%ds,%rax
  10 000b 488ED8   		movq	%rax,%ds
  11              		# Force a good alignment.
  12 000e 0000     		.p2align	4,0
