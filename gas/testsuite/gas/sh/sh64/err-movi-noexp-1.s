! Check that we get errors for MOVI operands out-of-range with -no-expand.

! { dg-do assemble }
! { dg-options "--abi=32 --isa=shmedia -no-expand" }

	.text
start:
	movi  externalsym + 123,r3
	movi  65535,r3		! { dg-error "not a 16-bit signed value" }
	movi  65536,r3		! { dg-error "not a 16-bit signed value" }
	movi  65535 << 16,r3	! { dg-error "not a 16-bit signed value" }
	movi  32767,r3
	movi  32768,r3		! { dg-error "not a 16-bit signed value" }
	movi  32767 << 16,r3	! { dg-error "not a 16-bit signed value" }
	movi  -32768,r3
	movi  -32769,r3		! { dg-error "not a 16-bit signed value" }
	movi  -32768 << 16,r3	! { dg-error "not a 16-bit signed value" }
	movi  localsym + 73,r4
	movi  forwardsym - 42,r4
	.set forwardsym,47

	.data
localsym:
	.long 1
