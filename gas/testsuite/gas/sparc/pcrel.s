	.text
	.align 4
1:	nop
2:	nop
	.globl foo
foo:	nop

	.data
	.align 32
	.word 0
	.word 1
	.word 1b + 16
	.word %r_disp32(1b + 16)
	.word 2b + 16
	.word %r_disp32(2b + 16)
3:	.word foo
	.word %r_disp32(foo)
	.word foo + 16
	.word %r_disp32(foo + 16)
	.byte %r_disp8(3b)
	.byte %r_disp8(4f)
	.half %r_disp16(3b)
	.half %r_disp16(4f)
	.uaword 2
	.half 0
4:
