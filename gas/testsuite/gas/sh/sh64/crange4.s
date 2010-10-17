! This will be two .cranges.  Original problem was that the second one was
! lost because .space just emitted a frag, without calling emit_expr as
! most other data-generating pseudos.

	.mode SHmedia
start:
	nop
	.space 20,0
