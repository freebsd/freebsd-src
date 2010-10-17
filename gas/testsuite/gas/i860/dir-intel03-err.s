# Intel assembler directives:
# The .dual, .enddual, and .atmp directives are valid only
# in Intel syntax mode.  Check that we issue an error if in
# AT&T/SVR4 mode.

	.text

	.atmp r31

	.dual
	fsub.ss	%f22,%f21,%f13
	nop
	.enddual

