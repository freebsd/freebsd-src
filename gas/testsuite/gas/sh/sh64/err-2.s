! { dg-do assemble }
! { dg-options "--abi=32 --isa=shmedia" }
!

! This is a mainly a copy of movi64-2.s, but we check that out-of-range
! errors are emitted for the 32-bit ABI.
	.text
start:
	movi  65536 << 16,r3	! { dg-error "not a 32-bit signed value" }
	movi  -32769 << 16,r3	! { dg-error "not a 32-bit signed value" }
	movi  32768 << 16,r3
	movi  -32768 << 16,r3
	movi  32767 << 48,r3	! { dg-error "not a 32-bit signed value" }
	movi  32768 << 48,r3	! { dg-error "not a 32-bit signed value" }
	movi  -32768 << 48,r3	! { dg-error "not a 32-bit signed value" }

