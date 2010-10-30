@ Tests for ALU group relocations.

	.text

	.macro alutest insn sym offset

	\insn	r0, r0, #:pc_g0:(\sym \offset)
	\insn	r0, r0, #:pc_g1:(\sym \offset)

@ Try this one without the hash; it should still work.
	\insn	r0, r0, :pc_g2:(\sym \offset)

	\insn	r0, r0, #:pc_g0_nc:(\sym \offset)
	\insn	r0, r0, #:pc_g1_nc:(\sym \offset)

	\insn	r0, r0, #:sb_g0:(\sym \offset)
	\insn	r0, r0, #:sb_g1:(\sym \offset)
	\insn	r0, r0, #:sb_g2:(\sym \offset)

	\insn	r0, r0, #:sb_g0_nc:(\sym \offset)
	\insn	r0, r0, #:sb_g1_nc:(\sym \offset)

	.endm

	alutest add f "+ 0x100"
	alutest add localsym "+ 0x100"
	alutest adds f "+ 0x100"
	alutest adds localsym "+ 0x100"

@ The following should cause the insns to be switched to SUB(S).

	alutest add f "- 0x100"
	alutest add localsym "- 0x100"
	alutest adds f "- 0x100"
	alutest adds localsym "- 0x100"

localsym:
	mov	r0, #0

