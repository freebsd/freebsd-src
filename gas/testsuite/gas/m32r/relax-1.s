; Test relaxation into non-zero offset to different segment.

	.section .branch, "ax",@progbits
	.balign 4
branch:
        bra Work


	.section .text
        .balign 4
DoesNotWork:
	nop
	nop

Work:
	nop
	nop
