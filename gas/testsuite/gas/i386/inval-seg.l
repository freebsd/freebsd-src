.*: Assembler messages:
.*:3: Error: .*
.*:4: Error: .*
GAS LISTING .*


   1 [ 	]*	.text
   2 [ 	]*# All the following should be illegal
   3 [ 	]*	movl	%ds,\(%eax\)
   4 [ 	]*	movl	\(%eax\),%ds
