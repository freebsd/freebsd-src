! There was a bug in which a .cranges data hunk could include a hunk of
! code in front of it.  The following illustrates a function (start)
! followed by constants output into .rodata, followed by a function
! (continue), with a case-table (.L173) in it.  The bug included code from
! the start of the function (continue) into the case-table range descriptor.

	.text
	.mode SHmedia
start:
	nop
	.section .rodata
	.long 0xabcdef01
	.long 0x12345678
	.text
continue:
	nop
	nop
	nop
	.align 2
	.align 2
.L173:
	.word 0x0123
	.word 0x5678
	.word 0x1234
	.word 0x5678
	.word 0x1234
	.word 0x5678
	.word 0x1234
	.word 0xfede
	nop
	nop
	nop
	nop
	nop
