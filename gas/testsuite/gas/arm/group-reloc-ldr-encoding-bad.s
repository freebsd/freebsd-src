@ Tests that are supposed to fail during encoding
@ for LDR group relocations.

	.text

	.macro ldrtest load store sym offset

	\load	r0, [r0, #:pc_g0:(\sym \offset)]
	\load	r0, [r0, #:pc_g1:(\sym \offset)]
	\load	r0, [r0, #:pc_g2:(\sym \offset)]
	\load	r0, [r0, #:sb_g0:(\sym \offset)]
	\load	r0, [r0, #:sb_g1:(\sym \offset)]
	\load	r0, [r0, #:sb_g2:(\sym \offset)]

	\store	r0, [r0, #:pc_g0:(\sym \offset)]
	\store	r0, [r0, #:pc_g1:(\sym \offset)]
	\store	r0, [r0, #:pc_g2:(\sym \offset)]
	\store	r0, [r0, #:sb_g0:(\sym \offset)]
	\store	r0, [r0, #:sb_g1:(\sym \offset)]
	\store	r0, [r0, #:sb_g2:(\sym \offset)]

	.endm

@ LDR/STR/LDRB/STRB only have 12 bits available for the magnitude of the addend.
@ So these should all fail.

	ldrtest ldr str f "+ 4096"
	ldrtest ldrb strb f "+ 4096"
	ldrtest ldr str f "- 4096"
	ldrtest ldrb strb f "- 4096"

	ldrtest ldr str localsym "+ 4096"
	ldrtest ldrb strb localsym "+ 4096"
	ldrtest ldr str localsym "- 4096"
	ldrtest ldrb strb localsym "- 4096"

localsym:
	mov	r0, #0

