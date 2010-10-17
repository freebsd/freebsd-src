# Check GNU-specific embedded relocs, for ELF.

	.text
	.set noreorder
	nop
l2:	jal	g1		# R_MIPS_GNU_REL16_S2	g1   -1
	nop
	b	g2		# R_MIPS_GNU_REL16_S2	g2   -1
	nop
	b	g2		# R_MIPS_GNU_REL16_S2	g2   -1
	nop
	jal	l1		# R_MIPS_GNU_REL16_S2	.foo 3F
	nop
	jal	l2		# R_MIPS_GNU_REL16_S2	.text 0  or -9
	nop
	b	l1+8		# R_MIPS_GNU_REL16_S2	.foo 41
	nop
l3:
	b	l2		# R_MIPS_GNU_REL16_S2	.text 0  or -D
	nop
	la	$3,g1-l3	# R_MIPS_GNU_REL_HI16   g1   0
				# R_MIPS_GNU_REL_LO16   g1   C
	la	$3,l1-l3	# R_MIPS_GNU_REL_HI16   .foo 0
				# R_MIPS_GNU_REL_LO16   .foo 114
	la	$3,l2-l3	# -30
	.word	g1		# R_MIPS_32	g1    0
	.word	l1		# R_MIPS_32	.foo  100
	.word	l2		# R_MIPS_32	.text 4
	.word	g1-l3		# R_MIPS_PC32	g1    28
	.word	l1-l3		# R_MIPS_PC32	.foo  12C
	.word	l2-l3		# -30
	.align 3
	.dword	g1		# R_MIPS_64	g1    0
	.dword	l1		# R_MIPS_64	.foo  100
	.dword	l2		# R_MIPS_64	.text 4
	.dword	g1-l3		# R_MIPS_PC64	g1    4C
	.dword	l1-l3		# R_MIPS_PC64	.foo  154
	.dword	l2-l3		# -30
l5:
	b	2f		# R_MIPS_GNU_REL16_S2	.text 32
	b	2f+4		# R_MIPS_GNU_REL16_S2	.text 33
	la	$3,2f-l5	# R_MIPS_GNU_REL_HI16	.text 0
	                        # R_MIPS_GNU_REL_LO16   .text D8
	la	$3,2f+8-l5	# R_MIPS_GNU_REL_HI16	.text 0
	                        # R_MIPS_GNU_REL_LO16   .text E8


	.word	2f		# R_MIPS_32	.text CC
	.word	2f-l5		# R_MIPS_PC32	.text EC  or 34
	.dword	2f		# R_MIPS_64	.text CC
	.dword	2f-l5		# R_MIPS_PC64	.text F8  or 34
	nop
2:				# at address 0xCC.
	b	2b		# R_MIPS_GNU_REL16_S2	.text 32
	b	2b+4		# R_MIPS_GNU_REL16_S2	.text 33
	la	$3,2b-l5	# R_MIPS_GNU_REL_HI16	.text 0
				# R_MIPS_GNU_REL_LO16	.text 10C
	la	$3,2b+8-l5	# R_MIPS_GNU_REL_HI16	.text 0
				# R_MIPS_GNU_REL_LO16	.text 11C
	.word	2b		# R_MIPS_32	.text CC
	.word	2b-l5		# R_MIPS_PC32	.text 11C  or 34
	nop
	.dword	2b		# R_MIPS_64	.text CC
	.dword	2b-l5		# R_MIPS_PC64	.text 98  or 34

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4

	.section ".foo","ax",@progbits
	nop
l4:	
	la	$3,g1-l4
	la	$3,l1-l4
	la	$3,l2-l4
	la	$3,g1-l4

	dla	$3,g1-l4
	dla	$3,l1-l4
	dla	$3,l2-l4

	.word	g1
	.word	l1
	.word	l2
	.word	g1-l4
	.word	l1-l4
	.word	l2-l4
	.dword	g1
	.dword	l1
	.dword	l2
	.dword	g1-l4
	.dword	l1-l4
	.dword	l2-l4

	la	$3,g1-l4+4
	la	$3,l1-l4+4
	la	$3,l2-l4+4

	dla	$3,g1-l4+4
	dla	$3,l1-l4+4
	dla	$3,l2-l4+4

	.word	g1+4
	.word	l1+4
	.word	l2+4
	.word	g1-l4+4
	.word	l1-l4+4
	.word	l2-l4+4
	.dword	g1+4
	.dword	l1+4
	.dword	l2+4
	.dword	g1-l4+4
	.dword	l1-l4+4
	.dword	l2-l4+4
l1:

	nop

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4
