;;
;; This file verifies the 68HC12 indexed addressing modes
;; with a 5, 9 and 16-bit offset.
;;
	.sect .text
	.globl _main
_main:
	nop
;;; Global check (1st)
	ldab	L1-_main,x	; Offset/const of these 2 insns must be
	ldaa	#L1-_main	; identical (likewise for 2nd global check)
;;; Test gas relax with difference of symbols (same section)
	ldaa	L2-L1,x		; -> ldaa 2,x (5-bit offset), text seg
	adda	L1-L2,y		; -> adda -2,y (5-bit offset), text seg

	orab	L7-L6,sp	; -> orab 8,sp  (5-bit offset), text seg
	anda	L8-L7,sp	; -> anda 15,sp (5-bit offset), text seg
	eora	L7-L8,sp	; -> eora -15,sp (5-bit offset), text seg
	eorb	L7-L9,sp	; -> eorb -16,sp (5-bit offset), text seg

	andb	L9-L7,sp	; -> andb 16,sp (9-bit offset), text seg
	staa	L7-L10,x	; -> staa -17,x (9-bit offset), text seg
	stab	L11-L10,y	; -> stab 128,y (9-bit offset), text seg
	stab	L10-L11,y	; -> stab -128,y (9-bit offset), text seg
	stab	L11-L10+1,y	; -> stab 129,y (9-bit offset), text seg
	stab	L10-L11-1,y	; -> stab -129,y (9-bit offset), text seg
	stab	L11-1-L10,y	; -> stab 127,y (9-bit offset), text seg
	stab	L10-1-L11,y	; -> stab -129,y (9-bit offset), text seg

	tst	L12-L10,x	; -> tst 255,x (9-bit offset), text seg
	tst	L10-L12,x	; -> tst -255,x (9-bit offset), text seg
	tst	L12-L10+1,x	; -> tst 256,x (16-bit offset), text seg
	mina	L13-L10,x	; -> mina 256,x (16-bit offset)
	mina	L10-L13,x	; -> mina -256,x (9-bit offset)

	maxa	L14-L10,x	; -> maxa 257,x (16-bit offset)
	maxa	L10-L14,x	; -> maxa -257,x (16-bit offset)

;;; Test gas relax with difference of symbols (different section)
	ldaa	D2-D1,x		; -> ldaa 2,x (5-bit offset), data seg
	adda	D1-D2,y		; -> adda -2,y (5-bit offset), data seg

	orab	D7-D6,sp	; -> orab 8,sp  (5-bit offset), data seg
	anda	D8-D7,sp	; -> anda 15,sp (5-bit offset), data seg
	eora	D7-D8,sp	; -> eora -15,sp (5-bit offset), data seg
	eorb	D7-D9,sp	; -> eorb -16,sp (5-bit offset), data seg

	andb	D9-D7,sp	; -> andb 16,sp (9-bit offset), data seg
	staa	D7-D10,x	; -> staa -17,x (9-bit offset), data seg
	stab	D11-D10,y	; -> stab 128,y (9-bit offset), data seg
	stab	D10-D11,y	; -> stab -128,y (9-bit offset), data seg
	stab	D11-D10+1,y	; -> stab 129,y (9-bit offset), data seg
	stab	D10-D11+1,y	; -> stab -127,y (9-bit offset), data seg
	stab	D11-1-D10,y	; -> stab 127,y (9-bit offset), data seg
	stab	D10-1-D11,y	; -> stab -129,y (9-bit offset), data seg

	tst	D12-D10,x	; -> tst 255,x (9-bit offset), data seg
	tst	D10-D12,x	; -> tst -255,x (9-bit offset), data seg
	tst	D12-D10+1,x	; -> tst 256,x (16-bit offset), data seg
	mina	D13-D10,x	; -> mina 256,x (16-bit offset)
	mina	D10-D13,x	; -> mina -256,x (9-bit offset)

	maxa	D14-D10,x	; -> maxa 257,x (16-bit offset)
	maxa	D10-D14,x	; -> maxa -257,x (16-bit offset)

;;; Global check (2nd)
	ldab	L1-_main,x
	ldaa	#L1-_main

;;; Indexed addressing with external symbol
	ldab	_external+128,x
	bra	L2
L1:
	.dc.w	0xaabb
L2:
L6:
	.ds.b	8, 0xa7
L7:
	.ds.b	15, 0xa7
L8:
	nop
L9:
	nop
L10:
	.skip	128
L11:
	.skip	127
L12:
	nop
L13:
	nop
L14:
	rts

	.sect .data
D1:
	.dc.w	0xaabb
D2:
D6:
	.ds.b	8, 0xa7
D7:
	.ds.b	15, 0xa7
D8:
	nop
D9:
	nop
D10:
	.skip	128
D11:
	.skip	127
D12:
	nop
D13:
	nop
D14:
