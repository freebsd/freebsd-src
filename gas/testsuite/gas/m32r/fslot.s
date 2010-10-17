# Test the FILL-SLOT attribute.
# The FILL-SLOT attribute ensures the next insn begins on a 32 byte boundary.
# This is needed for example with bl because the subroutine will return
# to a 32 bit boundary.

	.text
bl:
	bl bl
	ldi r0,#8
bl_s:
	bl.s bl_s
	ldi r0,#8

bra:
	bra bra
	ldi r0,#8
bra_s:
	bra.s bra_s
	ldi r0,#8

jl:
	jl r0
	ldi r0,#8

trap:
	trap #4
	ldi r0,#8
