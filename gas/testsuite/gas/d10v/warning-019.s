	;; Dest reg conflict

	.text
foo:
	add r0,r1       || mv r0,r2
	