! Check command-line error checking.  The option -no-expand is not valid
! unless SHcompact/SHmedia is specified.

! { dg-do assemble }
! { dg-options "-no-expand" }
! { dg-error ".* only valid with SHcompact or SHmedia" "" { target sh64-*-elf* } 0 }

	.text
start:
	nop
