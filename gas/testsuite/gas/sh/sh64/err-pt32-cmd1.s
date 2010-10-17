! Check command-line error checking.  The option -expand-pt32 is only valid
! with -abi=64

! { dg-do assemble }
! { dg-options "-expand-pt32" }
! { dg-error ".* only valid with -abi=64" "" { target sh64-*-* } 0 }

	.text
start:
	nop
