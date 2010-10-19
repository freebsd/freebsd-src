; Tests the broken-word function with a real switch table.  CRISv32 version.

start:	moveq 0,r0

	subs.b 87,r0
	bound.b 41,r0
	lapc sym2,acr
	addi r0.w,acr
	adds.w [acr],acr
	jump acr
	nop
sym2:
	.word	sym1 - .
	.word	sym3 - .
	.word	sym4 - .
	.word	sym5 - .
	.word	sym6 - .
	.word	sym7 - .
	.word	sym8 - .
	.word	sym9 - .
	.word	sym10 - .
	.word	sym11 - .
	.word	sym12 - .
	.word	sym13 - .
	.word	sym14 - .
	.word	sym15 - .
	.word	sym16 - .
	.word	sym17 - .
	.word	sym18 - .
	.word	sym19 - .
	.word	sym20 - .
	.word	sym21 - .
	.word	sym22 - .
	.word	sym23 - .
	.word	sym24 - .
	.word	sym25 - .
	.word	sym26 - .
	.word	sym27 - .
	.word	sym28 - .
	.word	sym29 - .
	.word	sym30 - .
	.word	sym31 - .
	.word	sym32 - .
	.word	sym33 - .
	.word	sym34 - .
	.word	sym35 - .
	.word	sym36 - .
	.word	sym37 - .
	.word	sym38 - .
	.word	sym39 - .
	.word	sym40 - .
	.word	sym41 - .
	.word	sym42 - .
	.word	sym43 - .

	.space	16, 0

	moveq 1,r0
; Medium-range branch around secondary jump table inserted here :
;	ba	next_label
;	nop
;	.skip	2,0
; Secondary jump table inserted here :
;	ba	sym1
;	nop
;	ba	sym3
;	nop
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
