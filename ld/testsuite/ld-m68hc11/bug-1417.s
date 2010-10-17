;;; Bug #1417:	Branch wrong after linker relaxation
;;; http://savannah.gnu.org/bugs/?func=detailbug&bug_id=1417&group_id=2424
;;; 
	.sect .text
	.globl _start
_start:
	tst	table
	bne	L1		; Branch was adjusted but it must not
	jsr	foo
L1:
	.relax	L2
	ldx	#table		; Instruction removed
	bset	0,x #4		; Changed to bset *table #4
L2:
	rts
foo:
	rts

	.sect .page0
	.globl table
table:	.long 0
