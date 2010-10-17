! A SHmedia object, that we will link to a SHcompact object.
! We will be using .text for the SHmedia code and .text.compact for the
! SHcompact code, so we don't get two ISA in the same section.
	.text
	.mode SHmedia

	.global start
	.global medialabel1
	.global medialabel2
	.global medialabel3
start:
	movi compactlabel1,r14
	movi compactlabel4,r14
medialabel1:
	pt  compactlabel2,tr6
medialabel2:
	nop

	.section .rodata
	.long compactlabel3
medialabel3:
	.long compactlabel5

	.data
	.global medialabel4
	.long 0
medialabel4:
	.long compactlabel2
