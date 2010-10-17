! { dg-do assemble { target sh*-*-elf} }
! { dg-options "-little" }
! { dg-error "-big required" "" { target sh*-*-elf } 0 }

! Check that a mismatch between command-line options and the .big
! directive is identified.

	.big
start:
	nop
