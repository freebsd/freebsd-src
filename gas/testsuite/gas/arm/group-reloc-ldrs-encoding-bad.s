@ Tests that are meant to fail during encoding of LDRS group relocations.

	.text

	.macro ldrtest2 load sym offset

	\load	r0, [r0, #:pc_g1:(\sym \offset)]
	\load	r0, [r0, #:pc_g2:(\sym \offset)]
	\load	r0, [r0, #:sb_g0:(\sym \offset)]
	\load	r0, [r0, #:sb_g1:(\sym \offset)]
	\load	r0, [r0, #:sb_g2:(\sym \offset)]

	.endm

	.macro ldrtest load store sym offset

	ldrtest2 \load \sym \offset

	\store	r0, [r0, #:pc_g1:(\sym \offset)]
	\store	r0, [r0, #:pc_g2:(\sym \offset)]
	\store	r0, [r0, #:sb_g0:(\sym \offset)]
	\store	r0, [r0, #:sb_g1:(\sym \offset)]
	\store	r0, [r0, #:sb_g2:(\sym \offset)]

	.endm

@ LDRD/STRD/LDRH/STRH/LDRSH/LDRSB only have 8 bits available for the 
@ magnitude of the addend.  So these should all (just) fail.

	ldrtest ldrd strd f "+ 256"
	ldrtest ldrh strh f "+ 256"
	ldrtest2 ldrsh f "+ 256"
	ldrtest2 ldrsb f "+ 256"

	ldrtest ldrd strd f "- 256"
	ldrtest ldrh strh f "- 256"
	ldrtest2 ldrsh f "- 256"
	ldrtest2 ldrsb f "- 256"

@ The same as the above, but for a local symbol.

	ldrtest ldrd strd localsym "+ 256"
	ldrtest ldrh strh localsym "+ 256"
	ldrtest2 ldrsh localsym "+ 256"
	ldrtest2 ldrsb localsym "+ 256"

	ldrtest ldrd strd localsym "- 256"
	ldrtest ldrh strh localsym "- 256"
	ldrtest2 ldrsh localsym "- 256"
	ldrtest2 ldrsb localsym "- 256"

localsym:
	mov	r0, #0

