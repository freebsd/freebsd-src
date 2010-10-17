! { dg-do assemble }

! Check that errors are emitted, with no crashes, when an external symbol
! is referenced in a conditional or unconditional branch.
start:
	nop
	bt externsym1	! { dg-error "undefined symbol" }
	nop
	bra externsym2	! { dg-error "undefined symbol" }
	nop

