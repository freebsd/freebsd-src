@ Tests for LDR group relocations.

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
@ So these should all (just) work.

	ldrtest ldr str f "+ 4095"
	ldrtest ldrb strb f "+ 4095"
	ldrtest ldr str f "- 4095"
	ldrtest ldrb strb f "- 4095"

@ The same as the above, but for a local symbol.  These should not be
@ resolved by the assembler but instead left to the linker.

	ldrtest ldr str localsym "+ 4095"
	ldrtest ldrb strb localsym "+ 4095"
	ldrtest ldr str localsym "- 4095"
	ldrtest ldrb strb localsym "- 4095"

localsym:
	mov	r0, #0

