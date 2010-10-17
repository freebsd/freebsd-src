# Test some assembler pseudo-operations:
# Floating point moves.

	.text
	fmov.ss		%f5,%f6
	fmov.dd		%f8,%f10
	fmov.sd		%f3,%f20
	fmov.ds		%f24,%f9
	pfmov.ds	%f28,%f3

