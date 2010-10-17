! Check command-line error checking.  The option -expand-pt32 is invalid with
! -no-expand.

! { dg-do assemble }
! { dg-options "-abi=64 -expand-pt32 -no-expand" }
! { dg-error ".* invalid together with -no-expand" "" { target sh64-*-* } 0 }

	.text
start:
	nop
