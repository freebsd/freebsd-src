# labels should be aligned on 8-byte boundries
	
	.text
	bra.s/tx _abc   || nop 
	nop     || nop 
	.word 0x0e000004
_abc:
	nop
	nop
	nop
	nop
