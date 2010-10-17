! Check .abi pseudo assertion.

! { dg-do assemble }
! { dg-options "-abi=64" }

	.text
	.abi 32		! { dg-error "options do not specify 32-bit ABI" }

start:
	nop
