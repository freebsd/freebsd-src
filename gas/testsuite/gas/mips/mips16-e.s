        .set noreorder
        .text
        nop
l1:     nop
1:      nop
        nop

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4

        .section "foo"
        .word   l1
        .word   l1+8
        .word   1b
        .word   1b+3
	.word	g1
	.word	g1+8

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4
