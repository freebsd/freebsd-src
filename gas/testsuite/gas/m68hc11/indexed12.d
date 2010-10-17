#objdump: -S
#as: -m68hc12 -gdwarf2
#name: 68HC12 indexed addressing mode with 5, 9 and 16-bit offsets (indexed12)
#source: indexed12.s

.*: +file format elf32\-m68hc12

Disassembly of section .text:

0+ <_main>:
;;
	.sect .text
	.globl _main
_main:
	nop
   0:	a7          	nop
;;; Global check \(1st\)
	ldab	L1\-_main,x	; Offset/const of these 2 insns must be
   1:	e6 e0 93    	ldab	147,X
	ldaa	#L1\-_main	; identical \(likewise for 2nd global check\)
   4:	86 93       	ldaa	#147
;;; Test gas relax with difference of symbols \(same section\)
	ldaa	L2\-L1,x		; \-> ldaa 2,x \(5\-bit offset\), text seg
   6:	a6 02       	ldaa	2,X
	adda	L1\-L2,y		; \-> adda \-2,y \(5\-bit offset\), text seg
   8:	ab 5e       	adda	\-2,Y

	orab	L7\-L6,sp	; \-> orab 8,sp  \(5\-bit offset\), text seg
   a:	ea 88       	orab	8,SP
	anda	L8\-L7,sp	; \-> anda 15,sp \(5\-bit offset\), text seg
   c:	a4 8f       	anda	15,SP
	eora	L7\-L8,sp	; \-> eora \-15,sp \(5\-bit offset\), text seg
   e:	a8 91       	eora	\-15,SP
	eorb	L7\-L9,sp	; \-> eorb \-16,sp \(5\-bit offset\), text seg
  10:	e8 90       	eorb	\-16,SP

	andb	L9\-L7,sp	; \-> andb 16,sp \(9\-bit offset\), text seg
  12:	e4 f0 10    	andb	16,SP
	staa	L7\-L10,x	; \-> staa \-17,x \(9\-bit offset\), text seg
  15:	6a e1 ef    	staa	\-17,X
	stab	L11\-L10,y	; \-> stab 128,y \(9\-bit offset\), text seg
  18:	6b e8 80    	stab	128,Y
	stab	L10\-L11,y	; \-> stab \-128,y \(9\-bit offset\), text seg
  1b:	6b e9 80    	stab	\-128,Y
	stab	L11\-L10\+1,y	; \-> stab 129,y \(9\-bit offset\), text seg
  1e:	6b e8 81    	stab	129,Y
	stab	L10\-L11\-1,y	; \-> stab \-129,y \(9\-bit offset\), text seg
  21:	6b e9 7f    	stab	\-129,Y
	stab	L11\-1\-L10,y	; \-> stab 127,y \(9\-bit offset\), text seg
  24:	6b e8 7f    	stab	127,Y
	stab	L10\-1\-L11,y	; \-> stab \-129,y \(9\-bit offset\), text seg
  27:	6b e9 7f    	stab	\-129,Y

	tst	L12\-L10,x	; \-> tst 255,x \(9\-bit offset\), text seg
  2a:	e7 e0 ff    	tst	255,X
	tst	L10\-L12,x	; \-> tst \-255,x \(9\-bit offset\), text seg
  2d:	e7 e1 01    	tst	\-255,X
	tst	L12\-L10\+1,x	; \-> tst 256,x \(16\-bit offset\), text seg
  30:	e7 e2 01 00 	tst	256,X
	mina	L13\-L10,x	; \-> mina 256,x \(16\-bit offset\)
  34:	18 19 e2 01 	mina	256,X
  38:	00 
	mina	L10\-L13,x	; \-> mina \-256,x \(9\-bit offset\)
  39:	18 19 e1 00 	mina	\-256,X

	maxa	L14\-L10,x	; \-> maxa 257,x \(16\-bit offset\)
  3d:	18 18 e2 01 	maxa	257,X
  41:	01 
	maxa	L10\-L14,x	; \-> maxa \-257,x \(16\-bit offset\)
  42:	18 18 e2 fe 	maxa	\-257,X
  46:	ff 

;;; Test gas relax with difference of symbols \(different section\)
	ldaa	D2\-D1,x		; \-> ldaa 2,x \(5\-bit offset\), data seg
  47:	a6 02       	ldaa	2,X
	adda	D1\-D2,y		; \-> adda \-2,y \(5\-bit offset\), data seg
  49:	ab 5e       	adda	\-2,Y

	orab	D7\-D6,sp	; \-> orab 8,sp  \(5\-bit offset\), data seg
  4b:	ea 88       	orab	8,SP
	anda	D8\-D7,sp	; \-> anda 15,sp \(5\-bit offset\), data seg
  4d:	a4 8f       	anda	15,SP
	eora	D7\-D8,sp	; \-> eora \-15,sp \(5\-bit offset\), data seg
  4f:	a8 91       	eora	\-15,SP
	eorb	D7\-D9,sp	; \-> eorb \-16,sp \(5\-bit offset\), data seg
  51:	e8 90       	eorb	\-16,SP

	andb	D9\-D7,sp	; \-> andb 16,sp \(9\-bit offset\), data seg
  53:	e4 f0 10    	andb	16,SP
	staa	D7\-D10,x	; \-> staa \-17,x \(9\-bit offset\), data seg
  56:	6a e1 ef    	staa	\-17,X
	stab	D11\-D10,y	; \-> stab 128,y \(9\-bit offset\), data seg
  59:	6b e8 80    	stab	128,Y
	stab	D10\-D11,y	; \-> stab \-128,y \(9\-bit offset\), data seg
  5c:	6b e9 80    	stab	\-128,Y
	stab	D11\-D10\+1,y	; \-> stab 129,y \(9\-bit offset\), data seg
  5f:	6b e8 81    	stab	129,Y
	stab	D10\-D11\+1,y	; \-> stab \-127,y \(9\-bit offset\), data seg
  62:	6b e9 81    	stab	\-127,Y
	stab	D11\-1\-D10,y	; \-> stab 127,y \(9\-bit offset\), data seg
  65:	6b e8 7f    	stab	127,Y
	stab	D10\-1\-D11,y	; \-> stab \-129,y \(9\-bit offset\), data seg
  68:	6b e9 7f    	stab	\-129,Y

	tst	D12\-D10,x	; \-> tst 255,x \(9\-bit offset\), data seg
  6b:	e7 e0 ff    	tst	255,X
	tst	D10\-D12,x	; \-> tst \-255,x \(9\-bit offset\), data seg
  6e:	e7 e1 01    	tst	\-255,X
	tst	D12\-D10\+1,x	; \-> tst 256,x \(16\-bit offset\), data seg
  71:	e7 e2 01 00 	tst	256,X
	mina	D13\-D10,x	; \-> mina 256,x \(16\-bit offset\)
  75:	18 19 e2 01 	mina	256,X
  79:	00 
	mina	D10\-D13,x	; \-> mina \-256,x \(9\-bit offset\)
  7a:	18 19 e1 00 	mina	\-256,X

	maxa	D14\-D10,x	; \-> maxa 257,x \(16\-bit offset\)
  7e:	18 18 e2 01 	maxa	257,X
  82:	01 
	maxa	D10\-D14,x	; \-> maxa \-257,x \(16\-bit offset\)
  83:	18 18 e2 fe 	maxa	\-257,X
  87:	ff 

;;; Global check \(2nd\)
	ldab	L1\-_main,x
  88:	e6 e0 93    	ldab	147,X
	ldaa	#L1\-_main
  8b:	86 93       	ldaa	#147

;;; Indexed addressing with external symbol
	ldab	_external\+128,x
  8d:	e6 e2 00 80 	ldab	128,X
	bra	L2
  91:	20 02       	bra	95 <L2>

0+93 <L1>:
  93:	aa bb       	oraa	5,SP\-

0+95 <L2>:
  95:	a7          	nop
  96:	a7          	nop
  97:	a7          	nop
  98:	a7          	nop
  99:	a7          	nop
  9a:	a7          	nop
  9b:	a7          	nop
  9c:	a7          	nop

0+9d <L7>:
  9d:	a7          	nop
  9e:	a7          	nop
  9f:	a7          	nop
  a0:	a7          	nop
  a1:	a7          	nop
  a2:	a7          	nop
  a3:	a7          	nop
  a4:	a7          	nop
  a5:	a7          	nop
  a6:	a7          	nop
  a7:	a7          	nop
  a8:	a7          	nop
  a9:	a7          	nop
  aa:	a7          	nop
  ab:	a7          	nop

0+ac <L8>:
L1:
	.dc.w	0xaabb
L2:
L6:
	.ds.b	8, 0xa7
L7:
	.ds.b	15, 0xa7
L8:
	nop
  ac:	a7          	nop

0+ad <L9>:
L9:
	nop
  ad:	a7          	nop

0+ae <L10>:
	...

0+12e <L11>:
	...

0+1ad <L12>:
L10:
	.skip	128
L11:
	.skip	127
L12:
	nop
 1ad:	a7          	nop

0+1ae <L13>:
L13:
	nop
 1ae:	a7          	nop

0+1af <L14>:
L14:
	rts
 1af:	3d          	rts
