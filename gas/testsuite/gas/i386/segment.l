   1              	.psize 0
   2              	.text
   3              	# test segment reg insns with memory operand
   4 0000 8C18     		movw	%ds,\(%eax\)
   5 0002 8C18     		mov	%ds,\(%eax\)
   6 0004 8E18     		movw	\(%eax\),%ds
   7 0006 8E18     		mov	\(%eax\),%ds
   8              		# Force a good alignment.
   9 0008 00000000 		.p2align	4,0
   9      00000000 
