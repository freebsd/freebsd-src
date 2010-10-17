	.code

	.align 4
; In gas-1.36 the ldil instruction using parenthesis generated
; garbage bits while the one without parens worked fine.
	ldil L%(0x00040000 | 0x00000008 | 0x00000002),%r21
	ldo L%(0x00040000 | 0x00000008 | 0x00000002) (%r21),%r21
	ldil L%0x00040000 | 0x00000008 | 0x00000002,%r21
	ldo L%0x00040000 | 0x00000008 | 0x00000002 (%r21),%r21

