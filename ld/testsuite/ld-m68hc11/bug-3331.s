;;; Bug #3331:	Invalid group relaxation, bset uses an invalid address
;;; http://savannah.gnu.org/bugs/?func=detailbug&bug_id=3331&group_id=2424
;;; 
	.sect .text
	.globl _start
_start:
	.relax	L1
	ldx	#foo		;; This relax group must not be changed.
	bset	0,x #4
L1:
	ldd	#2
	std	table		;; This instruction uses a symbol in page0
				;; and it triggered the relaxation of the
				;; previous relax group
	rts

	.sect .page0
	.globl table
table:	.long 0

	.sect .data
	.globl foo
foo:	.long 0
