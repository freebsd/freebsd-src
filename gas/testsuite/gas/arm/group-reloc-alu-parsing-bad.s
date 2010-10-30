@ Tests that should fail for ALU group relocations.

	.text

@ Group relocs aren't allowed on SUB(S) instructions...
	sub	r0, r0, #:pc_g0:(foo)
	subs	r0, r0, #:pc_g0:(foo)

@ Some nonexistent relocations:
	add	r0, r0, #:pc_g2_nc:(foo)
	add	r0, r0, #:sb_g2_nc:(foo)

