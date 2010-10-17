	.code
	.align 4
; We could/should test some of the corner cases for register and 
; immediate fields.  We should also check the assorted field
; selectors to make sure they're handled correctly.
copr_tests: 
	copr,4,5
	copr,4,115
	copr,4,5,n
	copr,4,115,n
