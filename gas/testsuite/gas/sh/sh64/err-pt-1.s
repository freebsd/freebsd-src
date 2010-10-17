! Check that we get errors for a PT operand out of range with -no-relax.

! { dg-do assemble }
! { dg-options "--abi=32 --no-expand" }

	.mode SHmedia
start:
	nop
start2:
	pt	x0,tr3		! { dg-error "operand out of range" }
x1:
	pt	x0,tr4
	.space 32767*4-4,0
x0:
	pt	x1,tr5
	pt	x1,tr6
	pt	x1,tr6		! { dg-error "operand out of range" }
	pt	x1,tr7		! { dg-error "operand out of range" }
