        .set noreorder
        .text
        nop
l1:     nop

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4

        .section "foo"
        .word   l1+3

# align section end to 16-byte boundary for easier testing on multiple targets
	.p2align 4
