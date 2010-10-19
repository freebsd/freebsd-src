# Source file used to test DWARF2 information for MIPS16.

	.set	mips16

	.text
.Ltext0:
	.p2align 2

	.file	1 "mips16-dwarf2.s"
stuff:
	.loc	1 1 0
	nop
	.loc	1 2 0
	li	$2, 0
	.loc	1 3 0
	li	$2, 0x1234
	.loc	1 4 0
	lw	$2, 0f
	.loc	1 5 0
	lw	$2, 1f
	.loc	1 6 0
	b	0f
	nop
	.loc	1 7 0
	b	1f
	nop
	.loc	1 8 0

	.p2align 8
0:
	.space	2048
1:
	nop
# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4
.Letext0:

	.section .debug_info,"",@progbits
.Ldebug_info0:
	.4byte	.Ledebug_info0 - .L1debug_info0	# length
.L1debug_info0:
	.2byte	2				# version
	.4byte	.Ldebug_abbrev0			# abbrev offset
	.byte	4				# address size
	.uleb128 0x1				# abbrev code
	.4byte	.Ldebug_line0			# DW_AT_stmt_list
	.4byte	.Ltext0				# DW_AT_low_pc
	.4byte	.Letext0			# DW_AT_high_pc
.Ledebug_info0:

	.section .debug_abbrev,"",@progbits
.Ldebug_abbrev0:
	.uleb128 0x1				# abbrev code
	.uleb128 0x11				# DW_TAG_compile_unit
	.byte	0x0				# DW_CHILDREN_no
	.uleb128 0x10				# DW_AT_stmt_list
	.uleb128 0x6				# DW_FORM_data4
	.uleb128 0x11				# DW_AT_low_pc
	.uleb128 0x1				# DW_FORM_addr
	.uleb128 0x12				# DW_AT_high_pc
	.uleb128 0x1				# DW_FORM_addr
	.byte	0x0
	.byte	0x0

	.section .debug_line,"",@progbits
.Ldebug_line0:
