	.text

	.p2align 2
code:
	bf      foo
	mov.l   bar, r0
	mov.w   bar, r0
	.globl  foo
foo:
	bra     foo
	nop
	.align 2
	.globl  bar
bar:
	.long   . - foo
	.word   . - foo
	.byte   . - foo
