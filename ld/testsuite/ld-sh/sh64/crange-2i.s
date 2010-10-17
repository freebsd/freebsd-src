! If this file comes before a file with a SHcompact .text section but with
! no symbols, we will have a symbol of the "wrong kind" before the
! SHcompact insns.
	.section .text,"ax"
	.mode SHmedia
	.align 2
	.global diversion
diversion:
