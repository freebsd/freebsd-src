# Test the FILL-SLOT attribute.
# The FILL-SLOT attribute ensures the next insn begins on a 32 byte boundary.
# This is needed for example with bl because the subroutine will return
# to a 32 bit boundary.

	.text
bcl:
	bcl bcl
	ldi r0,#8
bcl_s:
	bcl.s bcl_s
	ldi r0,#8

bncl:
	bncl bncl
	ldi r0,#8
bncl_s:
	bncl.s bncl_s
	ldi r0,#8
