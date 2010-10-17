! Check command-line error checking.  The option -expand-pt32 is invalid with
! -abi=32 just as it is invalid with no SHmedia/SHcompact options.

! { dg-do assemble }
! { dg-options "-abi=32 -expand-pt32" }
! { dg-error ".* only valid with -abi=64" "" { target sh64-*-* } 0 }

	.text
start:
	nop
