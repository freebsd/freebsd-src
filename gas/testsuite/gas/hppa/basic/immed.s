	.code
	.align 4
; Basic immediate instruction tests.  
;
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
immediate_tests: 
	ldo 5(%r26),%r26
	ldil L%0xdeadbeef,%r26
	addil L%0xdeadbeef,%r5

