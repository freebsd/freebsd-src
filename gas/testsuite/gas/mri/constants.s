	xdef	s01,s02,s03,s04,s05,s06,s07,s08,s09,s10,s11,s12,s13
s01	equ	%1010
s02	equ	1010b
s03	equ	@12
s04	equ	12o
s05	equ	12q
s06	equ	10
s07	equ	10d
s08	equ	$a
s09	equ	0ah
s10	equ	'a'
s11	equ	A'a'
s12	equ	'abcd'
s13	equ	'a''b'

	xdef	foo
foo
	moveq.l	#%1010,d0
	moveq.l	#1010b,d0
	moveq.l	#@12,d0
	moveq.l	#12o,d0
	moveq.l	#12q,d0
	moveq.l	#10,d0
	moveq.l	#10d,d0
	moveq.l	#$a,d0
	moveq.l	#0ah,d0
	moveq.l	#'a',d0
	moveq.l	#A'a',d0
	nop

	end
