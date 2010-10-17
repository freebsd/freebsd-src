	;; ops with immediate args

	.text
	.global foo
foo:	
        rac     r4,a0,bad_value
	