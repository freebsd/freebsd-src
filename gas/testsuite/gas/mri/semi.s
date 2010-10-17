semicolon  macro
	dc.b    '; '
	endm

colon	    macro
	dc.b    ': '
	endm

	semicolon
	dc.b	'; '
	colon
	dc.b	': '

	p2align	5
