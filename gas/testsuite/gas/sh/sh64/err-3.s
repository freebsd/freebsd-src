! { dg-do assemble }
! { dg-options "--abi=32" }

! Check that we get errors for immediate operands with expressions with
! resolvable differences between local symbols, but not in range for the
! operands, and no errors for nearby valid values.

	.text
	.mode SHmedia
start:
	addi r50,.Lab500 - .Lab1,r40
	addi r50,.Lab1000 - .Lab1,r40		! { dg-error "not a 10-bit signed value" }
	addi r50,.Lab500 - .Lab1 + 1,r40
	addi r50,.Lab500 - .Lab1 + 2,r40
	ld.uw r30,.Lab1000 - .Lab1,r40
	ld.uw r30,.Lab500 - .Lab1 + 1,r40	! { dg-error "not an even value" }
	ld.uw r30,.Lab500 - .Lab1 + 2,r40
	ld.uw r50,.Lab2000 - .Lab1,r20		! { dg-error "not a 11-bit signed value" }
	ld.l r50,.Lab2000 - .Lab1,r20
	ld.l r50,.Lab2000 - .Lab1 + 1,r20	! { dg-error "not a multiple of 4" }
	ld.l r50,.Lab2000 - .Lab1 + 2,r20	! { dg-error "not a multiple of 4" }
	ld.l r50,.Lab4000 - .Lab1,r20		! { dg-error "not a 12-bit signed value" }
	nop

	.data
	.long 0
.Lab1:
	.zero 500,0
.Lab500:
	.zero 500,0
.Lab1000:
	.zero 1000,0
.Lab2000:
	.zero 2000,0
.Lab4000:
	.long 0
