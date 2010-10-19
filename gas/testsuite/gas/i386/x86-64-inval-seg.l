.*: Assembler messages:
.*:3: Error: .*
.*:4: Error: .*
.*:5: Error: .*
.*:6: Error: .*
GAS LISTING .*


   1 [ 	]*	.text
   2 [ 	]*# All the following should be illegal
   3 [ 	]*	movq	%ds,\(%rax\)
   4 [ 	]*	movl	%ds,\(%rax\)
   5 [ 	]*	movq	\(%rax\),%ds
   6 [ 	]*	movl	\(%rax\),%ds
