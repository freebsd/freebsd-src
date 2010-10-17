	.text
# All the following should be illegal for x86-64
	calll *%eax	# 32-bit data size not allowed
        calll *(%ax)	# 32-bit data size not allowed
        calll *(%eax)	# 32-bit data size not allowed
        calll *(%r8)	# 32-bit data size not allowed
        calll *(%rax)	# 32-bit data size not allowed
        callq *(%ax)	# 32-bit data size not allowed
        callw *(%ax)	# no 16-bit addressing
foo:	jcxz foo	# No prefix exists to select CX as a counter
        popl %eax	# can't have 32-bit stack operands
        pushl %eax	# can't have 32-bit stack operands
        pushfl		# can't have 32-bit stack operands
	popfl		# can't have 32-bit stack operands
