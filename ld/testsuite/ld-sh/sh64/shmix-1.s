! Check mixed-mode objects; different sections holding different ISA:s.
	.mode SHcompact
	.text
	.global start
start:
	bt forw
	mova start2,r0
start2:
	nop
	nop
forw:
	nop
	.align 2
	.long $
	.long start2
	.long mediacode2

	.data
	.long $
	.long start2
	.long mediacode2

	.section .text.media,"ax"
	.mode SHmedia
	.align 2
mediacode:
	ptb forw,tr4
	pt start2,tr5
mediacode2:
	movi start2,r54
	movi mediacode2,r45
	pta mediacode2,tr7
	nop
