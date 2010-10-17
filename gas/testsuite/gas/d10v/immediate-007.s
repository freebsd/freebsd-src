	;; ops with immediate args

	.text
	.align 2
	.global foo

foo:	
        repi	2,bar

	nop 
	nop
	nop
	nop
	nop 
	nop
	nop
	nop
bar:	

