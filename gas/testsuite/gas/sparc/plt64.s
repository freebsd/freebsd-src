	.text
	.align 4
	call	foo
	 nop
	call	bar + 4
	.data
	.align 8
	.xword	%r_plt64(foo)
	.xword	%r_plt64(bar + 4)
        .byte   1
	.uaxword %r_plt64(foo)
	.byte	2, 3, 4
	.word	%r_plt32(bar + 4)
