;;; Test 68HC11 linker relaxation and fixup of brclr/brset branches
;;; 
	.sect .text
	.globl _start
_start:
start:
	brclr	140,x#200,L8	; Branch adjustment covers the whole test
;;; The 'addd' is relaxed and we win 1 byte.  The next brclr/brset
;;; branch must be fixed and reduced by 1.  We check for different
;;; addressing modes because the instruction has different opcode and
;;; different lengths.
L1:
	addd	_toto
	brclr	20,x,#3,L1
	brclr	90,x,#99,L3	; Likewise with forward branch
L2:
	addd	_toto
	brclr	19,y,#4,L2
	brclr	91,y,#98,L4
L3:
	addd	_toto
	brset	18,x,#5,L3
	brset	92,x,#97,L5
L4:
	addd	_toto
	brset	17,y,#6,L4
	brset	93,y,#96,L5
L5:
	addd	_toto
	brset	*_table,#7,L5
	brset	*_table+10,#95,L7
L6:
	addd	_toto
	brclr	*_table+1,#8,L6
	brset	*_table+11,#94,L8
L7:
	addd	_toto
	brclr	*_table+1,#8,L6
L8:
	brclr	140,x#200,_start ; Branch adjustment covers the whole test
	rts

	.sect	.page0
_bar:
	.long	0
_toto:
	.long	0
	.skip	32
stack:
	.skip	10
_table:
