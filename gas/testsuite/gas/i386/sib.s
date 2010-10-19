#Test the special case of the index bits, 0x4, in SIB.

	.text
foo:
	.byte	0x8B, 0x04, 0x23	# effect is: movl (%ebx), %eax
	.byte	0x8B, 0x04, 0x63	# effect is: movl (%ebx), %eax	
	.byte	0x8B, 0x04, 0xA3	# effect is: movl (%ebx), %eax
	.byte	0x8B, 0x04, 0xE3	# effect is: movl (%ebx), %eax
	nop
	nop
	.p2align	4,0
