@ Tests that should fail for ALU group relocations.

	.text

	.macro alutest insn sym offset

	\insn	r0, r0, #:pc_g0:(\sym + \offset)
	\insn	r0, r0, #:pc_g1:(\sym + \offset)
	\insn	r0, r0, #:pc_g2:(\sym + \offset)

	\insn	r0, r0, #:pc_g0_nc:(\sym + \offset)
	\insn	r0, r0, #:pc_g1_nc:(\sym + \offset)

	\insn	r0, r0, #:sb_g0:(\sym + \offset)
	\insn	r0, r0, #:sb_g1:(\sym + \offset)
	\insn	r0, r0, #:sb_g2:(\sym + \offset)

	\insn	r0, r0, #:sb_g0_nc:(\sym + \offset)
	\insn	r0, r0, #:sb_g1_nc:(\sym + \offset)

	.endm

	alutest add f 0x11001
	alutest add localsym 0x11001
	alutest adds f 0x11001
	alutest adds localsym 0x11001

	alutest add f "-0x11001"
	alutest add localsym "-0x11001"
	alutest adds f "-0x11001"
	alutest adds localsym "-0x11001"

localsym:
	mov	r0, #0

