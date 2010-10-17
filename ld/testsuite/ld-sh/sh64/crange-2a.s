! Simple example with assembler-generated .cranges that do not need more
! .cranges added by the linker: A single section with SHmedia, constants
! and SHcompact.
	.section .text.mixed,"ax"
	.align 2
! Make sure this symbol does not have the expected type.
	.mode SHcompact
	.global diversion2
diversion2:

	.mode SHmedia
start2:
	nop
	nop
	nop

	.long 42
	.long 43

	.mode SHcompact
	nop
	nop
