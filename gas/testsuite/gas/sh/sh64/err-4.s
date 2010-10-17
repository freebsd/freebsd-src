! { dg-do assemble }
! { dg-options "--abi=32 -no-mix" }

! Check that we can't have different ISA:s in the same section if disallowed.

	.text
	.mode SHmedia
start:
	nop

	.mode SHcompact
	nop			! { dg-error "not allowed in same section" }

	.section .text.other,"ax"
	.mode SHmedia
	nop

	.mode SHcompact
	nop			! { dg-error "not allowed in same section" }

	.section .text.more,"ax"
	.mode SHmedia
	nop

	.section .text.yetmore,"ax"
	.mode SHcompact
	nop
