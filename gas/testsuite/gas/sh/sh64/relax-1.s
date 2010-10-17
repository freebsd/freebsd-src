! Check limits of PT assembler relaxation states.  Unfortunately, we can't
! check the 32 and 48 bit limit on a host with 32-bit longs, so we just
! check the first state.  This also checks that a PT expansion without a
! relocation to 32 bits works.

	.mode SHmedia
start:
	nop
start2:
	pt	x0,tr3
x1:
	pt	x0,tr4
	.space 32767*4-4,0
x0:
	pt	x1,tr5
	pt	x1,tr6
	pt	x1,tr6
	pt	x1,tr7
