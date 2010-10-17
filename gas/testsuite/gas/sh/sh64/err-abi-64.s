! Check .abi pseudo assertion.

! { dg-do assemble }
! { dg-options "-abi=32" }

	.text
	.abi 64		! { dg-error "options do not specify 64-bit ABI" }

start:
	nop
