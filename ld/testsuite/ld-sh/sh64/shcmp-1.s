! A single SHcompact file, that should link correctly.
	.text
	.global start
start:
	mova next,r0
	nop
next:
	nop
	mov #42,r10

	.section .rodata
	.long start
here:
	.long here
	.long next
