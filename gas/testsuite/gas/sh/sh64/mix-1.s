! Check mixed-mode objects; different sections holding different ISA:s.
	.mode SHcompact
	.text
start:
	bt forw
	mova start2,r0
start2:
	nop
forw:
	nop

	.section .text.media,"ax"
	.mode SHmedia
mediacode:
	ptb forw,tr4
	pt start2,tr5
mediacode2:
	movi start2,r54
	movi mediacode2,r45
	pta mediacode2,tr7
	nop
