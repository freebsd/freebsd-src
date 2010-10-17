; Tests the broken-word function, single word.
	.syntax no_register_prefix
sym2:	moveq 0,r0

	.word	sym1 - sym2

	moveq 1,r0
; Short jump around secondary jump table inserted here :
;	ba	next_label
;	nop
;	.skip	2,0
; Secondary jump table inserted here :
;	jump	sym1
next_label:
	moveq 2,r0

	.space	32766, 0

sym1:	moveq 3,r0
