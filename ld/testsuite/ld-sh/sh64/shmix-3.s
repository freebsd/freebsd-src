! A SHcompact object, that we will link to a SHmedia object.
! We will be using .text for the SHmedia code and .text.compact for the
! SHcompact code, so we don't get two ISA in the same section.
	.section .text.compact,"ax"
	.mode SHcompact
	.global compactlabel1
	.global compactlabel2
	.global compactlabel3
	.global compactlabel4
	.global compactlabel5
locallabel:
	nop
compactlabel1:
	mova compactlabel2,r0
compactlabel2:
	mova compactlabel3,r0
	nop
compactlabel3:
	nop
	.align 2
	.long medialabel1
	.long medialabel4

	.section .rodata
	.long medialabel2
compactlabel4:
	.long medialabel3

	.data
	.long 0
compactlabel5:
	.long medialabel4
