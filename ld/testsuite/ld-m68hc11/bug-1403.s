;;; Bug #1403:	Branch adjustment to another section not correct when doing linker relaxation
;;; http://savannah.gnu.org/bugs/?func=detailbug&bug_id=1403&group_id=2424
;;; 
	.sect .text
	.globl _start
_start:
	.relax	L1
	ldx	#table
	bset	0,x #4
L1:
	bra	toto		; bra is assembled as a jmp and relaxed

	.sect .page0
	.globl table
table:	.long 0

	.sect	.text.toto
	.globl	toto
toto:
	rts
