; Tests the broken-word function, one more word than fits in a
; single branch.
	.syntax no_register_prefix
start:	moveq 0,r0

; Take the opportunity to (rudimentally) test case-recognition,
; as the diassembler gets overly confused by the table.
	subs.b 87,r0
	bound.b 41,r0
	adds.w [pc+r0.w],pc 
sym2:
	.word	sym1 - sym2
	.word	sym3 - sym2
	.word	sym4 - sym2
	.word	sym5 - sym2
	.word	sym6 - sym2
	.word	sym7 - sym2
	.word	sym8 - sym2
	.word	sym9 - sym2
	.word	sym10 - sym2
	.word	sym11 - sym2
	.word	sym12 - sym2
	.word	sym13 - sym2
	.word	sym14 - sym2
	.word	sym15 - sym2
	.word	sym16 - sym2
	.word	sym17 - sym2
	.word	sym18 - sym2
	.word	sym19 - sym2
	.word	sym20 - sym2
	.word	sym21 - sym2
	.word	sym22 - sym2
	.word	sym23 - sym2
	.word	sym24 - sym2
	.word	sym25 - sym2
	.word	sym26 - sym2
	.word	sym27 - sym2
	.word	sym28 - sym2
	.word	sym29 - sym2
	.word	sym30 - sym2
	.word	sym31 - sym2
	.word	sym32 - sym2
	.word	sym33 - sym2
	.word	sym34 - sym2
	.word	sym35 - sym2
	.word	sym36 - sym2
	.word	sym37 - sym2
	.word	sym38 - sym2
	.word	sym39 - sym2
	.word	sym40 - sym2
	.word	sym41 - sym2
	.word	sym42 - sym2
	.word	sym43 - sym2

	.space	16, 0

	moveq 1,r0
; Medium-range branch around secondary jump table inserted here :
;	ba	next_label
;	nop
;	.skip	2,0
; Secondary jump table inserted here :
;	jump	sym1
;	jump	sym3
;	...
next_label:
	moveq 2,r0

	.space	32768, 0

sym1:	moveq -3,r0
sym3: moveq 3,r0
sym4: moveq 4,r0
sym5: moveq 5,r0
sym6: moveq 6,r0
sym7: moveq 7,r0
sym8: moveq 8,r0
sym9: moveq 9,r0
sym10: moveq 10,r0
sym11: moveq 11,r0
sym12: moveq 12,r0
sym13: moveq 13,r0
sym14: moveq 14,r0
sym15: moveq 15,r0
sym16: moveq 16,r0
sym17: moveq 17,r0
sym18: moveq 18,r0
sym19: moveq 19,r0
sym20: moveq 20,r0
sym21: moveq 21,r0
sym22: moveq 22,r0
sym23: moveq 23,r0
sym24: moveq 24,r0
sym25: moveq 25,r0
sym26: moveq 26,r0
sym27: moveq 27,r0
sym28: moveq 28,r0
sym29: moveq 29,r0
sym30: moveq 30,r0
sym31: moveq 31,r0
sym32: moveq -32,r0
sym33: moveq -31,r0
sym34: moveq -30,r0
sym35: moveq -29,r0
sym36: moveq -28,r0
sym37: moveq -27,r0
sym38: moveq -26,r0
sym39: moveq -25,r0
sym40: moveq -24,r0
sym41: moveq -23,r0
sym42: moveq -22,r0
sym43: moveq -21,r0
