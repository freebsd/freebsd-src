	@ test ADRL pseudo-op
	.text
	.global foo
foo:
	.align 0
1:
        .space 8192
2:
        adrl    r0, 1b
	adrl	r0, 1f
        adrl    r0, 2b
	adrl	r0, 2f
	adrEQl	r0, 2f
2:
	adrl	r0, foo
	adrl	r0, X
	.space 8184
1:
	adral	lr, X
	.space	0x0104

	.globl X; 
X:
	.p2align 5,0
