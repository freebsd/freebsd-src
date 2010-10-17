/* Test the alignment pseudo-ops.  */
	.text

	.byte	0xff
	.p2align 1,0

	.byte	0xff
	.p2align 1,1

	.byte	0xff
	.p2align 2,2

	.byte	0xff
	.byte	0xff
	.p2alignw 2,0x0303

	.p2align 3,4
	.byte	0xff
	.byte	0xff
	.byte	0xff
	.byte	0xff
	.p2alignl 3,0x05050505

	.p2align 1,6
	.p2align 1,7

	.byte	0xff
	.p2align 3,8,5
	.byte	9
	.p2align 3,0xa

	.byte	0xff
	.balign	2,0

	.byte	0xff
	.balign	2,1

	.byte	0xff
	.balign	4,2

	.byte	0xff
	.byte	0xff
	.balignw 4,0x0303

	.balign	8,4
	.byte	0xff
	.byte	0xff
	.byte	0xff
	.byte	0xff
	.balignl 8,0x05050505

	.balign 2,6
	.balign 2,7

	.byte	0xff
	.balign	8,8,5
	.byte	9
	.balign	8,0xa

	.p2align 5
	.balign 32
