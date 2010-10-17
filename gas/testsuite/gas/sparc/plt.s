	.text
	.align 4
	call	foo
	 nop
	call	bar + 4
	.data
	.align 4
	.word	%r_plt32(foo)
	.word	%r_plt32(bar + 4)
	.byte	1
	.uaword	%r_plt32(foo)
	.byte	2, 3, 4
