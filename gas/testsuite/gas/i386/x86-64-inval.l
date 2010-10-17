.*: Assembler messages:
.*:3: Error: .*
.*:4: Error: .*
.*:5: Error: .*
.*:6: Error: .*
.*:7: Error: .*
.*:8: Error: .*
.*:9: Error: .*
.*:10: Error: .*
.*:11: Error: .*
.*:12: Error: .*
.*:13: Error: .*
.*:14: Error: .*
GAS LISTING .*


   1 [ 	]*	.text
   2 [ 	]*# All the following should be illegal for x86-64
   3 [ 	]*calll \*%eax	# 32-bit data size not allowed
   4 [ 	]*calll \*\(%ax\)	# 32-bit data size not allowed
   5 [ 	]*calll \*\(%eax\)	# 32-bit data size not allowed
   6 [ 	]*calll \*\(%r8\)	# 32-bit data size not allowed
   7 [ 	]*calll \*\(%rax\)	# 32-bit data size not allowed
   8 [ 	]*callq \*\(%ax\)	# 32-bit data size not allowed
   9 [ 	]*callw \*\(%ax\)	# no 16-bit addressing
  10 [ 	]*foo:[ 	]*jcxz foo	# No prefix exists to select CX as a counter
  11 [ 	]*popl %eax	# can\'t have 32-bit stack operands
  12 [ 	]*pushl %eax	# can\'t have 32-bit stack operands
  13 [ 	]*pushfl		# can\'t have 32-bit stack operands
  14 [ 	]*popfl		# can\'t have 32-bit stack operands
