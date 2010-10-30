	.text
	.align	512
	.globl foo
	.type	foo, @function
foo:
.LFB1:
	subq	$72, %rsp
.LCFI1:
	xorl	%eax, %eax
	movq	%rsp, %rdi
	call	bar@PLT
	addq	$72, %rsp
	ret
.LFE1:
	.size	foo, .-foo
	.globl bar
	.type	bar, @function
bar:
.LFB2:
	subq	$72, %rsp
.LCFI2:
	xorl	%eax, %eax
	movq	%rsp, %rdi
	call	bar@PLT
	addq	$72, %rsp
	ret
.LFE2:
	.size	bar, .-bar
	.section	.eh_frame,"a",@progbits
.Lframe1:
	.long	.LECIE1-.LSCIE1	# Length of Common Information Entry
.LSCIE1:
	.long	0x0	# CIE Identifier Tag
	.byte	0x1	# CIE Version
	.ascii "zR\0"	# CIE Augmentation
	.uleb128 0x1	# CIE Code Alignment Factor
	.sleb128 -8	# CIE Data Alignment Factor
	.byte	0x10	# CIE RA Column
	.uleb128 0x1	# Augmentation size
	.byte	0x1b	# FDE Encoding (pcrel sdata4)
	.byte	0xc	# DW_CFA_def_cfa
	.uleb128 0x7
	.uleb128 0x8
	.byte	0x90	# DW_CFA_offset, column 0x10
	.uleb128 0x1
	.align 8
.LECIE1:
.LSFDE1:
	.long	.LEFDE1-.LASFDE1	# FDE Length
.LASFDE1:
	.long	.LASFDE1-.Lframe1	# FDE CIE offset
	.long	.LFB1-.	# FDE initial location
	.long	.LFE1-.LFB1	# FDE address range
	.uleb128 0x0	# Augmentation size
	.byte	0x1	# DW_CFA_set_loc
	.long	.LCFI1-.
	.byte	0xe	# DW_CFA_def_cfa_offset
	.uleb128 0x50
	.align 8
.LEFDE1:
.Lframe2:
	.long	.LECIE2-.LSCIE2	# Length of Common Information Entry
.LSCIE2:
	.long	0x0	# CIE Identifier Tag
	.byte	0x1	# CIE Version
	.ascii "zR\0"	# CIE Augmentation
	.uleb128 0x1	# CIE Code Alignment Factor
	.sleb128 -8	# CIE Data Alignment Factor
	.byte	0x10	# CIE RA Column
	.uleb128 0x1	# Augmentation size
	.byte	0x1b	# FDE Encoding (pcrel sdata4)
	.byte	0xc	# DW_CFA_def_cfa
	.uleb128 0x7
	.uleb128 0x8
	.byte	0x90	# DW_CFA_offset, column 0x10
	.uleb128 0x1
	.align 8
.LECIE2:
.LSFDE2:
	.long	.LEFDE2-.LASFDE2	# FDE Length
.LASFDE2:
	.long	.LASFDE2-.Lframe2	# FDE CIE offset
	.long	.LFB2-.	# FDE initial location
	.long	.LFE2-.LFB2	# FDE address range
	.uleb128 0x0	# Augmentation size
	.byte	0x1	# DW_CFA_set_loc
	.long	.LCFI2-.
	.byte	0xe	# DW_CFA_def_cfa_offset
	.uleb128 0x50
	.align 8
.LEFDE2:
	.section	.note.GNU-stack,"",@progbits
